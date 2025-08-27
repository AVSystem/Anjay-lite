/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

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
 * NOTE: Log macros are expecting the level without the <c>ANJ_LOG_LEVEL_</c>
 * prefix.
 *
 * NOTE: On design choice - the options are named <c>L_level</c> instead of
 * just <c>level</c> as an attempt to avoid collisions with applications
 * that define e.g. <c>DEBUG</c>
 */
typedef enum {
    ANJ_LOG_LEVEL_L_TRACE,
    ANJ_LOG_LEVEL_L_DEBUG,
    ANJ_LOG_LEVEL_L_INFO,
    ANJ_LOG_LEVEL_L_WARNING,
    ANJ_LOG_LEVEL_L_ERROR,
    ANJ_LOG_LEVEL_L_MUTED
} anj_log_level_t;

void anj_log_handler_impl_full(anj_log_level_t level,
                               const char *module,
                               const char *file,
                               int line,
                               const char *format,
                               ...);

/**
 * Function used to output the formatted log strings, if one of builtin
 * handler implementations is enabled.
 *
 * NOTE: if @ref ANJ_LOG_HANDLER_OUTPUT_ALT is enabled, user must implement this
 * function.
 *
 * @param output Formatted log statement to output.
 *
 * @param len    Length of formatted log statement, effectively equal to
 *               <c>strlen(output)</c>
 */
void anj_log_handler_output(const char *output, size_t len);

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_LOG_LOG_IMPL_DECLS_H
