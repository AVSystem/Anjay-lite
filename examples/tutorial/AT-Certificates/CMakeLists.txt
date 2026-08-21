# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(anjay_lite_at_certificates C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)

set(ANJ_WITH_MBEDTLS ON)
set(ANJ_WITH_SECURITY ON)
set(ANJ_NET_WITH_DTLS ON)

set(ANJ_WITH_CERTIFICATES ON)
set(ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE 500)
set(ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE 500)
set(ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE 1000)

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(anjay_lite_DIR "../../../cmake")
    find_package(anjay_lite REQUIRED)
endif()

add_executable(anjay_lite_at_certificates src/main.c)
target_include_directories(anjay_lite_at_certificates PUBLIC
    "${CMAKE_SOURCE_DIR}"
    )

target_link_libraries(anjay_lite_at_certificates PRIVATE
                      anj
                      anj_extra_warning_flags)
