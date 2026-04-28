/*
 * Camera image gathering — nRF54L15 DK (Zephyr)
 *
 * sw0 (live)   → capture frame, freeze it on screen
 * sw0 (frozen) → return to live view
 * sw1 (frozen) → send frozen frame with metadata "drone"
 * sw2 (frozen) → send frozen frame with metadata "clear"
 */

#include <cstdint>

//Should not have extern "C".
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "../../lib/c/ov7670.h"
#include "../../lib/c/fifo.h"
#include "../../lib/c/tft_display.h"
#include "../../lib/c/uart_img_send.h"

// #include "../../lib/cpp/preproc/preproc_direct.hpp"
// #include "../../lib/cpp/preproc/preproc_grayscale.hpp"
// #include "../../lib/cpp/preproc/preproc_grayscale2.hpp"
// #include "../../lib/cpp/preproc/preproc_grayscale3.hpp"
// #include "../../lib/cpp/preproc/preproc_left_sobel.hpp"
// #include "../../lib/cpp/preproc/preproc_diff_direct.hpp"
// #include "../../lib/cpp/preproc/preproc_diff_grayscale_abs.hpp"
// #include "../../lib/cpp/preproc/preproc_diff_grayscale_minus.hpp"
// #include "../../lib/cpp/preproc/preproc_diff_color_abs.hpp"
// #include "../../lib/cpp/preproc/preproc_diff_color_minus.hpp"
// #include "../../lib/cpp/preproc/preproc_outline_sobel.hpp"
// #include "../../lib/cpp/preproc/preproc_downscale_grayscale_2x.hpp"
// #include "../../lib/cpp/preproc/preproc_downscale_grayscale_4x.hpp"
// #include "../../lib/cpp/preproc/preproc_downscale_diff_grayscale_minus_2x.hpp"
// #include "../../lib/cpp/preproc/preproc_downscale_diff_grayscale_minus_4x.hpp"
#include "../../lib/cpp/preproc/preproc_diff_scaling.hpp"

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

// static PreprocDirect preproc_direct = PreprocDirect(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocGrayscale preproc_grayscale = PreprocGrayscale(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocGrayscale2 preproc_grayscale2 = PreprocGrayscale2(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocGrayscale3 preproc_grayscale3 = PreprocGrayscale3(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocOutlineSobel preproc_outline_sobel = PreprocOutlineSobel(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocLeftSobel preproc_left_sobel = PreprocLeftSobel(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocDiffDirect preproc_diff_direct = PreprocDiffDirect(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocDiffGrayscaleAbs preproc_diff_grayscale_abs = PreprocDiffGrayscaleAbs(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocDiffGrayscaleMinus preproc_diff_grayscale_minus = PreprocDiffGrayscaleMinus(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocDiffColorAbs preproc_diff_color_abs = PreprocDiffColorAbs(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocDiffColorMinus preproc_diff_color_minus = PreprocDiffColorMinus(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocDownscaleGrayscale2x preproc_downscale_grayscale_2x = PreprocDownscaleGrayscale2x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocDownscaleGrayscale4x preproc_downscale_grayscale_4x = PreprocDownscaleGrayscale4x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocDownscaleDiffGrayscaleMinus2x preproc_downscale_diff_grayscale_minus_2x = PreprocDownscaleDiffGrayscaleMinus2x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

// static PreprocDownscaleDiffGrayscaleMinus4x preproc_downscale_diff_grayscale_minus_4x = PreprocDownscaleDiffGrayscaleMinus4x(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
// 	IMG_H, IMG_W, IMG_BPP);

//Inline array of pointers to the handlers, so that we can easily loop through them if we want to.
// static IPreprocHandler* handlers[] = {
// 	&preproc_direct,
// 	&preproc_grayscale,
// 	// &preproc_grayscale2,
// 	// &preproc_grayscale3,
// 	&preproc_downscale_grayscale_2x,
// 	&preproc_downscale_grayscale_4x,
// 	&preproc_left_sobel,
// 	&preproc_outline_sobel,
// 	// &preproc_diff_direct,
// 	&preproc_diff_grayscale_abs,
// 	&preproc_diff_grayscale_minus,
// 	&preproc_downscale_diff_grayscale_minus_2x,
// 	&preproc_downscale_diff_grayscale_minus_4x,
// 	&preproc_diff_color_abs,
// 	&preproc_diff_color_minus,
// };

static PreprocDiffScaling preproc_diff_scaling = PreprocDiffScaling(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
	IMG_H, IMG_W, IMG_BPP);

void show_handler(IPreprocHandler* handler, struct device* display) {
	handler->process(); //Process one frame immediately, to show something on the display right away.
	LOG_INF("Using handler: %s", handler->get_name());
	tft_draw_bounding_box(display, 0, 0, 160, 120, handler->get_name());
	k_msleep(300);
}

int main()
{
	LOG_INF("*** CPP Camera capture ***");

	const struct device* display = TFT_DEVICE();
	const struct device* uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

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


	preproc_diff_scaling.inject(const_cast<device*>(display), fifo_capture, tft_draw_image);
	preproc_diff_scaling.init();
	show_handler(&preproc_diff_scaling, const_cast<device*>(display));

	while (true) {
		/* sw0: toggle between live view and frozen frame */
		if (sw0_flag) {
			sw0_flag = false;
			if (app_state == AppState::LIVE) {
				preproc_diff_scaling.process();
				app_state = AppState::FROZEN;
				LOG_INF("Frame frozen using handler %s", preproc_diff_scaling.get_name());
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
				LOG_INF("Back to live view using handler %s", preproc_diff_scaling.get_name());
				preproc_diff_scaling.process();
				show_handler(&preproc_diff_scaling, const_cast<device*>(display));
			}
		}

		// if (sw1_flag) 
		// {
		// 	sw1_flag = false;
		// 	show_handler(&preproc_diff_scaling, const_cast<device*>(display));
		// }


		if (app_state == AppState::FROZEN) {
			if (sw1_flag || sw2_flag) {
				/* sw1/sw2: label and transmit the frozen frame */
				bool drone;
				if (sw1_flag) {
					sw1_flag = false;
					drone = true;
					// LOG_INF("Sending: drone");
					// uart_img_send(uart, frame_buf, 160, 120, 2, 1, "drone");
					// tft_draw_bounding_box(display, 0, 0, 160, 120, "drone");
					// LOG_INF("Done");
				} else if (sw2_flag) {
					sw2_flag = false;
					drone = false;
					// LOG_INF("Sending: clear");
					// uart_img_send(uart, frame_buf, 160, 120, 2, 1, "clear");
					// tft_draw_bounding_box(display, 0, 0, 160, 120, "clear");
					// LOG_INF("Done");
				}

				//The various buffers to send:
				//Current frame:
				uint8_t* current_frame_buf = preproc_diff_scaling.get_current_frame_buf();
				uint8_t* previous_frame_buf = preproc_diff_scaling.get_previous_frame_buf();
				uint8_t* unpadded_grayscale_buf = preproc_diff_scaling.get_current_grayscale_nopad();
				uint8_t* diff_grayscale_buf = preproc_diff_scaling.get_current_diff_grayscale_nopad();
				uint8_t* downscaled_2x2_grayscale_buf = preproc_diff_scaling.get_current_grayscale_downscaled_2x2_nopad();
				uint8_t* downscaled_3x3_grayscale_buf = preproc_diff_scaling.get_current_grayscale_downscaled_3x3_nopad();
				uint8_t* downscaled_4x4_grayscale_buf = preproc_diff_scaling.get_current_grayscale_downscaled_4x4_nopad();
				uint8_t* downscaled_2x2_diff_grayscale_buf = preproc_diff_scaling.get_current_diff_downscaled_2x2_nopad();
				uint8_t* downscaled_3x3_diff_grayscale_buf = preproc_diff_scaling.get_current_diff_downscaled_3x3_nopad();
				uint8_t* downscaled_4x4_diff_grayscale_buf = preproc_diff_scaling.get_current_diff_downscaled_4x4_nopad();

				//Used for current_frame_buf, previous_frame_buf, unpadded_grayscale_buf, diff_grayscale_buf
				int frame_height = IMG_H;
				int frame_width = IMG_W;

				//Used for downscaled_2x2_diff_grayscale_buf and downscaled_2x2_grayscale_buf
				int downscaled_2x2_height = IMG_H / 2;
				int downscaled_2x2_width = IMG_W / 2;

				//Used for downscaled_3x3_diff_grayscale_buf and downscaled_3x3_grayscale_buf
				int downscaled_3x3_height = IMG_H / 3;
				int downscaled_3x3_width = IMG_W / 3;

				//Used for downscaled_4x4_diff_grayscale_buf and downscaled_4x4_grayscale_buf
				int downscaled_4x4_height = IMG_H / 4; 
				int downscaled_4x4_width = IMG_W / 4; 

				//Send the various buffers over UART with appropriate metadata, e.g.:
				uart_img_send(uart, current_frame_buf, frame_width, frame_height, 2, 1, drone ? "drone" : "clear", "dataset/color", "current_frame.png");
				k_msleep(10);
				uart_img_send(uart, previous_frame_buf, frame_width, frame_height, 2, 1, drone ? "drone" : "clear", "dataset/color", "previous_frame.png");
				k_msleep(10);
				uart_img_send(uart, unpadded_grayscale_buf, frame_width, frame_height, 1, 1, drone ? "drone" : "clear", "dataset/grey", "current_frame.png");
				k_msleep(10);
				uart_img_send(uart, diff_grayscale_buf, frame_width, frame_height, 1, 1, drone ? "drone" : "clear", "dataset/grey", "diff_frame.png");
				k_msleep(10);
				uart_img_send(uart, downscaled_2x2_grayscale_buf, downscaled_2x2_width, downscaled_2x2_height, 1, 1, drone ? "drone" : "clear", "dataset/2x2", "current_frame.png");
				k_msleep(10);
				uart_img_send(uart, downscaled_2x2_diff_grayscale_buf, downscaled_2x2_width, downscaled_2x2_height, 1, 1, drone ? "drone" : "clear", "dataset/2x2", "diff_frame.png");
				k_msleep(10);
				uart_img_send(uart, downscaled_3x3_grayscale_buf, downscaled_3x3_width, downscaled_3x3_height, 1, 1, drone ? "drone" : "clear", "dataset/3x3", "current_frame.png");
				k_msleep(10);
				uart_img_send(uart, downscaled_3x3_diff_grayscale_buf, downscaled_3x3_width, downscaled_3x3_height, 1, 1, drone ? "drone" : "clear", "dataset/3x3", "diff_frame.png");
				k_msleep(10);
				uart_img_send(uart, downscaled_4x4_grayscale_buf, downscaled_4x4_width, downscaled_4x4_height, 1, 1, drone ? "drone" : "clear", "dataset/4x4", "current_frame.png");
				k_msleep(10);
				uart_img_send(uart, downscaled_4x4_diff_grayscale_buf, downscaled_4x4_width, downscaled_4x4_height, 1, 1, drone ? "drone" : "clear", "dataset/4x4", "diff_frame.png");

				LOG_INF("Sent all data: %s", drone ? "drone" : "clear");
			}

			k_msleep(10);
		}
		else {
			sw1_flag = false;
			sw2_flag = false;

			//next = k_uptime_get();
			preproc_diff_scaling.process();

			//The reader waits for a VSYNC anyway - skipping the sleep.
			// int32_t remaining = FRAME_INTERVAL_MS - static_cast<int32_t>(k_uptime_get() - next);
			// if (remaining > 0) {
			// 	k_msleep(remaining);
			// }
		}
	}

	return 0;
}
