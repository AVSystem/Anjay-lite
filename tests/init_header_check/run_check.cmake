# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

set(repo_root "${CMAKE_SOURCE_DIR}")
message(STATUS "Repo root: ${repo_root}")

file(GLOB_RECURSE anj_source_files
     RELATIVE "${repo_root}"
     "include_public/anj/*.h"
     "src/anj/*.[ch]")

function(check_source_file file_path)
    if(file_path STREQUAL "include_public/anj/init.h")
        # Skip the init.h file itself
        return()
    endif()

    file(STRINGS "${repo_root}/${file_path}" file_lines)
    # Note: assume first 8 lines are the license header. If the file has
    # an incorrect header, it will be checked by other script anyway.

    # Expect empty line before the include
    list(GET file_lines 8 expected_empty_line)
    if(NOT expected_empty_line STREQUAL "")
        set(invalid_files "${file_path}\n${invalid_files}" PARENT_SCOPE)
        return()
    endif()

    list(GET file_lines 9 expected_init_include_line)
    if(NOT expected_init_include_line MATCHES "^#include [<\"].*anj/init.h[>\"]$")
        set(invalid_files "${file_path}\n${invalid_files}" PARENT_SCOPE)
        return()
    endif()

    # Expect empty line after the include
    list(GET file_lines 10 expected_empty_line)
    if(NOT expected_empty_line STREQUAL "")
        set(invalid_files "${file_path}\n${invalid_files}" PARENT_SCOPE)
        return()
    endif()
endfunction()

foreach(source_file ${anj_source_files})
    check_source_file("${source_file}")
endforeach()

if(invalid_files)
    message(FATAL_ERROR "The following files do not include anj/init.h correctly:\n${invalid_files}")
else()
    message(STATUS "All files include anj/init.h correctly.")
endif()
