#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "tft_display.h"
#include "usnlogo.h"
#include "uart_img_send.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback btn_cb;
static volatile bool send_flag;

static void btn_pressed(const struct device *dev, struct gpio_callback *cb,
			uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	send_flag = true;
}

int main(void)
{
	const struct device *display = TFT_DEVICE();
	const struct device *uart    = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	if (tft_init(display) != 0) {
		LOG_ERR("Display not ready");
		return -1;
	}

	tft_fill_screen(display, TFT_COLOR_BLACK);
	tft_draw_image(display, 0, 0, USNLOGO_WIDTH, USNLOGO_HEIGHT,
		       usnlogo_rgb565);
	tft_draw_bounding_box(display, 40, 10, 80, 55, "USN 0.97");

	if (!gpio_is_ready_dt(&btn)) {
		LOG_ERR("Button GPIO not ready");
		return -ENODEV;
	}
	gpio_pin_configure_dt(&btn, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&btn_cb, btn_pressed, BIT(btn.pin));
	gpio_add_callback(btn.port, &btn_cb);

	LOG_INF("Ready — press Button 1 to send the USN logo over UART");

	while (1) {
		if (send_flag) {
			send_flag = false;
			k_msleep(50); /* debounce */
			LOG_INF("Sending USN logo (%u bytes)...",
				(unsigned)(USNLOGO_WIDTH * USNLOGO_HEIGHT * 2U));
			uart_img_send(uart, usnlogo_rgb565,
				      USNLOGO_WIDTH, USNLOGO_HEIGHT,
				      2, 1 /* big-endian */);
			LOG_INF("Done");
		}
		k_msleep(10);
	}

	return 0;
}
