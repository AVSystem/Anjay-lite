/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_INCLUDE_LOG_CONFIG_CHECK
#    error "This header shall not be included directly."
#endif // ANJ_INTERNAL_INCLUDE_LOG_CONFIG_CHECK

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ANJ_LOG_FULL
#    define ANJ_LOG_FULL_ENABLED 1
#else // ANJ_LOG_FULL
#    define ANJ_LOG_FULL_ENABLED 0
#endif // ANJ_LOG_FULL

#ifdef ANJ_LOG_ALT_IMPL_HEADER
#    define ANJ_LOG_ALT_IMPL_HEADER_ENABLED 1
#else // ANJ_LOG_ALT_IMPL_HEADER
#    define ANJ_LOG_ALT_IMPL_HEADER_ENABLED 0
#endif // ANJ_LOG_ALT_IMPL_HEADER

#define ANJ_LOG_TYPES_ENABLED \
    (ANJ_LOG_FULL_ENABLED + ANJ_LOG_ALT_IMPL_HEADER_ENABLED)

#if ANJ_LOG_TYPES_ENABLED > 1
#    error "Only one logger type can be enabled at a time."
#endif // ANJ_LOG_TYPES_ENABLED > 1

#ifdef ANJ_LOG_HANDLER_OUTPUT_STDERR
#    define ANJ_LOG_HANDLER_OUTPUT_STDERR_ENABLED 1
#else // ANJ_LOG_HANDLER_OUTPUT_STDERR
#    define ANJ_LOG_HANDLER_OUTPUT_STDERR_ENABLED 0
#endif // ANJ_LOG_HANDLER_OUTPUT_STDERR

#ifdef ANJ_LOG_HANDLER_OUTPUT_ALT
#    define ANJ_LOG_HANDLER_OUTPUT_ALT_ENABLED 1
#else // ANJ_LOG_HANDLER_OUTPUT_ALT
#    define ANJ_LOG_HANDLER_OUTPUT_ALT_ENABLED 0
#endif // ANJ_LOG_HANDLER_OUTPUT_ALT

#define ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED \
    (ANJ_LOG_HANDLER_OUTPUT_STDERR_ENABLED + ANJ_LOG_HANDLER_OUTPUT_ALT_ENABLED)

#if ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED > 1
#    error "Only one log handler output type can be enabled at a time."
#endif // ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED > 1

#if ANJ_LOG_TYPES_ENABLED == 1
#    define ANJ_LOG_ENABLED
#endif // ANJ_LOG_TYPES_ENABLED == 1

#if defined(ANJ_LOG_ENABLED) && !defined(ANJ_LOG_ALT_IMPL_HEADER)
#    define ANJ_LOG_USES_BUILTIN_HANDLER_IMPL

#    if ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED == 0
#        error "Log handler output type must be defined when using built-in log handler implementation."
#    endif // ANJ_LOG_HANDLER_OUTPUT_TYPES_ENABLED == 0

#    if !defined(ANJ_LOG_FORMATTER_BUF_SIZE) || ANJ_LOG_FORMATTER_BUF_SIZE <= 0
#        error "Log formatter buffer size must be greater than 0 when using built-in log handler implementation."
#    endif // !defined(ANJ_LOG_FORMATTER_BUF_SIZE) || ANJ_LOG_FORMATTER_BUF_SIZE
           // <= 0
#endif     // defined(ANJ_LOG_ENABLED) && !defined(ANJ_LOG_ALT_IMPL_HEADER)

#ifdef __cplusplus
}
#endif
