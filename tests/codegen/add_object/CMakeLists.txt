# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)
project(codegen_add_object_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)

set(anjay_lite_DIR "../../../cmake")
find_package(anjay_lite REQUIRED)

set(CODEGEN "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/anjay_codegen.py")

set(CODEGEN_OPTIONS "")
if("${CLI_RESOURCE_INSTANCES_HANDLING}" STREQUAL "dynamic")
    list(APPEND CODEGEN_OPTIONS "-dri")
endif()

if("${CLI_OBJECT_INSTANCES_HANDLING}" STREQUAL "dynamic")
    list(APPEND CODEGEN_OPTIONS "-di")
endif()

if(DEFINED CLI_OBJECT_INSTANCES_COUNT)
    list(APPEND CODEGEN_OPTIONS "-ni" ${CLI_OBJECT_INSTANCES_COUNT})
endif()

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/some_obj.c"
                   COMMAND "${CODEGEN}" -i "${CMAKE_CURRENT_SOURCE_DIR}/some_obj.xml" -o "${CMAKE_CURRENT_BINARY_DIR}/some_obj.c" ${CODEGEN_OPTIONS}
                   DEPENDS "${CODEGEN}")

set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c" "${CMAKE_CURRENT_BINARY_DIR}/some_obj.c")
add_executable(codegen_add_object_tests ${SOURCES})
target_link_libraries(codegen_add_object_tests PRIVATE anj)
