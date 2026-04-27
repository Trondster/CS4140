/*
 * AL422B FIFO interface driver.
 *
 * Pin mapping (nRF54L15 DK — P2.00–P2.05 freed via Nordic Board Configurator,
 * spi00 disabled in boards/nrf54l15dk_nrf54l15_cpuapp.overlay):
 *
 *   D0–D7   P2.00–P2.07  (8-bit data bus, read as: gpio2 & 0xFF)
 *   WEN     P0.04        /WE  active-low  (shared with Button3 — OK for demo)
 *   RRST    P1.14        active-low read-pointer reset  (shared with LED3)
 *   VSYNC   P2.08        frame sync input from OV7670
 *   RCK     P2.09        read clock output  (also LED0 — blinks during readout)
 *   WRST    P2.10        /WRST active-low write-pointer reset
 *
 * Fixed (no MCU pin):
 *   XCLK  → 12 MHz oscillator on camera PCB
 *   OE    → GND   (FIFO outputs always enabled)
 *   RST   → 3.3 V via 10 kΩ
 *   PWDN  → GND
 *
 * SCCB (not handled here):
 *   SIOD  → P1.11  (managed by i2c21 / Arduino I2C SDA header)
 *   SIOC  → P1.12  (managed by i2c21 / Arduino I2C SCL header)
 *   Note: keep SIOD/SIOC wires short — long or chained wires add capacitance
 *         that corrupts SCCB writes and produces stuck/tinted images.
 */

#include "fifo.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(fifo, LOG_LEVEL_INF);

/* ---- GPIO port handles --------------------------------------------------- */

static const struct device *const gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct device *const gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
static const struct device *const gpio2 = DEVICE_DT_GET(DT_NODELABEL(gpio2));

/* ---- Pin assignments ----------------------------------------------------- */

#define PIN_WEN    4   /* P0.04 — /WE  active-low */
#define PIN_RRST  14   /* P1.14 — RRST active-low */
#define PIN_VSYNC  8   /* P2.08 — frame sync input */
#define PIN_RCK    9   /* P2.09 — read clock (also LED0) */
#define PIN_WRST  10   /* P2.10 — /WRST active-low */

/* ---- Low-level helpers --------------------------------------------------- */

/* Read the 8-bit data bus: D0–D7 all on P2.00–P2.07. */
static inline uint8_t read_byte(void)
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
	/* Sequence from bitluni reference: assert RRST, clock one rising RCK
	 * edge so the AL422B latches the reset, then deassert RRST.  Leave
	 * RCK high so the first read_byte() sees valid data at address 0. */
	gpio_pin_set_raw(gpio1, PIN_RRST, 0);
	k_busy_wait(1);
	gpio_pin_set_raw(gpio2, PIN_RCK, 0);
	k_busy_wait(1);
	gpio_pin_set_raw(gpio2, PIN_RCK, 1);
	k_busy_wait(1);
	gpio_pin_set_raw(gpio1, PIN_RRST, 1);
}

/* ---- Public API ---------------------------------------------------------- */

int fifo_init(void)
{
	int ret;

	if (!device_is_ready(gpio0) ||
	    !device_is_ready(gpio1) ||
	    !device_is_ready(gpio2)) {
		LOG_ERR("GPIO port(s) not ready");
		return -ENODEV;
	}

	/* Data bus D0–D7: P2.00–P2.07 as inputs */
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

int fifo_capture(uint8_t *buf, size_t size, size_t line_stride)
{
	/* 1.  Wait for VSYNC = 1  (vertical blanking / end of old frame) */
	while (gpio_pin_get_raw(gpio2, PIN_VSYNC) == 0) {
	}
	/* 2.  Wait for VSYNC = 0  (start of new frame) */
	while (gpio_pin_get_raw(gpio2, PIN_VSYNC) != 0) {
	}

	/* 3.  Reset write pointer and enable writing (AL422B /WE active-low) */
	write_reset();
	gpio_pin_set_raw(gpio0, PIN_WEN, 0);

	/* 4.  Wait for the full frame to be written (VSYNC = 1 again) */
	while (gpio_pin_get_raw(gpio2, PIN_VSYNC) == 0) {
	}

	/* 5.  Stop writing */
	gpio_pin_set_raw(gpio0, PIN_WEN, 1);

	/* 6.  Reset read pointer */
	read_reset();

	/* 6b.  Set RCK low, to avoid the otherwise corrupt first byte. */
	gpio_pin_set_raw(gpio2, PIN_RCK, 0);

	/* 7.  Clock out every byte.
	 *     AL422B: data valid while RCK is HIGH; pointer advances on
	 *     falling edge.  RCK is also LED0 — it blinks during readout. */
	const int trouble_byte = line_stride - 2;
	for (size_t i = 0; i < size; i++) {
		gpio_pin_set_raw(gpio2, PIN_RCK, 1);
		buf[i] = read_byte();
		if (buf[i] != 0 && (i % line_stride) == trouble_byte) {
			//The second last byte in each LINE_STRIDE should always be 0x00.
			//Inserting zero as the second last byte, and inserting the read byte at the end:
			//Do not need to check for array bounds - we are at the second to last byte in a line.
			buf[i + 1] = buf[i];
			buf[i] = 0;
			++i;
		}
		gpio_pin_set_raw(gpio2, PIN_RCK, 0);
	}

	return 0;
}
