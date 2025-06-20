/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/anj_config.h>

#define ANJ_INTERNAL_INCLUDE_LOG_CONFIG_CHECK
#include <anj_internal/log/log_config_check.h>
#undef ANJ_INTERNAL_INCLUDE_LOG_CONFIG_CHECK

#ifdef ANJ_LOG_USES_BUILTIN_HANDLER_IMPL

#    include <assert.h>
#    include <stdarg.h>
#    include <stdbool.h>
#    include <stddef.h>
#    include <stdio.h>

#    include <anj/compat/log_impl_decls.h>
#    include <anj/utils.h>

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

#    ifdef ANJ_LOG_DEBUG_FORMAT_CONSTRAINTS_CHECK
static void debug_format_constraints_check(const char *format) {
    static const char *valid_specifiers[] = { "%",  "s",   "f",  "d",
                                              "ld", "lld", "zd", "u",
                                              "lu", "llu", "zu" };

    while (*format) {
        char maybe_percent_char = *format;
        format++;
        if (maybe_percent_char != '%') {
            continue;
        }

        bool specifier_found = false;
        for (size_t i = 0; i < ANJ_ARRAY_SIZE(valid_specifiers); i++) {
            const char *format_specifier_ptr = format;
            const char *valid_specifier = valid_specifiers[i];

            while (*format_specifier_ptr && *valid_specifier) {
                if (*format_specifier_ptr != *valid_specifier) {
                    break;
                }
                format_specifier_ptr++;
                valid_specifier++;
            }

            // if we were comparing a specifier found if format and we've
            // reached the end of one of allowed specifiers, this is certainly a
            // full match
            if (!*valid_specifier) {
                specifier_found = true;

                // skip the specifier in format string
                format = format_specifier_ptr;
                break;
            }
        }

        if (!specifier_found) {
            ANJ_UNREACHABLE("Invalid format specifier found");
            return;
        }
    }
}
#    else  // ANJ_LOG_DEBUG_FORMAT_CONSTRAINTS_CHECK
static inline void debug_format_constraints_check(const char *format) {
    (void) format;
}
#    endif // ANJ_LOG_DEBUG_FORMAT_CONSTRAINTS_CHECK

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
    debug_format_constraints_check(format);
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
#endif     // ANJ_LOG_USES_BUILTIN_HANDLER_IMPL
