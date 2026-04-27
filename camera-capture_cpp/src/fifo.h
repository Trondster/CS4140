#ifndef FIFO_H_
#define FIFO_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Image dimensions — QQVGA RGB565 */
#define IMG_W    160
#define IMG_H    120
#define IMG_BPP  2
#define IMG_SIZE (IMG_W * IMG_H * IMG_BPP)  /* 38 400 bytes */
#define LINE_STRIDE (IMG_W * IMG_BPP)
#define BUF_IMG_W (IMG_W + 2)
#define BUF_IMG_H (IMG_H + 2)
#define BUF_IMG_SIZE (BUF_IMG_W * BUF_IMG_H)  /* 19 524 bytes */


/**
 * @brief Configure GPIO pins for the AL422B FIFO interface.
 *
 * Sets up D0–D7 (P2.00–P2.07 as inputs), WEN (P0.04), RRST (P1.14),
 * VSYNC (P2.08), RCK (P2.09), and WRST (P2.10).  Call once at startup
 * before fifo_capture().
 *
 * @return 0 on success, negative errno on failure.
 */
int fifo_init(void);

/**
 * @brief Capture one full frame from the AL422B FIFO into @p buf.
 *
 * Waits for VSYNC frame boundaries, arms the write cycle, then clocks
 * out @p size bytes.
 *
 * @param buf   Destination buffer.  Must be at least @p size bytes.
 * @param size  Bytes to read — normally IMG_SIZE.
 * @return 0 on success.
 */
int fifo_capture(uint8_t *buf, size_t size, size_t line_stride);

#ifdef __cplusplus
}
#endif

#endif /* FIFO_H_ */
