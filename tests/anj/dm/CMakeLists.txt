# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(dm_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

set(ANJ_TESTING ON)
set(ANJ_DM_MAX_OBJECTS_NUMBER 10)
set(ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS ON)
set(ANJ_WITH_LWM2M12 ON)
set(ANJ_WITH_OBSERVE ON)
set(ANJ_WITH_BOOTSTRAP ON)
set(ANJ_WITH_BOOTSTRAP_DISCOVER ON)
set(ANJ_WITH_DISCOVER_ATTR ON)
set(ANJ_WITH_EXTERNAL_DATA ON)
set(ANJ_FOTA_WITH_COAP_TCP ON)
set(ANJ_WITH_SECURITY ON)
set(ANJ_WITH_PERSISTENCE ON)
set(ANJ_WITH_EXTERNAL_CRYPTO_STORAGE ON)

set(ANJ_WITH_OBSERVE_COMPOSITE ON)
set(ANJ_WITH_COMPOSITE_OPERATIONS ON)

set(anjay_lite_DIR "../../../cmake")

find_package(anjay_lite REQUIRED)
target_compile_options(anj PRIVATE -Wall -Wextra -Werror)

file(GLOB dm_tests_sources "*.c")
add_executable(dm_tests ${dm_tests_sources})

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../framework"
    "${CMAKE_CURRENT_BINARY_DIR}/framework_build")

target_link_libraries(dm_tests PRIVATE anj)
target_link_libraries(dm_tests PRIVATE test_framework)
