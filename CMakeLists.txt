# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.6.0)

# Core CMake code is located in cmake/anjay_lite-config.cmake and the library is
# supposed to be imported using find_package() mechanism - this file only meant
# to be a convenience wrapper for all example and test targets.

project(anjay_lite C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)

# Below find_package() call is meant for IDE indexing purposes.
set(anjay_lite_DIR "cmake")
find_package(anjay_lite REQUIRED)

find_program(VALGRIND_EXECUTABLE valgrind)

add_custom_target(run_tests)

function(add_standalone_target NAME PATH WITH_VALGRIND)
  set(workdir "${CMAKE_BINARY_DIR}/${NAME}")

  string(REPLACE " " ";" unescaped_c_flags "${CMAKE_C_FLAGS}")
  string(REPLACE " " ";" unescaped_exe_linker_flags "${CMAKE_EXE_LINKER_FLAGS}")

  add_custom_target(${NAME} ALL)
  add_custom_command(TARGET ${NAME} COMMAND ${CMAKE_COMMAND} -E make_directory
                                            "${workdir}")
  add_custom_command(
    TARGET ${NAME}
    COMMAND
      ${CMAKE_COMMAND} -S "${CMAKE_CURRENT_SOURCE_DIR}/${PATH}" -B .
      -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER}"
      -DCMAKE_C_FLAGS="${unescaped_c_flags}"
      -DCMAKE_EXE_LINKER_FLAGS="${unescaped_exe_linker_flags}"
    COMMAND ${CMAKE_COMMAND} --build . --target ${NAME} -- -j${NPROC}
    WORKING_DIRECTORY "${workdir}")

  if(WITH_VALGRIND AND VALGRIND_EXECUTABLE)
    add_custom_target(
      ${NAME}_with_valgrind
      DEPENDS ${NAME}
      COMMAND "${VALGRIND_EXECUTABLE}" --leak-check=full --track-origins=yes -q
              --error-exitcode=63 "${workdir}/${NAME}")
  endif()

  if(${NAME} MATCHES "_tests")
    add_dependencies(run_tests ${NAME})
    add_custom_command(TARGET run_tests COMMAND "${workdir}/${NAME}")
  endif()
endfunction()

# tests
add_standalone_target(dm_tests tests/anj/dm ON)
add_standalone_target(dm_without_composite_tests tests/anj/dm_without_composite ON)
add_standalone_target(observe_tests tests/anj/observe ON)
add_standalone_target(observe_without_composite_tests tests/anj/observe_without_composite ON)
add_standalone_target(exchange_tests tests/anj/exchange ON)
add_standalone_target(io_tests tests/anj/io ON)
add_standalone_target(io_tests_without_extended tests/anj/io_without_extended ON)
add_standalone_target(coap_tests tests/anj/coap ON)
add_standalone_target(net_tests tests/anj/net ON)
add_standalone_target(core_tests tests/anj/core ON)

# examples
add_standalone_target(anjay_lite_firmware_update examples/tutorial/firmware-update OFF)

add_standalone_target(anjay_lite_bc_initialization examples/tutorial/BC-Initialization OFF)
add_standalone_target(anjay_lite_bc_mandatory_objects examples/tutorial/BC-MandatoryObjects OFF)
add_standalone_target(anjay_lite_bc_object_impl examples/tutorial/BC-BasicObjectImplementation OFF)
add_standalone_target(anjay_lite_bc_notifications examples/tutorial/BC-Notifications OFF)
add_standalone_target(anjay_lite_bc_send examples/tutorial/BC-Send OFF)

add_standalone_target(anjay_lite_at_bootstrap examples/tutorial/AT-Bootstrap OFF)
add_standalone_target(anjay_lite_at_queue_mode examples/tutorial/AT-QueueMode OFF)
add_standalone_target(anjay_lite_at_multi_instance_object examples/tutorial/AT-MultiInstanceObject OFF)
add_standalone_target(anjay_lite_at_multi_instance_object_dynamic examples/tutorial/AT-MultiInstanceObjectDynamic OFF)
add_standalone_target(anjay_lite_at_multi_instance_resource examples/tutorial/AT-MultiInstanceResource OFF)
add_standalone_target(anjay_lite_at_multi_instance_resource_dynamic examples/tutorial/AT-MultiInstanceResourceDynamic OFF)

add_standalone_target(anjay_lite_minimal_network_api examples/custom-network/minimal OFF)
add_standalone_target(anjay_lite_reuse_port examples/custom-network/reuse-port OFF)

# Sphinx and doxygen documentation
add_subdirectory(doc)

# C++ header compatibility check
add_standalone_target(cxx_header_check tests/cxx_header_check OFF)
