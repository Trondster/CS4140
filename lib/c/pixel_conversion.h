#ifndef PIXEL_CONVERSION_H_
#define PIXEL_CONVERSION_H_


#include <stdint.h>

#define PER_COLOR_DIFF_GATE_VALUE 1
#define GRAYSCALE_DIFF_GATE_VALUE 3

#ifdef __cplusplus
extern "C" {
#endif


uint8_t calculate_grayscale(const uint8_t rgb565_pixel1, const uint8_t rgb565_pixel2);

uint8_t calculate_grayscale_2(const uint8_t rgb565_pixel1, const uint8_t rgb565_pixel2);

uint8_t calculate_grayscale_3(const uint8_t rgb565_pixel1, const uint8_t rgb565_pixel2);

uint16_t calculate_rgb565(const uint8_t gray);

void overwrite_unpadded_previous_grayscale_with_diff_minus(const uint8_t* current_unpadded_grayscale_buf,
   uint8_t* previous_unpadded_grayscale_buf,
   const int width, const int height, const int gate_value);

#ifdef __cplusplus
}
#endif


#endif /* PIXEL_CONVERSION_H_ */