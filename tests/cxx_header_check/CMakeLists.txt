# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(anj_cxx_header_check CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

set(anjay_lite_DIR "../../cmake")
find_package(anjay_lite REQUIRED)

# NOTE: since CMake 3.6 the results are ordered lexicographically
file(GLOB_RECURSE anj_headers
     RELATIVE "${CMAKE_SOURCE_DIR}/../../include_public"
     "../../include_public/anj/*.h")

set(test_src "${CMAKE_CURRENT_BINARY_DIR}/test_program.cpp")

file(WRITE "${test_src}" "#include <anj/anj_config.h>\n")
foreach(header ${anj_headers})
    file(APPEND "${test_src}" "#include <${header}>\n")
endforeach()
file(APPEND "${test_src}" "int main() { return 0; }\n")

add_executable(cxx_header_check "${test_src}")
target_link_libraries(cxx_header_check PRIVATE anj)
target_compile_options(cxx_header_check PRIVATE -pedantic -Wall -Wextra -Werror)
