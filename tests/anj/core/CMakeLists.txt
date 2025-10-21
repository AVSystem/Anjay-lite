# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(core_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

set(ANJ_TESTING ON)
set(ANJ_WITH_SOCKET_POSIX_COMPAT OFF)
set(ANJ_NET_WITH_UDP ON)
set(ANJ_NET_WITH_TCP OFF)
set(ANJ_NET_WITH_IPV4 ON)
set(ANJ_NET_WITH_IPV6 ON)
set(ANJ_WITH_TIME_POSIX_COMPAT OFF)
set(ANJ_LWM2M_SEND_QUEUE_SIZE 3)
set(ANJ_COAP_MAX_LOCATION_PATHS_NUMBER 3)
set(ANJ_COAP_MAX_LOCATION_PATH_SIZE 5)
set(ANJ_WITH_LWM2M12 ON)
set(ANJ_WITH_EXTERNAL_DATA ON)
set(ANJ_DM_MAX_OBJECTS_NUMBER 15)

set(ANJ_WITH_COAP_DOWNLOADER ON)
set(ANJ_COAP_DOWNLOADER_MAX_MSG_SIZE 100)

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)
target_compile_options(anj PRIVATE -Wall -Wextra -Werror)

file(GLOB core_tests_sources "*.c" "../mock/*.c")

add_executable(core_tests ${core_tests_sources})

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
    "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(core_tests PRIVATE anj)
target_link_libraries(core_tests PRIVATE test_framework)
