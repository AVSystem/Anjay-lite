/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Logging API for Anjay Lite.
 *
 * Provides macros for emitting log messages with compile-time filtering and
 * configurable backends.
 */

#ifndef ANJ_LOG_LOG_H
#    define ANJ_LOG_LOG_H

#    include <stdio.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    include <anj/compat/log_impl_decls.h> // IWYU pragma: export
#    include <anj/utils.h>

/** @cond */
#    define ANJ_INTERNAL_INCLUDE_LOG_FILTERING_UTILS
#    include <anj_internal/log/log_filtering_utils.h>
#    undef ANJ_INTERNAL_INCLUDE_LOG_FILTERING_UTILS
/** @endcond */

/** @cond */
/**
 * Makes compiler emit warnings for provided format string and arguments
 * without any overhead for the resulting application, as whole expression
 * is wrapped into <c>sizeof(...)</c> that if calculated at compile time.
 *
 * Value of <c>sizeof(...)</c> is later discarded by using comma operator.
 */
#    define ANJ_LOG_PRINTF_ARG_CHECK(...) ((void) sizeof(printf(__VA_ARGS__)))

/**
 * Enforces the format to be an inline string literal, by concatenating it
 * (the first vararg) with an empty string literal.
 */
#    define ANJ_LOG_ENFORCE_FORMAT_INLINE_LITERAL(...) "" __VA_ARGS__

/**
 * This macro is a best-effort check that:
 * - only a string literal is a valid argument as a format, so that the line
 *   number of a log statement can be used to determine the provided format
 *   string if it's stripped from the application
 * - enables emitting warnings when the provided format arguments do not match
 *   the format string, if the compiler supports it
 */
#    define ANJ_LOG_COMPILE_TIME_CHECK(...) \
        ANJ_LOG_PRINTF_ARG_CHECK(           \
                ANJ_LOG_ENFORCE_FORMAT_INLINE_LITERAL(__VA_ARGS__))

#    ifdef _ANJ_LOG_ENABLED

#        ifdef ANJ_LOG_FULL
#            define ANJ_LOG_HANDLER_IMPL_MACRO(Module, LogLevel, ...)        \
                anj_log_handler_impl_full(ANJ_LOG_LEVEL_##LogLevel,          \
                                          ANJ_QUOTE_MACRO(Module), __FILE__, \
                                          __LINE__, __VA_ARGS__)
#        endif // ANJ_LOG_FULL
#        ifdef ANJ_LOG_MICRO
#            ifndef ANJ_LOG_SOURCE_FILE_ID
#                error "ANJ_LOG_SOURCE_FILE_ID is not defined."
#            endif // ANJ_LOG_SOURCE_FILE_ID

#            define ANJ_LOG_HANDLER_IMPL_MACRO(Module, LogLevel, ...)        \
                anj_log_handler_impl_micro(ANJ_LOG_LEVEL_##LogLevel,         \
                                           ANJ_LOG_SOURCE_FILE_ID, __LINE__, \
                                           __VA_ARGS__)

#        endif // ANJ_LOG_MICRO

#        ifdef ANJ_LOG_ALT_IMPL_HEADER
#            include ANJ_LOG_ALT_IMPL_HEADER
#        endif // ANJ_LOG_ALT_IMPL_HEADER

#        ifdef ANJ_LOG_FILTERING_CONFIG_HEADER
#            include ANJ_LOG_FILTERING_CONFIG_HEADER
#        endif // ANJ_LOG_FILTERING_CONFIG_HEADER

#        ifndef ANJ_LOG_LEVEL_DEFAULT
#            define ANJ_LOG_LEVEL_DEFAULT L_INFO
#        endif // ANJ_LOG_LEVEL_DEFAULT

#        define ANJ_LOG_IF_ALLOWED_LOOKUP_ANJ_LOG_YES(Module, LogLevel, ...) \
            ANJ_LOG_HANDLER_IMPL_MACRO(Module, LogLevel, __VA_ARGS__)
#        define ANJ_LOG_IF_ALLOWED_LOOKUP_ANJ_LOG_NO(Module, LogLevel, ...) \
            ((void) 0)

#        define ANJ_LOG_IF_ALLOWED(Module, LogLevel, ...)                 \
            ANJ_CONCAT(ANJ_LOG_IF_ALLOWED_LOOKUP_,                        \
                       _ANJ_LOG_EMIT_CALL(LogLevel,                       \
                                          _ANJ_LOG_MODULE_LEVEL(Module))) \
            (Module, LogLevel, __VA_ARGS__)
#    else // _ANJ_LOG_ENABLED
#        define ANJ_LOG_IF_ALLOWED(Module, LogLevel, ...) ((void) 0)
#    endif // _ANJ_LOG_ENABLED
/** @endcond */

/**
 * Logs a message.
 *
 * Log statements are a subject to compile-time filtering. The level of this
 * statement must be equal or higher than the configured level of the module, if
 * defined, or the default level ( @ref ANJ_LOG_LEVEL_DEFAULT ).
 *
 * @code
 * anj_log(my_module, L_DEBUG, "Hello %s, %d!", "world", 42);
 * @endcode
 *
 * @param Module Name of the module that generates the message, given as a raw
 *               token.
 *
 * @param LogLevel Log level, specified as a name of @ref anj_log_level_t
 *                 (other than @c L_MUTED) with the leading
 *                 @c ANJ_LOG_LEVEL_ omitted.
 */
#    define anj_log(Module, LogLevel, ...)                          \
        ((void) (ANJ_LOG_IF_ALLOWED(Module, LogLevel, __VA_ARGS__), \
                 ANJ_LOG_COMPILE_TIME_CHECK(__VA_ARGS__)))

/** @cond */
#    ifdef ANJ_LOG_STRIP_CONSTANTS
#        define ANJ_LOG_DISPOSABLE_IMPL(Arg) " "
#    else // ANJ_LOG_STRIP_CONSTANTS
#        define ANJ_LOG_DISPOSABLE_IMPL(Arg) Arg
#    endif // ANJ_LOG_STRIP_CONSTANTS
/** @endcond */

/**
 * Replaces a string constant with <c>" "</c> if @ref ANJ_LOG_STRIP_CONSTANTS
 * is enabled. This is useful for wrapping constant parts of log messages, to
 * shorten them, and therefore reduce the size of the binary.
 *
 * @code
 * anj_log(my_module, L_DEBUG, ANJ_LOG_DISPOSABLE("The result is: ") "%d", 42);
 * @endcode
 *
 * @note Provided string constants shall not contain any format specifiers.
 *
 * @param Arg A string constant to be potentially replaced with <c>" "</c>.
 */
#    define ANJ_LOG_DISPOSABLE(Arg) ANJ_LOG_DISPOSABLE_IMPL(Arg)

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_LOG_LOG_H
