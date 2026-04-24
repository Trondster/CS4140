#ifndef OV7670_FIFO_H_
#define OV7670_FIFO_H_

#include <stddef.h>
#include <stdint.h>

/* QQVGA RGB565 frame geometry */
#define IMG_W    160
#define IMG_H    120
#define IMG_BPP  2
#define IMG_SIZE (IMG_W * IMG_H * IMG_BPP)  /* 38400 bytes */

/**
 * @brief Configure the OV7670 image sensor for QQVGA RGB565 output via SCCB.
 *
 * Scans the I2C bus, verifies the chip ID (PID=0x76, VER=0x73), writes the
 * QQVGA RGB565 register table, and reads back critical registers to confirm.
 *
 * @return 0 on success, negative errno on failure.
 */
int ov7670_init(void);

/**
 * @brief Configure GPIO pins for the AL422B FIFO interface.
 *
 * Sets up D0-D7 (P2.00-P2.07 as inputs), WEN (P0.04), RRST (P1.14),
 * VSYNC (P2.08), RCK (P2.09), and WRST (P2.10). Call once at startup.
 *
 * @return 0 on success, negative errno on failure.
 */
int fifo_init(void);

/**
 * @brief Capture one frame from the AL422B FIFO into @p buf.
 *
 * Waits for VSYNC frame boundaries, arms the write cycle, then clocks
 * out @p size bytes.
 *
 * @param buf   Destination buffer (must be at least @p size bytes).
 * @param size  Bytes to read — normally IMG_SIZE.
 * @return 0 on success.
 */
int fifo_capture(uint8_t *buf, size_t size);

#endif /* OV7670_FIFO_H_ */
