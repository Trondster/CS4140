#ifndef PIXEL_CALCULATION_HPP_
#define PIXEL_CALCULATION_HPP_

#include <cstdint>
#include <stddef.h>
#include <stdlib.h>

#include "../../c/pixel_conversion.h"


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

void downscale_grayscale_image(const uint8_t* padded_grayscale_buf, uint8_t* padded_output_grayscale_buf,
   const int width, const int height, const int conversion_factor) {
   int buf_img_w = width + 2;
   for (int y = conversion_factor; y < height; y += conversion_factor) {
      for (int x = conversion_factor; x < width; x += conversion_factor) {
         int idx = (y + 1) * buf_img_w + (x + 1);
         int aggregate_gray = 0;
         
         for (int dy = 0; dy < conversion_factor; dy++) {
            for (int dx = 0; dx < conversion_factor; dx++) {
               aggregate_gray += padded_grayscale_buf[idx + dy * buf_img_w + dx];
            }
         }
         uint8_t downscaled_gray = aggregate_gray / (conversion_factor * conversion_factor);

         for (int dy = 0; dy < conversion_factor; dy++) {
            for (int dx = 0; dx < conversion_factor; dx++) {
               padded_output_grayscale_buf[idx + dy * buf_img_w + dx] = downscaled_gray;
            }
         }
      }
   }
}


void downscale_grayscale_image_to_small_unpadded_image(const uint8_t* padded_grayscale_buf, uint8_t* unpadded_small_output_grayscale_buf,
   const int width, const int height, const int conversion_factor) {
   int buf_img_w = width + 2;
   const int smallwidth = width / conversion_factor;
   for (int y = conversion_factor; y < height; y += conversion_factor) {
      for (int x = conversion_factor; x < width; x += conversion_factor) {
         int padded_idx = (y + 1) * buf_img_w + (x + 1);
         int aggregate_gray = 0;
         
         for (int dy = 0; dy < conversion_factor; dy++) {
            for (int dx = 0; dx < conversion_factor; dx++) {
               aggregate_gray += padded_grayscale_buf[padded_idx + dy * buf_img_w + dx];
            }
         }
         uint8_t downscaled_gray = aggregate_gray / (conversion_factor * conversion_factor);

         int small_idx = (y / conversion_factor) * (smallwidth) + (x / conversion_factor);
         unpadded_small_output_grayscale_buf[small_idx] = downscaled_gray;
      }
   }
}

void strip_grayscale_padding(const uint8_t* padded_grayscale_buf, uint8_t* output_buf, const int width, const int height) {
   int buf_img_w = width + 2;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         output_buf[y * width + x] = padded_grayscale_buf[(y + 1) * buf_img_w + (x + 1)];
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
         int diff = (255 - previous_padded_grayscale_buf[idx] + current_padded_grayscale_buf[idx]) / 2;
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
         int r_diff = (31 - ((old_pixel >> 11) & 0x1F) + ((curr_pixel >> 11) & 0x1F)) / 2;
         int g_diff = (63 - ((old_pixel >> 5) & 0x3F) + ((curr_pixel >> 5) & 0x3F)) / 2;
         int b_diff = (31 - (old_pixel & 0x1F) + (curr_pixel & 0x1F)) / 2;

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