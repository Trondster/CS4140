#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


uint8_t calculate_grayscale(const uint8_t rgb565_pixel1, const uint8_t rgb565_pixel2);

uint8_t calculate_grayscale_2(const uint8_t rgb565_pixel1, const uint8_t rgb565_pixel2);

uint8_t calculate_grayscale_3(const uint8_t rgb565_pixel1, const uint8_t rgb565_pixel2);

uint16_t calculate_rgb565(const uint8_t gray);


#ifdef __cplusplus
}
#endif
