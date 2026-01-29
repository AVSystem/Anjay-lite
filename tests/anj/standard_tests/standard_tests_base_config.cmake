# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

# CMake basics
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

# Below categories mirror those in cmake/anjay_lite-config.cmake;
# While changing these options here try to keep them grouped and ordered
# the same way as in anjay_lite-config.cmake for easier comparison.

# Internal options
set(ANJ_TESTING ON)

# data model configuration
set(ANJ_DM_MAX_OBJECTS_NUMBER 15)

# FOTA object configuration
set(ANJ_FOTA_WITH_COAP_TCP ON)

# CoAP downloader configuration
set(ANJ_WITH_COAP_DOWNLOADER ON)

# NTP module configuration
set(ANJ_WITH_NTP ON)

# observe configuration
set(ANJ_WITH_OBSERVE_COMPOSITE ON)
set(ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER 5)
set(ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER 5)

# LwM2M Send
set(ANJ_LWM2M_SEND_QUEUE_SIZE 3)

# compat layer configuration
set(ANJ_WITH_TIME_POSIX_COMPAT OFF)
set(ANJ_WITH_SOCKET_POSIX_COMPAT OFF)

# security configuration
set(ANJ_WITH_SECURITY ON)
set(ANJ_WITH_EXTERNAL_CRYPTO_STORAGE ON)

# data formats configuration
set(ANJ_WITH_EXTERNAL_DATA ON)

# CoAP related configuration
set(ANJ_COAP_MAX_ATTR_OPTION_SIZE 50)

# persistence configuration
set(ANJ_WITH_PERSISTENCE ON)
