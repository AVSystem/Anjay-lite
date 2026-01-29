/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../src/anj/io/base64.h"

#include <anj_unit_test.h>

ANJ_UNIT_TEST(base64, padding) {
    char result[5];

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_base64_encode(result, sizeof(result), (const uint8_t *) "", 0));
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "");
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_encode(result, sizeof(result),
                                              (const uint8_t *) "a", 1));
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "YQ==");
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_encode(result, sizeof(result),
                                              (const uint8_t *) "aa", 2));
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "YWE=");
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_encode(result, sizeof(result),
                                              (const uint8_t *) "aaa", 3));
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "YWFh");
}

ANJ_UNIT_TEST(base64, encode) {
    char result[5];

    /* also encode terminating NULL byte */
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_base64_encode(result, sizeof(result), (const uint8_t *) "", 1));
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "AA==");
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_encode(result, sizeof(result),
                                              (const uint8_t *) "a", 2));
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "YQA=");
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_encode(result, sizeof(result),
                                              (const uint8_t *) "aa", 3));
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "YWEA");
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_encode(
            result, sizeof(result), (const uint8_t *) "\x0C\x40\x03", 3));
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "DEAD");
    /* output buffer too short */
    ANJ_UNIT_ASSERT_FAILED(anj_base64_encode(
            result, sizeof(result), (const uint8_t *) "\x0C\x40\x03\xAA", 4));
}

ANJ_UNIT_TEST(base64, decode) {
    char result[5];
    size_t result_length;
    char buf[5] = "AX==";
    const char *ch;
    for (ch = ANJ_BASE64_CHARS; *ch; ++ch) {
        buf[1] = *ch;
        ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode(
                &result_length, (uint8_t *) result, sizeof(result), buf));
        ANJ_UNIT_ASSERT_EQUAL(result_length, 1);
        ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode_strict(
                &result_length, (uint8_t *) result, sizeof(result), buf));
        ANJ_UNIT_ASSERT_EQUAL(result_length, 1);
    }
    /* terminating NULL byte is Base64 encoded */
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode(
            &result_length, (uint8_t *) result, sizeof(result), "AA=="));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 1);
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "");
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode(
            &result_length, (uint8_t *) result, sizeof(result), "YQA="));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 2);
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "a");
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode(
            &result_length, (uint8_t *) result, sizeof(result), "YWEA"));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 3);
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "aa");

    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode(
            &result_length, (uint8_t *) result, sizeof(result), ""));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 0);

    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode(
            &result_length, (uint8_t *) result, sizeof(result), "A+=="));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 1);

    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode(&result_length, (uint8_t *) result,
                                             sizeof(result), "\x01"));

    /* anj_base64_decode is not strict */
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_base64_decode(&result_length, (uint8_t *) result,
                              sizeof(result), "Y== ==\n\n\t\vWEA"));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 3);
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "aa");

    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode(
            &result_length, (uint8_t *) result, sizeof(result), "YQA"));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 2);
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "a");

    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode(
            &result_length, (uint8_t *) result, sizeof(result), "YQA=="));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 2);
    ANJ_UNIT_ASSERT_EQUAL_STRING(result, "a");
}

ANJ_UNIT_TEST(base64, decode_fail) {
    char result[5];
    char buf[5] = "AX==";
    char ch;
    ANJ_UNIT_ASSERT_FAILED(
            anj_base64_decode(NULL, (uint8_t *) result, 1, "AA=="));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode(NULL, (uint8_t *) result, 5, ","));

    for (ch = 1; ch < CHAR_MAX; ++ch) {
        buf[1] = ch;
        if (!strchr(ANJ_BASE64_CHARS, ch) && !isspace(ch) && ch != '=') {
            ANJ_UNIT_ASSERT_FAILED(
                    anj_base64_decode(NULL, (uint8_t *) result, 5, buf));
        }
        if (!strchr(ANJ_BASE64_CHARS, ch)) {
            ANJ_UNIT_ASSERT_FAILED(
                    anj_base64_decode_strict(NULL, (uint8_t *) result, 5, buf));
        }
    }
}

ANJ_UNIT_TEST(base64, decode_strict) {
    char result[16];
    /* no data - no problem */
    size_t result_length;
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode_strict(
            &result_length, (uint8_t *) result, sizeof(result), ""));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 0);

    /* not a multiple of 4 */
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "=="));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "="));

    /* invalid characters in the middle */
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "Zm9=v"));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "Zm9 v"));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "Zm9\0v"));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(
            NULL, (uint8_t *) result, sizeof(result), "Y== ==\n\n\t\vWEA"));

    /* invalid characters at the end */
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "Zm9v "));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(
            NULL, (uint8_t *) result, sizeof(result), "Zm9vYg== "));

    /* =-padded, invalid characters in the middle */
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(
            NULL, (uint8_t *) result, sizeof(result), "Zm9=Yg=="));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(
            NULL, (uint8_t *) result, sizeof(result), "Zm9 Yg=="));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(
            NULL, (uint8_t *) result, sizeof(result), "Zm9\0Yg=="));

    /* not a multiple of 4 (missing padding) */
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "Zm9vYg="));

    /* too much padding */
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(
            NULL, (uint8_t *) result, sizeof(result), "Zm9vY==="));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(
            NULL, (uint8_t *) result, sizeof(result), "Zm9v===="));

    /* too much padding + not a multiple of 4 */
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "Zm9vY=="));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "Zm9vY="));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "Zm9v=="));
    ANJ_UNIT_ASSERT_FAILED(anj_base64_decode_strict(NULL, (uint8_t *) result,
                                                    sizeof(result), "Zm9v="));

    /* valid, with single padding byte */
    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_decode_strict(
            &result_length, (uint8_t *) result, sizeof(result), "YQA="));
    ANJ_UNIT_ASSERT_EQUAL(result_length, 2);
}

ANJ_UNIT_TEST(base64, encoded_and_decoded_size) {
    char result[1024];
    uint8_t bytes[256];
    size_t i;
    size_t length;
    for (i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = (uint8_t) (rand() % 256);
    }
    for (i = 0; i < sizeof(bytes); ++i) {
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_base64_encode(result, sizeof(result), bytes, i));
        length = strlen(result);
        ANJ_UNIT_ASSERT_EQUAL(length + 1, anj_base64_encoded_size(i));
        /* anj_base64_estimate_decoded_size should be an upper bound */
        ANJ_UNIT_ASSERT_TRUE(anj_base64_estimate_decoded_size(length + 1) >= i);
    }
    ANJ_UNIT_ASSERT_EQUAL(anj_base64_estimate_decoded_size(0), 0);
    for (i = 1; i < 4; ++i) {
        ANJ_UNIT_ASSERT_EQUAL(anj_base64_estimate_decoded_size(i), 3);
    }
}

static void test_encoding_without_null_terminating(uint8_t *data_input,
                                                   size_t input_len,
                                                   uint8_t *data_expected,
                                                   size_t expected_len) {
    anj_base64_config_t config = {
        .alphabet = ANJ_BASE64_CHARS,
        .padding_char = '=',
        .allow_whitespace = false,
        .require_padding = true,
        .without_null_termination = true
    };

    char result[1024] = { 0 };

    ANJ_UNIT_ASSERT_SUCCESS(anj_base64_encode_custom(
            result, sizeof(result), data_input, input_len, config));
    ANJ_UNIT_ASSERT_EQUAL(anj_base64_encoded_size_custom(input_len, config),
                          expected_len);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(result, data_expected, expected_len);
}

#define TEST_ENCODING_WITHOUT_NT(Name, DataInput, DataEncoded)           \
    ANJ_UNIT_TEST(base64, encoding_without_null_terminating##Name) {     \
        test_encoding_without_null_terminating((uint8_t *) DataInput,    \
                                               sizeof(DataInput) - 1,    \
                                               (uint8_t *) DataEncoded,  \
                                               sizeof(DataEncoded) - 1); \
    }

TEST_ENCODING_WITHOUT_NT(0, "", "")
TEST_ENCODING_WITHOUT_NT(1, "Hello", "SGVsbG8=")
TEST_ENCODING_WITHOUT_NT(4, "base 64 encode", "YmFzZSA2NCBlbmNvZGU=")
TEST_ENCODING_WITHOUT_NT(5, "QWERTYUIOPAS", "UVdFUlRZVUlPUEFT")
TEST_ENCODING_WITHOUT_NT(6, "QWERTYUIOPASD", "UVdFUlRZVUlPUEFTRA==")
TEST_ENCODING_WITHOUT_NT(7, "QWERTYUIOPASDF", "UVdFUlRZVUlPUEFTREY=")
TEST_ENCODING_WITHOUT_NT(
        8,
        "QWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDF"
        "QWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDF"
        "QWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDF"
        "QWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASDFQWERTYUIOPASD"
        "F",
        "UVdFUlRZVUlPUEFTREZRV0VSVFlVSU9QQVNERlFXRVJUWVVJT1BBU0RGUVdFUlRZVUlPUE"
        "FTREZRV0VSVFlVSU9QQVNERlFXRVJUWVVJT1BBU0RGUVdFUlRZVUlPUEFTREZRV0VSVFlV"
        "SU9QQVNERlFXRVJUWVVJT1BBU0RGUVdFUlRZVUlPUEFTREZRV0VSVFlVSU9QQVNERlFXRV"
        "JUWVVJT1BBU0RGUVdFUlRZVUlPUEFTREZRV0VSVFlVSU9QQVNERlFXRVJUWVVJT1BBU0RG"
        "UVdFUlRZVUlPUEFTREZRV0VSVFlVSU9QQVNERlFXRVJUWVVJT1BBU0RGUVdFUlRZVUlPUE"
        "FTREZRV0VSVFlVSU9QQVNERg==")
