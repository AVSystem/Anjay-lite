# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.6.0)

project(exchange_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

set(ANJ_TESTING ON)
set(ANJ_WITH_TIME_POSIX_COMPAT OFF)

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)

file(GLOB exchange_tests_sources "*.c")
add_executable(exchange_tests ${exchange_tests_sources})

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
    "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(exchange_tests PRIVATE anj)
target_link_libraries(exchange_tests PRIVATE test_framework)

if (ANJ_IWYU_PATH)
    # Below OBJECT library is used only for iwyu checks
    add_library(exchange_tests_iwyu OBJECT ${exchange_tests_sources})
    target_include_directories(exchange_tests_iwyu PRIVATE
        $<TARGET_PROPERTY:exchange_tests,INCLUDE_DIRECTORIES>)
    use_iwyu_if_enabled(exchange_tests_iwyu)
endif ()
