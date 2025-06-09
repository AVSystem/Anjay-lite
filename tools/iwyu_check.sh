#!/usr/bin/env bash
# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

CLANG_MAJOR_VERSION_NUMBER=$(echo | clang -dM -E - | grep __clang_major__ | awk '{ print $NF }')

if [[ "${CLANG_MAJOR_VERSION_NUMBER}" != "18" ]]; then
    cat <<EOF >&2
The gitlab CI for this project operates on Ubuntu 24.04 image, which uses \
clang-18. It is different from your clang-${CLANG_MAJOR_VERSION_NUMBER}, \
therefore keep in mind that it may output differently. To handle that problem \
reinstall your clang to the same version and your IWYU to the corresponding \
0.22 version.
EOF
    read -sp "If you accept that, press ENTER to continue."
fi

declare -A targets
targets["tests/anj/io"]="io_tests_iwyu"
targets["tests/anj/io_without_extended"]="io_without_extended_tests_iwyu"
targets["tests/anj/coap"]="coap_tests_iwyu"
targets["tests/anj/dm"]="dm_tests_iwyu"
targets["tests/anj/dm_without_composite"]="dm_without_composite_tests_iwyu"
targets["tests/anj/observe"]="observe_tests_iwyu"
targets["tests/anj/observe_without_composite"]="observe_without_composite_tests_iwyu"
targets["tests/anj/exchange"]="exchange_tests_iwyu"
targets["tests/anj/net"]="net_tests_iwyu"
targets["tests/anj/core"]="core_tests_iwyu"
targets["tests/iwyu/anj"]="anj"

for example in "${!targets[@]}"; do
    pushd ${example}
        rm -rf build && mkdir build
        pushd build
            cmake -DANJ_IWYU_PATH="include-what-you-use" ..
            echo "empty" > /tmp/iwyu.out
            echo "Checking ${example}"
            make -j -B ${targets["${example}"]} 2> /tmp/iwyu.out
            # intercept general build fail
            if [ $? -ne 0 ]; then
                cat /tmp/iwyu.out
                exit 1
            fi
            # IWYU does not return error code, so we need to check the stderr
            grep -q "include-what-you-use" /tmp/iwyu.out
            if [ $? -eq 0 ]; then
                cat /tmp/iwyu.out
                echo "iwyu check FAILED for ${example}"
                exit 1
            fi
        popd
    popd
done
