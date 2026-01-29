# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

# HACK: This doesn't match the version per our scheme, but since we're in beta,
# we just don't want to satisfy queries for package in version 1 and higher.
#
# This needs to follow the format defined here:
# https://cmake.org/cmake/help/latest/command/find_package.html#find-package-version-format
set(PACKAGE_VERSION "0.1.0")

message(STATUS "anjay_lite-config-version.cmake: ${PACKAGE_FIND_VERSION}")
if("${PACKAGE_FIND_VERSION}" STREQUAL "")
  # If no version was specified, assume that the user is fine with any version
  set(PACKAGE_FIND_VERSION "${PACKAGE_VERSION}")
endif()

if(${PACKAGE_VERSION} VERSION_LESS ${PACKAGE_FIND_VERSION})
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
  if(${PACKAGE_FIND_VERSION} STREQUAL ${PACKAGE_VERSION})
    set(PACKAGE_VERSION_EXACT TRUE)
  endif()
endif()
