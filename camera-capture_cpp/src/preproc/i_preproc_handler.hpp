#ifndef PREPROC_HANDLER_H_
#define PREPROC_HANDLER_H_


#include <cstdint>
#include <stddef.h>
#include "pixel_calculation.hpp"

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

/// @brief Interface for preprocessing handlers, which will process the raw image data from the camera before it is displayed. This allows us to separate the concerns of image capture and image processing, and makes it easier to add new preprocessing steps in the future without modifying the main application logic.
class IPreprocHandler {
protected:
   int width;
   int height;
   int bpp;
   int width_bytes;
   int img_size;
   int buf_img_w;
   int buf_img_h;
   int buf_img_size;
   uint8_t* input_buf; //Has length of IMG_SIZE, contains raw RGB565 data from the camera
   uint8_t* second_buf; //Has length of IMG_SIZE, can be used as a secondary buffer for processing (e.g. for double buffering, or for storing intermediate results)
   uint8_t* grayscale_buf; //Has length of BUF_IMG_SIZE, contains grayscale data derived from the raw RGB565 data. The extra bytes are for padding, to simplify the Sobel operator implementation.
   uint8_t* second_grayscale_buf; //Has length of BUF_IMG_SIZE, can be used as a secondary buffer for grayscale data (e.g. for double buffering, or for storing intermediate results)
   struct device* display; //Pointer to the display device, so that the handler can call tft_draw_image to display processed frames.
   int (*fifo_capture)(uint8_t *buf, size_t size, size_t line_stride);
   void (*tft_draw_image)(const struct device *dev, int x, int y, int w, int h, const uint8_t *rgb565);
public:
   //ctor: takes pointers to the buffers, and pointers to fifo_capture and tft_draw_image, so that the handler can capture new frames and display processed frames as needed.
   //Also has function pointers to the fifo_capture and tft_draw_image functions, so that the handler can capture new frames and display processed frames as needed.
   IPreprocHandler(uint8_t* input_buf, uint8_t* second_buf, uint8_t* grayscale_buf, uint8_t* second_grayscale_buf,
      int height, int width, int bpp) :
      input_buf(input_buf), second_buf(second_buf), grayscale_buf(grayscale_buf), second_grayscale_buf(second_grayscale_buf),
      height(height), width(width), bpp(bpp),
      width_bytes(width * bpp), img_size(width * height * bpp),
      buf_img_w(width + 2), buf_img_h(height + 2), buf_img_size((width + 2) * (height + 2))
      {}

   virtual void inject(device* display, 
      int (*fifo_capture)(uint8_t *buf, size_t size, size_t line_stride),
      void (*tft_draw_image)(const struct device *dev, int x, int y, int w, int h, const uint8_t *rgb565)) {
      this->display = display;
      this->fifo_capture = fifo_capture;
      this->tft_draw_image = tft_draw_image;
   }
   
   virtual void init() {}; //Initialization code, called once at startup. Can be used to set up any necessary state or configuration for the handler.
   virtual void process() {
      //Default implementation does nothing - just pass through the raw image data. Subclasses can override this to implement specific preprocessing steps.
      //By default, just capture a new frame and display it without any processing.
      fifo_capture(input_buf, img_size, width_bytes);
      //Note: the handler is responsible for calling tft_draw_image to display the processed frame. This allows the handler to control when frames are displayed (e.g. it could choose to only display every nth frame, or to only display frames that meet certain criteria).
      //In the default implementation, we just display every captured frame without any processing.
      tft_draw_image(display, 0, 0, width, height, input_buf);
   }

   virtual const char* get_name() = 0;
};

#endif /* PREPROC_HANDLER_H_ */