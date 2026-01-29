/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdint.h>
#include <string.h>

#include <anj/defs.h>

#include "../../../../src/anj/coap/coap.h"
#include "../../../../src/anj/coap/options.h"

#include <anj_unit_test.h>

#define _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(Name, OptionsSize, MsgBuffSize) \
    anj_coap_option_t _Opt##Name[OptionsSize];                                 \
    uint8_t _OptionMsgBuffer##Name[MsgBuffSize] = { 0 };                       \
    anj_coap_options_t Name = {                                                \
        .options_size = OptionsSize,                                           \
        .options_number = 0,                                                   \
        .options = _Opt##Name,                                                 \
        .buff_size = MsgBuffSize,                                              \
        .buff_begin = (void *) _OptionMsgBuffer##Name                          \
    }

ANJ_UNIT_TEST(coap_options, insert_last) {
    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts, 10, 50);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_data(&opts, 0, "0", 1)); // num  0
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_string(&opts, 1, "1")); // num  1
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u16(&opts, 3, 0x1234)); // num  3
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u32(&opts, 4, 0x12345678)); // num  4

    const uint8_t EXPECTED[] = "\x01\x30"             // num  0 (+0), "0"
                               "\x11\x31"             // num  1 (+1), "1"
                               "\x22\x12\x34"         // num  3 (+1), 0x1234
                               "\x14\x12\x34\x56\x78" // num  4 (+1), 0x12345678
            ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(opts.buff_begin, EXPECTED,
                                      sizeof(EXPECTED) - 1);
}

ANJ_UNIT_TEST(coap_options, insert_first) {
    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts, 10, 50);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u32(&opts, 4, 0x12345678)); // num  4
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u16(&opts, 3, 0x1234)); // num  3
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_string(&opts, 1, "1")); // num  1
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_data(&opts, 0, "0", 1)); // num  0

    const uint8_t EXPECTED[] = "\x01\x30"             // num  0 (+0), "0"
                               "\x11\x31"             // num  1 (+1), "1"
                               "\x22\x12\x34"         // num  3 (+1), 0x1234
                               "\x14\x12\x34\x56\x78" // num  4 (+1), 0x12345678
            ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(opts.buff_begin, EXPECTED,
                                      sizeof(EXPECTED) - 1);
}

ANJ_UNIT_TEST(coap_options, insert_not_enough_space) {
    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts, 10, 10);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(&opts, 1, "123456"));
    ANJ_UNIT_ASSERT_FAILED(_anj_coap_options_add_string(&opts, 0, "123456"));
}

ANJ_UNIT_TEST(coap_options, insert_not_enough_space_in_options_array) {
    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts, 2, 50);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(&opts, 1, "123456"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(&opts, 2, "123456"));
    ANJ_UNIT_ASSERT_FAILED(_anj_coap_options_add_string(&opts, 0, "123456"));
}

ANJ_UNIT_TEST(coap_options, insert_middle) {
    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts, 10, 50);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_data(&opts, 0, "0", 1)); // num  0
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_string(&opts, 1, "1")); // num  1
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u16(&opts, 12, 0x4444)); // num 12
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u32(&opts, 4, 0x12345678)); // num  4
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u16(&opts, 3, 0x1234)); // num  3

    const uint8_t EXPECTED[] = "\x01\x30"             // num  0 (+0), "0"
                               "\x11\x31"             // num  1 (+1), "1"
                               "\x22\x12\x34"         // num  3 (+1), 0x1234
                               "\x14\x12\x34\x56\x78" // num  4 (+1), 0x12345678
                               "\x82\x44\x44"         // num 12 (+8), 0x4444
            ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(opts.buff_begin, EXPECTED,
                                      sizeof(EXPECTED) - 1);
}

ANJ_UNIT_TEST(coap_options, insert_repeated) {
    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts, 10, 50);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_data(&opts, 0, "0", 1)); // num  0
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_string(&opts, 1, "1")); // num  1
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u16(&opts, 12, 0x4444)); // num 12
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u32(&opts, 4, 0x12345678)); // num  4
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u16(&opts, 3, 0x1234)); // num  3
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_string(&opts, 1, "2")); // num  1
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_string(&opts, 1, "3")); // num  1

    const uint8_t EXPECTED[] = "\x01\x30"             // num  0 (+0), "0"
                               "\x11\x31"             // num  1 (+1), "1"
                               "\x01\x32"             // num  1 (+1), "2"
                               "\x01\x33"             // num  1 (+1), "3"
                               "\x22\x12\x34"         // num  3 (+1), 0x1234
                               "\x14\x12\x34\x56\x78" // num  4 (+1), 0x12345678
                               "\x82\x44\x44"         // num 12 (+8), 0x4444
            ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(opts.buff_begin, EXPECTED,
                                      sizeof(EXPECTED) - 1);
}

ANJ_UNIT_TEST(coap_options, content_format) {
    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts, 10, 50);

    uint16_t content_format_1 = _ANJ_COAP_FORMAT_PLAINTEXT;
    uint16_t content_format_2 = _ANJ_COAP_FORMAT_CBOR;
    uint16_t content_format_3 = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_u16(
            &opts, _ANJ_COAP_OPTION_CONTENT_FORMAT, content_format_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_u16(
            &opts, _ANJ_COAP_OPTION_CONTENT_FORMAT, content_format_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_u16(
            &opts, _ANJ_COAP_OPTION_CONTENT_FORMAT, content_format_3));

    const uint8_t EXPECTED[] = "\xC1\x3c"
                               "\x00"
                               "\x02\x2D\x18";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(opts.buff_begin, EXPECTED,
                                      sizeof(EXPECTED) - 1);
}

ANJ_UNIT_TEST(coap_options, get_string) {
    char opt1[] = "opt1";
    char opt2[] = "opt_2";

    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts, 5, 20);
    memset(opts.buff_begin, 0xFF,
           20); // _anj_coap_options_decode end on 0xFF marker or buffer end

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(
            &opts, _ANJ_COAP_OPTION_URI_PATH, opt1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(
            &opts, _ANJ_COAP_OPTION_URI_PATH, opt2));

    char buffer[32];
    size_t option_size;
    size_t iterator = 0;
    size_t bytes_read = 0;
    size_t msg_size = 100;
    _ANJ_COAP_OPTIONS_INIT_EMPTY(opts_r, 5);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_decode(&opts_r, opts.buff_begin,
                                                     msg_size, &bytes_read));

    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_options_get_string_iterate(
                                  &opts_r, _ANJ_COAP_OPTION_URI_PATH, &iterator,
                                  &option_size, buffer, sizeof(buffer)),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(option_size, sizeof(opt1));
    ANJ_UNIT_ASSERT_EQUAL_BYTES(buffer, opt1);

    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_options_get_string_iterate(
                                  &opts_r, _ANJ_COAP_OPTION_URI_PATH, &iterator,
                                  &option_size, buffer, sizeof(buffer)),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(option_size, sizeof(opt2));
    ANJ_UNIT_ASSERT_EQUAL_BYTES(buffer, opt2);

    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_options_get_string_iterate(
                                  &opts_r, _ANJ_COAP_OPTION_URI_PATH, &iterator,
                                  &option_size, buffer, sizeof(buffer)),
                          _ANJ_COAP_OPTION_MISSING);
}

ANJ_UNIT_TEST(coap_options, get_many_options) {
    char opt1[] = "1";
    char opt2[] = "_2";
    char opt3[] = "_3____________________";
    uint8_t opt4 = 0x22;
    uint16_t opt5 = 0x2277;
    uint32_t opt6 = 0x21372137;

    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts, 6, 100);
    memset(opts.buff_begin, 0xFF,
           100); // _anj_coap_options_decode end on 0xFF marker or buffer end

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(
            &opts, _ANJ_COAP_OPTION_PROXY_URI, opt1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(
            &opts, _ANJ_COAP_OPTION_MAX_AGE, opt2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_data(
            &opts, _ANJ_COAP_OPTION_MAX_AGE, opt3, sizeof(opt3) - 1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_data(
            &opts, _ANJ_COAP_OPTION_URI_PORT, &opt4, 1));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u16(&opts, _ANJ_COAP_OPTION_URI_PORT, opt5));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_options_add_u32(&opts, _ANJ_COAP_OPTION_OBSERVE, opt6));

    char buffer[100];
    size_t option_size;
    size_t iterator = 0;
    size_t bytes_read = 0;
    size_t msg_size = 100;
    _ANJ_COAP_OPTIONS_INIT_EMPTY(opts_r, 6);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_decode(&opts_r, opts.buff_begin,
                                                     msg_size, &bytes_read));
    ANJ_UNIT_ASSERT_EQUAL(opts_r.options_number, 6);

    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_options_get_string_iterate(
                                  &opts_r, _ANJ_COAP_OPTION_PROXY_URI, NULL,
                                  &option_size, buffer, sizeof(buffer)),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(option_size, sizeof(opt1));
    ANJ_UNIT_ASSERT_EQUAL_BYTES(buffer, opt1);

    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_options_get_string_iterate(
                                  &opts_r, _ANJ_COAP_OPTION_MAX_AGE, &iterator,
                                  &option_size, buffer, sizeof(buffer)),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(option_size, sizeof(opt2));
    ANJ_UNIT_ASSERT_EQUAL_BYTES(buffer, opt2);
    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_options_get_string_iterate(
                                  &opts_r, _ANJ_COAP_OPTION_MAX_AGE, &iterator,
                                  &option_size, buffer, sizeof(buffer)),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(option_size, sizeof(opt3));
    ANJ_UNIT_ASSERT_EQUAL_BYTES(buffer, opt3);

    iterator = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_options_get_data_iterate(
                                  &opts_r, _ANJ_COAP_OPTION_URI_PORT, &iterator,
                                  &option_size, buffer, sizeof(buffer)),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(option_size, sizeof(opt4));
    ANJ_UNIT_ASSERT_EQUAL(buffer[0], opt4);
    uint16_t u16_value = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_options_get_u16_iterate(
                                  &opts_r, _ANJ_COAP_OPTION_URI_PORT, &iterator,
                                  &u16_value),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(u16_value, opt5);
    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_options_get_data_iterate(
                                  &opts_r, _ANJ_COAP_OPTION_URI_PORT, &iterator,
                                  &option_size, buffer, sizeof(buffer)),
                          _ANJ_COAP_OPTION_MISSING);

    uint32_t u32_value = 0;
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_coap_options_get_u32_iterate(&opts_r, _ANJ_COAP_OPTION_OBSERVE,
                                              NULL, &u32_value),
            0);
    ANJ_UNIT_ASSERT_EQUAL(u32_value, opt6);
}

ANJ_UNIT_TEST(coap_options, get_options_errors_check) {
    char opt1[] = "1";
    char opt2[] = "_2";
    char opt3[] = "_3____________________";

    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts1, 2, 100);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(
            &opts1, _ANJ_COAP_OPTION_PROXY_URI, opt1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(
            &opts1, _ANJ_COAP_OPTION_MAX_AGE, opt2));
    ANJ_UNIT_ASSERT_FAILED(_anj_coap_options_add_data(
            &opts1, _ANJ_COAP_OPTION_MAX_AGE, opt3, sizeof(opt3) - 1));

    _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(opts2, 3, 10);
    memset(opts2.buff_begin, 0xFF, 10);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(
            &opts2, _ANJ_COAP_OPTION_PROXY_URI, opt1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_add_string(
            &opts2, _ANJ_COAP_OPTION_MAX_AGE, opt2));
    ANJ_UNIT_ASSERT_FAILED(_anj_coap_options_add_data(
            &opts2, _ANJ_COAP_OPTION_MAX_AGE, opt3, sizeof(opt3) - 1));

    size_t bytes_read = 0;
    _ANJ_COAP_OPTIONS_INIT_EMPTY(opts_r_1, 5);
    _ANJ_COAP_OPTIONS_INIT_EMPTY(opts_r_2, 1);
    _ANJ_COAP_OPTIONS_INIT_EMPTY(opts_r_3, 2);
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_options_decode(&opts_r_1, opts1.buff_begin, 100,
                                     &bytes_read)); // no 0xFF marker
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_options_decode(&opts_r_2, opts2.buff_begin, 100,
                                     &bytes_read)); // opt array too small
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_options_decode(
            &opts_r_3, opts2.buff_begin, 100, &bytes_read));
}
