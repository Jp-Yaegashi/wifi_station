# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_station)
    target_sources(app PRIVATE
        src/main.c
        src/wifi_power.c
        src/wifi_connect.c
        #src/https_api.c        
    )
set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/certs)
zephyr_include_directories(${gen_dir})
generate_inc_file_for_target(
    app
    cert/DigiCertGlobalG3.pem
    ${gen_dir}/DigiCertGlobalG3.pem.inc
    )
target_include_directories(app PRIVATE
    src/
)
