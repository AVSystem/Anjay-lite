# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(anj_iwyu C)

set(ANJ_TESTING ON)

# General options
set(ANJ_WITH_EXTRA_WARNINGS ON)

# data model configuration
set(ANJ_WITH_COMPOSITE_OPERATIONS ON)

# device object configuration
set(ANJ_WITH_DEFAULT_DEVICE_OBJ ON)

# security object configuration
set(ANJ_WITH_DEFAULT_SECURITY_OBJ ON)

# server object configuration
set(ANJ_WITH_DEFAULT_SERVER_OBJ ON)

# FOTA object configuration
set(ANJ_WITH_DEFAULT_FOTA_OBJ ON)
set(ANJ_FOTA_WITH_PULL_METHOD ON)
set(ANJ_FOTA_WITH_PUSH_METHOD ON)
set(ANJ_FOTA_WITH_COAP ON)
set(ANJ_FOTA_WITH_COAPS ON)
set(ANJ_FOTA_WITH_HTTP ON)
set(ANJ_FOTA_WITH_HTTPS ON)
set(ANJ_FOTA_WITH_COAP_TCP ON)
set(ANJ_FOTA_WITH_COAPS_TCP ON)

# CoAP downloader configuration
set(ANJ_WITH_COAP_DOWNLOADER ON)

# observe configuration
set(ANJ_WITH_OBSERVE ON)
set(ANJ_WITH_OBSERVE_COMPOSITE ON)

# bootstrap configuration
set(ANJ_WITH_BOOTSTRAP ON)
set(ANJ_WITH_BOOTSTRAP_DISCOVER ON)

# discover configuration
set(ANJ_WITH_DISCOVER ON)
set(ANJ_WITH_DISCOVER_ATTR ON)

# LwM2M Send
set(ANJ_WITH_LWM2M_SEND ON)

# compat layer configuration
set(ANJ_WITH_TIME_POSIX_COMPAT ON)
set(ANJ_WITH_SOCKET_POSIX_COMPAT ON)
set(ANJ_NET_WITH_IPV4 ON)
set(ANJ_NET_WITH_IPV6 ON)
set(ANJ_NET_WITH_UDP ON)
set(ANJ_NET_WITH_TCP ON)

# data formats configuration
set(ANJ_WITH_CBOR ON)
set(ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS ON)
set(ANJ_WITH_CBOR_DECODE_HALF_FLOAT ON)
set(ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES ON)
set(ANJ_WITH_CBOR_DECODE_STRING_TIME ON)
set(ANJ_WITH_LWM2M_CBOR ON)
set(ANJ_WITH_SENML_CBOR ON)
set(ANJ_WITH_PLAINTEXT ON)
set(ANJ_WITH_OPAQUE ON)
set(ANJ_WITH_TLV ON)
set(ANJ_WITH_EXTERNAL_DATA ON)

# CoAP related configuration
set(ANJ_COAP_WITH_UDP ON)
set(ANJ_COAP_WITH_TCP ON)
set(ANJ_WITH_CACHE ON)

# logger configuration
set(ANJ_LOG_FULL ON)
set(ANJ_LOG_FORMATTER_PRINTF ON)
set(ANJ_LOG_HANDLER_OUTPUT_STDERR ON)
set(ANJ_LOG_DEBUG_FORMAT_CONSTRAINTS_CHECK ON)
set(ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS OFF)

# other configuration
set(ANJ_WITH_LWM2M12 ON)
set(ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS ON)

set(anjay_lite_DIR "../../../cmake")
find_package(anjay_lite REQUIRED)

set(ANJ_IWYU_PATH_AND_OPTIONS
    ${ANJ_IWYU_PATH}
    -Xiwyu --keep=*/anj/init.h
)
set_target_properties(anj PROPERTIES
        C_INCLUDE_WHAT_YOU_USE "${ANJ_IWYU_PATH_AND_OPTIONS}")
