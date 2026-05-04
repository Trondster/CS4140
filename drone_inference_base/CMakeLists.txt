cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(drone_inference_base)

target_sources(
   app
   PRIVATE
   src/main.c
   src/inference.cc
   ../lib/c/ov7670.c
   ../lib/c/fifo.c
   ../lib/c/tft_display.c
#    ../lib/c/uart_img_send.c
   ../lib/c/pixel_conversion.c
)
