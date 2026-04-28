#include "uart_img_send.h"

#include <stdio.h>
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
		   uint8_t big_endian,
		   const char *label, const char *folder, const char *filename)
{
	/* Build metadata string: "label=X,folder=Y,filename=Z" (omit empty keys) */
	char meta_buf[256];
	int pos = 0;

	if (label && label[0]) {
		pos += snprintf(meta_buf + pos, sizeof(meta_buf) - pos,
				"label=%s", label);
	}
	if (folder && folder[0]) {
		if (pos > 0) {
			meta_buf[pos++] = ',';
		}
		pos += snprintf(meta_buf + pos, sizeof(meta_buf) - pos,
				"folder=%s", folder);
	}
	if (filename && filename[0]) {
		if (pos > 0) {
			meta_buf[pos++] = ',';
		}
		pos += snprintf(meta_buf + pos, sizeof(meta_buf) - pos,
				"filename=%s", filename);
	}

	uint16_t meta_len = (uint16_t)pos;

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
		uart_write(uart, (const uint8_t *)meta_buf, meta_len);
	}
	uart_write(uart, pixels, (size_t)width * height * bpp);
	uart_write(uart, FRAME_EOF, sizeof(FRAME_EOF));
}
