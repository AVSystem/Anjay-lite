# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.6.0)

project(anj_iwyu C)

set(ANJ_TESTING ON)

set(anjay_lite_DIR "../../../cmake")
find_package(anjay_lite REQUIRED)
use_iwyu_if_enabled(anj)
