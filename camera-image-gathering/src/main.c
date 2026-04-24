/*
 * Camera image gathering — nRF54L15 DK (Zephyr)
 *
 * sw0 (live)   → capture frame, freeze it on screen
 * sw0 (frozen) → return to live view
 * sw1 (frozen) → send frozen frame with metadata "label=drone"
 * sw2 (frozen) → send frozen frame with metadata "label=clear"
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "ov7670.h"
#include "fifo.h"
#include "tft_display.h"
#include "uart_img_send.h"

#define FRAME_RATE        5
#define FRAME_INTERVAL_MS (1000 / FRAME_RATE)
#define DEBOUNCE_MS       80

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static uint8_t frame_buf[IMG_SIZE] __aligned(4);

typedef enum { STATE_LIVE, STATE_FROZEN } app_state_t;
static volatile app_state_t app_state = STATE_LIVE;
static volatile bool sw0_flag;
static volatile bool sw1_flag;
static volatile bool sw2_flag;

static const struct gpio_dt_spec btn0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec btn1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static const struct gpio_dt_spec btn2 = GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios);
static struct gpio_callback btn0_cb, btn1_cb, btn2_cb;

static int64_t sw0_last_ms = INT32_MIN;
static int64_t sw1_last_ms = INT32_MIN;
static int64_t sw2_last_ms = INT32_MIN;

static void on_sw0(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
	int64_t now = k_uptime_get();
	if ((now - sw0_last_ms) >= DEBOUNCE_MS) {
		sw0_last_ms = now;
		sw0_flag = true;
	}
}

static void on_sw1(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
	int64_t now = k_uptime_get();
	if ((now - sw1_last_ms) >= DEBOUNCE_MS) {
		sw1_last_ms = now;
		sw1_flag = true;
	}
}

static void on_sw2(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
	int64_t now = k_uptime_get();
	if ((now - sw2_last_ms) >= DEBOUNCE_MS) {
		sw2_last_ms = now;
		sw2_flag = true;
	}
}

static int setup_button(const struct gpio_dt_spec *btn, struct gpio_callback *cb,
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

int main(void)
{
	LOG_INF("*** Camera image gathering ***");

	const struct device *display = TFT_DEVICE();
	const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

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
	while (1) {
		/* sw0: toggle between live view and frozen frame */
		if (sw0_flag) {
			sw0_flag = false;
			if (app_state == STATE_LIVE) {
				fifo_capture(frame_buf, IMG_SIZE);
				tft_draw_image(display, 0, 0, 160, 120, frame_buf);
				app_state = STATE_FROZEN;
				LOG_INF("Frame frozen — sw1=drone, sw2=clear, sw0=live");
			} else {
				app_state = STATE_LIVE;
				LOG_INF("Back to live view");
			}
		}

		if (app_state == STATE_FROZEN) {
			/* sw1/sw2: label and transmit the frozen frame */
			if (sw1_flag) {
				sw1_flag = false;
				LOG_INF("Sending: label=drone");
				uart_img_send(uart, frame_buf, 160, 120, 2, 1, "label=drone");
				LOG_INF("Done");
			} else if (sw2_flag) {
				sw2_flag = false;
				LOG_INF("Sending: label=clear");
				uart_img_send(uart, frame_buf, 160, 120, 2, 1, "label=clear");
				LOG_INF("Done");
			}
			k_msleep(10);
		} else {
			/* Discard label presses that arrive during live view */
			sw1_flag = false;
			sw2_flag = false;

			next = k_uptime_get();
			fifo_capture(frame_buf, IMG_SIZE);
			tft_draw_image(display, 0, 0, 160, 120, frame_buf);
			int32_t remaining = FRAME_INTERVAL_MS - (int32_t)(k_uptime_get() - next);
			if (remaining > 0) {
				k_msleep(remaining);
			}
		}
	}

	return 0;
}
