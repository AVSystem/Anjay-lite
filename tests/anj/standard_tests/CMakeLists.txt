# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(standard_tests C)

include(${CMAKE_CURRENT_LIST_DIR}/standard_tests_base_config.cmake)

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)
target_compile_options(anj PRIVATE -Wall -Wextra -Werror)

file(GLOB standard_tests
                "coap/*.c"
                "core/*.c"
                "dm/*.c"
                "downloader/*.c"
                "exchange/*.c"
                "io/*.c"
                "mock/*.c"
                "ntp/*.c"
                "observe/*.c"
                "time/*.c")
add_executable(standard_tests ${standard_tests})

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
    "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(standard_tests PRIVATE anj)
target_link_libraries(standard_tests PRIVATE test_framework)
