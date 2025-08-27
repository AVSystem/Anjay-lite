# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.6.0)

project(anjay_lite_firmware_update_pull C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)

set(ANJ_FOTA_WITH_PULL_METHOD ON)
set(ANJ_FOTA_WITH_PUSH_METHOD OFF)
set(ANJ_FOTA_WITH_COAP ON)
set(ANJ_WITH_COAP_DOWNLOADER ON)

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(anjay_lite_DIR "../../../cmake")
    find_package(anjay_lite REQUIRED)
endif()

add_executable(anjay_lite_firmware_update_pull src/main.c src/firmware_update.c)
target_include_directories(anjay_lite_firmware_update_pull PUBLIC
    "${CMAKE_SOURCE_DIR}"
    )

target_link_libraries(anjay_lite_firmware_update_pull PRIVATE
                      anj
                      anj_extra_warning_flags)
