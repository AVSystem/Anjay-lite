# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(net_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

set(ANJ_TESTING ON)
set(ANJ_WITH_SOCKET_POSIX_COMPAT ON)
set(ANJ_NET_WITH_UDP ON)
set(ANJ_NET_WITH_TCP ON)
set(ANJ_NET_WITH_IPV4 ON)
set(ANJ_NET_WITH_IPV6 ON)

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)

file(GLOB net_tests_sources "*.c")
add_executable(net_tests ${net_tests_sources})

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
    "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(net_tests PRIVATE anj)
target_link_libraries(net_tests PRIVATE test_framework)
