/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

/**
 * @file
 * @brief Global configuration validation header for Anjay Lite.
 *
 * This header is automatically included by all other public Anjay Lite headers.
 * Its main role is to validate build-time configuration macros and enforce
 * consistency between enabled features.
 *
 * End users normally do not need to include this header directly. However, it
 * may be included explicitly if the application code needs to make decisions
 * based on the Anjay Lite configuration.
 */

#ifndef ANJ_INIT_H
#define ANJ_INIT_H

#include <anj/anj_config.h> // IWYU pragma: export

/** @cond */

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(ANJ_NET_WITH_UDP) && !defined(ANJ_NET_WITH_DTLS) \
        && !defined(ANJ_NET_WITH_NON_IP_BINDING)
#    error "at least one of ANJ_NET_WITH_UDP, ANJ_NET_WITH_DTLS, ANJ_NET_WITH_NON_IP_BINDING should be enabled"
#endif // !defined(ANJ_NET_WITH_UDP) && !defined(ANJ_NET_WITH_DTLS) &&
       // !defined(ANJ_NET_WITH_NON_IP_BINDING)

#if defined(ANJ_WITH_CACHE) && ANJ_CACHE_ENTRIES_NUMBER <= 0
#    error "if response caching is enabled, number of cached entries has to be greater than 0"
#endif // defined(ANJ_WITH_CACHE) && ANJ_CACHE_ENTRIES_NUMBER <= 0

#if defined(ANJ_WITH_RST_AS_CANCEL_OBSERVE) && !defined(ANJ_WITH_OBSERVE)
#    error "RST as Cancel Observe only makes sense when Observations are supported"
#endif // defined(ANJ_WITH_RST_AS_CANCEL_OBSERVE) && !defined(ANJ_WITH_OBSERVE)

#if defined(ANJ_WITH_LWM2M_CBOR) && !defined(ANJ_WITH_LWM2M12)
#    error "ANJ_WITH_LWM2M_CBOR requires ANJ_WITH_LWM2M12 enabled"
#endif // defined(ANJ_WITH_LWM2M_CBOR) && !defined(ANJ_WITH_LWM2M12)

#if !defined(ANJ_WITH_SENML_CBOR) && !defined(ANJ_WITH_LWM2M_CBOR)
#    error "At least one of ANJ_WITH_SENML_CBOR or ANJ_WITH_LWM2M_CBOR must be enabled."
#endif // !defined(ANJ_WITH_SENML_CBOR) && !defined(ANJ_WITH_LWM2M_CBOR)

#if defined(ANJ_WITH_DISCOVER_ATTR) && !defined(ANJ_WITH_OBSERVE)
#    error "if discover attributes are enabled, observe module needs to be enabled"
#endif // defined(ANJ_WITH_DISCOVER_ATTR) && !defined(ANJ_WITH_OBSERVE)

#if defined(ANJ_WITH_COMPOSITE_OPERATIONS) \
        && !defined(ANJ_DM_MAX_COMP_READ_ENTRIES)
#    error "if composite operations are enabled, their parameters have to be defined"
#endif // defined(ANJ_WITH_COMPOSITE_OPERATIONS) &&
       // !defined(ANJ_DM_MAX_COMP_READ_ENTRIES)

#ifdef ANJ_WITH_OBSERVE
#    if !defined(ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER) \
            || !defined(ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER)
#        error "if observe module is enabled, its parameters has to be defined"
#    endif // !defined(ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER) ||
           // !defined(ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER)
#endif     // ANJ_WITH_OBSERVE

#if defined(ANJ_WITH_OBSERVE_COMPOSITE) \
        && (!defined(ANJ_WITH_OBSERVE)  \
            || !defined(ANJ_WITH_COMPOSITE_OPERATIONS))
#    error "if composite observations are enabled, observations and composite operations have to be enabled"
#endif // defined(ANJ_WITH_OBSERVE_COMPOSITE) && (!defined(ANJ_WITH_OBSERVE) ||
       // !defined(ANJ_WITH_COMPOSITE_OPERATIONS))

#ifdef ANJ_WITH_LWM2M_SEND
#    if !defined(ANJ_LWM2M_SEND_QUEUE_SIZE)
#        error "if LwM2M Send module is enabled, its parameters has to be defined"
#    endif // !defined(ANJ_LWM2M_SEND_QUEUE_SIZE)
#endif     // ANJ_WITH_LWM2M_SEND

#ifdef ANJ_WITH_DEFAULT_FOTA_OBJ
#    if !defined(ANJ_FOTA_WITH_PULL_METHOD) \
            && !defined(ANJ_FOTA_WITH_PUSH_METHOD)
#        error "if FW Update object is enabled, at least one of push or pull methods needs to be enabled"
#    endif // !defined(ANJ_FOTA_WITH_PULL_METHOD) &&
           // !defined(ANJ_FOTA_WITH_PUSH_METHOD)

#    if defined(ANJ_FOTA_WITH_PULL_METHOD) && !defined(ANJ_FOTA_WITH_COAP)   \
            && !defined(ANJ_FOTA_WITH_COAPS) && !defined(ANJ_FOTA_WITH_HTTP) \
            && !defined(ANJ_FOTA_WITH_HTTPS)                                 \
            && !defined(ANJ_FOTA_WITH_COAP_TCP)                              \
            && !defined(ANJ_FOTA_WITH_COAPS_TCP)
#        error "if pull method is enabled, at least one of CoAP, CoAPS, HTTP, HTTPS, TCP or TLS needs to be enabled"
#    endif // defined(ANJ_FOTA_WITH_PULL_METHOD) && !defined(ANJ_FOTA_WITH_COAP)
           // && !defined(ANJ_FOTA_WITH_COAPS) && !defined(ANJ_FOTA_WITH_HTTP)
           // && !defined(ANJ_FOTA_WITH_HTTPS) &&
           // !defined(ANJ_FOTA_WITH_COAP_TCP) &&
           // !defined(ANJ_FOTA_WITH_COAPS_TCP)
#endif     // ANJ_WITH_DEFAULT_FOTA_OBJ

#ifdef ANJ_WITH_DEFAULT_SECURITY_OBJ
#    if !defined(ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE)   \
            || !defined(ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE) \
            || !defined(ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE)
#        error "if default Security Object is enabled, its parameters needs to be defined"
#    endif // !defined(ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE) ||
           // !defined(ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE) ||
           // !defined(ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE)
#endif     // ANJ_WITH_DEFAULT_SECURITY_OBJ

#ifdef ANJ_WITH_COAP_DOWNLOADER
#    if !defined(ANJ_COAP_DOWNLOADER_MAX_MSG_SIZE) \
            || !defined(ANJ_COAP_DOWNLOADER_MAX_PATHS_NUMBER)
#        error "if CoAP Downloader module is enabled, its parameters has to be defined"
#    endif
#endif // ANJ_WITH_COAP_DOWNLOADER

#ifdef ANJ_LOG_FULL
#    define _ANJ_LOG_FULL_ENABLED 1
#else // ANJ_LOG_FULL
#    define _ANJ_LOG_FULL_ENABLED 0
#endif // ANJ_LOG_FULL

#ifdef ANJ_LOG_MICRO
#    define _ANJ_LOG_MICRO_ENABLED 1
#else // ANJ_LOG_MICRO
#    define _ANJ_LOG_MICRO_ENABLED 0
#endif // ANJ_LOG_MICRO

#ifdef ANJ_LOG_ALT_IMPL_HEADER
#    define _ANJ_LOG_ALT_IMPL_HEADER_ENABLED 1
#else // ANJ_LOG_ALT_IMPL_HEADER
#    define _ANJ_LOG_ALT_IMPL_HEADER_ENABLED 0
#endif // ANJ_LOG_ALT_IMPL_HEADER

#define _ANJ_LOG_TYPES_ENABLED                      \
    (_ANJ_LOG_FULL_ENABLED + _ANJ_LOG_MICRO_ENABLED \
     + _ANJ_LOG_ALT_IMPL_HEADER_ENABLED)

#if _ANJ_LOG_TYPES_ENABLED > 1
#    error "Only one logger type can be enabled at a time."
#endif // _ANJ_LOG_TYPES_ENABLED > 1

#ifdef ANJ_LOG_HANDLER_OUTPUT_STDERR
#    define _ANJ_LOG_HANDLER_OUTPUT_STDERR_ENABLED 1
#else // ANJ_LOG_HANDLER_OUTPUT_STDERR
#    define _ANJ_LOG_HANDLER_OUTPUT_STDERR_ENABLED 0
#endif // ANJ_LOG_HANDLER_OUTPUT_STDERR

#ifdef ANJ_LOG_HANDLER_OUTPUT_ALT
#    define _ANJ_LOG_HANDLER_OUTPUT_ALT_ENABLED 1
#else // ANJ_LOG_HANDLER_OUTPUT_ALT
#    define _ANJ_LOG_HANDLER_OUTPUT_ALT_ENABLED 0
#endif // ANJ_LOG_HANDLER_OUTPUT_ALT

#define _ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED \
    (_ANJ_LOG_HANDLER_OUTPUT_STDERR_ENABLED   \
     + _ANJ_LOG_HANDLER_OUTPUT_ALT_ENABLED)

#if _ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED > 1
#    error "Only one log handler output type can be enabled at a time."
#endif // _ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED > 1

#if _ANJ_LOG_TYPES_ENABLED == 1
#    define _ANJ_LOG_ENABLED
#endif // _ANJ_LOG_TYPES_ENABLED == 1

#if defined(_ANJ_LOG_ENABLED) && !defined(ANJ_LOG_ALT_IMPL_HEADER)
#    define _ANJ_LOG_USES_BUILTIN_HANDLER_IMPL

#    if _ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED == 0
#        error "Log handler output type must be defined when using built-in log handler implementation."
#    endif // _ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED == 0

#    if !defined(ANJ_LOG_FORMATTER_BUF_SIZE) || ANJ_LOG_FORMATTER_BUF_SIZE <= 0
#        error "Log formatter buffer size must be greater than 0 when using built-in log handler implementation."
#    endif // !defined(ANJ_LOG_FORMATTER_BUF_SIZE) || ANJ_LOG_FORMATTER_BUF_SIZE
           // <= 0
#endif     // defined(_ANJ_LOG_ENABLED) && !defined(ANJ_LOG_ALT_IMPL_HEADER)

#if !defined(ANJ_WITH_SECURITY)
#    ifdef ANJ_WITH_CERTIFICATES
#        error "ANJ_WITH_CERTIFICATES requires secure network transport to be enabled"
#    endif // ANJ_WITH_CERTIFICATES
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
#        error "ANJ_WITH_EXTERNAL_CRYPTO_STORAGE requires secure network transport to be enabled"
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
#endif     // !defined(ANJ_WITH_SECURITY)

#ifdef ANJ_WITH_MBEDTLS

#    define ANJ_UINT32_MAX 4294967295U

#    if ANJ_MBEDTLS_PSK_IDENTITY_MAX_LEN < 0
#        error "Wrong max length for psk identity"
#    endif

#    if ANJ_MBEDTLS_HS_INITIAL_TIMEOUT_VALUE_MS < 0                     \
            || ANJ_MBEDTLS_HS_INITIAL_TIMEOUT_VALUE_MS > ANJ_UINT32_MAX \
            || ANJ_MBEDTLS_HS_MAXIMUM_TIMEOUT_VALUE_MS < 0              \
            || ANJ_MBEDTLS_HS_MAXIMUM_TIMEOUT_VALUE_MS > ANJ_UINT32_MAX
#        error "Wrong handshake timeout values"
#    endif

#    undef ANJ_UINT32_MAX

#endif // ANJ_WITH_MBEDTLS

#ifdef __cplusplus
}
#endif

/** @endcond */

#endif // ANJ_INIT_H
