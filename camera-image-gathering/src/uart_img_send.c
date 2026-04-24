#include "uart_img_send.h"

#include <string.h>
#include <zephyr/drivers/uart.h>

static const uint8_t FRAME_SOF[] = {0xDE, 0xAD, 0xBE, 0xEF};
static const uint8_t FRAME_EOF[] = {0xCA, 0xFE, 0xBA, 0xBE};

static void uart_write(const struct device *uart, const uint8_t *buf,
		       size_t len)
{
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart, buf[i]);
	}
}

void uart_img_send(const struct device *uart, const uint8_t *pixels,
		   uint16_t width, uint16_t height, uint8_t bpp,
		   uint8_t big_endian, const char *metadata)
{
	uint16_t meta_len = (metadata != NULL) ? (uint16_t)strlen(metadata) : 0;

	uint8_t hdr[8] = {
		width    & 0xFF, (width    >> 8) & 0xFF,
		height   & 0xFF, (height   >> 8) & 0xFF,
		bpp,
		big_endian,
		meta_len & 0xFF, (meta_len >> 8) & 0xFF,
	};

	uart_write(uart, FRAME_SOF, sizeof(FRAME_SOF));
	uart_write(uart, hdr, sizeof(hdr));
	if (meta_len > 0) {
		uart_write(uart, (const uint8_t *)metadata, meta_len);
	}
	uart_write(uart, pixels, (size_t)width * height * bpp);
	uart_write(uart, FRAME_EOF, sizeof(FRAME_EOF));
}
