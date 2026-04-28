#include "pixel_conversion.h"


uint8_t calculate_grayscale(const uint8_t rgb565_pixel1, const uint8_t rgb565_pixel2) {
   //Extract R, G, B components from the RGB565 pixel
   uint16_t pixel = (rgb565_pixel1 << 8) | rgb565_pixel2;
   uint16_t r = (pixel >> 11) & 0x1F; // 5 bits for red
   uint16_t g = (pixel >> 5) & 0x3F;  // 6 bits for green
   uint16_t b = pixel & 0x1F;         // 5 bits for blue

   //Scaling up R, G, B to 0-255 range for better grayscale calculation
   r = r * 255 / 31;
   g = g * 255 / 63;
   b = b * 255 / 31;

   // Convert to grayscale using the formula: gray = 0.299*R + 0.587*G + 0.114*B
   uint8_t gray = (r * 299 + g * 587 + b * 114) / 1000;
   return gray;
}

uint8_t calculate_grayscale_2(const uint8_t rgb565_pixel1, const uint8_t rgb565_pixel2) {
   //Extract R, G, B components from the RGB565 pixel
   uint16_t pixel = (rgb565_pixel1 << 8) | rgb565_pixel2;
   uint16_t r = (pixel >> 11) & 0x1F; // 5 bits for red
   uint16_t g = (pixel >> 5) & 0x3F;  // 6 bits for green
   uint16_t b = pixel & 0x1F;         // 5 bits for blue

   r = r * 255 / 31;
   g = g * 255 / 63;
   b = b * 255 / 31;

   // Simple average method to convert to grayscale: gray = (R + G + B) / 3
   uint8_t gray = (r + g + b) / 3;
   return gray;
}


uint8_t calculate_grayscale_3(const uint8_t rgb565_pixel1, const uint8_t rgb565_pixel2) {
   //Extract R, G, B components from the RGB565 pixel
   uint16_t pixel = (rgb565_pixel1 << 8) | rgb565_pixel2;
   uint16_t r = (pixel >> 11) & 0x1F; // 5 bits for red
   uint16_t g = (pixel >> 5) & 0x3F;  // 6 bits for green
   uint16_t b = pixel & 0x1F;         // 5 bits for blue

   //Scale R, G, B to 0-255 range for better grayscale calculation
   r = r * 255 / 31;
   g = g * 255 / 63;
   b = b * 255 / 31;

   // Funky method to convert to grayscale: gray = (R + G + B) / 3
   uint8_t gray = (r * 2104 + g * 4130 + b * 802 + 2048) >> 12;
   return gray;
}

uint16_t calculate_rgb565(const uint8_t gray) {
    // Convert grayscale value back to RGB565 format (where R=G=B=gray)
    uint16_t pixel = ((gray >> 3) << 11) | ((gray >> 2) << 5) | (gray >> 3);
    return pixel;
}

