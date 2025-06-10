/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/utils.h>

#include "../utils.h"
#include "base64.h"

#ifdef ANJ_WITH_PLAINTEXT

const char ANJ_BASE64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz"
                                "0123456789+/";

const char ANJ_BASE64_URL_SAFE_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "0123456789-_";

const anj_base64_config_t ANJ_BASE64_DEFAULT_LOOSE_CONFIG = {
    .alphabet = ANJ_BASE64_CHARS,
    .padding_char = '=',
    .allow_whitespace = true,
    .require_padding = false,
    .without_null_termination = false
};

const anj_base64_config_t ANJ_BASE64_DEFAULT_STRICT_CONFIG = {
    .alphabet = ANJ_BASE64_CHARS,
    .padding_char = '=',
    .allow_whitespace = false,
    .require_padding = true,
    .without_null_termination = false
};

ANJ_STATIC_ASSERT(sizeof(ANJ_BASE64_CHARS) == 65, // 64 chars + NULL terminator
                  missing_base64_chars);

size_t anj_base64_encoded_size_custom(size_t input_length,
                                      anj_base64_config_t config) {
    size_t needed_size = (input_length / 3) * 4;

    size_t rest = input_length % 3;
    if (rest) {
        needed_size += !!config.padding_char ? 4 : rest + 1;
    }

    if (!config.without_null_termination) {
        needed_size += 1; /* NULL terminator */
    }
    return needed_size;
}

size_t anj_base64_estimate_decoded_size(size_t input_length) {
    return 3 * ((input_length + 3) / 4);
}

int anj_base64_encode_custom(char *out,
                             size_t out_length,
                             const uint8_t *input,
                             size_t input_length,
                             anj_base64_config_t config) {
    char *const out_begin = (char *) out;
    uint8_t num;
    size_t i;
    unsigned long sh = 0;

    if (anj_base64_encoded_size_custom(input_length, config) > out_length) {
        return -1;
    }

    for (i = 0; i < input_length; ++i) {
        num = input[i];
        if (i % 3 == 0) {
            *out++ = config.alphabet[num >> 2];
            sh = num & 0x03;
        } else if (i % 3 == 1) {
            *out++ = config.alphabet[(sh << 4) + (num >> 4)];
            sh = num & 0x0F;
        } else {
            *out++ = config.alphabet[(sh << 2) + (num >> 6)];
            *out++ = config.alphabet[num & 0x3F];
        }
    }

    if (i % 3 == 1) {
        *out++ = config.alphabet[sh << 4];
    } else if (i % 3 == 2) {
        *out++ = config.alphabet[sh << 2];
    }

    if (config.padding_char) {
        for (i = (size_t) (out - out_begin); i % 4; ++i) {
            *out++ = config.padding_char;
        }
    }

    if (!config.without_null_termination) {
        *out = '\0';
    }

    return 0;
}

int anj_base64_decode_custom(size_t *out_bytes_decoded,
                             uint8_t *out,
                             size_t out_size,
                             const char *b64_data,
                             anj_base64_config_t config) {

    uint32_t accumulator = 0;
    uint8_t bits = 0;
    const uint8_t *current = (const uint8_t *) b64_data;
    size_t out_length = 0;
    size_t padding = 0;

    while (*current) {
        int ch = *current++;

        if (out_length >= out_size) {
            return -1;
        }
        if (isspace(ch)) {
            if (config.allow_whitespace) {
                continue;
            } else {
                return -1;
            }
        } else if (ch == *(const char *) &config.padding_char) {
            if (config.require_padding && ++padding > 2) {
                return -1;
            }
            continue;
        } else if (padding) {
            // padding in the middle of input
            return -1;
        }
        const char *ptr = (const char *) memchr(config.alphabet, ch, 64);
        if (!ptr) {
            return -1;
        }
        assert(ptr >= config.alphabet);
        assert(ptr - config.alphabet < 64);
        accumulator <<= 6;
        bits = (uint8_t) (bits + 6);
        accumulator |= (uint8_t) (ptr - config.alphabet);
        if (bits >= 8) {
            bits = (uint8_t) (bits - 8u);
            out[out_length++] = (uint8_t) ((accumulator >> bits) & 0xffu);
        }
    }

    if (config.padding_char && config.require_padding
            && padding != (3 - (out_length % 3)) % 3) {

        return -1;
    }

    if (out_bytes_decoded) {
        *out_bytes_decoded = out_length;
    }
    return 0;
}

#endif // ANJ_WITH_PLAINTEXT
