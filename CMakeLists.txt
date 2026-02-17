cmake_minimum_required(VERSION 3.16)

project(lora_networking_lib
    VERSION 1.0.0
    LANGUAGES C
)

include(CTest)

add_subdirectory(tests)

add_library(lora_networking_lib)

target_sources(lora_networking_lib
    PRIVATE
    src/packet.c
    src/network_data_operations.c
    src/rx_packet_handler.c
)

target_include_directories(lora_networking_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(COMPILE_OPTIONS_DEBUG
    -g -O0
)

target_compile_options(lora_networking_lib
    PRIVATE
    -Wall -Wextra
    ${COMPILE_OPTIONS_DEBUG}
)
