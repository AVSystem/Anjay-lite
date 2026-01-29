# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(log_tests C)

set(ANJ_LOG_FULL OFF)
set(ANJ_LOG_ALT_IMPL_HEADER "log_alt_impl_header.h")
set(ANJ_LOG_FILTERING_CONFIG_HEADER "log_filtering_config_header.h")

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)
target_compile_options(anj PRIVATE -Wall -Wextra -Werror)
target_include_directories(anj PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(log_tests main.c)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
        "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(log_tests PRIVATE anj)
target_link_libraries(log_tests PRIVATE test_framework)
