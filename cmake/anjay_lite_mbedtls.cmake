# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

if(NOT "${MBEDTLS_ROOT_DIR}" STREQUAL "")
    if (NOT "${MBEDTLS_VERSION}" STREQUAL "")
        message(FATAL_ERROR "MBEDTLS_VERSION and MBEDTLS_ROOT_DIR cannot be set at the same time!")
    endif()

    if (NOT "${MBEDTLS_CFG_STR}" STREQUAL "")
        message(STATUS "MBEDTLS_CFG_STR: ${MBEDTLS_CFG_STR}")
        message(STATUS "MBEDTLS_ROOT_DIR: ${MBEDTLS_ROOT_DIR}")
        message(FATAL_ERROR "MBEDTLS_CFG_STR and MBEDTLS_ROOT_DIR cannot be set at the same time!")
    endif()
endif()

if ("${MBEDTLS_ROOT_DIR}" STREQUAL "")
    set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
    set(MBEDTLS_CACHE_SCRIPT "${REPO_ROOT}/tools/test-framework-tools/pymbedtls/mbedtls_cache.py")

    set(MBEDTLS_CONFIG_FLAG "")
    if (DEFINED MBEDTLS_CFG_STR AND NOT MBEDTLS_CFG_STR STREQUAL "")
        # HACK: Escape semicolons in the config string, as they are used as list separators in CMake
        string(REPLACE ";" "\\;" MBEDTLS_CFG_STR_ESCAPED "${MBEDTLS_CFG_STR}")
        set(MBEDTLS_CONFIG_FLAG "--config" "${MBEDTLS_CFG_STR_ESCAPED}")
    endif()

    set(MBEDTLS_VERSION_FLAG "")
    if (DEFINED MBEDTLS_VERSION AND NOT MBEDTLS_VERSION STREQUAL "")
        set(MBEDTLS_VERSION_FLAG "--version" "v${MBEDTLS_VERSION}")
    endif()

    find_package(Python3 COMPONENTS Interpreter)

    if(NOT Python3_FOUND)
        message(FATAL_ERROR [[
Can't automatically fetch and build mbedtls because Python3 is not available.
Please provide MBEDTLS_ROOT_DIR pointing to a built mbedtls package, or install Python3 and try again.
]])
    endif()

    # Try importing Python git and filelock packages to check if they are available.
    execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c
            "import filelock; import git"
            RESULT_VARIABLE PYTHON_IMPORT_CHECK_RESULT
            OUTPUT_VARIABLE PYTHON_IMPORT_CHECK_OUTPUT
            ERROR_VARIABLE PYTHON_IMPORT_CHECK_ERROR
    )

    if(NOT PYTHON_IMPORT_CHECK_RESULT EQUAL 0)
        message(FATAL_ERROR [[
Python packages required to fetch and build mbedtls automatically are missing.
Please provide MBEDTLS_ROOT_DIR pointing to a built mbedtls package, or install the required packages and try again.

Required packages:
  - filelock
  - GitPython

To install them, run:
  pip install filelock GitPython

Alternatively, please set up a virtual environment using devconfig.
]])
    endif()

    execute_process(
            COMMAND ${Python3_EXECUTABLE} ${MBEDTLS_CACHE_SCRIPT} ${MBEDTLS_VERSION_FLAG} ${MBEDTLS_CONFIG_FLAG}
            WORKING_DIRECTORY ${REPO_ROOT}
            RESULT_VARIABLE MBEDTLS_CACHE_RESULT
            OUTPUT_VARIABLE MBEDTLS_CACHE_OUTPUT
            ERROR_VARIABLE MBEDTLS_CACHE_ERROR
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT MBEDTLS_CACHE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to fetch and build mbedtls: ${MBEDTLS_CACHE_ERROR}")
    endif()
    message(STATUS "mbedtls_cache.py output: ${MBEDTLS_CACHE_OUTPUT}")
    set(_MBEDTLS_SEARCH_ARGS PATHS "${MBEDTLS_CACHE_OUTPUT}" NO_DEFAULT_PATH)
else()
    set(_MBEDTLS_SEARCH_ARGS PATHS "${MBEDTLS_ROOT_DIR}" NO_DEFAULT_PATH)
endif()

find_package(MbedTLS CONFIG REQUIRED ${_MBEDTLS_SEARCH_ARGS})
set(MBEDTLS_TARGETS MbedTLS::mbedtls MbedTLS::mbedx509 MbedTLS::mbedcrypto)

