#!/bin/bash
# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

TARGETS=(
    codegen_object_registry_check
    codegen_compilation_check
    codegen_add_object_tests
)
CC=(
    gcc
    clang
)
OBJECT_INSTANCES_COUNT=(
    1
    2
)
HANDLING=(
    static
    dynamic
)

mkdir -p build && cd build
for cc in "${CC[@]}"; do
    for obj_count in "${OBJECT_INSTANCES_COUNT[@]}"; do
        for handling_res in "${HANDLING[@]}"; do
            for handling_obj in "${HANDLING[@]}"; do
            CC="$cc" cmake ..  -DCLI_OBJECT_INSTANCES_HANDLING="$handling_obj" -DCLI_RESOURCE_INSTANCES_HANDLING="$handling_res" -DCLI_OBJECT_INSTANCES_COUNT="$obj_count" -DCMAKE_C_FLAGS="-Werror"
                for target in "${TARGETS[@]}"; do
                    make "$target"
                    if [[ "$target" == "codegen_add_object_tests" ]]; then 
                        ./codegen_add_object_tests/codegen_add_object_tests; 
                    fi
                done
            done 
        done
    done
done

echo -e "\e[32mCode generation tests: PASSED\e[0m"
