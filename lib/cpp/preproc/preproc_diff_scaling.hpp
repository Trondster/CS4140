#ifndef PREPROC_DIFF_SCALING_H_
#define PREPROC_DIFF_SCALING_H_

#include <cstdint>
#include "i_preproc_handler.hpp"

//Implements IPreprocHandler
class PreprocDiffScaling : public IPreprocHandler {
private:
   uint8_t current_grayscale_nopad[IMG_W * IMG_H]; //Grayscale version of the current frame without padding, used for calculating the difference and applying the gate value.
   uint8_t current_diff_nopad[IMG_W * IMG_H]; //Grayscale difference image without padding, used for calculating the difference and applying the gate value.

   uint8_t current_grayscale_downscaled_2x2_nopad[IMG_W / 2 * IMG_H / 2]; //Downscaled (by 2x2 average pooling) version of the current grayscale difference image, without padding. This is what we will actually display on the screen, after converting it back to RGB565 format.
   uint8_t current_grayscale_downscaled_3x3_nopad[(IMG_W / 3) * (IMG_H / 3)]; //Downscaled (by 3x3 average pooling) version of the current grayscale difference image, without padding. This is what we will actually display on the screen, after converting it back to RGB565 format.
   uint8_t current_grayscale_downscaled_4x4_nopad[IMG_W / 4 * IMG_H / 4]; //Downscaled (by 4x4 average pooling) version of the current grayscale difference image, without padding. This is what we will actually display on the screen, after converting it back to RGB565 format.

   uint8_t current_diff_downscaled_2x2_nopad[IMG_W / 2 * IMG_H / 2]; //Downscaled (by 2x2 average pooling) version of the current grayscale difference image, without padding. This is what we will actually display on the screen, after converting it back to RGB565 format.
   uint8_t current_diff_downscaled_3x3_nopad[(IMG_W / 3) * (IMG_H / 3)]; //Downscaled (by 3x3 average pooling) version of the current grayscale difference image, without padding. This is what we will actually display on the screen, after converting it back to RGB565 format.
   uint8_t current_diff_downscaled_4x4_nopad[IMG_W / 4 * IMG_H / 4]; //Downscaled (by 4x4 average pooling) version of the current grayscale difference image, without padding. This is what we will actually display on the screen, after converting it back to RGB565 format.

public:
   bool input_buf_is_current = true; //Keeps track of whether input_buf contains the most recent frame from the camera, or whether second_buf does (after init(), second_buf contains the first captured frame, and input_buf will contain the next captured frame when process() is called for the first time).

   int gate_value = GRAYSCALE_DIFF_GATE_VALUE;
   //The minimum absolute difference between the current frame and previous frame that will be shown on the display.
   // This is to filter out noise - if the difference is below this value, we will treat it as 0 (i.e. no change) to avoid showing a lot of noise on the display.


   PreprocDiffScaling(uint8_t* input_buf, uint8_t* second_buf, uint8_t* grayscale_buf, uint8_t* second_grayscale_buf,
               int height, int width, int bpp) :
      IPreprocHandler(input_buf, second_buf, grayscale_buf, second_grayscale_buf, height, width, bpp) {}


   void prepare_data() {
      uint8_t* current_grayscale_buf = input_buf_is_current ? grayscale_buf : second_grayscale_buf;
      uint8_t* previous_grayscale_buf = input_buf_is_current ? second_grayscale_buf : grayscale_buf;

      //Downscale the grayscale difference image to create the downscaled versions that we will display.
      downscale_grayscale_image_to_small_unpadded_image(current_grayscale_buf, current_grayscale_downscaled_2x2_nopad, width, height, 2);
      downscale_grayscale_image_to_small_unpadded_image(current_grayscale_buf, current_grayscale_downscaled_3x3_nopad, width, height, 3);
      downscale_grayscale_image_to_small_unpadded_image(current_grayscale_buf, current_grayscale_downscaled_4x4_nopad, width, height, 4);

      //Downscale the grayscale difference image to create the downscaled versions that we will display.
      downscale_grayscale_image_to_small_unpadded_image(previous_grayscale_buf, current_diff_downscaled_2x2_nopad, width, height, 2);
      downscale_grayscale_image_to_small_unpadded_image(previous_grayscale_buf, current_diff_downscaled_3x3_nopad, width, height, 3);
      downscale_grayscale_image_to_small_unpadded_image(previous_grayscale_buf, current_diff_downscaled_4x4_nopad, width, height, 4);

      //Strip the padding from the current grayscale buffer and previous grayscale buffer, so that we can apply the gate value and display the difference image without the padding.
      strip_grayscale_padding(current_grayscale_buf, current_grayscale_nopad, width, height);
      strip_grayscale_padding(previous_grayscale_buf, current_diff_nopad, width, height);
   }

   void process() override {
      //Toggle which buffer is current for the current frame.
      input_buf_is_current = !input_buf_is_current; 

      //Capture a new frame into input_buf
      uint8_t* current_frame_buf = input_buf_is_current ? input_buf : second_buf;
      //uint8_t* previous_frame_buf = input_buf_is_current ? second_buf : input_buf;
      uint8_t* current_grayscale_buf = input_buf_is_current ? grayscale_buf : second_grayscale_buf;
      uint8_t* previous_grayscale_buf = input_buf_is_current ? second_grayscale_buf : grayscale_buf;

      //Reading data into the current frame buffer.
      //The previous frame buffer still contains the previous frame, which we will use to calculate the difference.
      fifo_capture(current_frame_buf, img_size, width_bytes);
      calculate_grayscale_image(current_frame_buf, current_grayscale_buf, width, height, bpp);

      //Overwrite the previous grayscale buffer with the difference of the current grayscale and previous grayscale.
      //Using the alternate difference calculation.
      overwrite_previous_grayscale_with_diff_minus(current_grayscale_buf, previous_grayscale_buf, width, height, gate_value);


      //Print the original color image to the display.
      //tft_draw_image(display, 0, 0, width, height, current_frame_buf);
   }

   uint8_t* get_current_frame_buf() {
      return input_buf_is_current ? input_buf : second_buf;
   }

   uint8_t* get_previous_frame_buf() {
      return input_buf_is_current ? second_buf : input_buf;
   }

   uint8_t* get_current_grayscale_nopad() {
      return current_grayscale_nopad;
   }

   uint8_t* get_current_diff_grayscale_nopad() {
      return current_diff_nopad;
   }

   uint8_t* get_current_grayscale_padded() {
      return input_buf_is_current ? grayscale_buf : second_grayscale_buf;
   }

   uint8_t* get_current_diff_grayscale_padded() {
      return input_buf_is_current ? second_grayscale_buf : grayscale_buf;
   }

   uint8_t* get_current_grayscale_downscaled_2x2_nopad() {
      return current_grayscale_downscaled_2x2_nopad;
   }

   uint8_t* get_current_grayscale_downscaled_3x3_nopad() {
      return current_grayscale_downscaled_3x3_nopad;
   }
   
   uint8_t* get_current_grayscale_downscaled_4x4_nopad() {
      return current_grayscale_downscaled_4x4_nopad;
   }

   uint8_t* get_current_diff_downscaled_2x2_nopad() {
      return current_diff_downscaled_2x2_nopad;
   }

   uint8_t* get_current_diff_downscaled_3x3_nopad() {
      return current_diff_downscaled_3x3_nopad;
   }

   uint8_t* get_current_diff_downscaled_4x4_nopad() {
      return current_diff_downscaled_4x4_nopad;
   }

   void init() override {
      //Storing the previous frame in second_buf, so that we can calculate the difference with the next frame in process().
      fifo_capture(second_buf, img_size, width_bytes);
      calculate_grayscale_image(second_buf, second_grayscale_buf, width, height, bpp);
      input_buf_is_current = false;
   }
    
   const char* get_name() override {
      return "Diff Scaling";
   }
};

#endif /* PREPROC_DIFF_SCALING_H_ */