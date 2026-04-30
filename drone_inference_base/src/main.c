#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "..\..\lib\c\ov7670.h"
#include "..\..\lib\c\fifo.h"
#include "..\..\lib\c\tft_display.h"
#include "..\..\lib\c\pixel_conversion.h"

#define DEBOUNCE_MS       80

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


uint8_t gray_image_buffer_a[SCALED_W * SCALED_H];
uint8_t gray_image_buffer_b[SCALED_W * SCALED_H];

uint16_t aggregated_gray_line[IMG_W / PIC_SCALING];



void read_grayscale_to_image_buffer(uint8_t* current_buffer) {
   fifo_grayscale_capture_565(current_buffer, IMG_W, IMG_H, PIC_SCALING, aggregated_gray_line);
}

int main(void)
{
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

   if (setup_button(&btn0, &btn0_cb, on_sw0) != 0 ||
	    setup_button(&btn1, &btn1_cb, on_sw1) != 0 ||
	    setup_button(&btn2, &btn2_cb, on_sw2) != 0) {
		return -ENODEV;
	}

	k_msleep(300);   /* auto-exposure settle */
	
	tft_fill_screen(display, TFT_COLOR_BLACK);

   uint8_t* current_buffer = gray_image_buffer_a;
   uint8_t* other_buffer = gray_image_buffer_b;
   bool current_is_a = true;
   read_grayscale_to_image_buffer(current_buffer);

   bool show_grayscale = true;
   while (1) {

      //Preparing the next frame
      current_is_a = !current_is_a;
      current_buffer = current_is_a ? gray_image_buffer_a : gray_image_buffer_b;
      other_buffer = current_is_a ? gray_image_buffer_b : gray_image_buffer_a;
      read_grayscale_to_image_buffer(current_buffer);
      overwrite_unpadded_previous_grayscale_with_diff_minus(current_buffer, other_buffer, SCALED_W, SCALED_H, GRAYSCALE_DIFF_GATE_VALUE);

      bool got_sw0 = false;
		if (sw0_flag) {
			sw0_flag = false;
         got_sw0 = true;
         show_grayscale = !show_grayscale;
      }

      uint8_t* shown_buffer = show_grayscale ? current_buffer : other_buffer;

      tft_draw_scaled_grayscale_image(display, 0, 0, SCALED_W, SCALED_H, shown_buffer, PIC_SCALING);
      if (got_sw0) {
      	tft_draw_bounding_box(display, 0, 0, 160, 120, show_grayscale ? "grayscale": "diff");
         k_msleep(200);
      }

      k_msleep(10);
   }
   return 0;
}
