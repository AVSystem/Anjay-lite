# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(downloader_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

set(ANJ_TESTING ON)
set(ANJ_WITH_SOCKET_POSIX_COMPAT OFF)
set(ANJ_WITH_TIME_POSIX_COMPAT OFF)
set(ANJ_WITH_COAP_DOWNLOADER ON)

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)

file(GLOB downloader_tests_sources "*.c" "../mock/*.c")
add_executable(downloader_tests ${downloader_tests_sources})

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
    "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(downloader_tests PRIVATE anj)
target_link_libraries(downloader_tests PRIVATE test_framework)
