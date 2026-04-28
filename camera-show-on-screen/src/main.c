/*
 * OV7670 + FIFO Camera Demo — nRF54L15 DK (Zephyr)
 *
 * Captures QQVGA (160×120) RGB565 frames once per second.
 * Set DUMP_FRAME_UART to 1 to hex-dump each frame over UART.
 * See README.md for hardware wiring and Board Configurator requirements.
 * See src/fifo.c for the full pin mapping.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "../../lib/c/ov7670.h"
#include "../../lib/c/fifo.h"
#include "../../lib/c/tft_display.h"

#define FRAME_RATE 5
#define FRAME_INTERVAL_MS (1000 / FRAME_RATE)

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static uint8_t frame_buf[IMG_SIZE] __aligned(4);

int main(void)
{
	LOG_INF("\n*** Camera capture and show on screen ***\n");

	const struct device *display = TFT_DEVICE();

	if (tft_init(display) != 0) {
		LOG_ERR("Display not ready\n");
		return -1;
	}

	if (ov7670_init() != 0) {
		LOG_ERR("Camera init failed\n");
		return -EIO;
	}

	if (fifo_init() != 0) {
		LOG_ERR("FIFO init failed\n");
		return -EIO;
	}

	k_msleep(300);   /* auto-exposure settle */
	
	tft_fill_screen(display, TFT_COLOR_BLACK);

	long long next = 0;
	while (1) {
		next = k_uptime_get();

		fifo_capture(frame_buf, IMG_SIZE, LINE_STRIDE);
		
		tft_draw_image(display, 0, 0, 160, 120, frame_buf);
		
		// tft_draw_bounding_box(display, 0, 0, 160, 120, "Test");

		// k_msleep(MAX(0, FRAME_INTERVAL_MS - (k_uptime_get() - next)));
	}

	return 0;
}
