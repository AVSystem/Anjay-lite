# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.6.0)

project(observe_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

set(ANJ_TESTING ON)
set(ANJ_WITH_OBSERVE ON)
set(ANJ_WITH_OBSERVE_COMPOSITE ON)
set(ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER 5)
set(ANJ_COAP_WITH_UDP ON)
set(ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER 5)
set(ANJ_WITH_LWM2M12 ON)
set(ANJ_WITH_DISCOVER_ATTR ON)
set(ANJ_WITH_LWM2M_CBOR OFF)

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)

file(GLOB observe_tests_sources "*.c")
add_executable(observe_tests ${observe_tests_sources})

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
    "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(observe_tests PRIVATE anj)
target_link_libraries(observe_tests PRIVATE test_framework)

if (ANJ_IWYU_PATH)
    # Below OBJECT library is used only for iwyu checks
    add_library(observe_tests_iwyu OBJECT ${observe_tests_sources})
    target_include_directories(observe_tests_iwyu PRIVATE
        $<TARGET_PROPERTY:observe_tests,INCLUDE_DIRECTORIES>)
    use_iwyu_if_enabled(observe_tests_iwyu)
endif ()
