#include "uart_img_send.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

static const uint8_t FRAME_SOF[] = {0xDE, 0xAD, 0xBE, 0xEF};
static const uint8_t FRAME_EOF[] = {0xCA, 0xFE, 0xBA, 0xBE};

static void uart_write(const struct device *uart, const uint8_t *buf,
		       size_t len)
{
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart, buf[i]);
	}
}

/* Poll for one ACK/NAK byte; returns true only on ACK. */
static bool wait_for_ack(const struct device *uart)
{
	int64_t deadline = k_uptime_get() + UART_IMG_ACK_TIMEOUT_MS;
	unsigned char c;

	while (k_uptime_get() < deadline) {
		if (uart_poll_in(uart, &c) == 0) {
			return c == UART_IMG_ACK;
		}
		k_sleep(K_MSEC(1));
	}
	return false;
}

bool uart_img_send(const struct device *uart, const uint8_t *pixels,
		   uint16_t width, uint16_t height, uint8_t bpp,
		   uint8_t big_endian,
		   const char *label, const char *folder, const char *filename,
		   uint8_t max_tries)
{
	/* Build metadata string once — same for every attempt. */
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

	for (uint8_t attempt = 0; attempt < max_tries; attempt++) {
		uart_write(uart, FRAME_SOF, sizeof(FRAME_SOF));
		uart_write(uart, hdr, sizeof(hdr));
		if (meta_len > 0) {
			uart_write(uart, (const uint8_t *)meta_buf, meta_len);
		}
		uart_write(uart, pixels, (size_t)width * height * bpp);
		uart_write(uart, FRAME_EOF, sizeof(FRAME_EOF));

		if (wait_for_ack(uart)) {
			return true;
		}
		k_msleep(10 * (attempt + 1));
	}
	return false;
}
