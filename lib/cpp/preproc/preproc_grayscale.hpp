#ifndef PREPROC_GRAYSCALE_H_
#define PREPROC_GRAYSCALE_H_

#include <cstdint>
#include "i_preproc_handler.hpp"
#include "pixel_calculation.hpp"

//Implements IPreprocHandler
class PreprocGrayscale : public IPreprocHandler {
public:
    PreprocGrayscale(uint8_t* input_buf, uint8_t* second_buf, uint8_t* grayscale_buf, uint8_t* second_grayscale_buf,
                  int height, int width, int bpp) :
        IPreprocHandler(input_buf, second_buf, grayscale_buf, second_grayscale_buf, height, width, bpp) {}
   void process() override {
      //Capture a new frame into input_buf
      fifo_capture(input_buf, img_size, width_bytes);

      //Calculate the grayscale image and store it in grayscale_buf (with padding)
      calculate_grayscale_image(input_buf, grayscale_buf, width, height, bpp);

      //Convert the grayscale image back to RGB565 format and store it in second_buf, so that we can display it using tft_draw_image
      convert_grayscale_to_rgb565(grayscale_buf, second_buf, width, height, bpp);

      tft_draw_image(display, 0, 0, width, height, second_buf);
   }

   const char* get_name() override {
      return "Grayscale";
   }
};

#endif /* PREPROC_GRAYSCALE_H_ */