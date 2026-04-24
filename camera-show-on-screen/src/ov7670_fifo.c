/*
 * OV7670 + AL422B FIFO camera driver — nRF54L15 DK (Zephyr)
 *
 * Configures the OV7670 for QQVGA (160×120) RGB565 over SCCB (I2C),
 * then reads frames from the companion AL422B FIFO using bit-banged GPIO.
 *
 * Pin mapping:
 *   SCCB (i2c21): SIOD = P1.11 (SDA), SIOC = P1.12 (SCL)
 *   D0-D7  — P2.00-P2.07  (data bus, read as low byte of port 2)
 *   WEN    — P0.04  /WE  active-low  (shared with Button3)
 *   RRST   — P1.14  active-low read-pointer reset  (shared with LED3)
 *   VSYNC  — P2.08  frame-sync input from OV7670
 *   RCK    — P2.09  read clock output  (also LED0 — blinks during readout)
 *   WRST   — P2.10  /WRST active-low write-pointer reset
 */

#include "ov7670_fifo.h"
#include "ov7670_regs.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ov7670_fifo, LOG_LEVEL_INF);

/* ── Device handles ─────────────────────────────────────────────────────── */

static const struct device *const gpio0   = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct device *const gpio1   = DEVICE_DT_GET(DT_NODELABEL(gpio1));
static const struct device *const gpio2   = DEVICE_DT_GET(DT_NODELABEL(gpio2));
static const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));

/* ── FIFO pin assignments ───────────────────────────────────────────────── */

#define PIN_WEN    4   /* P0.04 — /WE  active-low */
#define PIN_RRST  14   /* P1.14 — RRST active-low */
#define PIN_VSYNC  8   /* P2.08 — frame sync input */
#define PIN_RCK    9   /* P2.09 — read clock (also LED0) */
#define PIN_WRST  10   /* P2.10 — /WRST active-low */

/* ── OV7670 QQVGA RGB565 register init table ────────────────────────────── */

static const struct { uint8_t reg; uint8_t val; } init_tbl[] = {
	/* Software reset — 100 ms delay inserted by the write loop */
	{OV7670_REG_COM7,               0x80},
	/* RGB output mode */
	{OV7670_REG_COM7,               0x04},
	/* Use external 12 MHz clock directly, no prescaler */
	{OV7670_REG_CLKRC,              0x80},
	/* Enable downsampling / scaling */
	{OV7670_REG_COM3,               0x04},
	/* Manual scaling + PCLK divider ÷4 */
	{OV7670_REG_COM14,              0x1A},
	/* Downsample by 4 (H and V) */
	{OV7670_REG_SCALING_DCWCTR,     0x22},
	/* DSP clock divider ÷4 */
	{OV7670_REG_SCALING_PCLK_DIV,   0xF2},
	{OV7670_REG_SCALING_XSC,        0x3A},
	{OV7670_REG_SCALING_YSC,        0x35},
	{OV7670_REG_SCALING_PCLK_DELAY, 0x02},
	/* RGB565, full output range [00..FF] */
	{OV7670_REG_COM15,              0xD0},
	/* Disable RGB444 */
	{OV7670_REG_RGB444,             0x00},
	{OV7670_REG_COM1,               0x00},
	{OV7670_REG_TSLB,               0x04},
	/* QQVGA window */
	{OV7670_REG_HSTART,             0x16},
	{OV7670_REG_HSTOP,              0x04},
	{OV7670_REG_HREF,               0x24},
	{OV7670_REG_VSTRT,              0x02},
	{OV7670_REG_VSTOP,              0x7A},
	{OV7670_REG_VREF,               0x0A},
	/* AGC gain ceiling ×8 */
	{OV7670_REG_COM9,               0x48},
	/* Colour matrix */
	{OV7670_REG_MTX1,               0x80},
	{OV7670_REG_MTX2,               0x80},
	{OV7670_REG_MTX3,               0x00},
	{OV7670_REG_MTX4,               0x22},
	{OV7670_REG_MTX5,               0x5E},
	{OV7670_REG_MTX6,               0x80},
	{OV7670_REG_MTXS,               0x9E},
	/* Gamma + UV auto adjust */
	{OV7670_REG_COM13,              0xC0},
	/* Undocumented — required for correct colours on FIFO modules */
	{0xB0,                          0x84},
	/* COM8: AGC + AWB + AEC all on */
	{OV7670_REG_COM8,               0xE7},
	/* Simple AWB */
	{OV7670_REG_AWBCTR0,            0x9F},
};

/* ── Critical registers verified after table write ──────────────────────── */

static const struct {
	uint8_t reg; uint8_t expect; const char *name;
} verify_tbl[] = {
	{OV7670_REG_COM7,             0x04, "COM7  (RGB mode)"},
	{OV7670_REG_COM15,            0xD0, "COM15 (RGB565)"},
	{OV7670_REG_CLKRC,            0x80, "CLKRC (ext clk)"},
	{OV7670_REG_COM14,            0x1A, "COM14 (scale ÷4)"},
	{OV7670_REG_SCALING_DCWCTR,   0x22, "DCWCTR (ds ÷4)"},
	{OV7670_REG_SCALING_PCLK_DIV, 0xF2, "PCLK_DIV (÷4)"},
};

/* ── SCCB (I2C) helpers ─────────────────────────────────────────────────── */

static int write_reg(uint8_t reg, uint8_t val)
{
	uint8_t buf[2] = {reg, val};

	return i2c_write(i2c_dev, buf, 2, OV7670_I2C_ADDR);
}

static int read_reg(uint8_t reg, uint8_t *val)
{
	/*
	 * OV7670 SCCB requires a STOP between the address phase and the data
	 * read — use two separate transactions, not i2c_write_read().
	 */
	int ret = i2c_write(i2c_dev, &reg, 1, OV7670_I2C_ADDR);

	if (ret) {
		return ret;
	}
	return i2c_read(i2c_dev, val, 1, OV7670_I2C_ADDR);
}

/* ── FIFO helpers ───────────────────────────────────────────────────────── */

/* Read the 8-bit data bus: D0-D7 all on P2.00-P2.07. */
static inline uint8_t read_data_bus(void)
{
	gpio_port_value_t v;

	gpio_port_get_raw(gpio2, &v);
	return (uint8_t)(v & 0xFF);
}

static void write_reset(void)
{
	gpio_pin_set_raw(gpio2, PIN_WRST, 0);
	k_busy_wait(1);
	gpio_pin_set_raw(gpio2, PIN_WRST, 1);
	k_busy_wait(1);
}

static void read_reset(void)
{
	/* Assert RRST, clock one rising RCK edge so the AL422B latches the
	 * reset, then deassert. Leave RCK high so the first sample sees
	 * valid data at address 0. */
	gpio_pin_set_raw(gpio1, PIN_RRST, 0);
	k_busy_wait(1);
	gpio_pin_set_raw(gpio2, PIN_RCK, 0);
	k_busy_wait(1);
	gpio_pin_set_raw(gpio2, PIN_RCK, 1);
	k_busy_wait(1);
	gpio_pin_set_raw(gpio1, PIN_RRST, 1);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

int ov7670_init(void)
{
	uint8_t pid, ver;
	int ret;

	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device (i2c21) not ready");
		return -ENODEV;
	}

	/* I2C bus scan — helps diagnose address / wiring issues */
	LOG_INF("I2C scan on i2c21 (P1.11=SDA, P1.12=SCL):");
	bool found = false;
	uint8_t dummy;

	for (uint8_t addr = 0x08; addr < 0x78; addr++) {
		if (i2c_write(i2c_dev, &dummy, 0, addr) == 0) {
			LOG_INF("  found device at 0x%02X", addr);
			found = true;
		}
	}
	if (!found) {
		LOG_WRN("  no devices — check wiring and pull-ups on P1.11/P1.12");
	}

	ret = read_reg(OV7670_REG_PID, &pid);
	if (ret) {
		LOG_ERR("Cannot read PID (I2C error %d) — check SIOD/SIOC wiring", ret);
		return ret;
	}
	read_reg(OV7670_REG_VER, &ver);
	LOG_INF("OV7670 PID=0x%02X VER=0x%02X", pid, ver);

	if (pid != OV7670_PID_MAGIC || ver != OV7670_VER_MAGIC) {
		LOG_WRN("Unexpected chip ID (expected PID=0x76, VER=0x73)");
	}

	for (size_t i = 0; i < ARRAY_SIZE(init_tbl); i++) {
		ret = write_reg(init_tbl[i].reg, init_tbl[i].val);
		if (ret) {
			LOG_ERR("Reg 0x%02X write failed: %d", init_tbl[i].reg, ret);
			return ret;
		}
		/* Extra pause after software reset */
		if (init_tbl[i].reg == OV7670_REG_COM7 && init_tbl[i].val == 0x80) {
			k_msleep(100);
		} else {
			k_usleep(300);
		}
	}
	LOG_INF("OV7670 configured — QQVGA %dx%d RGB565", IMG_W, IMG_H);

	for (size_t i = 0; i < ARRAY_SIZE(verify_tbl); i++) {
		uint8_t rd = 0xFF;
		int rc = read_reg(verify_tbl[i].reg, &rd);

		if (rc) {
			LOG_ERR("Readback 0x%02X (%s) I2C err %d",
				verify_tbl[i].reg, verify_tbl[i].name, rc);
		} else if (rd != verify_tbl[i].expect) {
			LOG_WRN("Readback 0x%02X (%s): got 0x%02X, want 0x%02X",
				verify_tbl[i].reg, verify_tbl[i].name,
				rd, verify_tbl[i].expect);
		} else {
			LOG_INF("Readback 0x%02X (%s): OK", verify_tbl[i].reg,
				verify_tbl[i].name);
		}
	}

	return 0;
}

int fifo_init(void)
{
	int ret;

	if (!device_is_ready(gpio0) ||
	    !device_is_ready(gpio1) ||
	    !device_is_ready(gpio2)) {
		LOG_ERR("GPIO port(s) not ready");
		return -ENODEV;
	}

	/* Data bus D0-D7: P2.00-P2.07 as inputs */
	for (int i = 0; i < 8; i++) {
		ret = gpio_pin_configure(gpio2, i, GPIO_INPUT);
		if (ret) {
			LOG_ERR("data pin P2.%02d cfg failed: %d", i, ret);
			return ret;
		}
	}

	/* WEN (P0.04): output, start high (write disabled) */
	ret = gpio_pin_configure(gpio0, PIN_WEN, GPIO_OUTPUT);
	if (ret) {
		LOG_ERR("P0.%02d (WEN) cfg failed: %d", PIN_WEN, ret);
		return ret;
	}
	gpio_pin_set_raw(gpio0, PIN_WEN, 1);

	/* RRST (P1.14): output, start high (deasserted) */
	ret = gpio_pin_configure(gpio1, PIN_RRST, GPIO_OUTPUT);
	if (ret) {
		LOG_ERR("P1.%02d (RRST) cfg failed: %d", PIN_RRST, ret);
		return ret;
	}
	gpio_pin_set_raw(gpio1, PIN_RRST, 1);

	/* Port 2 control pins */
	static const struct { uint8_t pin; int dir; uint8_t init; } p2ctrl[] = {
		{PIN_VSYNC, GPIO_INPUT,  0},
		{PIN_WRST,  GPIO_OUTPUT, 1},
		{PIN_RCK,   GPIO_OUTPUT, 1},
	};
	for (int i = 0; i < ARRAY_SIZE(p2ctrl); i++) {
		ret = gpio_pin_configure(gpio2, p2ctrl[i].pin, p2ctrl[i].dir);
		if (ret) {
			LOG_ERR("P2.%02d cfg failed: %d", p2ctrl[i].pin, ret);
			return ret;
		}
		if (p2ctrl[i].dir == GPIO_OUTPUT) {
			gpio_pin_set_raw(gpio2, p2ctrl[i].pin, p2ctrl[i].init);
		}
	}

	return 0;
}

int fifo_capture(uint8_t *buf, size_t size)
{
	/* 1. Wait for VSYNC = 1 (vertical blanking / end of old frame) */
	while (gpio_pin_get_raw(gpio2, PIN_VSYNC) == 0) {
	}
	/* 2. Wait for VSYNC = 0 (start of new frame) */
	while (gpio_pin_get_raw(gpio2, PIN_VSYNC) != 0) {
	}

	/* 3. Reset write pointer and enable writing (AL422B /WE active-low) */
	write_reset();
	gpio_pin_set_raw(gpio0, PIN_WEN, 0);

	/* 4. Wait for the full frame to be written (VSYNC = 1 again) */
	while (gpio_pin_get_raw(gpio2, PIN_VSYNC) == 0) {
	}

	/* 5. Stop writing */
	gpio_pin_set_raw(gpio0, PIN_WEN, 1);

	/* 6. Reset read pointer */
	read_reset();

	/* 7. Clock out every byte.
	 *    AL422B: data valid while RCK is HIGH; pointer advances on falling
	 *    edge. RCK is also LED0 — it blinks during readout. */
	for (size_t i = 0; i < size; i++) {
		gpio_pin_set_raw(gpio2, PIN_RCK, 1);
		buf[i] = read_data_bus();
		gpio_pin_set_raw(gpio2, PIN_RCK, 0);
	}

	return 0;
}
