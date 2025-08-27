/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
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
#    include <anj/utils.h>

#    include "coap/coap.h"

/**
 * Returns a pseudo-random integer from range [0, UINT32_MAX].
 */
uint32_t _anj_rand32_r(_anj_rand_seed_t *seed);

/**
 * Returns a pseudo-random integer from range [0, UINT64_MAX].
 */
static inline uint64_t _anj_rand64_r(_anj_rand_seed_t *seed) {
    return (((uint64_t) _anj_rand32_r(seed)) << 32)
           | ((uint64_t) _anj_rand32_r(seed));
}

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

#endif // SRC_ANJ_UTILS_H
