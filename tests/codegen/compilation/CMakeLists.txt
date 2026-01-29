# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)
project(codegen_compilation_check C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)

set(anjay_lite_DIR "../../../cmake")
find_package(anjay_lite REQUIRED)

set(CODEGEN "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/anjay_codegen.py")
set(CODEGEN_TEST_INPUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/input")

file(GLOB CODEGEN_INPUTS "${CODEGEN_TEST_INPUT_ROOT}/*.xml")

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

foreach(CODEGEN_INPUT ${CODEGEN_INPUTS})
    get_filename_component(CODEGEN_TEST "${CODEGEN_INPUT}" NAME_WE)

    set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CODEGEN_TEST}.c")

    add_custom_command(OUTPUT "${OUTPUT}"
                       COMMAND "${CODEGEN}" -i "${CODEGEN_INPUT}" -o "${OUTPUT}" ${CODEGEN_OPTIONS}
                       DEPENDS "${CODEGEN}" "${CODEGEN_INPUT}")
    list(APPEND CODEGEN_SOURCES "${OUTPUT}")
endforeach()

add_library(codegen_compilation_check ${CODEGEN_SOURCES})
target_link_libraries(codegen_compilation_check PRIVATE anj)
