# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(io_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

set(ANJ_TESTING ON)
set(ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS ON)
set(ANJ_NET_WITH_UDP ON)
set(ANJ_NET_WITH_TCP ON)
set(ANJ_COAP_WITH_UDP ON)
set(ANJ_COAP_WITH_TCP ON)
set(ANJ_WITH_LWM2M12 ON)
set(ANJ_WITH_EXTERNAL_DATA ON)
set(ANJ_COAP_MAX_ATTR_OPTION_SIZE 50)

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)
target_compile_options(anj PRIVATE -Wall -Wextra -Werror)

file(GLOB io_tests_sources "*.c")
add_executable(io_tests ${io_tests_sources})

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
    "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(io_tests PRIVATE anj)
target_link_libraries(io_tests PRIVATE test_framework)
