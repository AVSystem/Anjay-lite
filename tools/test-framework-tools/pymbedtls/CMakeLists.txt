# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED PY_BUILD_CMAKE_VERSION)
    message(FATAL_ERROR
        "This CMake project is intended to be built via py-build-cmake"
    )
endif()

if(NOT PY_BUILD_CMAKE_IMPORT_NAME)
    message(FATAL_ERROR "PY_BUILD_CMAKE_IMPORT_NAME not set by py-build-cmake")
endif()

project(pymbedtls CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB sources "src/*.cpp")
file(GLOB headers "src/*.hpp")

include(cmake/QueryPythonForPybind11.cmake)
find_pybind11_python_first()

pybind11_add_module(pymbedtls MODULE ${sources} ${headers})

# Avoid polluting the exported symbols
set_target_properties(pymbedtls PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

# These variables may also be supplied through the environment, which is
# convenient when pymbedtls is built through pip/py-build-cmake.
foreach(_var MBEDTLS_ROOT_DIR MBEDTLS_VERSION MBEDTLS_CFG_STR MBEDTLS_CACHE_DIR)
    if(NOT DEFINED ${_var} AND DEFINED ENV{${_var}})
        set(${_var} "$ENV{${_var}}")
    endif()
endforeach()

if(NOT "${MBEDTLS_ROOT_DIR}" STREQUAL "")
    if(NOT "${MBEDTLS_VERSION}" STREQUAL "")
        message(FATAL_ERROR
            "MBEDTLS_VERSION and MBEDTLS_ROOT_DIR cannot be set at the same time!"
        )
    endif()

    if(NOT "${MBEDTLS_CFG_STR}" STREQUAL "")
        message(FATAL_ERROR
            "MBEDTLS_CFG_STR and MBEDTLS_ROOT_DIR cannot be set at the same time!"
        )
    endif()

    set(_MBEDTLS_SEARCH_ARGS PATHS "${MBEDTLS_ROOT_DIR}" NO_DEFAULT_PATH)
else()
    set(MBEDTLS_CACHE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/mbedtls_cache.py")

    set(MBEDTLS_CONFIG_FLAG "")
    if(NOT "${MBEDTLS_CFG_STR}" STREQUAL "")
        # Semicolons are CMake list separators, so preserve them as one command
        # line argument passed to mbedtls_cache.py.
        string(REPLACE ";" "\;" MBEDTLS_CFG_STR_ESCAPED "${MBEDTLS_CFG_STR}")
        set(MBEDTLS_CONFIG_FLAG "--config" "${MBEDTLS_CFG_STR_ESCAPED}")
    endif()

    set(MBEDTLS_VERSION_FLAG "")
    if(NOT "${MBEDTLS_VERSION}" STREQUAL "")
        set(MBEDTLS_VERSION_NORMALIZED "${MBEDTLS_VERSION}")
        if(NOT MBEDTLS_VERSION_NORMALIZED MATCHES "^v")
            set(MBEDTLS_VERSION_NORMALIZED "v${MBEDTLS_VERSION_NORMALIZED}")
        endif()
        set(MBEDTLS_VERSION_FLAG "--version" "${MBEDTLS_VERSION_NORMALIZED}")
    endif()

    if("${MBEDTLS_CACHE_DIR}" STREQUAL "")
        # Keep the dependency cache with py-build-cmake's persistent build tree
        # instead of relying on the source directory being a Git checkout.
        set(MBEDTLS_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/.mbedtls_cache")
    endif()

    find_package(Python3 COMPONENTS Interpreter)
    if(NOT Python3_FOUND)
        message(FATAL_ERROR [[
Can't automatically fetch and build Mbed TLS because Python 3 is not available.
Please provide MBEDTLS_ROOT_DIR pointing to an installed Mbed TLS package, or
install Python 3 and try again.
]])
    endif()

    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}" "${MBEDTLS_CACHE_SCRIPT}"
            "--cache-dir" "${MBEDTLS_CACHE_DIR}"
            "--make-PIC"
            ${MBEDTLS_VERSION_FLAG}
            ${MBEDTLS_CONFIG_FLAG}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE MBEDTLS_CACHE_RESULT
        OUTPUT_VARIABLE MBEDTLS_CACHE_OUTPUT
        ERROR_VARIABLE MBEDTLS_CACHE_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT MBEDTLS_CACHE_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Failed to fetch and build Mbed TLS:
${MBEDTLS_CACHE_ERROR}"
        )
    endif()

    message(STATUS "mbedtls_cache.py output: ${MBEDTLS_CACHE_OUTPUT}")
    set(_MBEDTLS_SEARCH_ARGS PATHS "${MBEDTLS_CACHE_OUTPUT}" NO_DEFAULT_PATH)
endif()

find_package(MbedTLS CONFIG REQUIRED ${_MBEDTLS_SEARCH_ARGS})
target_link_libraries(pymbedtls PRIVATE
    MbedTLS::mbedtls
    MbedTLS::mbedx509
    MbedTLS::mbedcrypto
)

# Make cached or explicitly selected shared Mbed TLS libraries discoverable at
# runtime without requiring LD_LIBRARY_PATH. Generator expressions work for
# both single- and multi-configuration imported targets.
set_target_properties(pymbedtls PROPERTIES
    BUILD_RPATH "$<TARGET_FILE_DIR:MbedTLS::mbedtls>"
    INSTALL_RPATH "$<TARGET_FILE_DIR:MbedTLS::mbedtls>"
)

install(TARGETS pymbedtls
        DESTINATION ${PY_BUILD_CMAKE_IMPORT_NAME})
