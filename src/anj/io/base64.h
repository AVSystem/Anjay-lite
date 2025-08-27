/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_IO_BASE64_H
#    define SRC_ANJ_IO_BASE64_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    ifdef ANJ_WITH_PLAINTEXT

/**
 * Array of characters for standard base64 encoder, i.e.
 * ANJ_BASE64_CHARS[0] through ANJ_BASE64_CHARS[63] are the characters
 * that encode corresponding numbers.
 */
extern const char ANJ_BASE64_CHARS[];

/**
 * Array of characters for alternate base64 alphabet, as defined by RFC 4648
 * Section 5.
 */
extern const char ANJ_BASE64_URL_SAFE_CHARS[];

/**
 * Base64-style encoding configuration structure.
 */
typedef struct {
    /**
     * Pointer to an array of 64 characters, specifying the characters to use as
     * base64 digits.
     */
    const char *alphabet;

    /**
     * Character to use as final padding, or '\0' if no padding is used.
     */
    char padding_char;

    /**
     * True if whitespace characters are to be allowed between digits when
     * decoding. If false, any whitespace character will cause a decoding error.
     */
    bool allow_whitespace;

    /**
     * On decoding, if <c>padding_char</c> is not '\0' and this flag is true,
     * the input (stripped of any whitespace characters if
     * <c>allow_whitespace</c> is true) is required to have a length that is a
     * multiple of four, and end with appropriate number of padding characters.
     *
     * Otherwise, the padding character is ignored. Note that the value of this
     * flag has no effect if @p padding_char is '\0'.
     */
    bool require_padding;

    /**
     * Controls whether the encoded string includes a null-termination byte.
     *
     * When set to false (the default), a null byte is appended to the end of
     * the string. When set to true, the null byte is omitted.
     */
    bool without_null_termination;
} anj_base64_config_t;

/**
 * Base64 configuration with the default alphabet (@ref ANJ_BASE64_CHARS),
 * default padding character (<c>=</c>), <c>allow_whitespace</c> set to true and
 * <c>require_padding</c> set to false.
 */
extern const anj_base64_config_t ANJ_BASE64_DEFAULT_LOOSE_CONFIG;

/**
 * Base64 configuration with the default alphabet (@ref ANJ_BASE64_CHARS),
 * default padding character (<c>=</c>), <c>allow_whitespace</c> set to false
 * and <c>require_padding</c> set to true.
 */
extern const anj_base64_config_t ANJ_BASE64_DEFAULT_STRICT_CONFIG;

/**
 * Returns the number of bytes required to store input encoded in base64. Allows
 * to set configuration of the base64 encoding. Options impacting returned size
 * are padding_char and without_null_termination in @ref anj_base64_config_t
 * @p config.
 *
 * @param input_length Length of input in bytes.
 * @param config       Configuration of the base64 variant to use.
 *
 * @returns length of base64 encoded input of length @p input_length.
 */
size_t anj_base64_encoded_size_custom(size_t input_length,
                                      anj_base64_config_t config);

/**
 * Returns the number of bytes required to store input encoded in base64 if
 * padding is used.
 *
 * @param input_length Length of input in bytes.
 *
 * @returns length of base64 encoded input of length @p input_length.
 */
static inline size_t anj_base64_encoded_size(size_t input_length) {
    return anj_base64_encoded_size_custom(input_length,
                                          ANJ_BASE64_DEFAULT_STRICT_CONFIG);
}

/**
 * Returns the number of bytes required to store input encoded in base64 if
 * padding is NOT used.
 *
 * @param input_length Length of input in bytes.
 *
 * @returns length of base64 encoded input of length @p input_length.
 */
static inline size_t
anj_base64_encoded_size_without_padding(size_t input_length) {
    return anj_base64_encoded_size_custom(input_length,
                                          ANJ_BASE64_DEFAULT_LOOSE_CONFIG);
}

/**
 * Returns amount of bytes that would be sufficient to store input decoded from
 * base64.
 *
 * Warning: this function computes just a rough estimate of amount of bytes that
 * are actually necessary by taking 3 * (input_length / 4) as a approximation.
 * Therefore the returned value is always an upper bound.
 *
 * @param input_length  Length of base64 encoded input.
 *
 * @returns estimate of the length of buffer required to store decoded input.
 */
size_t anj_base64_estimate_decoded_size(size_t input_length);

/**
 * Encodes specified input into a custom variant of base64.
 *
 * Note: this function fails if @p out_length is too small to encode @p input,
 * to predict buffer requirements use @ref anj_base64_encoded_size .
 *
 * @param out          Pointer to user-allocated array where encoded data will
 *                     be written.
 * @param out_length   Length of user-allocated array.
 * @param input        Input to encode.
 * @param input_length Length of the input.
 * @param config       Configuration of the base64 variant to use.
 *
 * @returns 0 on success, negative value in case of error.
 */
int anj_base64_encode_custom(char *out,
                             size_t out_length,
                             const uint8_t *input,
                             size_t input_length,
                             anj_base64_config_t config);

/**
 * Encodes specified input into base64.
 *
 * Note: this function fails if @p out_length is too small to encode @p input,
 * to predict buffer requirements use @ref anj_base64_encoded_size .
 *
 * @param out           Pointer to user-allocated array where encoded data will
 *                      be written.
 * @param out_length    Length of user-allocated array.
 * @param input         Input to encode.
 * @param input_length  Length of the input.
 *
 * @returns 0 on success, negative value in case of error.
 */
static inline int anj_base64_encode(char *out,
                                    size_t out_length,
                                    const uint8_t *input,
                                    size_t input_length) {
    return anj_base64_encode_custom(out, out_length, input, input_length,
                                    ANJ_BASE64_DEFAULT_STRICT_CONFIG);
}

/**
 * Decodes specified input in a custom base64 format.
 *
 * Note that this function fails if @p out_length is too small. To predict
 * buffer requirements use @ref anj_base64_estimate_decoded_size .
 *
 * @param out_bytes_decoded Pointer to a variable that, on successful exit, will
 *                          be set to the number of decoded bytes. May be NULL
 *                          if not needed.
 * @param out               Pointer to user-allocated array where decoded data
 *                          will be stored.
 * @param out_length        Length of user-allocated array.
 * @param input             Null terminated input to decode.
 * @param config            Configuration of the base64 variant to use.
 *
 * @returns 0 on success, negative value in case of error.
 */
int anj_base64_decode_custom(size_t *out_bytes_decoded,
                             uint8_t *out,
                             size_t out_length,
                             const char *input,
                             anj_base64_config_t config);

/**
 * Decodes specified base64 input.
 *
 * Note:
 * 1. It does not accept inputs with whitespace characters of any kind.
 * 2. It does not accept inputs with with superflous padding characters.
 * 3. It does not accept inputs that are not padded properly.
 * 4. As a consequence it does not accepts inputs whose length is not a multiple
 *    of four.
 *
 * Moreover, this function fails if @p out_length is too small. To predict
 * buffer requirements use @ref anj_base64_estimate_decoded_size (which, for
 * inputs accepted by this function will return the exact amount of bytes
 * needed).
 *
 * @param out_bytes_decoded Pointer to a variable that, on successful exit, will
 *                          be set to the number of decoded bytes. May be NULL
 *                          if not needed.
 * @param out               Pointer to user-allocated array where decoded data
 *                          will be stored.
 * @param out_length        Length of user-allocated array.
 * @param input             Null terminated input to decode.
 *
 * @returns 0 on success, negative value in case of error.
 */
static inline int anj_base64_decode_strict(size_t *out_bytes_decoded,
                                           uint8_t *out,
                                           size_t out_length,
                                           const char *input) {
    return anj_base64_decode_custom(out_bytes_decoded, out, out_length, input,
                                    ANJ_BASE64_DEFAULT_STRICT_CONFIG);
}

/**
 * Does the same as @ref anj_base64_decode_strict except that it ignores
 * superflous whitespaces and padding characters.
 *
 * Note that this function fails if @p out_length is too small. To predict
 * buffer requirements use @ref anj_base64_estimate_decoded_size .
 *
 * @param out_bytes_decoded Pointer to a variable that, on successful exit, will
 *                          be set to the number of decoded bytes. May be NULL
 *                          if not needed.
 * @param out               Pointer to user-allocated array where decoded data
 *                          will be stored.
 * @param out_length        Length of user-allocated array.
 * @param input             Null terminated input to decode.
 *
 * @returns 0 on success, negative value in case of error.
 */
static inline int anj_base64_decode(size_t *out_bytes_decoded,
                                    uint8_t *out,
                                    size_t out_length,
                                    const char *input) {
    return anj_base64_decode_custom(out_bytes_decoded, out, out_length, input,
                                    ANJ_BASE64_DEFAULT_LOOSE_CONFIG);
}

#    endif // ANJ_WITH_PLAINTEXT

#endif /* SRC_ANJ_IO_BASE64_H */
