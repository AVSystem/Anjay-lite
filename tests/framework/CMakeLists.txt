# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

add_library(test_framework STATIC
    anj_unit_test.c)

target_include_directories(test_framework INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR})
