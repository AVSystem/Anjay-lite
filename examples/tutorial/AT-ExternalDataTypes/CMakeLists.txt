# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.6.0)

project(anjay_lite_at_external_data_types C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)

set(ANJ_WITH_EXTRA_WARNINGS ON)
set(ANJ_WITH_EXTERNAL_DATA ON)

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(anjay_lite_DIR "../../../cmake")
    find_package(anjay_lite REQUIRED)
endif()

add_executable(anjay_lite_at_external_data_types src/main.c)
target_include_directories(anjay_lite_at_external_data_types PUBLIC
    "${CMAKE_SOURCE_DIR}"
    )

target_link_libraries(anjay_lite_at_external_data_types PRIVATE anj)
