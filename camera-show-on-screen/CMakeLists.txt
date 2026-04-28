cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(camera-show-screen)

target_sources(app PRIVATE
    src/main.c
    ../lib/c/ov7670.c
    ../lib/c/fifo.c
    ../lib/c/tft_display.c
)