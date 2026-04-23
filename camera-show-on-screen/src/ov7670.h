#ifndef OV7670_H_
#define OV7670_H_

/**
 * @brief Initialise the OV7670 image sensor over SCCB (I2C).
 *
 * Performs an I2C bus scan, verifies the OV7670 chip ID, writes the QQVGA
 * RGB565 register table, and reads back six critical registers to confirm
 * the configuration was accepted.
 *
 * Requires i2c21 to be ready before calling.
 *
 * @return 0 on success, negative errno on failure.
 */
int ov7670_init(void);

#endif /* OV7670_H_ */
