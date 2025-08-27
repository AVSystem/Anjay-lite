/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_UTILS_H
#    define ANJ_UTILS_H

#    include <stddef.h>
#    include <stdint.h>
#    include <stdlib.h>

#    include <anj/defs.h>

#    define ANJ_INTERNAL_INCLUDE_UTILS
#    include <anj_internal/utils.h>
#    undef ANJ_INTERNAL_INCLUDE_UTILS

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * Value reserved by the LwM2M spec for all kinds of IDs (Object IDs, Object
 * Instance IDs, Resource IDs, Resource Instance IDs, Short Server IDs).
 */
#    define ANJ_ID_INVALID UINT16_MAX

#    define ANJ_CONTAINER_OF(ptr, type, member) \
        ((type *) (void *) ((char *) (intptr_t) (ptr) -offsetof(type, member)))
#    define ANJ_MIN(a, b) ((a) < (b) ? (a) : (b))
#    define ANJ_MAX(a, b) ((a) < (b) ? (b) : (a))
#    define ANJ_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

/**
 * Stringifies a token.
 */
#    define ANJ_QUOTE(Value) #    Value

/**
 * Stringifies a token with performing additional macro expansion step.
 *
 * @param Value Token to stringify.
 */
#    define ANJ_QUOTE_MACRO(Value) ANJ_QUOTE(Value)

/*
 * Definition of an assert with hard-coded string literal message that does not
 * trigger compiler warnings.
 */
#    define ANJ_ASSERT(cond, msg) assert((cond) && (bool) "" msg)

/*
 * Marks an execution path as breaking some invariants, which should never
 * happen in correct code.
 */
#    define ANJ_UNREACHABLE(msg) ANJ_ASSERT(0, msg)

/**@{*/
/**
 * Concatenates tokens passed as arguments. Can be used to do macro expansion
 * before standard C preprocessor concatenation.
 */
#    define ANJ_CONCAT(...)                                              \
        _ANJ_CONCAT_INTERNAL__(                                          \
                _ANJ_CONCAT_INTERNAL__(_ANJ_CONCAT_INTERNAL_,            \
                                       _ANJ_VARARG_LENGTH(__VA_ARGS__)), \
                __)                                                      \
        (__VA_ARGS__)
/**@}*/

/**
 * C89-compliant replacement for <c>static_assert</c>.
 */
#    define ANJ_STATIC_ASSERT(condition, message)    \
        struct ANJ_CONCAT(static_assert_, message) { \
            char message[(condition) ? 1 : -1];      \
        }

/**
 * Below group of ANJ_MAKE_*_PATH macros are used to construct in place
 * an unnamed structure (compound literal) of an anj_uri_path_t type.
 */
#    define ANJ_MAKE_RESOURCE_INSTANCE_PATH(Oid, Iid, Rid, Riid) \
        _ANJ_MAKE_URI_PATH(Oid, Iid, Rid, Riid, 4)

#    define ANJ_MAKE_RESOURCE_PATH(Oid, Iid, Rid) \
        _ANJ_MAKE_URI_PATH(Oid, Iid, Rid, ANJ_ID_INVALID, 3)

#    define ANJ_MAKE_INSTANCE_PATH(Oid, Iid) \
        _ANJ_MAKE_URI_PATH(Oid, Iid, ANJ_ID_INVALID, ANJ_ID_INVALID, 2)

#    define ANJ_MAKE_OBJECT_PATH(Oid) \
        _ANJ_MAKE_URI_PATH(           \
                Oid, ANJ_ID_INVALID, ANJ_ID_INVALID, ANJ_ID_INVALID, 1)

#    define ANJ_MAKE_ROOT_PATH()           \
        _ANJ_MAKE_URI_PATH(ANJ_ID_INVALID, \
                           ANJ_ID_INVALID, \
                           ANJ_ID_INVALID, \
                           ANJ_ID_INVALID, \
                           0)

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

static inline size_t anj_uri_path_length(const anj_uri_path_t *path) {
    return path->uri_len;
}

static inline bool anj_uri_path_has(const anj_uri_path_t *path,
                                    anj_id_type_t id_type) {
    return path->uri_len > id_type;
}

static inline bool anj_uri_path_is(const anj_uri_path_t *path,
                                   anj_id_type_t id_type) {
    return path->uri_len == (size_t) id_type + 1u;
}

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

bool anj_uri_path_increasing(const anj_uri_path_t *previous_path,
                             const anj_uri_path_t *current_path);

/**
 * Determines the size of the buffer consistent with the requirements of
 * Block-Wise transfers - power of two and range from <c>16</c> to <c>1024</c>.
 * The calculated size will always be equal to or less than @p buff_size. If the
 * @p buff_size is less than <c>16</c>, the function will return 0.
 *
 * @param buff_size Size of the buffer.
 *
 * @returns Block-Wise buffer size.
 */
uint16_t anj_determine_block_buffer_size(size_t buff_size);

/**
 * Converts uint16_t value to string and copies it to @p out_buff (without the
 * terminating nullbyte). The minimum required @p out_buff size is @ref
 * ANJ_U16_STR_MAX_LEN.
 *
 * @param[out] out_buff Output buffer.
 * @param      value    Input value.

 *
 * @return Number of bytes written.
 */
size_t anj_uint16_to_string_value(char *out_buff, uint16_t value);

/**
 * Converts uint32_t value to string and copies it to @p out_buff (without the
 * terminating nullbyte). The minimum required @p out_buff size is @ref
 * ANJ_U32_STR_MAX_LEN.
 *
 * @param[out] out_buff Output buffer.
 * @param      value    Input value.
 *
 * @return Number of bytes written.
 */
size_t anj_uint32_to_string_value(char *out_buff, uint32_t value);

/**
 * Converts uint64_t value to string and copies it to @p out_buff (without the
 * terminating nullbyte). The minimum required @p out_buff size is @ref
 * ANJ_U64_STR_MAX_LEN.
 *
 * @param[out] out_buff Output buffer.
 * @param      value    Input value.
 *
 * @return Number of bytes written.
 */
size_t anj_uint64_to_string_value(char *out_buff, uint64_t value);

/**
 * Converts int64_t value to string and copies it to @p out_buff (without the
 * terminating nullbyte). The minimum required @p out_buff size is @ref
 * ANJ_I64_STR_MAX_LEN.
 *
 * @param[out] out_buff Output buffer.
 * @param      value    Input value.
 *
 * @return Number of bytes written.
 */
size_t anj_int64_to_string_value(char *out_buff, int64_t value);

/**
 * Converts double value to string and copies it to @p out_buff (without the
 * terminating nullbyte). The minimum required @p out_buff size is @ref
 * ANJ_DOUBLE_STR_MAX_LEN.
 *
 * IMPORTANT: This function is used to encode LwM2M attributes whose
 * float/double format is defined by LwM2M Specification: 1*DIGIT ["."1*DIGIT].
 * However for absolute values greater than UINT64_MAX and less than
 * <c>1e-10</c> (or <c>1e-5</c> in case of using sprintf) exponential notation
 * is used. Since the specification does not define the format for the value of
 * NaN and infinite, so in this case "nan" and "inf" will be set.
 *
 * IMPORTANT: This function doesn't use sprintf() if @ref
 * ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS is defined and is intended to be
 * lightweight. For very large and very small numbers, a rounding error may
 * occur.
 *
 * @param[out] out_buff   Output buffer.
 * @param      value      Input value.
 *
 * @return Number of bytes written.
 */
size_t anj_double_to_string_value(char *out_buff, double value);

/**
 * Converts string representation of numerical value to uint32_t value.
 *
 * @param[out] out_val   Output value.
 * @param      buff      Input buffer.
 * @param      buff_len  Input buffer length.
 *
 * @return 0 in case of success and -1 in case of:
 *  - @p buff_len is equal to 0
 *  - there are characters in the @p buff that are not digits
 *  - string represented numerical value exceeds UINT32_MAX
 *  - string is too long
 */
int anj_string_to_uint32_value(uint32_t *out_val,
                               const char *buff,
                               size_t buff_len);

/**
 * Converts string representation of numerical value to uint64_t value.
 *
 * @param[out] out_val   Output value.
 * @param      buff      Input buffer.
 * @param      buff_len  Input buffer length.
 *
 * @return 0 in case of success and -1 in case of:
 *  - @p buff_len is equal to 0
 *  - there are characters in the @p buff that are not digits
 *  - string represented numerical value exceeds UINT64_MAX
 *  - string is too long
 */
int anj_string_to_uint64_value(uint64_t *out_val,
                               const char *buff,
                               size_t buff_len);

/**
 * Converts string representation of numerical value to int64_t value.
 *
 * @param[out] out_val   Output value.
 * @param      buff      Input buffer.
 * @param      buff_len  Input buffer length.
 *
 * @return 0 in case of success and -1 in case of:
 *  - @p buff_len is equal to 0
 *  - there are characters in the @p buff that are not digits
 *  - string represented numerical value exceeds INT64_MAX or is less than
 * INT64_MIN
 *  - string is too long
 */
int anj_string_to_int64_value(int64_t *out_val,
                              const char *buff,
                              size_t buff_len);

/**
 * Converts string representation of an LwM2M Objlnk value to
 * a <c>anj_objlnk_value_t</c> strucure.
 *
 * @param out    Structure to store the parsed value in.
 * @param objlnk Null-terminated string.
 *
 * @return 0 in case of success or -1 if the input was not a valid Objlnk
 * string.
 */
int anj_string_to_objlnk_value(anj_objlnk_value_t *out, const char *objlnk);

/**
 * Converts string representation of numerical value to double value. Does not
 * support infinitive and NAN values (LwM2M attributes representation doesn't
 * allow for this).
 *
 * @param[out] out_val   Output value.
 * @param      buff      Input buffer.
 * @param      buff_len  Input buffer length.
 *
 * @return 0 in case of success and -1 if @p buff_len is equal to 0 or there are
 * characters in the @p buff that are not digits (exceptions shown above).
 */
int anj_string_to_double_value(double *out_val,
                               const char *buff,
                               size_t buff_len);

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_UTILS_H
