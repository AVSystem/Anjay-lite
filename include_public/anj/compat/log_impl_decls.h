/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Platform hooks for log output.
 *
 * This header declares the minimal API that platform integrators may implement
 * or override to control how Anjay Lite logs are processed:
 * - @ref anj_log_handler_impl_full - full log handler with metadata
 * - @ref anj_log_handler_output - sink for formatted log strings
 *
 * The implementation may route logs to console, syslog, RTT, UART, or any
 * other platform-specific output. Which functions are active depends on
 * compile-time configuration (e.g. @c ANJ_LOG_FULL or
 * @c ANJ_LOG_HANDLER_OUTPUT_ALT).
 */

#ifndef ANJ_LOG_LOG_IMPL_DECLS_H
#    define ANJ_LOG_LOG_IMPL_DECLS_H

#    include <stdarg.h>
#    include <stddef.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * Specifies level of a log statement.
 *
 * @note Log macros are expecting the level without the @c ANJ_LOG_LEVEL_
 *       prefix.
 *
 * @note On design choice: the options are named @c L_level instead of just
 * @c level to avoid collisions with applications that define e.g. @c DEBUG.
 */
typedef enum {
    ANJ_LOG_LEVEL_L_TRACE,
    ANJ_LOG_LEVEL_L_DEBUG,
    ANJ_LOG_LEVEL_L_INFO,
    ANJ_LOG_LEVEL_L_WARNING,
    ANJ_LOG_LEVEL_L_ERROR,
    ANJ_LOG_LEVEL_L_MUTED
} anj_log_level_t;

/**
 * Full implementation of log handler, enabled if @ref ANJ_LOG_FULL is defined.
 *
 * @warning This function is not meant to be called directly. Use the
 *          @ref anj_log macro instead.
 *
 * @param level  Log level of the message.
 * @param module Name of the module originating the log message.
 * @param file   Name of the source file where the log message originates
 *                   from.
 * @param line   Line number in the source file where the log message
 *                   originates from.
 * @param format printf-style format string for the log message.
 * @param ...    Arguments for the format string.
 */
void anj_log_handler_impl_full(anj_log_level_t level,
                               const char *module,
                               const char *file,
                               int line,
                               const char *format,
                               ...);

/**
 * Function used to output the formatted log strings, if one of builtin handler
 * implementations is enabled.
 *
 * @note if @ref ANJ_LOG_HANDLER_OUTPUT_ALT is enabled, user must implement this
 *       function.
 *
 * @param output Formatted log statement to output.
 * @param len    Length of formatted log statement, effectively equal to
 *               @c strlen(output)
 */
void anj_log_handler_output(const char *output, size_t len);

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_LOG_LOG_IMPL_DECLS_H
