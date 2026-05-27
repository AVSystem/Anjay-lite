# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16)

project(mbedtls-build)

include(ExternalProject)

set(MBEDTLS_BUILD_DIR "${CMAKE_BINARY_DIR}/_deps/mbedtls-build")
set(MBEDTLS_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/mbedtls-src")
set(MBEDTLS_INSTALL_DIR "${CMAKE_BINARY_DIR}/_deps/mbedtls-install")

ExternalProject_Add(mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG        v3.6.4
    GIT_SHALLOW    TRUE

    SOURCE_DIR     "${MBEDTLS_SOURCE_DIR}"
    BINARY_DIR     "${MBEDTLS_BUILD_DIR}"
    INSTALL_DIR    "${MBEDTLS_INSTALL_DIR}"

    CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=Debug
    -DENABLE_PROGRAMS=OFF
    -DENABLE_TESTING=OFF
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)
