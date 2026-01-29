# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(anjay_lite C)

# Core CMake code is located in cmake/anjay_lite-config.cmake and the library is
# supposed to be imported using find_package() mechanism - this file only meant
# to be a convenience wrapper for all example and test targets.

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
include(ExternalProject)

# Although we have cmake/anjay_lite-config.cmake which calls
# anjay_lite_mbedtls.cmake internally, we couldn't use this
# cmake script here because FetchContent builds mbedtls in a way
# that can't be utilized by find_package(MbedTLS).
#
# When calling root project CMakeLists.txt, MbedTLS will be
# either downloaded by ExternalProject call below, or user
# will provide MBEDTLS_ROOT_DIR variable pointing to an existing
# MbedTLS installation. MbedTLS fetched by ExternalProject will
# be passed to subprojects via -DMBEDTLS_ROOT_DIR=... argument.
# This allows to utilize single MbedTLS build for all subprojects.
#
# When calling subproject CMakeLists.txt directly, user is expected
# to provide MBEDTLS_ROOT_DIR variable pointing to an existing MbedTLS
# installation, otherwise, subproject will include anjay_lite_mbedtls.cmake
# and download MbedTLS via FetchContent mechanism, once for each subproject.
if(NOT MBEDTLS_ROOT_DIR)
    if(NOT MBEDTLS_VERSION)
        set(MBEDTLS_VERSION "3.6.4")
    endif()

    message(STATUS "No MBEDTLS_ROOT_DIR provided. Fetching MbedTLS ${MBEDTLS_VERSION} ...")
    set(MBEDTLS_BUILD_DIR "${CMAKE_BINARY_DIR}/_deps/mbedtls-build")
    ExternalProject_Add(mbedtls
            GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
            GIT_TAG        v${MBEDTLS_VERSION}
            GIT_SHALLOW    TRUE

            SOURCE_DIR     "${CMAKE_BINARY_DIR}/_deps/mbedtls-src"
            BINARY_DIR     "${MBEDTLS_BUILD_DIR}"

            CMAKE_ARGS
            -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
            -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
            -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
            -DCMAKE_EXE_LINKER_FLAGS:STRING=${CMAKE_EXE_LINKER_FLAGS}
            -DENABLE_PROGRAMS=OFF
            -DENABLE_TESTING=OFF

            INSTALL_COMMAND ""    # skip "make install"
    )
    set(TARGETS_MBEDTLS_DIR "${MBEDTLS_BUILD_DIR}")
else()
    message("Using MbedTLS from: ${MBEDTLS_ROOT_DIR}")
    set(TARGETS_MBEDTLS_DIR "${MBEDTLS_ROOT_DIR}")
endif()

find_program(VALGRIND_EXECUTABLE valgrind)

add_custom_target(run_tests)

# determine list of -D variables set by users, they need to be prefixed with CLI_
get_cmake_property(_cache_vars CACHE_VARIABLES)
set(COMMAND_LINE_FLAGS "")
foreach(_var ${_cache_vars})
    if(_var MATCHES "CLI_")
        list(APPEND COMMAND_LINE_FLAGS "-D${_var}=${${_var}}")
    endif()
endforeach()

message(STATUS "Command line flags: ${COMMAND_LINE_FLAGS}")

function(add_standalone_target NAME PATH WITH_VALGRIND WITH_MBEDTLS)
  set(workdir "${CMAKE_BINARY_DIR}/${NAME}")

  set(DEPENDS_ON_LIST "")
  if(TARGET mbedtls AND WITH_MBEDTLS)
    list(APPEND DEPENDS_ON_LIST mbedtls)
  endif()

  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${PATH})
      ExternalProject_Add(
              ${NAME}
              SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PATH}"
              BINARY_DIR "${workdir}"
              CMAKE_CACHE_ARGS
              -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
              -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
              -DCMAKE_EXE_LINKER_FLAGS:STRING=${CMAKE_EXE_LINKER_FLAGS}
              -DMBEDTLS_ROOT_DIR:STRING=${TARGETS_MBEDTLS_DIR}
              ${COMMAND_LINE_FLAGS}
              DEPENDS ${DEPENDS_ON_LIST}
              INSTALL_COMMAND ""  # skip "make install"
              BUILD_ALWAYS 1
      )

      if(WITH_VALGRIND AND VALGRIND_EXECUTABLE)
          add_custom_target(
                  ${NAME}_with_valgrind
                  DEPENDS ${NAME}
                  COMMAND "${VALGRIND_EXECUTABLE}" --leak-check=full --track-origins=yes -q
                  --error-exitcode=63 "${workdir}/${NAME}")
      endif()

      if(${NAME} MATCHES "_tests")
          add_dependencies(run_tests ${NAME})
          add_custom_command(TARGET run_tests COMMAND "${workdir}/${NAME}" POST_BUILD)
      endif()
  endif()
endfunction()

# tests
add_standalone_target(standard_tests tests/anj/standard_tests ON ON)
add_standalone_target(standard_tests_without_composite tests/anj/standard_tests_without_composite ON ON)
add_standalone_target(standard_tests_without_external_data tests/anj/standard_tests_without_external_data ON ON)
add_standalone_target(standard_tests_without_lwm2m_1_2 tests/anj/standard_tests_without_lwm2m_1_2 ON ON)
add_standalone_target(log_tests tests/anj/log ON OFF)
add_standalone_target(net_tests tests/anj/net ON OFF)

# examples
add_standalone_target(anjay_lite_firmware_update examples/tutorial/firmware-update OFF OFF)
add_standalone_target(anjay_lite_firmware_update_pull examples/tutorial/firmware-update-coap-downloader OFF OFF)
add_standalone_target(anjay_lite_secure_firmware_update_pull examples/tutorial/firmware-update-coaps-downloader OFF ON)

add_standalone_target(anjay_lite_bc_initialization examples/tutorial/BC-Initialization OFF OFF)
add_standalone_target(anjay_lite_bc_mandatory_objects examples/tutorial/BC-MandatoryObjects OFF OFF)
add_standalone_target(anjay_lite_bc_security examples/tutorial/BC-Security OFF ON)
add_standalone_target(anjay_lite_bc_object_impl examples/tutorial/BC-BasicObjectImplementation OFF OFF)
add_standalone_target(anjay_lite_bc_notifications examples/tutorial/BC-Notifications OFF OFF)
add_standalone_target(anjay_lite_bc_send examples/tutorial/BC-Send OFF OFF)

add_standalone_target(anjay_lite_at_bootstrap examples/tutorial/AT-Bootstrap OFF OFF)
add_standalone_target(anjay_lite_at_persistence examples/tutorial/AT-Persistence OFF OFF)
add_standalone_target(anjay_lite_at_queue_mode examples/tutorial/AT-QueueMode OFF OFF)
add_standalone_target(anjay_lite_at_multi_instance_object examples/tutorial/AT-MultiInstanceObject OFF OFF)
add_standalone_target(anjay_lite_at_multi_instance_object_dynamic examples/tutorial/AT-MultiInstanceObjectDynamic OFF OFF)
add_standalone_target(anjay_lite_at_multi_instance_resource examples/tutorial/AT-MultiInstanceResource OFF OFF)
add_standalone_target(anjay_lite_at_multi_instance_resource_dynamic examples/tutorial/AT-MultiInstanceResourceDynamic OFF OFF)
add_standalone_target(anjay_lite_at_time_synchronization examples/tutorial/AT-TimeSynchronization OFF OFF)
add_standalone_target(anjay_lite_at_time_synchronization_persistence examples/tutorial/AT-TimeSynchronizationPersistence OFF OFF)
add_standalone_target(anjay_lite_at_micro_logs examples/tutorial/AT-MicroLogs OFF OFF)

add_standalone_target(anjay_lite_minimal_network_api examples/custom-network/minimal OFF OFF)

# Sphinx and doxygen documentation
# Note: documentation is configured and built only when the target is
# explicitly invoked (e.g. via `make doc_sphinx`).
function(add_doc_proxy_target TARGET_NAME)
    add_custom_target(${TARGET_NAME}
        COMMAND ${CMAKE_COMMAND} -S "${CMAKE_SOURCE_DIR}/doc" -B "${CMAKE_BINARY_DIR}/doc"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}/doc" --target ${TARGET_NAME}
        USES_TERMINAL
    )
endfunction()

add_doc_proxy_target(doc)
add_doc_proxy_target(doc_sphinx)
add_doc_proxy_target(doc_doxygen)
add_doc_proxy_target(test_doc_snippet)

# C++ header compatibility check
add_standalone_target(cxx_header_check tests/cxx_header_check OFF OFF)

add_standalone_target(codegen_compilation_check tests/codegen/compilation OFF OFF)
add_standalone_target(codegen_object_registry_check tests/codegen/object_registry OFF OFF)
add_standalone_target(codegen_add_object_tests tests/codegen/add_object OFF OFF)

# Check for correct inclusion of anj/init.h
add_custom_target(init_header_check
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/init_header_check/run_check.cmake"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
