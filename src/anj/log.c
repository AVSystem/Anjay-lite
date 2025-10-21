/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include <anj/compat/log_impl_decls.h>
#include <anj/utils.h>

#ifdef _ANJ_LOG_USES_BUILTIN_HANDLER_IMPL

/**
 * TODO: add support for alternative implementations of the formatter, including
 * a lightweight one that does not use printf.
 */
#    ifdef ANJ_LOG_FORMATTER_PRINTF
#        define formatter_va_list(...) vsnprintf(__VA_ARGS__)
#    endif // ANJ_LOG_FORMATTER_PRINTF

// generic variadic wrapper for formatters that accept a va_list
static int
formatter_variadic(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int len = formatter_va_list(buffer, size, format, args);
    va_end(args);
    return len;
}

static inline int actual_formatter_str_len(size_t buffer_size,
                                           int formatter_retval) {
    assert(formatter_retval >= 0);
    // [v]snprintf returns the number of characters that would have been written
    // if the buffer was large enough (excluding the null terminator)
    return ANJ_MIN(formatter_retval, (int) (buffer_size - 1));
}

#    ifdef ANJ_LOG_FULL
static const char *level_as_string(anj_log_level_t level) {
    static const char *level_strings[] = {
        [ANJ_LOG_LEVEL_L_TRACE] = "TRACE",
        [ANJ_LOG_LEVEL_L_DEBUG] = "DEBUG",
        [ANJ_LOG_LEVEL_L_INFO] = "INFO",
        [ANJ_LOG_LEVEL_L_WARNING] = "WARNING",
        [ANJ_LOG_LEVEL_L_ERROR] = "ERROR"
    };
    return level < ANJ_LOG_LEVEL_L_MUTED ? level_strings[level] : "???";
}

void anj_log_handler_impl_full(anj_log_level_t level,
                               const char *module,
                               const char *file,
                               int line,
                               const char *format,
                               ...) {
    char buffer[ANJ_LOG_FORMATTER_BUF_SIZE];

    int header_len =
            formatter_variadic(buffer, sizeof(buffer),
                               "%s [%s] [%s:%d]: ", level_as_string(level),
                               module, file, line);
    assert(header_len >= 0);
    if (header_len < 0) {
        return;
    }
    header_len = actual_formatter_str_len(sizeof(buffer), header_len);

    va_list args;
    va_start(args, format);
    int msg_len = formatter_va_list(buffer + header_len,
                                    sizeof(buffer) - (unsigned int) header_len,
                                    format, args);
    va_end(args);

    assert(msg_len >= 0);
    if (msg_len < 0) {
        return;
    }
    msg_len =
            actual_formatter_str_len(sizeof(buffer) - (unsigned int) header_len,
                                     msg_len);

    anj_log_handler_output(buffer, (size_t) (header_len + msg_len));
}
#    endif // ANJ_LOG_FULL

#    ifdef ANJ_LOG_HANDLER_OUTPUT_STDERR
void anj_log_handler_output(const char *output, size_t len) {
    (void) len;
    fputs(output, stderr);
    fputc('\n', stderr);
}
#    endif // ANJ_LOG_HANDLER_OUTPUT_STDERR
#endif     // _ANJ_LOG_USES_BUILTIN_HANDLER_IMPL
