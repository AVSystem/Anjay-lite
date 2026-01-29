# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)
project(codegen_object_registry_check C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)

set(anjay_lite_DIR "../../../cmake")
find_package(anjay_lite REQUIRED)

set(CODEGEN "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/anjay_codegen.py")
set(REGISTRY "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/lwm2m_object_registry.py")
set(TEST_OIDS "0" "1" "2" "3" "4" "5" "22" "2050" "3303")

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

foreach(TEST_OID ${TEST_OIDS})
    set(REGISTRY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${TEST_OID}.xml")
    add_custom_command(OUTPUT "${REGISTRY_OUTPUT}"
                       COMMAND "${REGISTRY}" --get-xml "${TEST_OID}" -o "${REGISTRY_OUTPUT}"
                       DEPENDS "${REGISTRY}")

    set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${TEST_OID}.c")
    add_custom_command(OUTPUT "${OUTPUT}"
                       COMMAND "${CODEGEN}" -i "${REGISTRY_OUTPUT}" -o "${OUTPUT}"
                       DEPENDS "${CODEGEN}" "${CODEGEN_INPUT}" "${REGISTRY_OUTPUT}")
    list(APPEND CODEGEN_SOURCES "${OUTPUT}")
endforeach()

add_library(codegen_object_registry_check ${CODEGEN_SOURCES})
target_link_libraries(codegen_object_registry_check PRIVATE anj)
