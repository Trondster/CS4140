#ifndef UART_IMG_SEND_H_
#define UART_IMG_SEND_H_

#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pixel format — pass as the bpp argument to uart_img_send().
 *
 *   UART_IMG_PIXFMT_GREYSCALE  1 byte  per pixel, 8-bit grey
 *   UART_IMG_PIXFMT_RGB565     2 bytes per pixel, 5-6-5 packed (endian applies)
 *   UART_IMG_PIXFMT_RGB888     3 bytes per pixel, 8-8-8 planar
 */
#define UART_IMG_PIXFMT_GREYSCALE  1u
#define UART_IMG_PIXFMT_RGB565     2u
#define UART_IMG_PIXFMT_RGB888     3u

/*
 * Binary frame protocol:
 *
 *   [0xDE 0xAD 0xBE 0xEF]    4 bytes — start-of-frame marker
 *   [W_lo  W_hi]              2 bytes — image width  (little-endian uint16)
 *   [H_lo  H_hi]              2 bytes — image height (little-endian uint16)
 *   [bpp]                     1 byte  — bytes per pixel (see UART_IMG_PIXFMT_*)
 *   [endian]                  1 byte  — 0 = little-endian pixels, 1 = big-endian
 *                                       (only meaningful for RGB565)
 *   [meta_len_lo meta_len_hi] 2 bytes — metadata length (little-endian uint16)
 *   [meta_len bytes]          UTF-8 metadata string (no null terminator)
 *                             Format: "label=X,folder=Y,filename=Z"
 *                             All keys are optional.
 *   [W × H × bpp bytes]       raw pixel data
 *   [0xCA 0xFE 0xBA 0xBE]    4 bytes — end-of-frame marker
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
 * @param bpp        Bytes per pixel — use UART_IMG_PIXFMT_* constants
 * @param big_endian 1 if pixels are big-endian (e.g. ST7735R format), 0 if little-endian
 * @param label      Label / class name used as a subfolder on the receiver (e.g. "drone"), or NULL
 * @param folder     Base output folder on the receiver (e.g. "dataset/train"), or NULL
 * @param filename   Output filename on the receiver (e.g. "frame001.png"), or NULL for auto
 */
void uart_img_send(const struct device *uart, const uint8_t *pixels,
		   uint16_t width, uint16_t height, uint8_t bpp,
		   uint8_t big_endian,
		   const char *label, const char *folder, const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* UART_IMG_SEND_H_ */
