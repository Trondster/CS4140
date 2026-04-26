#ifndef UART_IMG_SEND_H_
#define UART_IMG_SEND_H_

#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Binary frame protocol:
 *
 *   [0xDE 0xAD 0xBE 0xEF]    4 bytes — start-of-frame marker
 *   [W_lo  W_hi]              2 bytes — image width  (little-endian uint16)
 *   [H_lo  H_hi]              2 bytes — image height (little-endian uint16)
 *   [bpp]                     1 byte  — bytes per pixel (2 = RGB565)
 *   [endian]                  1 byte  — 0 = little-endian pixels, 1 = big-endian
 *   [meta_len_lo meta_len_hi] 2 bytes — metadata length (little-endian uint16)
 *   [meta_len bytes]          UTF-8 metadata string (no null terminator)
 *   [W × H × bpp bytes]       raw pixel data
 *   [0xCA 0xFE 0xBA 0xBE]    4 bytes — end-of-frame marker
 *
 * The metadata field is a free-form UTF-8 string. Pass NULL or "" for none.
 * Example values:
 *   "drone=1"
 *   "class=drone,conf=0.97"
 *   "label=clear"
 *
 * Use scripts/receive_image.py on the host to capture and decode frames.
 */

/**
 * @brief Send a raw pixel buffer over UART using the binary frame protocol.
 *
 * @param uart       UART device (e.g. DEVICE_DT_GET(DT_CHOSEN(zephyr_console)))
 * @param pixels     Pointer to pixel data
 * @param width      Image width in pixels
 * @param height     Image height in pixels
 * @param bpp        Bytes per pixel (2 for RGB565)
 * @param big_endian 1 if pixels are big-endian (e.g. ST7735R format), 0 if little-endian
 * @param metadata   Null-terminated UTF-8 string attached to the frame, or NULL
 */
void uart_img_send(const struct device *uart, const uint8_t *pixels,
		   uint16_t width, uint16_t height, uint8_t bpp,
		   uint8_t big_endian, const char *metadata);

#ifdef __cplusplus
}
#endif

#endif /* UART_IMG_SEND_H_ */
