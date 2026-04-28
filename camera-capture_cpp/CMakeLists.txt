cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(camera-image-gathering)

target_sources(app PRIVATE
    src/main.cpp
    src/ov7670.c
    src/fifo.c
    src/tft_display.c
    src/uart_img_send.c
    src/pixel_conversion.c
)
