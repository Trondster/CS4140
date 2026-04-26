#ifndef PIXEL_CALCULATION_HPP_
#define PIXEL_CALCULATION_HPP_

#include <cstdint>
#include <stddef.h>
#include <stdlib.h>

#define PER_COLOR_DIFF_GATE_VALUE 1
#define GRAYSCALE_DIFF_GATE_VALUE 3

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

void calculate_grayscale_image(const uint8_t* input_buf, uint8_t* padded_grayscale_buf,
   const int width, const int height, const int bpp) {
   int buf_img_w = width + 2;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int idx = y * width * bpp + x * bpp;
         uint8_t gray = calculate_grayscale(input_buf[idx], input_buf[idx + 1]);
         padded_grayscale_buf[(y + 1) * buf_img_w + (x + 1)] = gray;
      }
   }
}

void calculate_grayscale_image_2(const uint8_t* input_buf, uint8_t* padded_grayscale_buf,
   const int width, const int height, const int bpp) {
   int buf_img_w = width + 2;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int idx = y * width * bpp + x * bpp;
         uint8_t gray = calculate_grayscale_2(input_buf[idx], input_buf[idx + 1]);
         padded_grayscale_buf[(y + 1) * buf_img_w + (x + 1)] = gray;
      }
   }
}

void calculate_grayscale_image_3(const uint8_t* input_buf, uint8_t* padded_grayscale_buf,
   const int width, const int height, const int bpp) {
   int buf_img_w = width + 2;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int idx = y * width * bpp + x * bpp;
         uint8_t gray = calculate_grayscale_3(input_buf[idx], input_buf[idx + 1]);
         padded_grayscale_buf[(y + 1) * buf_img_w + (x + 1)] = gray;
      }
   }
}

void convert_grayscale_to_rgb565(const uint8_t* padded_grayscale_buf, uint8_t* output_buf,
   const int width, const int height,const int bpp) {
   int buf_img_w = width + 2;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int grayidx = (y + 1) * buf_img_w + (x + 1);
         uint8_t gray = padded_grayscale_buf[grayidx];
         uint16_t pixel = calculate_rgb565(gray);
         output_buf[y * width * bpp + x * bpp] = pixel >> 8;
         output_buf[y * width * bpp + x * bpp + 1] = pixel & 0xFF;
      }
   }
}

void calculate_left_sobel(const uint8_t* padded_grayscale_buf, uint8_t* padded_output_buf, const int width, const int height) {
   int buf_img_w = width + 2;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int idx = (y + 1) * buf_img_w + (x + 1);
         int gx = -     padded_grayscale_buf[idx - buf_img_w - 1]
                  - 2 * padded_grayscale_buf[idx - 1]
                  -     padded_grayscale_buf[idx + buf_img_w - 1]
                  +     padded_grayscale_buf[idx - buf_img_w + 1]
                  + 2 * padded_grayscale_buf[idx + 1]
                  +     padded_grayscale_buf[idx + buf_img_w + 1];
         padded_output_buf[(y + 1) * buf_img_w + (x + 1)] = (uint8_t)(abs(gx) / 4); // Normalize and store in output buffer
      }
   }
}

//Calculates outline using Sobel operator
void calculate_outline(const uint8_t* padded_grayscale_buf, uint8_t* padded_output_buf, const int width, const int height) {
   int buf_img_w = width + 2;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int idx = (y + 1) * buf_img_w + (x + 1);
         int gx = -     padded_grayscale_buf[idx - buf_img_w - 1]
                  - 2 * padded_grayscale_buf[idx - 1]
                  -     padded_grayscale_buf[idx + buf_img_w - 1]
                  +     padded_grayscale_buf[idx - buf_img_w + 1]
                  + 2 * padded_grayscale_buf[idx + 1]
                  +     padded_grayscale_buf[idx + buf_img_w + 1];
         int gy =      padded_grayscale_buf[idx - buf_img_w - 1]
                  + 2 * padded_grayscale_buf[idx - buf_img_w]
                  +     padded_grayscale_buf[idx - buf_img_w + 1]
                  -     padded_grayscale_buf[idx + buf_img_w - 1]
                  - 2 * padded_grayscale_buf[idx + buf_img_w]
                  -     padded_grayscale_buf[idx + buf_img_w + 1];
         int magnitude = abs(gx) + abs(gy); // Approximate magnitude
         padded_output_buf[(y + 1) * buf_img_w + (x + 1)] = (uint8_t)(magnitude / 8); // Normalize and store in output buffer
      }
   }
}  


/// @brief Overwrites the previous frame buffer with direct byte to byte difference of the current frame and previous frame.
/// This is a very naive implementation that does not do any special handling for underflow/overflow, and just wraps around in those cases.
/// The output is still in RGB565 format, so it can be displayed directly without needing to convert to grayscale first.
/// This is intended as a simple baseline for comparison with more sophisticated difference methods that we might implement later.
/// @param current_frame_buf 
/// @param previous_frame_buf 
/// @param width 
/// @param height 
/// @param bpp 
/// @param gate_value 
void overwrite_previous_frame_with_direct_diff(const uint8_t* current_frame_buf, uint8_t* previous_frame_buf,
   const int width, const int height, const int bpp, const int gate_value) {
   int img_size = width * height * bpp;
   for (int i = 0; i < img_size; i++) {
      int diff = abs((int)current_frame_buf[i] - (int)previous_frame_buf[i]);
      previous_frame_buf[i] = (diff >= gate_value) ? (uint8_t)diff : 0;
   }
}

void overwrite_previous_grayscale_with_diff_abs(const uint8_t* current_padded_grayscale_buf, uint8_t* previous_padded_grayscale_buf,
   const int width, const int height, const int gate_value) {
   int buf_img_w = width + 2;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int idx = (y + 1) * buf_img_w + (x + 1);
         int diff = abs((int)current_padded_grayscale_buf[idx] - (int)previous_padded_grayscale_buf[idx]);
         previous_padded_grayscale_buf[idx] = (diff >= gate_value) ? (uint8_t)diff : 0;
      }
   }
}

void overwrite_previous_grayscale_with_diff_minus(const uint8_t* current_padded_grayscale_buf, uint8_t* previous_padded_grayscale_buf,
   const int width, const int height, const int gate_value) {
   const int gate_low = 128 - gate_value;
   const int gate_high = 128 + gate_value;
   const int buf_img_w = width + 2;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int idx = (y + 1) * buf_img_w + (x + 1);
         int diff = (255 - current_padded_grayscale_buf[idx] + previous_padded_grayscale_buf[idx]) / 2;
         previous_padded_grayscale_buf[idx] = (diff > gate_high || diff < gate_low) ? (uint8_t)diff : (uint8_t)128;
      }
   }
}

void overwrite_previous_frame_with_color_diff_minus(const uint8_t* current_frame_buf, uint8_t* previous_frame_buf,
   const int width, const int height, const int bpp, const int gate_value) {
   const int gate_low_5 = 16 - gate_value;
   const int gate_high_5 = 16 + gate_value;
   const int gate_low_6 = 32 - gate_value;
   const int gate_high_6 = 32 + gate_value;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int idx = y * width * bpp + x * bpp;
         uint16_t old_pixel = (previous_frame_buf[idx] << 8) | previous_frame_buf[idx + 1];
         uint16_t curr_pixel = (current_frame_buf[idx] << 8) | current_frame_buf[idx + 1];
         int r_diff = (31 - ((curr_pixel >> 11) & 0x1F) + ((old_pixel >> 11) & 0x1F)) / 2;
         int g_diff = (63 - ((curr_pixel >> 5) & 0x3F) + ((old_pixel >> 5) & 0x3F)) / 2;
         int b_diff = (31 - (curr_pixel & 0x1F) + (old_pixel & 0x1F)) / 2;

         if (r_diff >= gate_low_5 && r_diff <= gate_high_5 &&
               g_diff >= gate_low_6 && g_diff <= gate_high_6 &&
               b_diff >= gate_low_5 && b_diff <= gate_high_5) {
            r_diff = 16;
            g_diff = 32;
            b_diff = 16;
         }

         uint16_t diff_rgb565_pixel = ((r_diff << 11) & 0xF800) | ((g_diff << 5) & 0x07E0) | (b_diff & 0x001F);
         previous_frame_buf[idx] = diff_rgb565_pixel >> 8;
         previous_frame_buf[idx + 1] = diff_rgb565_pixel & 0xFF;
      }
   }
}

void overwrite_previous_frame_with_color_diff_abs(const uint8_t* current_frame_buf, uint8_t* previous_frame_buf,
   const int width, const int height, const int bpp, const int gate_value) {
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         int idx = y * width * bpp + x * bpp;
         uint16_t old_pixel = (previous_frame_buf[idx] << 8) | previous_frame_buf[idx + 1];
         uint16_t curr_pixel = (current_frame_buf[idx] << 8) | current_frame_buf[idx + 1];
         int r_diff = abs(((curr_pixel >> 11) & 0x1F) - ((old_pixel >> 11) & 0x1F));
         int g_diff = abs(((curr_pixel >> 5) & 0x3F) - ((old_pixel >> 5) & 0x3F));
         int b_diff = abs((curr_pixel & 0x1F) - (old_pixel & 0x1F));

         if (r_diff < gate_value) r_diff = 0;
         if (g_diff < gate_value) g_diff = 0;
         if (b_diff < gate_value) b_diff = 0;

         uint16_t diff_rgb565_pixel = ((r_diff << 11) & 0xF800) | ((g_diff << 5) & 0x07E0) | (b_diff & 0x001F);
         previous_frame_buf[idx] = diff_rgb565_pixel >> 8;
         previous_frame_buf[idx + 1] = diff_rgb565_pixel & 0xFF;
      }
   }
}

//TODO for tomorrow. :)
//void calculate_rgb565_left_sobel(const uint8_t* rgb565_input_buf, uint8_t* rgb565_output_buf, const int width, const int height) {
//}

#endif /* PIXEL_CALCULATION_HPP_ */