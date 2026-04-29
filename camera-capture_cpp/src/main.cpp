/*
 * Camera image gathering — nRF54L15 DK (Zephyr)
 *
 * sw0 (live)   → capture frame, freeze it on screen
 * sw0 (frozen) → return to live view
 * sw1 (frozen) → send frozen frame with metadata "drone"
 * sw2 (frozen) → send frozen frame with metadata "clear"
 */

#include <cstdint>

// Should not have extern "C".
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

#define FRAME_RATE 5
#define FRAME_INTERVAL_MS (1000 / FRAME_RATE)
#define DEBOUNCE_MS 80

#define RETRY_TRANSMISSIONS_ON_FAILURE 10

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static uint8_t frame_buf[IMG_SIZE] __aligned(4);

static uint8_t second_frame_buf[IMG_SIZE] __aligned(4);

// Two-dimensional array for grayscale data - first dimension is IMG_H + 2, second dimension is IMG_W+2.
// The extra bytes are for padding, to simplify the Sobel operator implementation.
static uint8_t grayscale_buf[BUF_IMG_SIZE] __aligned(4);

static uint8_t second_grayscale_buf[BUF_IMG_SIZE] __aligned(4);

enum class AppState
{
	LIVE,
	FROZEN
};
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

static void on_sw0(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	int64_t now = k_uptime_get();
	if ((now - sw0_last_ms) >= DEBOUNCE_MS)
	{
		sw0_last_ms = now;
		sw0_flag = true;
	}
}

static void on_sw1(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	int64_t now = k_uptime_get();
	if ((now - sw1_last_ms) >= DEBOUNCE_MS)
	{
		sw1_last_ms = now;
		sw1_flag = true;
	}
}

static void on_sw2(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	int64_t now = k_uptime_get();
	if ((now - sw2_last_ms) >= DEBOUNCE_MS)
	{
		sw2_last_ms = now;
		sw2_flag = true;
	}
}

static int setup_button(const struct gpio_dt_spec *btn, struct gpio_callback *cb,
								gpio_callback_handler_t handler)
{
	if (!gpio_is_ready_dt(btn))
	{
		LOG_ERR("Button GPIO not ready");
		return -ENODEV;
	}
	gpio_pin_configure_dt(btn, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(btn, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(cb, handler, BIT(btn->pin));
	gpio_add_callback(btn->port, cb);
	return 0;
}

static PreprocDiffScaling preproc_diff_scaling = PreprocDiffScaling(frame_buf, second_frame_buf, grayscale_buf, second_grayscale_buf,
																						  IMG_H, IMG_W, IMG_BPP);

void show_handler(IPreprocHandler *handler, struct device *display)
{
	handler->process(); // Process one frame immediately, to show something on the display right away.
	LOG_INF("Using handler: %s", handler->get_name());
	tft_draw_bounding_box(display, 0, 0, 160, 120, handler->get_name());
	k_msleep(300);
}

struct data_to_send {
	uint8_t* buf;
	int32_t width;
	int32_t height;
	size_t bytes_per_pixel;
	const char* folder;
	const char* filename;
};

int main()
{
	LOG_INF("*** CPP Camera capture ***");

	const struct device *display = TFT_DEVICE();
	const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	if (tft_init(display) != 0)
	{
		LOG_ERR("Display not ready");
		return -1;
	}
	if (ov7670_init() != 0)
	{
		LOG_ERR("Camera init failed");
		return -EIO;
	}
	if (fifo_init() != 0)
	{
		LOG_ERR("FIFO init failed");
		return -EIO;
	}
	if (setup_button(&btn0, &btn0_cb, on_sw0) != 0 ||
		 setup_button(&btn1, &btn1_cb, on_sw1) != 0 ||
		 setup_button(&btn2, &btn2_cb, on_sw2) != 0)
	{
		return -ENODEV;
	}

	k_msleep(300);
	tft_fill_screen(display, TFT_COLOR_BLACK);

	LOG_INF("Ready — sw0=capture/live, sw1=send drone, sw2=send clear");

	int64_t next = 0;

	preproc_diff_scaling.inject(const_cast<device *>(display), fifo_capture, tft_draw_image);
	preproc_diff_scaling.init();
	show_handler(&preproc_diff_scaling, const_cast<device *>(display));

	bool showing_grayscale = true;
	bool was_grayscale = true;
	uint64_t generated_timestamp = 0;

	int32_t sleep_timeouts[] = {
		0,
		50,
		100,
		200,
		400,
	};
	size_t sleep_timeout_idx = 0;
	while (true)
	{
		/* sw0: toggle between live view and frozen frame */
		if (sw0_flag)
		{
			if (app_state == AppState::LIVE)
			{
				was_grayscale = showing_grayscale;
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
			}
			else
			{
				generated_timestamp = 0;
				app_state = AppState::LIVE;
				LOG_INF("Back to live view using handler %s", preproc_diff_scaling.get_name());
				preproc_diff_scaling.process();
				showing_grayscale = was_grayscale;
				tft_draw_bounding_box(display, 0, 0, 160, 120, showing_grayscale ? "live grayscale" : "live diff");
				k_msleep(200);
			}

			sw0_flag = false;
		}

		if (app_state == AppState::FROZEN)
		{
			if (sw1_flag || sw2_flag)
			{
				/* sw1/sw2: label and transmit the frozen frame */
				bool drone = sw1_flag ? true : false;
				sw1_flag = false;
				sw2_flag = false;

				//Hacky invariant: Data is ready if and only if the timestamp is set.
				if (generated_timestamp == 0) {
					generated_timestamp = k_uptime_get();
					preproc_diff_scaling.prepare_data();
				}

				// The various buffers to send:
				// Current frame:
				uint8_t *current_frame_buf = preproc_diff_scaling.get_current_frame_buf();
				uint8_t *previous_frame_buf = preproc_diff_scaling.get_previous_frame_buf();
				uint8_t *unpadded_grayscale_buf = preproc_diff_scaling.get_current_grayscale_nopad();
				uint8_t *diff_grayscale_buf = preproc_diff_scaling.get_current_diff_grayscale_nopad();
				uint8_t *downscaled_2x2_grayscale_buf = preproc_diff_scaling.get_current_grayscale_downscaled_2x2_nopad();
				uint8_t *downscaled_3x3_grayscale_buf = preproc_diff_scaling.get_current_grayscale_downscaled_3x3_nopad();
				uint8_t *downscaled_4x4_grayscale_buf = preproc_diff_scaling.get_current_grayscale_downscaled_4x4_nopad();
				uint8_t *downscaled_2x2_diff_grayscale_buf = preproc_diff_scaling.get_current_diff_downscaled_2x2_nopad();
				uint8_t *downscaled_3x3_diff_grayscale_buf = preproc_diff_scaling.get_current_diff_downscaled_3x3_nopad();
				uint8_t *downscaled_4x4_diff_grayscale_buf = preproc_diff_scaling.get_current_diff_downscaled_4x4_nopad();

				// Used for current_frame_buf, previous_frame_buf, unpadded_grayscale_buf, diff_grayscale_buf
				int frame_height = IMG_H;
				int frame_width = IMG_W;

				// Used for downscaled_2x2_diff_grayscale_buf and downscaled_2x2_grayscale_buf
				int downscaled_2x2_height = IMG_H / 2;
				int downscaled_2x2_width = IMG_W / 2;

				// Used for downscaled_3x3_diff_grayscale_buf and downscaled_3x3_grayscale_buf
				int downscaled_3x3_height = IMG_H / 3;
				int downscaled_3x3_width = IMG_W / 3;

				// Used for downscaled_4x4_diff_grayscale_buf and downscaled_4x4_grayscale_buf
				int downscaled_4x4_height = IMG_H / 4;
				int downscaled_4x4_width = IMG_W / 4;

				tft_draw_grayscale_image(display, 0, 0, IMG_W, IMG_H, preproc_diff_scaling.get_current_grayscale_padded(), true);
				tft_draw_bounding_box(display, 0, 0, 160, 120, drone ? "sending drone" : "sending clear");


				char prefix[30];
				char current_frame_filename[64];
				char previous_frame_filename[64];
				char diff_frame_filename[64];

				snprintf(prefix, sizeof(prefix), "%llu_%s_", generated_timestamp, drone ? "drone" : "clear");
				snprintf(current_frame_filename, sizeof(current_frame_filename), "%scurrent_frame.png", prefix);
				snprintf(previous_frame_filename, sizeof(previous_frame_filename), "%sprevious_frame.png", prefix);
				snprintf(diff_frame_filename, sizeof(diff_frame_filename), "%sdiff_frame.png", prefix);
				const char* drone_or_clear = drone ? "drone" : "clear";

				struct data_to_send data[] = {
					//{current_frame_buf, frame_width, frame_height, 2, "color", current_frame_filename},
					//{previous_frame_buf, frame_width, frame_height, 2, "color", previous_frame_filename},
					{unpadded_grayscale_buf, frame_width, frame_height, 1, "grey", current_frame_filename},
					{diff_grayscale_buf, frame_width, frame_height, 1, "grey", diff_frame_filename},
					{downscaled_2x2_grayscale_buf, downscaled_2x2_width, downscaled_2x2_height, 1, "2x2", current_frame_filename},
					{downscaled_2x2_diff_grayscale_buf, downscaled_2x2_width, downscaled_2x2_height, 1, "2x2", diff_frame_filename},
					{downscaled_3x3_grayscale_buf, downscaled_3x3_width, downscaled_3x3_height, 1, "3x3", current_frame_filename},
					{downscaled_3x3_diff_grayscale_buf, downscaled_3x3_width, downscaled_3x3_height, 1, "3x3", diff_frame_filename},
					{downscaled_4x4_grayscale_buf, downscaled_4x4_width, downscaled_4x4_height, 1, "4x4", current_frame_filename},
					{downscaled_4x4_diff_grayscale_buf, downscaled_4x4_width, downscaled_4x4_height, 1, "4x4", diff_frame_filename},
				};

				bool transmission_failed = false;
				for (size_t i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
					if (!uart_img_send(uart, data[i].buf, data[i].width, data[i].height, data[i].bytes_per_pixel, 1, drone_or_clear, data[i].folder, data[i].filename, RETRY_TRANSMISSIONS_ON_FAILURE)) {
						LOG_ERR("Failed to send %s", data[i].filename);
						transmission_failed = true;
					} else {
						LOG_INF("Sent %s", data[i].filename);
					}
					k_msleep(10);
				}

				if (transmission_failed) {
					LOG_ERR("Failed to send all data for %s", prefix);
					showing_grayscale = true;
					tft_draw_grayscale_image(display, 0, 0, IMG_W, IMG_H, preproc_diff_scaling.get_current_grayscale_padded(), true);
					tft_draw_bounding_box(display, 0, 0, 160, 120, "TRANSMISSION FAILED");
					k_msleep(1000);
					sw1_flag = false;
					sw2_flag = false;
				} else {
					LOG_INF("Sent all data: %s", prefix);
					generated_timestamp = 0;
					app_state = AppState::LIVE;
					LOG_INF("Transferred %s", prefix);
					preproc_diff_scaling.process();
					preproc_diff_scaling.process();
					showing_grayscale = was_grayscale;
					uint8_t* current_buffer = showing_grayscale ? preproc_diff_scaling.get_current_grayscale_padded() : preproc_diff_scaling.get_current_diff_grayscale_padded();
					tft_draw_grayscale_image(display, 0, 0, IMG_W, IMG_H, current_buffer, true);
					tft_draw_bounding_box(display, 0, 0, 160, 120, "SENT OK!");
					k_msleep(400);
				}
			}
			else
			{
				if (showing_grayscale)
				{
					tft_draw_grayscale_image(display, 0, 0, IMG_W, IMG_H, preproc_diff_scaling.get_current_grayscale_padded(), true);
					tft_draw_bounding_box(display, 0, 0, 160, 120, "grayscale");
					k_msleep(200);
				}
				else
				{
					tft_draw_grayscale_image(display, 0, 0, IMG_W, IMG_H, preproc_diff_scaling.get_current_diff_grayscale_padded(), true);
					tft_draw_bounding_box(display, 0, 0, 160, 120, "diff");
					k_msleep(200);
				}

				showing_grayscale = !showing_grayscale;
			}

			k_msleep(10);
		}
		else
		{
			k_msleep(1);
			bool sw1_pressed = sw1_flag;
			if (sw1_pressed)
			{
				showing_grayscale = !showing_grayscale;
			}

			bool sw2_pressed = sw2_flag;

			// next = k_uptime_get();
			preproc_diff_scaling.process();
			uint8_t* active_grayscale = showing_grayscale ? preproc_diff_scaling.get_current_grayscale_padded() : preproc_diff_scaling.get_current_diff_grayscale_padded();

			tft_draw_grayscale_image(display, 0, 0, IMG_W, IMG_H, active_grayscale, true);

			if (sw1_pressed)
			{
				tft_draw_bounding_box(display, 0, 0, 160, 120, showing_grayscale ? "grayscale" : "diff");
				k_msleep(100);
				sw1_flag = false;
				sw2_flag = false;
			} else if (sw2_pressed) {
				//Go to the next index in the loop;
				sleep_timeout_idx = (sleep_timeout_idx + 1) % (sizeof(sleep_timeouts) / sizeof(int32_t));
				char buffer[12];
				snprintf(buffer, sizeof(buffer), "%d", sleep_timeouts[sleep_timeout_idx]); // Convert to string
				tft_draw_bounding_box(display, 0, 0, 160, 120, buffer);
				sw1_flag = false;
				sw2_flag = false;
			}


			k_msleep(sleep_timeouts[sleep_timeout_idx]);
		}
	}

	return 0;
}
