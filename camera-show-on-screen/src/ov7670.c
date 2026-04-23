/*
 * OV7670 CMOS image sensor driver (SCCB / I2C).
 *
 * Configures the sensor for QQVGA (160×120) RGB565 output.
 *
 * SCCB is I2C-compatible but requires a STOP condition between the
 * register-address write and the data read — i2c_write_read() must NOT
 * be used; two separate transactions are used instead.
 */

#include "ov7670.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ov7670, LOG_LEVEL_INF);

/* ---- SCCB address (7-bit) ----------------------------------------------- */

#define OV7670_ADDR  0x21

/* ---- Register map -------------------------------------------------------- */

#define REG_PID               0x0A
#define REG_VER               0x0B
#define REG_COM1              0x04
#define REG_COM3              0x0C
#define REG_COM7              0x12
#define REG_COM9              0x14
#define REG_COM13             0x3D
#define REG_COM14             0x3E
#define REG_COM15             0x40
#define REG_CLKRC             0x11
#define REG_TSLB              0x3A
#define REG_HSTART            0x17
#define REG_HSTOP             0x18
#define REG_HREF              0x32
#define REG_VSTART            0x19
#define REG_VSTOP             0x1A
#define REG_VREF              0x03
#define REG_RGB444            0x8C
#define REG_SCALING_XSC       0x70
#define REG_SCALING_YSC       0x71
#define REG_SCALING_DCWCTR    0x72
#define REG_SCALING_PCLK_DIV  0x73
#define REG_SCALING_PCLK_DLY  0xA2
#define REG_MTX1              0x4F
#define REG_MTX2              0x50
#define REG_MTX3              0x51
#define REG_MTX4              0x52
#define REG_MTX5              0x53
#define REG_MTX6              0x54
#define REG_MTXS              0x58

/* ---- QQVGA RGB565 initialisation table ----------------------------------- */

static const struct { uint8_t reg; uint8_t val; } init_tbl[] = {
	/* software reset — 100 ms pause inserted by the write loop */
	{REG_COM7,             0x80},
	/* RGB output mode */
	{REG_COM7,             0x04},
	/* use external 12 MHz clock directly, no prescaler */
	{REG_CLKRC,            0x80},
	/* enable downsampling / scaling */
	{REG_COM3,             0x04},
	/* manual scaling + PCLK divider ÷4 */
	{REG_COM14,            0x1A},
	/* downsample by 4 (H and V) */
	{REG_SCALING_DCWCTR,   0x22},
	/* DSP clock divider ÷4 */
	{REG_SCALING_PCLK_DIV, 0xF2},
	{REG_SCALING_XSC,      0x3A},
	{REG_SCALING_YSC,      0x35},
	{REG_SCALING_PCLK_DLY, 0x02},
	/* RGB565, full output range */
	{REG_COM15,            0xD0},
	/* disable RGB444 */
	{REG_RGB444,           0x00},
	{REG_COM1,             0x00},
	{REG_TSLB,             0x04},
	/* QQVGA window */
	{REG_HSTART,           0x16},
	{REG_HSTOP,            0x04},
	{REG_HREF,             0x24},
	{REG_VSTART,           0x02},
	{REG_VSTOP,            0x7A},
	{REG_VREF,             0x0A},
	/* AGC gain ceiling */
	{REG_COM9,             0x48},
	/* colour matrix */
	{REG_MTX1,             0x80},
	{REG_MTX2,             0x80},
	{REG_MTX3,             0x00},
	{REG_MTX4,             0x22},
	{REG_MTX5,             0x5E},
	{REG_MTX6,             0x80},
	{REG_MTXS,             0x9E},
	/* gamma + UV auto adjust */
	{REG_COM13,            0xC0},
	/* registers from bitluni reference — critical for correct colours */
	{0xB0,                 0x84},  /* undocumented */
	{0x13,                 0xE7},  /* COM8: AGC + AWB + AEC */
	{0x6F,                 0x9F},  /* simple AWB */
};

/* ---- Critical registers verified after table write ----------------------- */

static const struct {
	uint8_t reg; uint8_t expect; const char *name;
} verify_tbl[] = {
	{REG_COM7,             0x04, "COM7  (RGB mode)"},
	{REG_COM15,            0xD0, "COM15 (RGB565)"},
	{REG_CLKRC,            0x80, "CLKRC (ext clk)"},
	{REG_COM14,            0x1A, "COM14 (scale ÷4)"},
	{REG_SCALING_DCWCTR,   0x22, "DCWCTR (ds ÷4)"},
	{REG_SCALING_PCLK_DIV, 0xF2, "PCLK_DIV (÷4)"},
};

/* ---- Device handle ------------------------------------------------------- */

static const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));

/* ---- Low-level SCCB helpers ---------------------------------------------- */

static int write_reg(uint8_t reg, uint8_t val)
{
	uint8_t buf[2] = {reg, val};

	return i2c_write(i2c_dev, buf, 2, OV7670_ADDR);
}

static int read_reg(uint8_t reg, uint8_t *val)
{
	/*
	 * OV7670 SCCB requires a STOP between the address phase and the data
	 * read — use two separate transactions, not i2c_write_read().
	 */
	int ret = i2c_write(i2c_dev, &reg, 1, OV7670_ADDR);

	if (ret) {
		return ret;
	}
	return i2c_read(i2c_dev, val, 1, OV7670_ADDR);
}

/* ---- Public API ---------------------------------------------------------- */

int ov7670_init(void)
{
	uint8_t pid, ver;
	int ret;

	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device (i2c21) not ready");
		return -ENODEV;
	}

	/* I2C bus scan — helps diagnose address / wiring issues */
	printk("I2C scan on i2c21 (P1.11=SDA, P1.12=SCL):\n");
	bool found = false;
	uint8_t dummy;

	for (uint8_t addr = 0x08; addr < 0x78; addr++) {
		if (i2c_write(i2c_dev, &dummy, 0, addr) == 0) {
			printk("  found device at 0x%02X\n", addr);
			found = true;
		}
	}
	if (!found) {
		printk("  no devices found — check wiring and pull-ups on P1.11/P1.12!\n");
	}

	ret = read_reg(REG_PID, &pid);
	if (ret) {
		LOG_ERR("Cannot read OV7670 PID (I2C error %d) — check SIOD/SIOC wiring", ret);
		return ret;
	}
	read_reg(REG_VER, &ver);
	LOG_INF("OV7670 found — PID 0x%02X  VER 0x%02X", pid, ver);

	if (pid != 0x76 || ver != 0x73) {
		LOG_WRN("Unexpected chip ID (expected PID=0x76, VER=0x73)");
	}

	/* Write register table */
	for (size_t i = 0; i < ARRAY_SIZE(init_tbl); i++) {
		ret = write_reg(init_tbl[i].reg, init_tbl[i].val);
		if (ret) {
			LOG_ERR("Reg 0x%02X write failed: %d", init_tbl[i].reg, ret);
			return ret;
		}
		/* extra pause after software reset */
		if (init_tbl[i].reg == REG_COM7 && init_tbl[i].val == 0x80) {
			k_msleep(100);
		} else {
			k_usleep(300);
		}
	}
	LOG_INF("OV7670 configured — QQVGA 160x120 RGB565");

	/* Readback verification */
	for (size_t i = 0; i < ARRAY_SIZE(verify_tbl); i++) {
		uint8_t rd = 0xFF;
		int rc = read_reg(verify_tbl[i].reg, &rd);

		if (rc) {
			LOG_ERR("Readback 0x%02X (%s) I2C err %d",
				verify_tbl[i].reg, verify_tbl[i].name, rc);
		} else if (rd != verify_tbl[i].expect) {
			LOG_WRN("Readback 0x%02X (%s): got 0x%02X, expected 0x%02X",
				verify_tbl[i].reg, verify_tbl[i].name, rd, verify_tbl[i].expect);
		} else {
			LOG_INF("Readback 0x%02X (%s): OK (0x%02X)",
				verify_tbl[i].reg, verify_tbl[i].name, rd);
		}
	}

	return 0;
}
