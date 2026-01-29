/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_UTILS_H
#    define SRC_ANJ_UTILS_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/defs.h>

#    include "coap/coap.h"

#    define _ANJ_CBOR_VAL_OR_LEN_LEN_IMPL(Val_or_len)   \
        ((Val_or_len) <= 23                             \
                 ? 1                                    \
                 : (Val_or_len) <= UINT8_MAX            \
                           ? 2                          \
                           : (Val_or_len) <= UINT16_MAX \
                                     ? 3                \
                                     : (Val_or_len) <= UINT32_MAX ? 5 : 9)
#    define _ANJ_CBOR_VAL_OR_LEN_LEN_AVOID_ZERO(Val_or_len) \
        ((Val_or_len) == 0 ? 1 : (Val_or_len))
/**
 * HACK: Invoking this macro with 0 leads to compile-time comparisons of
 * unsigned 0 with other compile-time unsigned integer, which in some versions
 * of gcc yields a warning about the comparison being always true/false. To
 * avoid comparisons with 0 directly, 0 is replaced with 1 before comparison;
 * this yields the same results and avoids the warning.
 */
#    define _ANJ_CBOR_VAL_OR_LEN_LEN(Val_or_len) \
        _ANJ_CBOR_VAL_OR_LEN_LEN_IMPL(           \
                _ANJ_CBOR_VAL_OR_LEN_LEN_AVOID_ZERO(Val_or_len))

/**
 * Check if path points to Security or OSCORE object.
 *
 * @param path  Path that will be verified.
 *
 * @return true if path points to Security or OSCORE object.
 */
bool _anj_uri_path_to_security_or_oscore_obj(const anj_uri_path_t *path);

/**
 * Compares two tokens.
 *
 * @param left  First token.
 * @param right Second token.
 *
 * @returns true if tokens are equal, false otherwise.
 */
bool _anj_tokens_equal(const _anj_coap_token_t *left,
                       const _anj_coap_token_t *right);

/**
 * Validates the version of the object - accepted format is X.Y where X and Y
 * are digits.
 *
 * @param version  Object version.
 *
 * @returns 0 on success or @ref _ANJ_IO_ERR_INPUT_ARG value in case of
 * incorrect format.
 */
int _anj_validate_obj_version(const char *version);

/**
 * Convert a 16-bit integer between native byte order and big-endian byte order.
 *
 * Note that it is a symmetric operation, so the same function may be used for
 * conversion in either way. If the host architecture is natively big-endian,
 * this function is a no-op.
 */
uint16_t _anj_convert_be16(uint16_t value);

/**
 * Convert a 32-bit integer between native byte order and big-endian byte order.
 *
 * Note that it is a symmetric operation, so the same function may be used for
 * conversion in either way. If the host architecture is natively big-endian,
 * this function is a no-op.
 */
uint32_t _anj_convert_be32(uint32_t value);

/**
 * Convert a 64-bit integer between native byte order and big-endian byte order.
 *
 * Note that it is a symmetric operation, so the same function may be used for
 * conversion in either way. If the host architecture is natively big-endian,
 * this function is a no-op.
 */
uint64_t _anj_convert_be64(uint64_t value);

/**
 * Convert a 32-bit floating-point value into a big-endian order value
 * type-punned as an integer.
 *
 * Inverse to @ref _anj_ntohf
 */
uint32_t _anj_htonf(float f);

/**
 * Convert a 64-bit floating-point value into a big-endian order value
 * type-punned as an integer.
 *
 * Inverse to @ref _anj_ntohd
 */
uint64_t _anj_htond(double d);

/**
 * Convert a 32-bit floating-point value type-punned as an integer in big-endian
 * order into a native value.
 *
 * Inverse to @ref _anj_htonf
 */
float _anj_ntohf(uint32_t v);

/**
 * Convert a 64-bit floating-point value type-punned as an integer in big-endian
 * order into a native value.
 *
 * Inverse to @ref _anj_htond
 */
double _anj_ntohd(uint64_t v);

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
uint16_t _anj_determine_block_buffer_size(size_t buff_size);

/**
 * Returns @c true if @p value is losslessly convertible to int64_t and
 * @c false otherwise.
 */
bool _anj_double_convertible_to_int64(double value);

/**
 * Returns @c true if @p value is losslessly convertible to uint64_t and
 * @c false otherwise.
 */
bool _anj_double_convertible_to_uint64(double value);

/** Tests whether @p value is a power of two */
static inline bool _anj_is_power_of_2(size_t value) {
    return value > 0 && !(value & (value - 1));
}

char *_anj_coap_code_format(char (*buff)[5], uint32_t code);

#    define COAP_CODE_FORMAT(Code) _anj_coap_code_format(&(char[5]){ "" }, Code)

#endif // SRC_ANJ_UTILS_H
