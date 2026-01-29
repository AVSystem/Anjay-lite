# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(standard_tests_without_composite C)

include(${CMAKE_CURRENT_LIST_DIR}/../standard_tests/standard_tests_base_config.cmake)

set(ANJ_WITH_OBSERVE_COMPOSITE OFF)
set(ANJ_WITH_COMPOSITE_OPERATIONS OFF)

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)
target_compile_options(anj PRIVATE -Wall -Wextra -Werror)

file(GLOB standard_tests_without_composite
                "../standard_tests/coap/*.c"
                "../standard_tests/core/*.c"
                "../standard_tests/dm/*.c"
                "../standard_tests/downloader/*.c"
                "../standard_tests/exchange/*.c"
                "../standard_tests/io/*.c"
                "../standard_tests/mock/*.c"
                "../standard_tests/ntp/*.c"
                "../standard_tests/observe/*.c"
                "../standard_tests/time/*.c")
add_executable(standard_tests_without_composite ${standard_tests_without_composite})

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
    "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(standard_tests_without_composite PRIVATE anj)
target_link_libraries(standard_tests_without_composite PRIVATE test_framework)
