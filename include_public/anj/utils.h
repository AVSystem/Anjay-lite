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
 * @brief Public utility helpers used throughout Anjay Lite.
 *
 * This header provides small generic macros, helpers for working with LwM2M
 * paths, and number/string conversion routines used by the library. It is safe
 * to include from application code.
 */

#ifndef ANJ_UTILS_H
#    define ANJ_UTILS_H

#    include <stddef.h>
#    include <stdint.h>
#    include <stdlib.h>

#    include <anj/defs.h>

/** @cond */
#    define ANJ_INTERNAL_INCLUDE_UTILS
#    include <anj_internal/utils.h>
#    undef ANJ_INTERNAL_INCLUDE_UTILS
/** @endcond */

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * Special “invalid” value used by LwM2M for all identifier kinds: Object IDs,
 * Object Instance IDs, Resource IDs, Resource Instance IDs and Short Server
 * IDs.
 */
#    define ANJ_ID_INVALID UINT16_MAX

/**
 * Returns a pointer to the structure that contains @p member at address @p ptr.
 *
 * @param ptr    Pointer to a structure member.
 * @param type   Type of the outer structure.
 * @param member Member name inside @p type.
 */
#    define ANJ_CONTAINER_OF(ptr, type, member) \
        ((type *) (void *) ((char *) (intptr_t) (ptr) -offsetof(type, member)))

/** Minimum of two values. */
#    define ANJ_MIN(a, b) ((a) < (b) ? (a) : (b))

/** Maximum of two values. */
#    define ANJ_MAX(a, b) ((a) < (b) ? (b) : (a))

/** Number of elements in a C array. */
#    define ANJ_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

/** Turns a token into a string literal without expanding it. */
#    define ANJ_QUOTE(Value) #    Value

/** Turns a token into a string literal after performing macro expansion. */
#    define ANJ_QUOTE_MACRO(Value) ANJ_QUOTE(Value)

/** Assertion helper with a fixed string literal message. */
#    define ANJ_ASSERT(cond, msg) assert((cond) && (bool) "" msg)

/**
 * Marks code that should be unreachable in a correct program flow. Triggers an
 * assertion failure in debug builds.
 */
#    define ANJ_UNREACHABLE(msg) ANJ_ASSERT(0, msg)

/**
 * Concatenates tokens after performing macro-argument expansion.
 * Useful when building identifiers from macro parameters.
 */
#    define ANJ_CONCAT(...)                                              \
        _ANJ_CONCAT_INTERNAL__(                                          \
                _ANJ_CONCAT_INTERNAL__(_ANJ_CONCAT_INTERNAL_,            \
                                       _ANJ_VARARG_LENGTH(__VA_ARGS__)), \
                __)                                                      \
        (__VA_ARGS__)

/**
 * C99-compatible replacement for @c static_assert.
 *
 * Produces a compile-time error if @p condition is false.
 *
 * @param condition Boolean expression evaluated at compile time.
 * @param message   Identifier used in the generated struct name.
 */
#    define ANJ_STATIC_ASSERT(condition, message)    \
        struct ANJ_CONCAT(static_assert_, message) { \
            char message[(condition) ? 1 : -1];      \
        }

/**
 * Constructs an @ref anj_uri_path_t compound literal representing a Resource
 * Instance path: <tt>/Oid/Iid/Rid/Riid</tt>.
 */
#    define ANJ_MAKE_RESOURCE_INSTANCE_PATH(Oid, Iid, Rid, Riid) \
        _ANJ_MAKE_URI_PATH(Oid, Iid, Rid, Riid, 4)

/**
 * Constructs an @ref anj_uri_path_t compound literal representing a Resource
 * path: <tt>/Oid/Iid/Rid</tt>.
 */
#    define ANJ_MAKE_RESOURCE_PATH(Oid, Iid, Rid) \
        _ANJ_MAKE_URI_PATH(Oid, Iid, Rid, ANJ_ID_INVALID, 3)

/**
 * Constructs an @ref anj_uri_path_t compound literal representing an Object
 * Instance path: <tt>/Oid/Iid</tt>.
 */
#    define ANJ_MAKE_INSTANCE_PATH(Oid, Iid) \
        _ANJ_MAKE_URI_PATH(Oid, Iid, ANJ_ID_INVALID, ANJ_ID_INVALID, 2)

/**
 * Constructs an @ref anj_uri_path_t compound literal representing an Object
 * path: <tt>/Oid</tt>.
 */
#    define ANJ_MAKE_OBJECT_PATH(Oid) \
        _ANJ_MAKE_URI_PATH(           \
                Oid, ANJ_ID_INVALID, ANJ_ID_INVALID, ANJ_ID_INVALID, 1)

/**
 * Constructs an @ref anj_uri_path_t compound literal representing the root
 * path.
 */
#    define ANJ_MAKE_ROOT_PATH()           \
        _ANJ_MAKE_URI_PATH(ANJ_ID_INVALID, \
                           ANJ_ID_INVALID, \
                           ANJ_ID_INVALID, \
                           ANJ_ID_INVALID, \
                           0)

/**
 * Compares two LwM2M paths for equality (same length and same components).
 */
static inline bool anj_uri_path_equal(const anj_uri_path_t *left,
                                      const anj_uri_path_t *right) {
    if (left->uri_len != right->uri_len) {
        return false;
    }
    for (size_t i = 0; i < left->uri_len; ++i) {
        if (left->ids[i] != right->ids[i]) {
            return false;
        }
    }
    return true;
}

/** Returns the number of components in @p path. */
static inline size_t anj_uri_path_length(const anj_uri_path_t *path) {
    return path->uri_len;
}

/**
 * Checks whether @p path contains the component identified by @p id_type.
 * For example, <tt>/Oid/Iid</tt> “has” @ref ANJ_ID_OID and @ref ANJ_ID_IID.
 */
static inline bool anj_uri_path_has(const anj_uri_path_t *path,
                                    anj_id_type_t id_type) {
    return path->uri_len > id_type;
}

/**
 * Checks whether @p path is exactly of the level identified by @p id_type.
 * For example, <tt>/Oid/Iid</tt> “is” @ref ANJ_ID_IID.
 */
static inline bool anj_uri_path_is(const anj_uri_path_t *path,
                                   anj_id_type_t id_type) {
    return path->uri_len == (size_t) id_type + 1u;
}

/**
 * Checks whether @p path lies outside of the subtree rooted at @p base.
 * Returns true if @p path is shorter than @p base or diverges at any component.
 */
static inline bool anj_uri_path_outside_base(const anj_uri_path_t *path,
                                             const anj_uri_path_t *base) {
    if (path->uri_len < base->uri_len) {
        return true;
    }
    for (size_t i = 0; i < base->uri_len; ++i) {
        if (path->ids[i] != base->ids[i]) {
            return true;
        }
    }
    return false;
}

/**
 * Checks that @p current_path is lexicographically greater than
 * @p previous_path.
 */
bool anj_uri_path_increasing(const anj_uri_path_t *previous_path,
                             const anj_uri_path_t *current_path);

/**
 * Determine a Block-Wise transfer buffer size that satisfies RFC 7959
 * constraints (power of two in the range 16-1024), not exceeding @p buff_size.
 *
 * If @p buff_size is smaller than 16, returns 0.
 *
 * @param buff_size Available buffer size in bytes.
 * @return Selected block size in bytes.
 */
uint16_t anj_determine_block_buffer_size(size_t buff_size);

/**
 * Convert a @c uint16_t to a decimal string (without a terminating nullchar).
 * The buffer must be at least @ref ANJ_U16_STR_MAX_LEN bytes long.
 *
 * @return Number of bytes written.
 */
size_t anj_uint16_to_string_value(char *out_buff, uint16_t value);

/**
 * Convert a @c uint32_t to a decimal string (without a terminating nullchar).
 * The buffer must be at least @ref ANJ_U32_STR_MAX_LEN bytes long.
 *
 * @return Number of bytes written.
 */
size_t anj_uint32_to_string_value(char *out_buff, uint32_t value);

/**
 * Convert a @c uint64_t to a decimal string (without a terminating nullchar).
 * The buffer must be at least @ref ANJ_U64_STR_MAX_LEN bytes long.
 *
 * @return Number of bytes written.
 */
size_t anj_uint64_to_string_value(char *out_buff, uint64_t value);

/**
 * Convert an @c int64_t to a decimal string (without a terminating nullchar).
 * The buffer must be at least @ref ANJ_I64_STR_MAX_LEN bytes long.
 *
 * @return Number of bytes written.
 */
size_t anj_int64_to_string_value(char *out_buff, int64_t value);

/**
 * Convert a @c double to a string (without a terminating NUL).
 *
 * This function is used to encode LwM2M attribute values whose textual format
 * is defined as <tt>1*DIGIT ["." 1*DIGIT]</tt>. For very large/small magnitudes
 * (>|UINT64_MAX|, <1e-10) an exponential notation may be used. @c NaN and
 * infinities are emitted as <tt>"nan"</tt> and <tt>"inf"</tt>.
 *
 * If @ref ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS is enabled, a lightweight
 * formatter is used (may incur rounding error at extreme magnitudes).
 *
 * The buffer must be at least @ref ANJ_DOUBLE_STR_MAX_LEN bytes long.
 *
 * @return Number of bytes written.
 */
size_t anj_double_to_string_value(char *out_buff, double value);

/**
 * Parse a decimal string into @c uint32_t.
 *
 * @note The input string is not required to be null-terminated.
 *       The caller must explicitly provide the length in @p buff_len
 *       (e.g., using @c strlen() if the content is null-terminated).
 *
 * @param[out] out_val  Parsed value.
 * @param      buff     Input buffer.
 * @param      buff_len Input length.
 *
 * @return 0 on success, -1 on error (empty input, non-digits present,
 *         overflow, or excessive length).
 */
int anj_string_to_uint32_value(uint32_t *out_val,
                               const char *buff,
                               size_t buff_len);

/**
 * Parse a decimal string into @c uint64_t.
 *
 * @note The input string is not required to be null-terminated.
 *       The caller must explicitly provide the length in @p buff_len
 *       (e.g., using @c strlen() if the content is null-terminated).
 *
 * @param[out] out_val  Parsed value.
 * @param      buff     Input buffer.
 * @param      buff_len Input length.
 *
 * @return 0 on success, -1 on error (empty input, non-digits present,
 *         overflow, or excessive length).
 */
int anj_string_to_uint64_value(uint64_t *out_val,
                               const char *buff,
                               size_t buff_len);

/**
 * Parse a decimal string into @c int64_t (optional leading sign allowed).
 *
 * @note The input string is not required to be null-terminated.
 *       The caller must explicitly provide the length in @p buff_len
 *       (e.g., using @c strlen() if the content is null-terminated).
 *
 * @return 0 on success, -1 on error (empty input, invalid characters,
 *         or value outside @c INT64_MIN..@c INT64_MAX).
 *
 *
 * @param[out] out_val  Parsed value.
 * @param      buff     Input buffer.
 * @param      buff_len Input length.
 *
 * @return 0 on success, -1 on error (empty input, non-digits present (except
 *         leading plus/minus sign), overflow, or excessive length).
 */
int anj_string_to_int64_value(int64_t *out_val,
                              const char *buff,
                              size_t buff_len);

/**
 * Parse an Objlnk string (e.g., <tt>"3:0"</tt>) into @ref anj_objlnk_value_t.
 *
 * @note Unlike the numeric parsers, this function expects a standard
 *       null-terminated string.
 *
 * @param[out] out    Output structure.
 * @param      objlnk null-terminated input string.
 *
 * @return 0 on success, -1 if the input is not a valid Objlnk.
 */
int anj_string_to_objlnk_value(anj_objlnk_value_t *out, const char *objlnk);

/**
 * Parse a decimal string into @c double (no @c inf / @c nan support,
 * consistent with LwM2M attribute representation).
 *
 * @note The input string is not required to be null-terminated.
 *       The caller must explicitly provide the length in @p buff_len
 *       (e.g., using @c strlen() if the content is null-terminated).
 *
 * @return 0 on success, -1 on error (empty input, invalid characters, etc.).
 */
int anj_string_to_double_value(double *out_val,
                               const char *buff,
                               size_t buff_len);

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_UTILS_H
