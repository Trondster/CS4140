cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(send-image-to-computer)

target_sources(app PRIVATE
    src/main.c
    src/uart_img_send.c
)
