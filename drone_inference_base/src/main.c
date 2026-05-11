#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "inference.h"

#include "..\..\lib\c\ov7670.h"
#include "..\..\lib\c\fifo.h"
#include "..\..\lib\c\tft_display.h"
#include "..\..\lib\c\pixel_conversion.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define DEBOUNCE_MS 80

#define NEW_IMAGE_TIMEOUT 600

#ifdef CONFIG_SCALING_1x1
#define PIC_SCALING 1
#endif
#ifdef CONFIG_SCALING_2x2
#define PIC_SCALING 2
#endif
#ifdef CONFIG_SCALING_3x3
#define PIC_SCALING 3
#endif
#ifdef CONFIG_SCALING_4x4
#define PIC_SCALING 4
#endif

#define SCALED_W (IMG_W / PIC_SCALING)
#define SCALED_H (IMG_H / PIC_SCALING)

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

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

uint8_t gray_image_buffer_a[SCALED_W * SCALED_H];
uint8_t gray_image_buffer_b[SCALED_W * SCALED_H];

uint16_t aggregated_gray_line[IMG_W / PIC_SCALING];

void read_grayscale_to_image_buffer(uint8_t *current_buffer)
{
	fifo_grayscale_capture_565(current_buffer, IMG_W, IMG_H, PIC_SCALING, aggregated_gray_line);
}

int main(void)
{
	const struct device *display = TFT_DEVICE();

	// Setting up Inference...
	bool inference_ok = (drone_inference_init() == 0);
	if (!inference_ok)
	{
		LOG_ERR("Inference init failed\n");
		return -1;
	}
	// Finished setting up Inference

	if (tft_init(display) != 0)
	{
		LOG_ERR("Display not ready\n");
		return -1;
	}

	if (ov7670_init() != 0)
	{
		LOG_ERR("Camera init failed\n");
		return -EIO;
	}

	if (fifo_init() != 0)
	{
		LOG_ERR("FIFO init failed\n");
		return -EIO;
	}

	if (setup_button(&btn0, &btn0_cb, on_sw0) != 0 ||
		 setup_button(&btn1, &btn1_cb, on_sw1) != 0 ||
		 setup_button(&btn2, &btn2_cb, on_sw2) != 0)
	{
		return -ENODEV;
	}

	k_msleep(300); /* auto-exposure settle */

	tft_fill_screen(display, TFT_COLOR_BLACK);

	uint8_t *current_buffer = gray_image_buffer_a;
	uint8_t *other_buffer = gray_image_buffer_b;
	bool current_is_a = true;

	int64_t prev_timestamp;

	read_grayscale_to_image_buffer(current_buffer);
	prev_timestamp = k_uptime_get();

	bool show_grayscale = true;

	bool show_nodrone = false;

	while (1)
	{

		// Preparing the next frame
		current_is_a = !current_is_a;
		current_buffer = current_is_a ? gray_image_buffer_a : gray_image_buffer_b;
		other_buffer = current_is_a ? gray_image_buffer_b : gray_image_buffer_a;

		const int64_t curr_timestamp = k_uptime_get();
		int64_t delta_ms = curr_timestamp - prev_timestamp;

		LOG_INF("New frame: %llu ms", delta_ms);
		// delta_ms is about 200 milliseconds.
		if (delta_ms > NEW_IMAGE_TIMEOUT)
		{
			// Reading multiple times, to ensure that the camera works!
			read_grayscale_to_image_buffer(other_buffer);
			read_grayscale_to_image_buffer(other_buffer);

			// This is the "real" previous image
			read_grayscale_to_image_buffer(other_buffer);
			// Suitable sleep duration - we want a difference in the last two images
			k_msleep(100);
		}

		prev_timestamp = k_uptime_get();
		read_grayscale_to_image_buffer(current_buffer);
		overwrite_unpadded_previous_grayscale_with_diff_minus(current_buffer, other_buffer, SCALED_W, SCALED_H, GRAYSCALE_DIFF_GATE_VALUE);

		bool got_sw0 = false;
		if (sw0_flag)
		{
			sw0_flag = false;
			got_sw0 = true;
			show_grayscale = !show_grayscale;
		}
		bool got_sw1 = false;
		if (sw1_flag)
		{
			sw1_flag = false;
			got_sw1 = true;
			show_nodrone = !show_nodrone;
		}

		uint8_t *shown_buffer = show_grayscale ? current_buffer : other_buffer;

		if (got_sw0)
		{
			tft_draw_scaled_grayscale_image(display, 0, 0, SCALED_W, SCALED_H, shown_buffer, PIC_SCALING);
			tft_draw_bounding_box(display, 0, 0, 160, 120, show_grayscale ? "grayscale" : "diff");
			k_msleep(200);
		}
		else if (got_sw1)
		{
			tft_draw_scaled_grayscale_image(display, 0, 0, SCALED_W, SCALED_H, shown_buffer, PIC_SCALING);
			tft_draw_bounding_box(display, 0, 0, 160, 120, show_nodrone ? "Show Nodrone" : "Hide Nodrone");
			k_msleep(200);
		}
		else
		{
			drone_result_t result;
			delta_ms = k_uptime_get();
			int rc = drone_inference_run(current_buffer, other_buffer, SCALED_W * SCALED_H, &result);
			LOG_INF("Inference: %llu ms", k_uptime_get() - delta_ms);

			char logtext[30];
			int decimals = (int)(result.confidence * 1000);
			if (decimals == 1000)
			{
				decimals = 999;
			}
			snprintf(logtext, sizeof(logtext), "%s .%03d", result.detected ? "D" : "Nodrone", decimals);

			//Delay drawing until the inference is done, to minimize flickering.
			tft_draw_scaled_grayscale_image(display, 0, 0, SCALED_W, SCALED_H, shown_buffer, PIC_SCALING);

			uint16_t color;
			if (rc != 0)
			{
				tft_draw_bounding_box_color(display, 0, 0, IMG_W, IMG_H, "ERROR", TFT_COLOR_RED);
			}
			else if (result.detected)
			{
				if (result.confidence > 0.9)
				{
					color = TFT_COLOR_RED;
				}
				else if (result.confidence > 0.8)
				{
					color = TFT_COLOR_ORANGE;
				}
				else
				{
					color = TFT_COLOR_YELLOW;
				}

				int box_w = (int)(result.w * IMG_W);
				int box_h = (int)(result.h * IMG_H);
				box_w = MIN(box_w, IMG_W);
				box_h = MIN(box_h, IMG_H);

				int box_x = (int)((result.x - (result.w / 2)) * IMG_W);
				int box_y = (int)((result.y - (result.h / 2)) * IMG_H);
				box_x = MAX(box_x, 0);
				box_x = MIN(box_x, IMG_W);
				box_y = MAX(box_y, 0);
				box_y = MIN(box_y, IMG_H);

				if (box_x + box_w > IMG_W)
				{
					box_w = IMG_W - box_x;
				}

				if (box_y + box_h > IMG_H)
				{
					box_y = IMG_H - box_y;
				}

				tft_draw_bounding_box_color(display, box_x, box_y, box_w, box_h, logtext, color);
			}
			else
			{
				if (show_nodrone)
				{
					color = result.confidence < 0.3 ? TFT_COLOR_DARKGREEN : TFT_COLOR_GREEN;
					tft_draw_bounding_box_color(display, 0, 0, IMG_W, IMG_H, logtext, color);
				}
			}
		}

		k_msleep(1);
	}
	return 0;
}
