cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(camera-show-on-screen)

target_sources(app PRIVATE
    src/main.c
    src/ov7670_fifo.c
    src/tft_display.c
)