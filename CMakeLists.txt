# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_station)
    target_sources(app PRIVATE
        src/main.c
        src/wifi_credentials.c
        #src/wifi_power.c
        #src/ili9341_custom.c  # Custom ILI9341 display driver
        #src/test_ILI9341.c    # ILI9341 display test functions
    )

target_include_directories(app PRIVATE
    src/
)
