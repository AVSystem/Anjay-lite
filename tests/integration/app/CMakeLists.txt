# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16)

project(test_app C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ANJ_WITH_MBEDTLS ON)
set(ANJ_WITH_SECURITY ON)
set(ANJ_NET_WITH_DTLS ON)

set(anjay_lite_DIR "../../../cmake")
find_package(anjay_lite REQUIRED)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
     "src/*.cpp"
     "src/*.c"
)

add_executable(test_app ${SOURCES})

target_include_directories(test_app PRIVATE nlohmann)
if(APPLE)
    target_compile_definitions(test_app PRIVATE
        _DARWIN_C_SOURCE
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(test_app PRIVATE
        _DEFAULT_SOURCE
    )
endif()
# target_compile_definitions(test_app PRIVATE _POSIX_C_SOURCE=200809L)

target_link_libraries(test_app PRIVATE
        anj)
