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

/*
 * Send one packet and wait for ACK, retrying up to max_tries times.
 *
 * Packet wire format:
 *   [seq_lo seq_hi len_lo len_hi] [payload...] [xor_checksum]
 *
 * checksum = XOR of every byte in the packet header and payload.
 */
static bool send_packet(const struct device *uart, uint16_t seq,
			const uint8_t *data, uint16_t len, uint8_t max_tries)
{
	uint8_t pkt_hdr[4] = {
		seq & 0xFF, (seq >> 8) & 0xFF,
		len & 0xFF, (len >> 8) & 0xFF,
	};

	uint8_t checksum = 0;
	for (int i = 0; i < 4; i++) {
		checksum ^= pkt_hdr[i];
	}
	for (uint16_t i = 0; i < len; i++) {
		checksum ^= data[i];
	}

	for (uint8_t attempt = 0; attempt < max_tries; attempt++) {
		uart_write(uart, pkt_hdr, sizeof(pkt_hdr));
		uart_write(uart, data, len);
		uart_write(uart, &checksum, 1);
		if (wait_for_ack(uart)) {
			return true;
		}
		k_msleep(10 * (attempt + 1));
	}
	return false;
}

bool uart_img_send(const struct device *uart, const uint8_t *pixels,
		   uint16_t width, uint16_t height, uint8_t bpp,
		   uint8_t big_endian,
		   const char *label, const char *folder, const char *filename,
		   uint8_t max_tries)
{
	/* Build metadata string once. */
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

	uint8_t img_hdr[8] = {
		width    & 0xFF, (width    >> 8) & 0xFF,
		height   & 0xFF, (height   >> 8) & 0xFF,
		bpp,
		big_endian,
		meta_len & 0xFF, (meta_len >> 8) & 0xFF,
	};

	/* Header packet payload: 8-byte image header + metadata (max 263 bytes total). */
	uint8_t hdr_pkt[8 + sizeof(meta_buf)];
	memcpy(hdr_pkt, img_hdr, sizeof(img_hdr));
	if (meta_len > 0) {
		memcpy(hdr_pkt + sizeof(img_hdr), meta_buf, meta_len);
	}
	uint16_t hdr_pkt_len = (uint16_t)(sizeof(img_hdr) + meta_len);

	uart_write(uart, FRAME_SOF, sizeof(FRAME_SOF));

	/* Packet 0: image header + metadata. */
	if (!send_packet(uart, 0, hdr_pkt, hdr_pkt_len, max_tries)) {
		return false;
	}

	/* Packets 1..N: pixel data in UART_IMG_PACKET_SIZE-byte chunks. */
	size_t total = (size_t)width * height * bpp;
	uint16_t seq = 1;

	for (size_t offset = 0; offset < total; offset += UART_IMG_PACKET_SIZE) {
		uint16_t chunk = (uint16_t)((total - offset > UART_IMG_PACKET_SIZE)
					    ? UART_IMG_PACKET_SIZE
					    : (total - offset));
		if (!send_packet(uart, seq++, pixels + offset, chunk, max_tries)) {
			return false;
		}
	}

	/* End-of-frame marker; receiver validates and sends a final ACK. */
	uart_write(uart, FRAME_EOF, sizeof(FRAME_EOF));

	return wait_for_ack(uart);
}
