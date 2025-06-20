/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/utils.h>

#include "../../../src/anj/io/cbor_decoder_ll.h"
#include "../../../src/anj/io/internal.h"
#include "../../../src/anj/io/io.h"
#include "../../../src/anj/utils.h"

#include <anj_unit_test.h>

#if defined(ANJ_WITH_SENML_CBOR) || defined(ANJ_WITH_LWM2M_CBOR) \
        || defined(ANJ_WITH_CBOR)

typedef struct {
    const char *data;
    size_t size;
} test_data_t;

#    define TEST_DATA_INITIALIZER(Data) \
        {                               \
            .data = Data,               \
            .size = sizeof(Data) - 1    \
        }

/**
 * Convenience macro used with test_decode_uint(), e.g.:
 * > test_decode_uint(MAKE_TEST_DATA("\x00"), 0);
 */
#    define MAKE_TEST_DATA(Data) ((test_data_t) TEST_DATA_INITIALIZER(Data))

ANJ_UNIT_TEST(cbor_decoder_ll, tags_are_ignored) {
    static const test_data_t inputs[] = {
        // tag with 1 byte extended length, with one byte of follow up
        TEST_DATA_INITIALIZER("\xD8\x01\x0F"),
        // tag with 2 bytes extended length, with one byte of follow up
        TEST_DATA_INITIALIZER("\xD9\x01\x02\x0F"),
        // tag with 4 bytes extended length, with one byte of follow up
        TEST_DATA_INITIALIZER("\xDA\x01\x02\x03\x04\x0F"),
        // tag with 8 bytes extended length, with one byte of follow up
        TEST_DATA_INITIALIZER("\xDB\x01\x02\x03\x04\x05\x06\x07\x08\x0F"),
    };
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(inputs); ++i) {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, inputs[i].data, inputs[i].size, true));
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, eof_while_parsing_tag) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xDB\x01\x02\x03\x04\x05\x06\x07";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, tags_without_following_bytes_are_invalid) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_feed_payload(&ctx, "\xC6", 1, true));
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll,
              tag_followed_by_tag_without_following_bytes_are_invalid) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xC6\xC6";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, feed_payload_invalid_state_1) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_feed_payload(&ctx, "\x00", 1, false));
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_feed_payload(&ctx, "\x00", 1,
                                                           false),
                          _ANJ_IO_ERR_LOGIC);
}

ANJ_UNIT_TEST(cbor_decoder_ll, feed_payload_invalid_state_2) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_feed_payload(&ctx, "\x00", 1, true));

    _anj_cbor_ll_number_t decoded_number;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &decoded_number));
    ANJ_UNIT_ASSERT_EQUAL(decoded_number.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(decoded_number.value.u64, 0);

    // payload already finished
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_feed_payload(&ctx, "\x00", 1,
                                                           true),
                          _ANJ_IO_ERR_LOGIC);
}

static const uint64_t DECODE_UINT_FAILURE = UINT64_MAX;

static int test_decode_uint(test_data_t test_data, uint64_t expected_value) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, test_data.data, test_data.size, true));
    _anj_cbor_ll_value_type_t type;
    _anj_cbor_ll_number_t decoded_number;
    int result;
    if ((result = anj_cbor_ll_decoder_current_value_type(&ctx, &type))
            || (result = anj_cbor_ll_decoder_number(&ctx, &decoded_number))) {
        return result;
    }
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(decoded_number.type, ANJ_CBOR_LL_VALUE_UINT);
    if (expected_value == DECODE_UINT_FAILURE) {
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx),
                              _ANJ_IO_ERR_FORMAT);
    } else {
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
        ANJ_UNIT_ASSERT_EQUAL(decoded_number.value.u64, expected_value);
    }
    return 0;
}

ANJ_UNIT_TEST(cbor_decoder_ll, uint_small) {
    for (unsigned small_value = 0; small_value < 24; ++small_value) {
        const char data = (char) ((CBOR_MAJOR_TYPE_UINT << 5) | small_value);
        ANJ_UNIT_ASSERT_SUCCESS(test_decode_uint(
                (test_data_t) { &data, sizeof(data) }, small_value));
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, uint_extended_length_of_1_byte) {
    ANJ_UNIT_ASSERT_SUCCESS(test_decode_uint(MAKE_TEST_DATA("\x18\xFF"), 0xFF));
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x18"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, uint_extended_length_of_2_byte) {
    ANJ_UNIT_ASSERT_SUCCESS(
            test_decode_uint(MAKE_TEST_DATA("\x19\xAA\xBB"), 0xAABB));
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x19"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x19\xAA"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, uint_extended_length_of_4_byte) {
    ANJ_UNIT_ASSERT_SUCCESS(test_decode_uint(
            MAKE_TEST_DATA("\x1A\xAA\xBB\xCC\xDD"), 0xAABBCCDD));
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x1A\xAA"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x1A\xAA\xBB"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x1A\xAA\xBB\xCC"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, uint_extended_length_of_8_byte) {
    ANJ_UNIT_ASSERT_SUCCESS(test_decode_uint(
            MAKE_TEST_DATA("\x1B\xAA\xBB\xCC\xDD\x00\x11\x22\x33"),
            0xAABBCCDD00112233ULL));
    ANJ_UNIT_ASSERT_EQUAL(
            test_decode_uint(MAKE_TEST_DATA("\x1B\xAA\xBB\xCC\xDD\x00\x11\x22"),
                             DECODE_UINT_FAILURE),
            _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(
            test_decode_uint(MAKE_TEST_DATA("\x1B\xAA\xBB\xCC\xDD\x00\x11"),
                             DECODE_UINT_FAILURE),
            _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(
            test_decode_uint(MAKE_TEST_DATA("\x1B\xAA\xBB\xCC\xDD\x00"),
                             DECODE_UINT_FAILURE),
            _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(
            test_decode_uint(MAKE_TEST_DATA("\x1B\xAA\xBB\xCC\xDD"),
                             DECODE_UINT_FAILURE),
            _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x1B\xAA\xBB\xCC"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x1B\xAA\xBB"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x1B\xAA"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(test_decode_uint(MAKE_TEST_DATA("\x1B"),
                                           DECODE_UINT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, large_uint_with_large_tag_unfinished_payload) {
    static const char data[] = "\xDB\x01\x02\x03\x04\x05\x06\x07\x08"
                               "\x1B\xAA\xBB\xCC\xDD\x00\x11\x22\x33";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, false));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 0xAABBCCDD00112233ULL);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx),
                          _ANJ_IO_WANT_NEXT_PAYLOAD);

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_feed_payload(&ctx, NULL, 0, true));

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_decoder_ll, large_uint_with_large_tag_split_payload) {
    static const char data[] = "\xDB\x01\x02\x03\x04\x05\x06\x07\x08"
                               "\x1B\xAA\xBB\xCC\xDD\x00\x11\x22\x33";
    for (size_t split = 0; split < sizeof(data) - 1; ++split) {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_feed_payload(&ctx, data, split, false));

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data + split, sizeof(data) - 1 - split, true));
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 0xAABBCCDD00112233ULL);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
}

static const int64_t DECODE_NEGATIVE_INT_FAILURE = INT64_MAX;

static int test_decode_negative_int(test_data_t test_data,
                                    int64_t expected_value) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, test_data.data, test_data.size, true));
    _anj_cbor_ll_value_type_t type;
    _anj_cbor_ll_number_t decoded_number;
    int result;
    if ((result = anj_cbor_ll_decoder_current_value_type(&ctx, &type))
            || (result = anj_cbor_ll_decoder_number(&ctx, &decoded_number))) {
        return result;
    }
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_NEGATIVE_INT);
    ANJ_UNIT_ASSERT_EQUAL(decoded_number.type, ANJ_CBOR_LL_VALUE_NEGATIVE_INT);
    if (expected_value == DECODE_NEGATIVE_INT_FAILURE) {
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx),
                              _ANJ_IO_ERR_FORMAT);
    } else {
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
        ANJ_UNIT_ASSERT_EQUAL(decoded_number.value.i64, expected_value);
    }
    return 0;
}

ANJ_UNIT_TEST(cbor_decoder_ll, neg_int_small) {
    for (int small_value = 0; small_value < 24; ++small_value) {
        const char data =
                (uint8_t) ((CBOR_MAJOR_TYPE_NEGATIVE_INT << 5) | small_value);
        ANJ_UNIT_ASSERT_SUCCESS(test_decode_negative_int(
                (test_data_t) { &data, sizeof(data) }, -small_value - 1));
    }
    const char data = (uint8_t) ((CBOR_MAJOR_TYPE_NEGATIVE_INT << 5) | 24);
    ANJ_UNIT_ASSERT_EQUAL(
            test_decode_negative_int((test_data_t) { &data, sizeof(data) },
                                     DECODE_NEGATIVE_INT_FAILURE),
            _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, neg_int_extended_length_of_1_byte) {
    ANJ_UNIT_ASSERT_SUCCESS(
            test_decode_negative_int(MAKE_TEST_DATA("\x38\xFF"), -256));
    ANJ_UNIT_ASSERT_EQUAL(test_decode_negative_int(MAKE_TEST_DATA("\x38"),
                                                   DECODE_NEGATIVE_INT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, neg_int_extended_length_of_2_byte) {
    ANJ_UNIT_ASSERT_SUCCESS(
            test_decode_negative_int(MAKE_TEST_DATA("\x39\x00\x01"), -2));
    ANJ_UNIT_ASSERT_EQUAL(test_decode_negative_int(MAKE_TEST_DATA("\x39\x00"),
                                                   DECODE_NEGATIVE_INT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(test_decode_negative_int(MAKE_TEST_DATA("\x39"),
                                                   DECODE_NEGATIVE_INT_FAILURE),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, neg_int_boundary) {
    ANJ_UNIT_ASSERT_SUCCESS(test_decode_negative_int(
            MAKE_TEST_DATA("\x3B\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF"), INT64_MIN));
    // Overflow.
    ANJ_UNIT_ASSERT_EQUAL(
            test_decode_negative_int(
                    MAKE_TEST_DATA("\x3B\x80\x00\x00\x00\x00\x00\x00\x00"),
                    DECODE_NEGATIVE_INT_FAILURE),
            _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, bytes_short) {
    // - 1st byte: code,
    // - maximum 23 bytes of payload.
    // - last byte: small integer
    uint8_t input_bytes[1 + 23 + 1];

    for (size_t short_len = 0; short_len < 24; ++short_len) {
        input_bytes[0] =
                (uint8_t) ((CBOR_MAJOR_TYPE_BYTE_STRING << 5) | short_len);
        for (size_t i = 0; i < short_len; ++i) {
            input_bytes[i + 1] = (uint8_t) rand();
        }
        uint8_t small_int = (uint8_t) (rand() % 24);
        input_bytes[short_len + 1] =
                (uint8_t) ((CBOR_MAJOR_TYPE_UINT << 5) | small_int);
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, input_bytes, short_len + 2, true));

        _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
        ptrdiff_t total_size = 0;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
        ANJ_UNIT_ASSERT_EQUAL(total_size, (ptrdiff_t) short_len);
        const void *output_bytes = NULL;
        size_t output_bytes_size;
        bool message_finished;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes_get_some(bytes_ctx,
                                                   &output_bytes,
                                                   &output_bytes_size,
                                                   &message_finished));
        ANJ_UNIT_ASSERT_TRUE(message_finished);
        ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, short_len);
        // anj_cbor_ll_decoder_bytes_get_some() returns pointers
        // inside the input buffer
        ANJ_UNIT_ASSERT_TRUE(output_bytes == &input_bytes[1]);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_UINT);

        _anj_cbor_ll_number_t decoded_number;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_number(&ctx, &decoded_number));
        ANJ_UNIT_ASSERT_EQUAL(decoded_number.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(decoded_number.value.u64, small_int);
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, bytes_indefinite) {
    // (_ h'AABBCCDD', h'EEFF99'), 7
    uint8_t input_bytes[] = { 0x5F, 0x44, 0xAA, 0xBB, 0xCC, 0xDD,
                              0x43, 0xEE, 0xFF, 0x99, 0xFF, 0x07 };

    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, input_bytes, sizeof(input_bytes), true));

    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
    ptrdiff_t total_size = 0;
#    ifdef ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
    ANJ_UNIT_ASSERT_EQUAL(total_size, ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);
    const void *output_bytes = NULL;
    size_t output_bytes_size;
    bool message_finished;
    // first chunk
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bytes_get_some(
            bytes_ctx, &output_bytes, &output_bytes_size, &message_finished));
    ANJ_UNIT_ASSERT_TRUE(output_bytes == &input_bytes[2]);
    ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, 4);
    ANJ_UNIT_ASSERT_FALSE(message_finished);
    // second chunk
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bytes_get_some(
            bytes_ctx, &output_bytes, &output_bytes_size, &message_finished));
    ANJ_UNIT_ASSERT_TRUE(output_bytes == &input_bytes[7]);
    ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, 3);
    ANJ_UNIT_ASSERT_FALSE(message_finished);
    // end of indefinite bytes
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bytes_get_some(
            bytes_ctx, &output_bytes, &output_bytes_size, &message_finished));
    ANJ_UNIT_ASSERT_NULL(output_bytes);
    ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, 0);
    ANJ_UNIT_ASSERT_TRUE(message_finished);

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_UINT);

    _anj_cbor_ll_number_t decoded_number;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &decoded_number));
    ANJ_UNIT_ASSERT_EQUAL(decoded_number.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(decoded_number.value.u64, 7);
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
#    else  // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx,
                                                    &total_size),
                          _ANJ_IO_ERR_FORMAT);
#    endif // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
}

ANJ_UNIT_TEST(cbor_decoder_ll, bytes_indefinite_empty) {
    // (_ ), 7
    uint8_t input_bytes[] = { 0x5F, 0xFF, 0x07 };

    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, input_bytes, sizeof(input_bytes), true));

    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
    ptrdiff_t total_size = 0;
#    ifdef ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
    ANJ_UNIT_ASSERT_EQUAL(total_size, ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);
    const void *output_bytes = NULL;
    size_t output_bytes_size;
    bool message_finished;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bytes_get_some(
            bytes_ctx, &output_bytes, &output_bytes_size, &message_finished));
    ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, 0);
    ANJ_UNIT_ASSERT_TRUE(message_finished);

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_UINT);

    _anj_cbor_ll_number_t decoded_number;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &decoded_number));
    ANJ_UNIT_ASSERT_EQUAL(decoded_number.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(decoded_number.value.u64, 7);
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
#    else  // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx,
                                                    &total_size),
                          _ANJ_IO_ERR_FORMAT);
#    endif // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
}

ANJ_UNIT_TEST(cbor_decoder_ll, bytes_indefinite_invalid_integer_inside) {
    // (_ 21 )
    uint8_t input_bytes[] = { 0x5F, 0x15, 0xFF };

    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, input_bytes, sizeof(input_bytes), true));

    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
    ptrdiff_t total_size = 0;
#    ifdef ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
    ANJ_UNIT_ASSERT_EQUAL(total_size, ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);
    const void *output_bytes = NULL;
    size_t output_bytes_size;
    bool message_finished;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes_get_some(
                                  bytes_ctx, &output_bytes, &output_bytes_size,
                                  &message_finished),
                          _ANJ_IO_ERR_FORMAT);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
#    else  // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx,
                                                    &total_size),
                          _ANJ_IO_ERR_FORMAT);
#    endif // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
}

ANJ_UNIT_TEST(cbor_decoder_ll, bytes_indefinite_invalid_map_inside) {
    // (_ {2: 5} )
    uint8_t input_bytes[] = { 0x5F, 0xA1, 0x02, 0x05, 0xFF };

    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, input_bytes, sizeof(input_bytes), true));

    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
    ptrdiff_t total_size = 0;
#    ifdef ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
    ANJ_UNIT_ASSERT_EQUAL(total_size, ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);
    const void *output_bytes = NULL;
    size_t output_bytes_size;
    bool message_finished;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes_get_some(
                                  bytes_ctx, &output_bytes, &output_bytes_size,
                                  &message_finished),
                          _ANJ_IO_ERR_FORMAT);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
#    else  // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx,
                                                    &total_size),
                          _ANJ_IO_ERR_FORMAT);
#    endif // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
}

ANJ_UNIT_TEST(cbor_decoder_ll, bytes_indefinite_invalid_bytes_and_map_inside) {
    // (_ h'001122', {2: 5} )
    uint8_t input_bytes[] = { 0x5F, 0x43, 0x00, 0x11, 0x22,
                              0xA1, 0x02, 0x05, 0xFF };

    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, input_bytes, sizeof(input_bytes), true));

    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
    ptrdiff_t total_size = 0;
#    ifdef ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
    ANJ_UNIT_ASSERT_EQUAL(total_size, ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);
    const void *output_bytes = NULL;
    size_t output_bytes_size;
    bool message_finished;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bytes_get_some(
            bytes_ctx, &output_bytes, &output_bytes_size, &message_finished));
    ANJ_UNIT_ASSERT_TRUE(output_bytes == &input_bytes[2]);
    ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, 3);
    ANJ_UNIT_ASSERT_FALSE(message_finished);
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes_get_some(
                                  bytes_ctx, &output_bytes, &output_bytes_size,
                                  &message_finished),
                          _ANJ_IO_ERR_FORMAT);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
#    else  // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx,
                                                    &total_size),
                          _ANJ_IO_ERR_FORMAT);
#    endif // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
}

ANJ_UNIT_TEST(cbor_decoder_ll, bytes_nested_indefinite) {
    uint8_t input_bytes[] = { 0x5F, 0x5F, 0xFF, 0xFF };

    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, input_bytes, sizeof(input_bytes), true));

    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
    ptrdiff_t total_size = 0;
#    ifdef ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
    ANJ_UNIT_ASSERT_EQUAL(total_size, ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);
    const void *output_bytes = NULL;
    size_t output_bytes_size;
    bool message_finished;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes_get_some(
                                  bytes_ctx, &output_bytes, &output_bytes_size,
                                  &message_finished),
                          _ANJ_IO_ERR_FORMAT);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
#    else  // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx,
                                                    &total_size),
                          _ANJ_IO_ERR_FORMAT);
#    endif // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
}

ANJ_UNIT_TEST(cbor_decoder_ll, bytes_long) {
    // - 1st byte: code,
    // - 2nd byte: extended length high byte,
    // - 3rd byte: extended length low byte,
    // - rest: 256 bytes of payload.
    enum { PAYLOAD_LEN = 256 };
    uint8_t input_bytes[3 + PAYLOAD_LEN];

    input_bytes[0] = 0x59; // major-type=bytes, extended-length=2bytes
    input_bytes[1] = PAYLOAD_LEN >> 8;
    input_bytes[2] = PAYLOAD_LEN & 0xFF;
    for (size_t i = 3; i < sizeof(input_bytes); ++i) {
        input_bytes[i] = (uint8_t) rand();
    }

    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, input_bytes, sizeof(input_bytes), true));

    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
    ptrdiff_t total_size = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
    ANJ_UNIT_ASSERT_EQUAL(total_size, PAYLOAD_LEN);
    const void *output_bytes = NULL;
    size_t output_bytes_size;
    bool message_finished;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bytes_get_some(
            bytes_ctx, &output_bytes, &output_bytes_size, &message_finished));
    ANJ_UNIT_ASSERT_TRUE(output_bytes == &input_bytes[3]);
    ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, PAYLOAD_LEN);
    ANJ_UNIT_ASSERT_TRUE(message_finished);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}

#    if _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0
ANJ_UNIT_TEST(cbor_decoder_ll, bytes_long_split_payload) {
    // - 1st byte: code,
    // - 2nd byte: extended length high byte,
    // - 3rd byte: extended length low byte,
    // - rest: 256 bytes of payload.
    enum { PAYLOAD_LEN = 256 };
    uint8_t input_bytes[3 + PAYLOAD_LEN];

    input_bytes[0] = 0x59; // major-type=bytes, extended-length=2bytes
    input_bytes[1] = PAYLOAD_LEN >> 8;
    input_bytes[2] = PAYLOAD_LEN & 0xFF;
    for (size_t i = 3; i < sizeof(input_bytes); ++i) {
        input_bytes[i] = (uint8_t) rand();
    }

    for (size_t split = 0; split < 4; ++split) {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, input_bytes, split, false));

        _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, NULL),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, input_bytes + split, sizeof(input_bytes) - split, true));

        size_t nesting_level;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

        ptrdiff_t total_size;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
        ANJ_UNIT_ASSERT_EQUAL(total_size, PAYLOAD_LEN);
        const void *output_bytes = NULL;
        size_t output_bytes_size;
        bool message_finished;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes_get_some(bytes_ctx,
                                                   &output_bytes,
                                                   &output_bytes_size,
                                                   &message_finished));
        ANJ_UNIT_ASSERT_TRUE(output_bytes == &input_bytes[3]);
        ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, PAYLOAD_LEN);
        ANJ_UNIT_ASSERT_TRUE(message_finished);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
    for (size_t split = 4; split < 9; ++split) {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, input_bytes, split, false));

        _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, NULL),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, input_bytes + split, sizeof(input_bytes) - split, true));

        size_t nesting_level;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

        ptrdiff_t total_size;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
        ANJ_UNIT_ASSERT_EQUAL(total_size, PAYLOAD_LEN);
        const void *output_bytes = NULL;
        size_t output_bytes_size;
        bool message_finished;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes_get_some(bytes_ctx,
                                                   &output_bytes,
                                                   &output_bytes_size,
                                                   &message_finished));
        ANJ_UNIT_ASSERT_TRUE(output_bytes == &ctx.prebuffer[3]);
        ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, sizeof(ctx.prebuffer) - 3);
        ANJ_UNIT_ASSERT_FALSE(message_finished);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes_get_some(bytes_ctx,
                                                   &output_bytes,
                                                   &output_bytes_size,
                                                   &message_finished));
        ANJ_UNIT_ASSERT_TRUE(output_bytes
                             == &input_bytes[sizeof(ctx.prebuffer)]);
        ANJ_UNIT_ASSERT_EQUAL(output_bytes_size,
                              PAYLOAD_LEN - sizeof(ctx.prebuffer) + 3);
        ANJ_UNIT_ASSERT_TRUE(message_finished);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
    for (size_t split = 9; split < sizeof(input_bytes); ++split) {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, input_bytes, split, false));

        _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
        ptrdiff_t total_size;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size));
        ANJ_UNIT_ASSERT_EQUAL(total_size, PAYLOAD_LEN);

        const void *output_bytes = NULL;
        size_t output_bytes_size;
        bool message_finished;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes_get_some(bytes_ctx,
                                                   &output_bytes,
                                                   &output_bytes_size,
                                                   &message_finished));
        ANJ_UNIT_ASSERT_TRUE(output_bytes == &input_bytes[3]);
        ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, split - 3);
        ANJ_UNIT_ASSERT_FALSE(message_finished);

        ANJ_UNIT_ASSERT_EQUAL(
                anj_cbor_ll_decoder_bytes_get_some(bytes_ctx,
                                                   &output_bytes,
                                                   &output_bytes_size,
                                                   &message_finished),
                _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, input_bytes + split, sizeof(input_bytes) - split, true));

        size_t nesting_level;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_bytes_get_some(bytes_ctx,
                                                   &output_bytes,
                                                   &output_bytes_size,
                                                   &message_finished));
        ANJ_UNIT_ASSERT_TRUE(output_bytes == &input_bytes[split]);
        ANJ_UNIT_ASSERT_EQUAL(output_bytes_size, PAYLOAD_LEN - split + 3);
        ANJ_UNIT_ASSERT_TRUE(message_finished);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
}
#    endif // _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0

#    ifdef ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
ANJ_UNIT_TEST(cbor_decoder_ll, bytes_indefinite_and_then_value_split) {
    uint8_t input_bytes[256];
    uint8_t compare_buffer[256];
    for (size_t chunk1_size = 9; chunk1_size <= sizeof(input_bytes) - 7;
         ++chunk1_size) {
        size_t chunk2_size = sizeof(input_bytes) - chunk1_size - 7;
        input_bytes[0] = 0x5F; // major-type=bytes, extended-length=indefinite
        input_bytes[1] = 0x58; // major-type=bytes, extended-length=1byte
        input_bytes[2] = (uint8_t) chunk1_size;
        for (size_t i = 0; i < chunk1_size; ++i) {
            input_bytes[i + 3] = (uint8_t) rand();
        }
        // major-type=bytes, extended-length=1byte
        input_bytes[chunk1_size + 3] = 0x58;
        input_bytes[chunk1_size + 4] = (uint8_t) chunk2_size;
        for (size_t i = 0; i < chunk2_size; ++i) {
            input_bytes[chunk1_size + i + 5] = (uint8_t) rand();
        }
        input_bytes[sizeof(input_bytes) - 2] = 0xFF; // indefinite end
        input_bytes[sizeof(input_bytes) - 1] = 0x01; // integer

        for (size_t split = 0; split < sizeof(input_bytes); ++split) {
            _anj_cbor_ll_decoder_t ctx;
            anj_cbor_ll_decoder_init(&ctx);
            ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                    &ctx, input_bytes, split, false));
            bool second_chunk_fed = false;

            _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
            ptrdiff_t total_size;
            int result =
                    anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx, &total_size);
            if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                        &ctx, input_bytes + split, sizeof(input_bytes) - split,
                        true));
                second_chunk_fed = true;
                result = anj_cbor_ll_decoder_bytes(&ctx, &bytes_ctx,
                                                   &total_size);
            }
            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(total_size,
                                  ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);

            const void *output_bytes = NULL;
            size_t output_bytes_size = 0;
            bool message_finished = false;
            uint8_t *compare_buffer_ptr = compare_buffer;
            while (!message_finished) {
                if ((result = anj_cbor_ll_decoder_bytes_get_some(
                             bytes_ctx, &output_bytes, &output_bytes_size,
                             &message_finished))
                        == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                    ANJ_UNIT_ASSERT_FALSE(second_chunk_fed);
                    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                            &ctx, input_bytes + split,
                            sizeof(input_bytes) - split, true));
                    second_chunk_fed = true;

                    size_t nesting_level;
                    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_nesting_level(
                            &ctx, &nesting_level));
                    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

                    result = anj_cbor_ll_decoder_bytes_get_some(
                            bytes_ctx, &output_bytes, &output_bytes_size,
                            &message_finished);
                }
                ANJ_UNIT_ASSERT_SUCCESS(result);
                if (output_bytes_size) {
                    memcpy(compare_buffer_ptr, output_bytes, output_bytes_size);
                    compare_buffer_ptr += output_bytes_size;
                }
            }

            ANJ_UNIT_ASSERT_EQUAL(compare_buffer_ptr - compare_buffer,
                                  chunk1_size + chunk2_size);
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(input_bytes + 3, compare_buffer,
                                              chunk1_size);
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(input_bytes + chunk1_size + 5,
                                              compare_buffer + chunk1_size,
                                              chunk2_size);

            _anj_cbor_ll_number_t value;
            ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
            ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
            ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 1);

            ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
        }
    }
}
#    endif // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES

#    if _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0
ANJ_UNIT_TEST(cbor_decoder_ll, flat_array) {
    // array [1u, 2u, 3u]
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\x83\x01\x02\x03";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(ANJ_CBOR_LL_VALUE_ARRAY, type);

    size_t nesting_level;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);
    ptrdiff_t array_size = 0;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, 3);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 1);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 1);

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 1);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 2);

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 1);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 3);

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_LOGIC);
}

ANJ_UNIT_TEST(cbor_decoder_ll, flat_empty_array) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_feed_payload(&ctx, "\x80", 1, true));

    size_t nesting_level;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);
    ptrdiff_t array_size = 0;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, 0);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_decoder_ll, flat_empty_array_with_uint_afterwards) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_feed_payload(&ctx, "\x80\x01", 2, true));

    size_t nesting_level;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);
    ptrdiff_t array_size = 0;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, 0);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_UINT);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 1);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_LOGIC);
}
#    endif // _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0

#    if _ANJ_MAX_CBOR_NEST_STACK_SIZE >= 2
ANJ_UNIT_TEST(cbor_decoder_ll, nested_array) {
    // array [[1u,2u,3u], 4]
    {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        static const char data_array_first[] = "\x82\x83\x01\x02\x03\x04";
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data_array_first, sizeof(data_array_first) - 1, true));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_ARRAY);

        size_t nesting_level;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);
        ptrdiff_t array_size = 0;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
        ANJ_UNIT_ASSERT_EQUAL(array_size, 2);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 1);
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
        ANJ_UNIT_ASSERT_EQUAL(array_size, 3);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 2);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 1);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 2);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 2);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 2);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 3);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 1);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 4);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }

    {
        // array [1u, [2u, 3u, 4u]]
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        static const char data_array_last[] = "\x82\x01\x83\x02\x03\x04";
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data_array_last, sizeof(data_array_last) - 1, true));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_ARRAY);

        size_t nesting_level;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);
        ptrdiff_t array_size = 0;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
        ANJ_UNIT_ASSERT_EQUAL(array_size, 2);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 1);
        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 1);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 1);
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
        ANJ_UNIT_ASSERT_EQUAL(array_size, 3);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 2);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 2);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 2);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 3);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 2);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 4);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
}
#    endif // _ANJ_MAX_CBOR_NEST_STACK_SIZE >= 2

#    if _ANJ_MAX_CBOR_NEST_STACK_SIZE == 5
ANJ_UNIT_TEST(cbor_decoder_ll, array_too_many_nest_levels) {
    // array [[[[[[[]]]]]]]
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\x81\x81\x81\x81\x81\x81\x80";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    size_t nesting_level;
    ptrdiff_t array_size = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 1);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 2);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 3);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 4);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 5);
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_enter_array(&ctx, &array_size),
                          _ANJ_IO_ERR_FORMAT);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
}
#    endif // _ANJ_MAX_CBOR_NEST_STACK_SIZE == 5

#    if _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0
ANJ_UNIT_TEST(cbor_decoder_ll, array_too_large_size) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    // array(2^63)
    static const char data[] = "\x9B\x80\x00\x00\x00\x00\x00\x00\x00";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_enter_array(&ctx, NULL),
                          _ANJ_IO_ERR_FORMAT);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
}

static const char *read_short_string(_anj_cbor_ll_decoder_t *ctx) {
    static char short_string[128];
    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx = NULL;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bytes(ctx, &bytes_ctx, NULL));
    const void *data = NULL;
    size_t data_size;
    bool message_finished;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bytes_get_some(
            bytes_ctx, &data, &data_size, &message_finished));
    ANJ_UNIT_ASSERT_NOT_NULL(data);
    ANJ_UNIT_ASSERT_TRUE(data_size < sizeof(short_string));
    ANJ_UNIT_ASSERT_TRUE(message_finished);
    memcpy(short_string, data, data_size);
    short_string[data_size] = '\0';
    return short_string;
}

ANJ_UNIT_TEST(cbor_decoder_ll, array_indefinite) {
    // indefinite_array [
    //      "Fun",
    //      "Stuff",
    // ]
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\x9F\x63"
                               "Fun"
                               "\x65"
                               "Stuff"
                               "\xFF";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_ARRAY);

    ptrdiff_t array_size;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(read_short_string(&ctx), "Fun",
                                      sizeof("Fun"));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(read_short_string(&ctx), "Stuff",
                                      sizeof("Stuff"));

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_decoder_ll, indefinite_break_in_definite_array) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\x81\xFF";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(ANJ_CBOR_LL_VALUE_ARRAY, type);

    size_t nesting_level;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);
    ptrdiff_t array_size;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_array(&ctx, &array_size));
    ANJ_UNIT_ASSERT_EQUAL(array_size, 1);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_current_value_type(&ctx, &type),
                          _ANJ_IO_ERR_FORMAT);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, flat_map) {
    // map { 42: 300 }
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xA1\x18\x2A\x19\x01\x2C";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(ANJ_CBOR_LL_VALUE_MAP, type);

    size_t nesting_level;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);
    ptrdiff_t pair_count;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_map(&ctx, &pair_count));
    ANJ_UNIT_ASSERT_EQUAL(pair_count, 1);

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 1);

    _anj_cbor_ll_number_t key;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &key));
    ANJ_UNIT_ASSERT_EQUAL(key.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(key.value.u64, 42);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 300);

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_decoder_ll, empty_map) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_feed_payload(&ctx, "\xA0", 1, true));
    ptrdiff_t pair_count;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_map(&ctx, &pair_count));
    ANJ_UNIT_ASSERT_EQUAL(pair_count, 0);
    // We enter the map, and then we immediately exit it, because it is empty.
    size_t nesting_level;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level));
    ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}
#    endif // _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0

#    ifdef ANJ_WITH_CBOR_DECODE_HALF_FLOAT
#        define TEST_HALF(Name, Value, Expected)                            \
            ANJ_UNIT_TEST(cbor_decoder_ll, Name) {                          \
                _anj_cbor_ll_decoder_t ctx;                                 \
                anj_cbor_ll_decoder_init(&ctx);                             \
                static const char data[] = "\xF9" Value;                    \
                ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(   \
                        &ctx, data, sizeof(data) - 1, true));               \
                _anj_cbor_ll_number_t value;                                \
                ANJ_UNIT_ASSERT_SUCCESS(                                    \
                        anj_cbor_ll_decoder_number(&ctx, &value));          \
                ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_FLOAT); \
                ANJ_UNIT_ASSERT_EQUAL(value.value.f32, Expected);           \
            }

TEST_HALF(half_float_value, "\x50\x00", 32.0f)
TEST_HALF(half_float_subnormal_value, "\x03\xFF", 6.097555160522461e-05f)
TEST_HALF(half_float_nan, "\x7E\x00", NAN)
TEST_HALF(half_float_inf, "\x7C\x00", INFINITY)
#    endif // ANJ_WITH_CBOR_DECODE_HALF_FLOAT

ANJ_UNIT_TEST(cbor_decoder_ll, half_float_premature_eof) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xF9\x50";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

#    define TEST_FLOAT(Name, Value)                                            \
        ANJ_UNIT_TEST(cbor_decoder_ll, Name) {                                 \
            _anj_cbor_ll_decoder_t ctx;                                        \
            anj_cbor_ll_decoder_init(&ctx);                                    \
            char data[sizeof(float) + 1] = "\xFA";                             \
            uint32_t encoded = _anj_htonf(Value);                              \
            memcpy(data + 1, &encoded, sizeof(encoded));                       \
            ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(          \
                    &ctx, data, sizeof(data), true));                          \
            _anj_cbor_ll_number_t value;                                       \
            ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value)); \
            ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_FLOAT);        \
            ANJ_UNIT_ASSERT_EQUAL(value.value.f32, Value);                     \
        }

TEST_FLOAT(float_value, 32.0f)
TEST_FLOAT(float_nan, NAN)
TEST_FLOAT(float_inf, INFINITY)

ANJ_UNIT_TEST(cbor_decoder_ll, float_premature_eof) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xFA\x50";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

#    define TEST_DOUBLE(Name, Value)                                           \
        ANJ_UNIT_TEST(cbor_decoder_ll, Name) {                                 \
            _anj_cbor_ll_decoder_t ctx;                                        \
            anj_cbor_ll_decoder_init(&ctx);                                    \
            char data[sizeof(double) + 1] = "\xFB";                            \
            uint64_t encoded = _anj_htond(Value);                              \
            memcpy(data + 1, &encoded, sizeof(encoded));                       \
            ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(          \
                    &ctx, data, sizeof(data), true));                          \
            _anj_cbor_ll_number_t value;                                       \
            ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value)); \
            ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE);       \
            ANJ_UNIT_ASSERT_EQUAL(value.value.f64, Value);                     \
        }

TEST_DOUBLE(double_value, 32.0)
TEST_DOUBLE(double_nan, NAN)
TEST_DOUBLE(double_inf, INFINITY)

ANJ_UNIT_TEST(cbor_decoder_ll, double_premature_eof) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xFB\x50";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, boolean_true_and_false) {
    {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        static const char data[] = "\xF5";
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data, sizeof(data) - 1, true));
        bool value;
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bool(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value, true);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
    {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        static const char data[] = "\xF4";
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data, sizeof(data) - 1, true));
        bool value;
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bool(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value, false);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, boolean_integers_are_not_real_booleans) {
    {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        static const char data[] = "\x00";
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data, sizeof(data) - 1, true));
        bool value;
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bool(&ctx, &value),
                              _ANJ_IO_ERR_FORMAT);
    }
    {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        static const char data[] = "\x01";
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data, sizeof(data) - 1, true));
        bool value;
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_bool(&ctx, &value),
                              _ANJ_IO_ERR_FORMAT);
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, null_value) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xF6";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_null(&ctx));

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_decoder_ll, undefined_value) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xF7";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_current_value_type(&ctx, &type),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, invalid_simple_value) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xF8";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_current_value_type(&ctx, &type),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, loose_indefinite_break) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xFF";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_current_value_type(&ctx, &type),
                          _ANJ_IO_ERR_FORMAT);
}

#    ifdef ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS
static uint8_t make_header(cbor_major_type_t major_type, uint8_t value) {
    return (uint8_t) ((((uint8_t) major_type) << 5) | value);
}

static void encode_int(char **out_buffer, int64_t value) {
    const uint64_t encoded =
            _anj_convert_be64((uint64_t) (value < 0 ? -(value + 1) : value));
    const uint8_t major_type =
            value < 0 ? CBOR_MAJOR_TYPE_NEGATIVE_INT : CBOR_MAJOR_TYPE_UINT;
    const uint8_t header = make_header(major_type, CBOR_EXT_LENGTH_8BYTE);

    memcpy(*out_buffer, &header, 1);
    *out_buffer += 1;
    memcpy(*out_buffer, &encoded, sizeof(encoded));
    *out_buffer += sizeof(encoded);
}

#        define TEST_TYPICAL_DECIMAL_FRACTION(Name, Exponent, Mantissa)      \
            ANJ_UNIT_TEST(cbor_decoder_ll, typical_decimal_##Name) {         \
                /* Tag(4), Array [ Exponent, Mantissa ] */                   \
                char data[2 + 2 * (sizeof(uint8_t) + sizeof(uint64_t))] =    \
                        "\xC4\x82";                                          \
                char *integers = &data[2];                                   \
                encode_int(&integers, (Exponent));                           \
                encode_int(&integers, (Mantissa));                           \
                _anj_cbor_ll_decoder_t ctx;                                  \
                anj_cbor_ll_decoder_init(&ctx);                              \
                ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(    \
                        &ctx, data, sizeof(data), true));                    \
                _anj_cbor_ll_number_t value;                                 \
                ANJ_UNIT_ASSERT_SUCCESS(                                     \
                        anj_cbor_ll_decoder_number(&ctx, &value));           \
                ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE); \
                ANJ_UNIT_ASSERT_EQUAL(value.value.f64,                       \
                                      (Mantissa) *pow(10.0, (Exponent)));    \
            }

TEST_TYPICAL_DECIMAL_FRACTION(small, 2, 3)
TEST_TYPICAL_DECIMAL_FRACTION(small_negative_mantissa, 2, -3)
TEST_TYPICAL_DECIMAL_FRACTION(small_negative_exponent, -2, 3)
TEST_TYPICAL_DECIMAL_FRACTION(small_negative_exponent_and_mantissa, -2, -3)
TEST_TYPICAL_DECIMAL_FRACTION(big_exponent, 100, 2)
TEST_TYPICAL_DECIMAL_FRACTION(big_negative_exponent, -100, 2)
TEST_TYPICAL_DECIMAL_FRACTION(big_negative_exponent_and_mantissa, -100, -2)

ANJ_UNIT_TEST(cbor_decoder_ll, decimal_fraction_and_then_value) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xC4\x82\x02\x03\x04";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value.value.f64, 300.0);

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 4);
}

ANJ_UNIT_TEST(cbor_decoder_ll, decimal_fraction_and_then_value_split_payload) {
    static const char data[] = "\xC4\x82"
                               "\x1B\x00\x00\x00\x00\x00\x00\x00\x02"
                               "\x1B\x00\x00\x00\x00\x00\x00\x00\x03"
                               "\x1B\x00\x00\x00\x00\x00\x00\x00\x04";
    for (size_t split = 0; split < sizeof(data) - 1; ++split) {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_feed_payload(&ctx, data, split, false));

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data + split, sizeof(data) - 1 - split, true));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_DOUBLE);

        size_t nesting_level;
        anj_cbor_ll_decoder_nesting_level(&ctx, &nesting_level);
        ANJ_UNIT_ASSERT_EQUAL(nesting_level, 0);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE);
        ANJ_UNIT_ASSERT_EQUAL(value.value.f64, 300.0);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 4);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
}
#    endif // ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS

ANJ_UNIT_TEST(cbor_decoder_ll, decimal_fraction_invalid_length_1) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xC4\x81\x02";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, decimal_fraction_invalid_length_2) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xC4\x83\x02\x03\x04";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, decimal_fraction_invalid_length_and_then_value) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xC4\x81\x02\x03";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, decimal_fraction_invalid_inner_type) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xC4\x82\xF9\x03\xFF\x03";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, decimal_fraction_tag_after_tag) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xC4\xC4\x82\x02\x03";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

#    ifdef ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS
ANJ_UNIT_TEST(cbor_decoder_ll, decimal_fraction_tag_but_no_data) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xC4";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_value_type_t value_type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &value_type));
    ANJ_UNIT_ASSERT_EQUAL(value_type, ANJ_CBOR_LL_VALUE_DOUBLE);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}
#    endif // ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS

#    if _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0
ANJ_UNIT_TEST(cbor_decoder_ll, indefinite_map) {
    // indefinite_map {
    //      "Fun": true,
    //      "Stuff": -2,
    // }
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xBF\x63"
                               "Fun"
                               "\xF5\x65"
                               "Stuff"
                               "\x21\xFF";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_MAP);

    ptrdiff_t total_size;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_map(&ctx, &total_size));
    ANJ_UNIT_ASSERT_EQUAL(total_size, ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(read_short_string(&ctx), "Fun",
                                      sizeof("Fun"));
    bool value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bool(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value, true);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(read_short_string(&ctx), "Stuff",
                                      sizeof("Stuff"));
    _anj_cbor_ll_number_t number;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &number));
    ANJ_UNIT_ASSERT_EQUAL(number.type, ANJ_CBOR_LL_VALUE_NEGATIVE_INT);
    ANJ_UNIT_ASSERT_EQUAL(number.value.i64, -2);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_decoder_ll, indefinite_map_with_odd_number_of_items) {
    // indefinite_map {
    //      "Fun": true,
    //      "Stuff":
    // }
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    static const char data[] = "\xBF\x63"
                               "Fun"
                               "\xF5\x65"
                               "Stuff"
                               "\xFF";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ptrdiff_t total_size;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_enter_map(&ctx, &total_size));
    ANJ_UNIT_ASSERT_EQUAL(total_size, ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(read_short_string(&ctx), "Fun",
                                      sizeof("Fun"));
    bool value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_bool(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value, true);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(read_short_string(&ctx), "Stuff",
                                      sizeof("Stuff"));

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, map_too_large_size_1) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    // map(2^63)
    static const char data[] = "\xBB\x80\x00\x00\x00\x00\x00\x00\x00";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_enter_map(&ctx, NULL),
                          _ANJ_IO_ERR_FORMAT);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, map_too_large_size_2) {
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    // map(2^62)
    static const char data[] = "\xBB\x40\x00\x00\x00\x00\x00\x00\x00";
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_enter_map(&ctx, NULL),
                          _ANJ_IO_ERR_FORMAT);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_ERR_FORMAT);
}
#    endif // _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0

ANJ_UNIT_TEST(cbor_decoder_ll, timestamp_uint) {
    static const char data[] = "\xC1\x1B\xAA\xBB\xCC\xDD\x00\x11\x22\x33";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 0xAABBCCDD00112233ULL);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_decoder_ll, timestamp_uint_split) {
    static const char data[] = "\xC1\x1B\xAA\xBB\xCC\xDD\x00\x11\x22\x33";
    for (size_t split = 0; split < 9; ++split) {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_feed_payload(&ctx, data, split, false));

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data + split, sizeof(data) - 1 - split, true));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 0xAABBCCDD00112233ULL);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
    {
        // split == 9
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data, sizeof(data) - 2, false));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data + sizeof(data) - 2, 1, true));

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 0xAABBCCDD00112233ULL);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
    {
        // split == 10
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data, sizeof(data) - 1, false));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
        ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 0xAABBCCDD00112233ULL);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_feed_payload(&ctx, NULL, 0, true));

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, timestamp_float) {
    static const char data[] = "\xC1\xF9\x50\x00";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_FLOAT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.f32, 32.0f);

    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
}

#    ifdef ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS
ANJ_UNIT_TEST(cbor_decoder_ll, timestamp_in_decimal_fraction_illegal) {
    static const char data[] = "\xC4\x82\x02\xC1\x03";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_DOUBLE);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}
#    endif // ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS

ANJ_UNIT_TEST(cbor_decoder_ll, decimal_fraction_in_timestamp_illegal) {
    static const char data[] = "\xC1\xC4\x82\x02\x03";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

#    ifdef ANJ_WITH_CBOR_DECODE_STRING_TIME
ANJ_UNIT_TEST(cbor_decoder_ll, string_time_simple) {
    static const char data[] = "\xC0\x74"
                               "2003-12-13T18:30:02Z";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 1071340202);
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_with_fraction) {
    static const char data[] = "\xC0\x77"
                               "2003-12-13T18:30:02.25Z";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value.value.f64, 1071340202.25);
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_with_timezone) {
    static const char data[] = "\xC0\x78\x19"
                               "2003-12-13T18:30:02+01:00";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 1071336602);
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_with_fraction_and_timezone) {
    static const char data[] = "\xC0\x78\x1C"
                               "2003-12-13T18:30:02.25+01:00";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value.value.f64, 1071336602.25);
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_leap_year) {
    static const char data[] = "\xC0\x74"
                               "2004-12-13T18:30:02Z";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value.value.u64, 1102962602);
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_max_length) {
    static const char data[] = "\xC0\x78\x23"
                               "2024-01-16T13:22:40.763933581+01:00";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
    ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value.value.f64, 1705407760.763933581);
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_too_long) {
    static const char data[] = "\xC0\x78\x24"
                               "2024-01-16T13:22:40.7639335809+01:00";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_garbled_input) {
    static const char data[] = "\xC0\x78\x23"
                               "2024-01-16T13:22:40.763933581+01:00";
    for (size_t i = 3; i < sizeof(data) - 1; ++i) {
        char garbled_data[sizeof(data)];
        memcpy(garbled_data, data, sizeof(data));
        garbled_data[i] = 'x';

        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, garbled_data, sizeof(garbled_data) - 1, true));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                              _ANJ_IO_ERR_FORMAT);
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_limits) {
    struct {
        const char *data;
        _anj_cbor_ll_value_type_t type;
        double value;
    } data[] = { { "0000-01-01T00:00:00.000000000-99:59",
                   ANJ_CBOR_LL_VALUE_NEGATIVE_INT, -62166859260 },
                 { "0000-01-01T00:00:00.000000000-00:00",
                   ANJ_CBOR_LL_VALUE_NEGATIVE_INT, -62167219200 },
                 { "0000-01-01T00:00:00.000000000+00:00",
                   ANJ_CBOR_LL_VALUE_NEGATIVE_INT, -62167219200 },
                 { "0000-01-01T00:00:00.000000000+99:59",
                   ANJ_CBOR_LL_VALUE_NEGATIVE_INT, -62167579140 },
                 { "9999-12-31T23:59:60.999999999-99:59",
                   ANJ_CBOR_LL_VALUE_DOUBLE, 253402660740.999999999 },
                 { "9999-12-31T23:59:60.999999999-00:00",
                   ANJ_CBOR_LL_VALUE_DOUBLE, 253402300800.999999999 },
                 { "9999-12-31T23:59:60.999999999+00:00",
                   ANJ_CBOR_LL_VALUE_DOUBLE, 253402300800.999999999 },
                 { "9999-12-31T23:59:60.999999999+99:59",
                   ANJ_CBOR_LL_VALUE_DOUBLE, 253401940860.999999999 } };
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(data); ++i) {
        uint8_t buf[258] = "\xC0\x78";
        buf[2] = (uint8_t) strlen(data[i].data);
        memcpy(buf + 3, data[i].data, buf[2]);

        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_feed_payload(&ctx, buf, buf[2] + 3, true));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, data[i].type);
        switch (value.type) {
        case ANJ_CBOR_LL_VALUE_NEGATIVE_INT:
            ANJ_UNIT_ASSERT_EQUAL(value.value.i64, (int64_t) data[i].value);
            break;
        case ANJ_CBOR_LL_VALUE_DOUBLE:
            ANJ_UNIT_ASSERT_EQUAL(value.value.f64, data[i].value);
            break;
        default:
            ANJ_UNIT_ASSERT_TRUE(false);
        }
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_out_of_limits) {
    const char *data[] = { "2024-00-16T13:22:40.763933581+01:00",
                           "2024-13-16T13:22:40.763933581+01:00",
                           "2024-01-00T13:22:40.763933581+01:00",
                           "2024-01-32T13:22:40.763933581+01:00",
                           "2024-01-16T25:22:40.763933581+01:00",
                           "2024-01-16T13:60:40.763933581+01:00",
                           "2024-01-16T13:22:61.763933581+01:00",
                           "2024-01-16T13:22:40.763933581+01:60" };
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(data); ++i) {
        uint8_t buf[258] = "\xC0\x78";
        buf[2] = (uint8_t) strlen(data[i]);
        memcpy(buf + 3, data[i], buf[2]);

        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_feed_payload(&ctx, buf, buf[2] + 3, true));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                              _ANJ_IO_ERR_FORMAT);
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_superfluous_data) {
    static const char data[] = "\xC0\x75"
                               "2003-12-13T18:30:02Z0";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_split) {
    static const char data[] = "\xC0\x78\x23"
                               "2024-01-16T13:22:40.763933581+01:00";
    for (size_t split = 0; split < 9; ++split) {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_feed_payload(&ctx, data, split, false));

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data + split, sizeof(data) - 1 - split, true));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE);
        ANJ_UNIT_ASSERT_EQUAL(value.value.f64, 1705407760.763933581);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
    for (size_t split = 9; split < 38; ++split) {
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_feed_payload(&ctx, data, split, false));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data + split, sizeof(data) - 1 - split, true));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE);
        ANJ_UNIT_ASSERT_EQUAL(value.value.f64, 1705407760.763933581);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
    {
        // split == sizeof(data) - 1
        _anj_cbor_ll_decoder_t ctx;
        anj_cbor_ll_decoder_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
                &ctx, data, sizeof(data) - 1, false));

        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

        _anj_cbor_ll_value_type_t type;
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_current_value_type(&ctx, &type));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

        _anj_cbor_ll_number_t value;
        ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_number(&ctx, &value));
        ANJ_UNIT_ASSERT_EQUAL(value.type, ANJ_CBOR_LL_VALUE_DOUBLE);
        ANJ_UNIT_ASSERT_EQUAL(value.value.f64, 1705407760.763933581);

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_cbor_ll_decoder_feed_payload(&ctx, NULL, 0, true));

        ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_errno(&ctx), _ANJ_IO_EOF);
    }
}

ANJ_UNIT_TEST(cbor_decoder_ll, string_time_wrong_type) {
    static const char data[] = "\xC0\x54"
                               "2003-12-13T18:30:02Z";
    _anj_cbor_ll_decoder_t ctx;
    anj_cbor_ll_decoder_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_feed_payload(
            &ctx, data, sizeof(data) - 1, true));

    ANJ_UNIT_ASSERT_SUCCESS(anj_cbor_ll_decoder_errno(&ctx));

    _anj_cbor_ll_value_type_t type;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_cbor_ll_decoder_current_value_type(&ctx, &type));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_CBOR_LL_VALUE_TIMESTAMP);

    _anj_cbor_ll_number_t value;
    ANJ_UNIT_ASSERT_EQUAL(anj_cbor_ll_decoder_number(&ctx, &value),
                          _ANJ_IO_ERR_FORMAT);
}
#    endif // ANJ_WITH_CBOR_DECODE_STRING_TIME

#endif // defined(ANJ_WITH_SENML_CBOR) || defined(ANJ_WITH_LWM2M_CBOR) ||
       // defined(ANJ_WITH_CBOR)
