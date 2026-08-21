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

# Below find_package() call is meant for IDE indexing purposes.
set(anjay_lite_DIR "cmake")
find_package(anjay_lite REQUIRED)

set(ANJAY_LITE_VERSION "3.0.0" CACHE STRING "Anjay library version")

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

function(add_standalone_target NAME PATH WITH_VALGRIND)
  set(workdir "${CMAKE_BINARY_DIR}/${NAME}")

  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${PATH})
      ExternalProject_Add(
              ${NAME}
              SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PATH}"
              BINARY_DIR "${workdir}"
              CMAKE_CACHE_ARGS
              -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
              -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
              -DCMAKE_EXE_LINKER_FLAGS:STRING=${CMAKE_EXE_LINKER_FLAGS}
              -DMBEDTLS_ROOT_DIR:STRING=${MBEDTLS_ROOT_DIR} # pass mbedtls path to examples and tests
              ${COMMAND_LINE_FLAGS}
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
add_standalone_target(standard_tests tests/anj/standard_tests ON)
add_standalone_target(standard_tests_without_composite tests/anj/standard_tests_without_composite ON)
add_standalone_target(standard_tests_without_external_data tests/anj/standard_tests_without_external_data ON)
add_standalone_target(standard_tests_without_lwm2m_1_2 tests/anj/standard_tests_without_lwm2m_1_2 ON)
add_standalone_target(standard_tests_without_certificates tests/anj/standard_tests_without_certificates ON)
add_standalone_target(log_tests tests/anj/log ON)
add_standalone_target(net_tests tests/anj/net ON)

# examples
add_standalone_target(anjay_lite_firmware_update examples/tutorial/firmware-update OFF)
add_standalone_target(anjay_lite_firmware_update_pull examples/tutorial/firmware-update-coap-downloader OFF)
add_standalone_target(anjay_lite_firmware_update_suspend_resume examples/tutorial/firmware-update-downloader-suspend-resume OFF)
add_standalone_target(anjay_lite_secure_firmware_update_pull examples/tutorial/firmware-update-coaps-downloader OFF)
add_standalone_target(anjay_lite_certs_firmware_update_pull examples/tutorial/firmware-update-coaps-downloader-certificates OFF)

add_standalone_target(anjay_lite_bc_initialization examples/tutorial/BC-Initialization OFF)
add_standalone_target(anjay_lite_bc_mandatory_objects examples/tutorial/BC-MandatoryObjects OFF)
add_standalone_target(anjay_lite_bc_security examples/tutorial/BC-Security OFF)
add_standalone_target(anjay_lite_bc_object_impl examples/tutorial/BC-BasicObjectImplementation OFF)
add_standalone_target(anjay_lite_bc_notifications examples/tutorial/BC-Notifications OFF)
add_standalone_target(anjay_lite_bc_send examples/tutorial/BC-Send OFF)

add_standalone_target(anjay_lite_at_bootstrap examples/tutorial/AT-Bootstrap OFF)
add_standalone_target(anjay_lite_at_certificates examples/tutorial/AT-Certificates OFF)
add_standalone_target(anjay_lite_at_trust_store examples/tutorial/AT-TrustStore OFF)
add_standalone_target(anjay_lite_at_persistence examples/tutorial/AT-Persistence OFF)
add_standalone_target(anjay_lite_at_queue_mode examples/tutorial/AT-QueueMode OFF)
add_standalone_target(anjay_lite_at_connectivity_errors examples/tutorial/AT-ConnectivityErrors OFF)
add_standalone_target(anjay_lite_at_multi_instance_object examples/tutorial/AT-MultiInstanceObject OFF)
add_standalone_target(anjay_lite_at_multi_instance_object_dynamic examples/tutorial/AT-MultiInstanceObjectDynamic OFF)
add_standalone_target(anjay_lite_at_multi_instance_resource examples/tutorial/AT-MultiInstanceResource OFF)
add_standalone_target(anjay_lite_at_multi_instance_resource_dynamic examples/tutorial/AT-MultiInstanceResourceDynamic OFF)
add_standalone_target(anjay_lite_at_time_synchronization examples/tutorial/AT-TimeSynchronization OFF)
add_standalone_target(anjay_lite_at_time_synchronization_persistence examples/tutorial/AT-TimeSynchronizationPersistence OFF)
add_standalone_target(anjay_lite_at_micro_logs examples/tutorial/AT-MicroLogs OFF)
add_standalone_target(anjay_lite_at_external_data_types examples/tutorial/AT-ExternalDataTypes OFF)

add_standalone_target(anjay_lite_minimal_network_api examples/custom-network/minimal OFF)

# Sphinx/doxygen and documentation
add_subdirectory(doc)

# C++ header compatibility check
add_standalone_target(cxx_header_check tests/cxx_header_check OFF)

add_standalone_target(codegen_compilation_check tests/codegen/compilation OFF)
add_standalone_target(codegen_add_object_tests tests/codegen/add_object OFF)

# Check for correct inclusion of anj/init.h
add_custom_target(init_header_check
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/init_header_check/run_check.cmake"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
