/*
 * OV7670 + FIFO Camera Demo — nRF54L15 DK (Zephyr)
 *
 * Captures QQVGA (160×120) RGB565 frames and displays them on the
 * 1.8" TFT screen (ST7735R, 160×128).
 * See src/ov7670_fifo.c for the full pin mapping.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ov7670_regs.h"
#include "ov7670_fifo.h"
#include "tft_display.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static uint8_t frame_buf[IMG_SIZE] __aligned(4);

int main(void)
{
	LOG_INF("*** Camera capture and show on screen ***");

	const struct device *display = TFT_DEVICE();

	if (tft_init(display) != 0) {
		LOG_ERR("Display not ready");
		return -1;
	}

	if (ov7670_init() != 0) {
		return -EIO;
	}

	if (fifo_init() != 0) {
		return -EIO;
	}

	k_msleep(300);   /* auto-exposure settle */

	tft_fill_screen(display, TFT_COLOR_BLACK);

	while (1) {
		fifo_capture(frame_buf, sizeof(frame_buf));

		tft_draw_image(display, 0, 0, IMG_W, IMG_H, frame_buf);

		tft_draw_bounding_box(display, 0, 0, IMG_W, IMG_H, "Test");

		k_msleep(33);
	}

	return 0;
}
