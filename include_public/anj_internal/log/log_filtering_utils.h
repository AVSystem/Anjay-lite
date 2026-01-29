/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_INCLUDE_LOG_FILTERING_UTILS
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_LOG_FILTERING_UTILS

#ifdef __cplusplus
extern "C" {
#endif

/** Below defines are @anj_internal_api_do_not_use */
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_ERRORL_TRACE ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_ERRORL_DEBUG ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_ERRORL_INFO ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_ERRORL_WARNING ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_ERRORL_ERROR ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_ERRORL_MUTED ANJ_LOG_NO

#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_WARNINGL_TRACE ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_WARNINGL_DEBUG ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_WARNINGL_INFO ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_WARNINGL_WARNING ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_WARNINGL_ERROR ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_WARNINGL_MUTED ANJ_LOG_NO

#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_INFOL_TRACE ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_INFOL_DEBUG ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_INFOL_INFO ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_INFOL_WARNING ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_INFOL_ERROR ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_INFOL_MUTED ANJ_LOG_NO

#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_DEBUGL_TRACE ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_DEBUGL_DEBUG ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_DEBUGL_INFO ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_DEBUGL_WARNING ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_DEBUGL_ERROR ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_DEBUGL_MUTED ANJ_LOG_NO

#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_TRACEL_TRACE ANJ_LOG_YES
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_TRACEL_DEBUG ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_TRACEL_INFO ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_TRACEL_WARNING ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_TRACEL_ERROR ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_TRACEL_MUTED ANJ_LOG_NO

#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_MUTEDL_TRACE ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_MUTEDL_DEBUG ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_MUTEDL_INFO ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_MUTEDL_WARNING ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_MUTEDL_ERROR ANJ_LOG_NO
#define _ANJ_LOG_EMIT_CALL_LOOKUP_L_MUTEDL_MUTED ANJ_LOG_NO

/**
 * @anj_internal_api_do_not_use
 * Expands to <c>ANJ_LOG_YES</c> or <c>ANJ_LOG_NO</c>, depending on whether log
 * statement of level @p LogLevel shall be logged if the module is set to
 * @p ModuleLevel .
 *
 * The result helps with picking the appropriate macro to expand to for a single
 * log statement.
 */
#define _ANJ_LOG_EMIT_CALL(LogLevel, ModuleLevel) \
    ANJ_CONCAT(_ANJ_LOG_EMIT_CALL_LOOKUP_, LogLevel, ModuleLevel)

#define _ANJ_LOG_PREPEND_DUMMY_IF_LEVEL_LOOKUP_L_ERROR ANJ_LOG_DUMMY, L_ERROR
#define _ANJ_LOG_PREPEND_DUMMY_IF_LEVEL_LOOKUP_L_WARNING \
    ANJ_LOG_DUMMY, L_WARNING
#define _ANJ_LOG_PREPEND_DUMMY_IF_LEVEL_LOOKUP_L_INFO ANJ_LOG_DUMMY, L_INFO
#define _ANJ_LOG_PREPEND_DUMMY_IF_LEVEL_LOOKUP_L_DEBUG ANJ_LOG_DUMMY, L_DEBUG
#define _ANJ_LOG_PREPEND_DUMMY_IF_LEVEL_LOOKUP_L_TRACE ANJ_LOG_DUMMY, L_TRACE
#define _ANJ_LOG_PREPEND_DUMMY_IF_LEVEL_LOOKUP_L_MUTED ANJ_LOG_DUMMY, L_MUTED

#define _ANJ_LOG_PREPEND_DUMMY_IF_LEVEL(LevelOrGarbage) \
    ANJ_CONCAT(_ANJ_LOG_PREPEND_DUMMY_IF_LEVEL_LOOKUP_, LevelOrGarbage)

#define _ANJ_LOG_MODULE_LEVEL_OR_GARBAGE(Module) \
    ANJ_CONCAT(ANJ_LOG_LEVEL_FOR_MODULE_, Module)

#define _ANJ_LOG_EXTRACT_2ND(Arg1, Arg2, ...) Arg2

/**
 * @anj_internal_api_do_not_use
 * Extracts the second argument of argument list. Arguments are expanded before
 * extraction, so if the first argument expands to multiple items, the second
 * item the first argument expands to will be returned instead of the second
 * argument provided to the call.
 */
#define _ANJ_LOG_EXTRACT_2ND_MACRO(...) \
    _ANJ_LOG_EXTRACT_2ND(__VA_ARGS__, ANJ_LOG_DUMMY)

/**
 * @anj_internal_api_do_not_use
 * Returns the logging level of a module if defined, otherwise
 * @ref ANJ_LOG_LEVEL_DEFAULT .
 *
 * NOTE: the mechanism that falls back to the default level, if the appropriate
 * macro to define module's logging level is defined, is implemented as follows:
 * - the macro attempts to expand concatenation of
 *   <c>ANJ_LOG_LEVEL_FOR_MODULE_</c> and module name, which is the expected
 *   macro name that should be defined by the user to override the level of a
 *   module
 * - if the concatenation expands to a valid logging level, a dummy is appended
 *   before the level expanded to
 * - result of expansion attept, followed by @ref ANJ_LOG_LEVEL_DEFAULT , is
 *   passed to @ref ANJ_LOG_EXTRACT_2ND_MACRO - this effectively extracts the
 *   level for a module if it's defined because of the prepended dummy,
 *   otherwise the first argument is a single (garbage) argument, therefore
 *   @ref ANJ_LOG_LEVEL_DEFAULT will be the extracted value.
 */
#define _ANJ_LOG_MODULE_LEVEL(Module)                          \
    _ANJ_LOG_EXTRACT_2ND_MACRO(                                \
            _ANJ_LOG_PREPEND_DUMMY_IF_LEVEL(                   \
                    _ANJ_LOG_MODULE_LEVEL_OR_GARBAGE(Module)), \
            ANJ_LOG_LEVEL_DEFAULT)

#ifdef __cplusplus
}
#endif
