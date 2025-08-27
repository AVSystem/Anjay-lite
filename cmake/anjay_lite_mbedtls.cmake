# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

if(NOT MBEDTLS_VERSION STREQUAL "" AND NOT MBEDTLS_ROOT_DIR STREQUAL "")
    message(FATAL_ERROR "MBEDTLS_VERSION and MBEDTLS_ROOT_DIR cannot be set at the same time!")
endif()

if(MBEDTLS_VERSION STREQUAL "")
    set(MBEDTLS_VERSION "3.6.0")
endif()

if(NOT MBEDTLS_ROOT_DIR)
    include(FetchContent)
    message(STATUS "No MBEDTLS_ROOT_DIR provided. Fetching MbedTLS ${MBEDTLS_VERSION} ...")
    FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG        v${MBEDTLS_VERSION}
    )
    FetchContent_MakeAvailable(mbedtls)
    set(MBEDTLS_TARGETS MbedTLS::mbedtls MbedTLS::mbedx509 MbedTLS::mbedcrypto)

    # Add aliases so that targets have consistent names with find_package(MbedTLS)
    add_library(MbedTLS::mbedtls ALIAS mbedtls)
    add_library(MbedTLS::mbedcrypto ALIAS mbedcrypto)
    add_library(MbedTLS::mbedx509 ALIAS mbedx509)
else()
    message(STATUS "Using MbedTLS from: ${MBEDTLS_ROOT_DIR}")
    set(_MBEDTLS_SEARCH_ARGS PATHS "${MBEDTLS_ROOT_DIR}" NO_DEFAULT_PATH)
    find_package(MbedTLS CONFIG REQUIRED ${_MBEDTLS_SEARCH_ARGS})
    set(MBEDTLS_TARGETS MbedTLS::mbedtls MbedTLS::mbedx509 MbedTLS::mbedcrypto)
endif()
