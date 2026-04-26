#ifndef PREPROC_DIFF_GRAYSCALE_ABS_H_
#define PREPROC_DIFF_GRAYSCALE_ABS_H_

#include <cstdint>
#include "i_preproc_handler.hpp"

//Implements IPreprocHandler
class PreprocDiffGrayscaleAbs : public IPreprocHandler {
public:
   bool input_buf_is_current = true; //Keeps track of whether input_buf contains the most recent frame from the camera, or whether second_buf does (after init(), second_buf contains the first captured frame, and input_buf will contain the next captured frame when process() is called for the first time).

   int gate_value = 7;
   //The minimum absolute difference between the current frame and previous frame that will be shown on the display.
   // This is to filter out noise - if the difference is below this value, we will treat it as 0 (i.e. no change) to avoid showing a lot of noise on the display.

   PreprocDiffGrayscaleAbs(uint8_t* input_buf, uint8_t* second_buf, uint8_t* grayscale_buf, uint8_t* second_grayscale_buf,
               int height, int width, int bpp) :
      IPreprocHandler(input_buf, second_buf, grayscale_buf, second_grayscale_buf, height, width, bpp) {}
   void process() override {
      //Capture a new frame into input_buf
      uint8_t* current_frame_buf = input_buf_is_current ? input_buf : second_buf;
      uint8_t* previous_frame_buf = input_buf_is_current ? second_buf : input_buf;
      uint8_t* current_grayscale_buf = input_buf_is_current ? grayscale_buf : second_grayscale_buf;
      uint8_t* previous_grayscale_buf = input_buf_is_current ? second_grayscale_buf : grayscale_buf;

      //Reading data into the current frame buffer.
      //The previous frame buffer still contains the previous frame, which we will use to calculate the difference.
      fifo_capture(current_frame_buf, img_size, width_bytes);
      calculate_grayscale_image(current_frame_buf, current_grayscale_buf, width, height, bpp);

      //Overwrite the previous grayscale buffer with the difference of the current grayscale and previous grayscale.
      overwrite_previous_grayscale_with_diff_abs(current_grayscale_buf, previous_grayscale_buf, width, height, gate_value);

      //Convert the grayscale diff image back to RGB565 format and store it in previous_frame_buf, so that we can display it using tft_draw_image
      convert_grayscale_to_rgb565(previous_grayscale_buf, previous_frame_buf, width, height, bpp);

      //Print the diff image (which is now stored in previous_frame_buf) to the display.
      tft_draw_image(display, 0, 0, width, height, previous_frame_buf);

      //Toggle which buffer is current for the next frame.
      input_buf_is_current = !input_buf_is_current; 
   }

   void init() override {
      //Storing the previous frame in second_buf, so that we can calculate the difference with the next frame in process().
      input_buf_is_current = true;
      fifo_capture(second_buf, img_size, width_bytes);
      calculate_grayscale_image(second_buf, second_grayscale_buf, width, height, bpp);
   }
    
   const char* get_name() override {
      return "Diff Grayscale Abs";
   }
};

#endif /* PREPROC_DIFF_GRAYSCALE_ABS_H_ */