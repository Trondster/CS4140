/*
 * Camera image gathering — nRF54L15 DK (Zephyr)
 *
 * sw0 (live)   → capture frame, freeze it on screen
 * sw0 (frozen) → return to live view
 * sw1 (frozen) → send frozen frame with metadata "label=drone"
 * sw2 (frozen) → send frozen frame with metadata "label=clear"
 */

#include <cstdint>

//Should not have extern "C".
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "ov7670.h"
#include "fifo.h"
#include "tft_display.h"
#include "uart_img_send.h"

#include "preproc/preproc_direct.hpp"
#include "preproc/preproc_grayscale.hpp"
#include "preproc/preproc_grayscale2.hpp"
#include "preproc/preproc_grayscale3.hpp"
#include "preproc/preproc_left_sobel.hpp"
#include "preproc/preproc_diff_direct.hpp"
#include "preproc/preproc_diff_grayscale_abs.hpp"
#include "preproc/preproc_diff_grayscale_minus.hpp"
#include "preproc/preproc_diff_color_abs.hpp"
#include "preproc/preproc_diff_color_minus.hpp"
#include "preproc/preproc_outline_sobel.hpp"
#include "preproc/preproc_downscale_grayscale_2x.hpp"
#include "preproc/preproc_downscale_grayscale_3x.hpp"
#include "preproc/preproc_downscale_grayscale_4x.hpp"
#include "preproc/preproc_downscale_diff_grayscale_minus_2x.hpp"
#include "preproc/preproc_downscale_diff_grayscale_minus_3x.hpp"
#include "preproc/preproc_downscale_diff_grayscale_minus_4x.hpp"

#define FRAME_RATE        5
#define FRAME_INTERVAL_MS (1000 / FRAME_RATE)
#define DEBOUNCE_MS       80


LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static uint8_t frame_buf[IMG_SIZE] __aligned(4);

static uint8_t second_frame_buf[IMG_SIZE] __aligned(4);

//Two-dimensional array for grayscale data - first dimension is IMG_H + 2, second dimension is IMG_W+2.
//The extra bytes are for padding, to simplify the Sobel operator implementation.
static uint8_t grayscale_buf[BUF_IMG_SIZE] __aligned(4);

static uint8_t second_grayscale_buf[BUF_IMG_SIZE] __aligned(4);


enum class AppState { LIVE, FROZEN };
static volatile AppState app_state = AppState::LIVE;
static volatile bool sw0_flag = false;
static volatile bool sw1_flag = false;
static volatile bool sw2_flag = false;

static const struct gpio_dt_spec btn0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec btn1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static const struct gpio_dt_spec btn2 = GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios);
static struct gpio_callback btn0_cb, btn1_cb, btn2_cb;

static int64_t sw0_last_ms = -1;
static int64_t sw1_last_ms = -1;
static int64_t sw2_last_ms = -1;

static void on_sw0(const struct device* dev, struct gpio_callback* cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	int64_t now = k_uptime_get();
	if ((now - sw0_last_ms) >= DEBOUNCE_MS) {
		sw0_last_ms = now;
		sw0_flag = true;
	}
}

static void on_sw1(const struct device* dev, struct gpio_callback* cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	int64_t now = k_uptime_get();
	if ((now - sw1_last_ms) >= DEBOUNCE_MS) {
		sw1_last_ms = now;
		sw1_flag = true;
	}
}

static void on_sw2(const struct device* dev, struct gpio_callback* cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	int64_t now = k_uptime_get();
	if ((now - sw2_last_ms) >= DEBOUNCE_MS) {
		sw2_last_ms = now;
		sw2_flag = true;
	}
}

static int setup_button(const struct gpio_dt_spec* btn, struct gpio_callback* cb,
			gpio_callback_handler_t handler)
{
	if (!gpio_is_ready_dt(btn)) {
		LOG_ERR("Button GPIO not ready");
		return -ENODEV;
	}
	gpio_pin_configure_dt(btn, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(btn, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(cb, handler, BIT(btn->pin));
	gpio_add_callback(btn->port, cb);
	return 0;
}

static PreprocDirect preproc_direct = PreprocDirect(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocGrayscale preproc_grayscale = PreprocGrayscale(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

// static PreprocGrayscale2 preproc_grayscale2 = PreprocGrayscale2(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocGrayscale3 preproc_grayscale3 = PreprocGrayscale3(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

static PreprocOutlineSobel preproc_outline_sobel = PreprocOutlineSobel(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocLeftSobel preproc_left_sobel = PreprocLeftSobel(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

// static PreprocDiffDirect preproc_diff_direct = PreprocDiffDirect(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

static PreprocDiffGrayscaleAbs preproc_diff_grayscale_abs = PreprocDiffGrayscaleAbs(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocDiffGrayscaleMinus preproc_diff_grayscale_minus = PreprocDiffGrayscaleMinus(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocDiffColorAbs preproc_diff_color_abs = PreprocDiffColorAbs(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocDiffColorMinus preproc_diff_color_minus = PreprocDiffColorMinus(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocDownscaleGrayscale2x preproc_downscale_grayscale_2x = PreprocDownscaleGrayscale2x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocDownscaleGrayscale3x preproc_downscale_grayscale_3x = PreprocDownscaleGrayscale3x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocDownscaleGrayscale4x preproc_downscale_grayscale_4x = PreprocDownscaleGrayscale4x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocDownscaleDiffGrayscaleMinus2x preproc_downscale_diff_grayscale_minus_2x = PreprocDownscaleDiffGrayscaleMinus2x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocDownscaleDiffGrayscaleMinus3x preproc_downscale_diff_grayscale_minus_3x = PreprocDownscaleDiffGrayscaleMinus3x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

static PreprocDownscaleDiffGrayscaleMinus4x preproc_downscale_diff_grayscale_minus_4x = PreprocDownscaleDiffGrayscaleMinus4x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

//Inline array of pointers to the handlers, so that we can easily loop through them if we want to.
static IPreprocHandler* handlers[] = {
	&preproc_direct,
	&preproc_grayscale,
	// &preproc_grayscale2,
	// &preproc_grayscale3,
	&preproc_downscale_grayscale_2x,
	&preproc_downscale_grayscale_3x,
	&preproc_downscale_grayscale_4x,
	&preproc_left_sobel,
	&preproc_outline_sobel,
	// &preproc_diff_direct,
	&preproc_diff_grayscale_abs,
	&preproc_diff_grayscale_minus,
	&preproc_downscale_diff_grayscale_minus_2x,
	&preproc_downscale_diff_grayscale_minus_3x,
	&preproc_downscale_diff_grayscale_minus_4x,
	&preproc_diff_color_abs,
	&preproc_diff_color_minus,
};

void show_handler(IPreprocHandler* handler, struct device* display) {
	handler->process(); //Process one frame immediately, to show something on the display right away.
	LOG_INF("Using handler: %s", handler->get_name());
	tft_draw_bounding_box(display, 0, 0, 160, 120, handler->get_name());
	k_msleep(300);
}

int main()
{
	LOG_INF("*** CPP Camera image gathering ***");

	const struct device* display = TFT_DEVICE();
	const struct device* uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	for (int i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
		handlers[i]->inject(const_cast<device*>(display), fifo_capture, tft_draw_image);
	}

	int handler_idx = 0;
	int handler_count = sizeof(handlers) / sizeof(handlers[0]);

	if (tft_init(display) != 0) {
		LOG_ERR("Display not ready");
		return -1;
	}
	if (ov7670_init() != 0) {
		LOG_ERR("Camera init failed");
		return -EIO;
	}
	if (fifo_init() != 0) {
		LOG_ERR("FIFO init failed");
		return -EIO;
	}
	if (setup_button(&btn0, &btn0_cb, on_sw0) != 0 ||
	    setup_button(&btn1, &btn1_cb, on_sw1) != 0 ||
	    setup_button(&btn2, &btn2_cb, on_sw2) != 0) {
		return -ENODEV;
	}

	k_msleep(300);
	tft_fill_screen(display, TFT_COLOR_BLACK);

	LOG_INF("Ready — sw0=capture/live, sw1=send drone, sw2=send clear");

	int64_t next = 0;

	IPreprocHandler* current_handler = handlers[handler_idx];
	current_handler->init();
	show_handler(current_handler, const_cast<device*>(display));

	while (true) {
		/* sw0: toggle between live view and frozen frame */
		if (sw0_flag) {
			sw0_flag = false;
			if (app_state == AppState::LIVE) {
				current_handler->process();
				app_state = AppState::FROZEN;
				LOG_INF("Frame frozen using handler %s", current_handler->get_name());
				// Trond's ugly hex debug dump. Leaving the comment here, just in case.
				// printk("\r\n---FRAME_START---\r\n");
				// for (size_t i = 0; i < IMG_SIZE; i++) {
				// 	if ((i % LINE_STRIDE) == 0) {
				// 		printk("\r\n");
				// 	}
				// 	printk("%02x", frame_buf[i]);
				// }
				// printk("\r\n---FRAME_END---\r\n");
			} else {
				app_state = AppState::LIVE;
				LOG_INF("Back to live view using handler %s", current_handler->get_name());
				current_handler->process();
				show_handler(current_handler, const_cast<device*>(display));
			}
		}

		if (sw1_flag) 
		{
			sw1_flag = false;
			handler_idx = (handler_idx + 1) % handler_count;
			current_handler = handlers[handler_idx];
			current_handler->init();
			current_handler->process();
			show_handler(current_handler, const_cast<device*>(display));
		}


		if (app_state != AppState::FROZEN) {
			/* Discard label presses that arrive during live view */
			sw1_flag = false;
			sw2_flag = false;

			next = k_uptime_get();
			current_handler->process();

			//The reader waits for a VSYNC anyway - skipping the sleep.
			// int32_t remaining = FRAME_INTERVAL_MS - static_cast<int32_t>(k_uptime_get() - next);
			// if (remaining > 0) {
			// 	k_msleep(remaining);
			// }
		}
	}

	return 0;
}
