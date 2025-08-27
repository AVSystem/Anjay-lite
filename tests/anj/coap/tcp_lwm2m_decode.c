/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <anj/defs.h>

#include "../../src/anj/coap/coap.h"
#include "../io/bigdata.h"

#include <anj_unit_test.h>

ANJ_UNIT_TEST(anj_decode_tcp, decode_write_replace) {
    uint8_t MSG[] = "\xB8"                             // msg_len 11, tkl 8
                    "\x03"                             // PUT
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35"     // uri-path_1 URI_PATH 11 /5
                    "\x01\x30"     // uri-path_2             /0
                    "\x01\x31"     // uri-path_3             /1
                    "\x10"         // content_format 12 PLAINTEXT 0
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_REPLACE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 11);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 3);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[18]);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_write_with_extra_bytes) {
    uint8_t MSG[] = "\xB8"                             // msg_len 11, tkl 8
                    "\x03"                             // PUT
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35"     // uri-path_1 URI_PATH 11 /5
                    "\x01\x30"     // uri-path_2             /0
                    "\x01\x31"     // uri-path_3             /1
                    "\x10"         // content_format 12 PLAINTEXT 0
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
                    "\xAA\xBB\xCC" // additional out_data
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_TRUE(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset)
            == _ANJ_INF_COAP_TCP_MORE_DATA_PRESENT);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(MSG + offset,
                                      ((uint8_t[]){ 0xAA, 0xBB, 0xCC }), 3);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_write_replace_with_block) {
    uint8_t MSG[] = "\xD8"                             // msg_len 13, tkl 8
                    "\x01"                             // extended length 14
                    "\x03"                             // PUT
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35"     // uri-path_1 URI_PATH 11 /5
                    "\x01\x30"     // uri-path_2             /0
                    "\x01\x31"     // uri-path_3             /1
                    "\x10"         // content_format 12 PLAINTEXT 0
                    "\xD1\x02\xEe" // BLOCK1 27 NUM:14 M:1 SZX:1024
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_REPLACE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 14);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 3);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[22]);
    ANJ_UNIT_ASSERT_EQUAL(out_data.block.block_type, ANJ_OPTION_BLOCK_1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.block.size, 1024);
    ANJ_UNIT_ASSERT_EQUAL(out_data.block.more_flag, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.block.number, 14);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_discover) {
    uint8_t MSG[] = "\x68"                             // msg_len 6, tkl 8
                    "\x01"                             // GET
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35" // uri-path_1 URI_PATH 11 /5
                    "\x01\x35" // uri-path_2             /5
                    "\x61\x28" // accept ACCEPT 17 LINK_FORMAT 40
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_DISCOVER);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 6);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_LINK_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.discover_attr.has_depth, false);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_discover_with_depth) {
    uint8_t MSG[] = "\xD8"                             // msg_len 13, tkl 8
                    "\x01"                             // extended length 14
                    "\x01"                             // GET
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35" // uri-path_1 URI_PATH 11 /5
                    "\x01\x35" // uri-path_2             /5
                    "\x47\x64\x65\x70\x74\x68\x3D\x32" // URI_QUERY 15 depth=2
                    "\x21\x28" // accept ACCEPT 17 LINK_FORMAT 40
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_DISCOVER);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 14);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_LINK_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.discover_attr.has_depth, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.discover_attr.depth, 2);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_bootstrap_finish) {
    uint8_t MSG[] = "\x38"                             // msg_len 3, tkl 8
                    "\x02"                             // POST
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB2\x62\x73" // uri-path_1 URI_PATH 11 /bs
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_BOOTSTRAP_FINISH);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_read_composite) {
    uint8_t MSG[] = "\xB8"                             // msg_len 11, tkl 8
                    "\x05"                             // FETCH
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xC0"                     // content_format 12 PLAINTEXT 0
                    "\x52\x2D\x17"             // accept 17 LWM2M_JSON 11543
                    "\xFF"                     // payload marker
                    "\x33\x44\x55\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_READ_COMP);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 11);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_OMA_LWM2M_JSON);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 6);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[15]);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_observe_with_pmin_pmax) {
    uint8_t MSG[] =
            "\xD8"                                 // msg_len 13, tkl 8
            "\x0E"                                 // extended length 27
            "\x01"                                 // GET
            "\x12\x34\x56\x78\x11\x11\x11\x11"     // token
            "\x61\x00"                             // observe 6 = 0
            "\x51\x35"                             // uri-path_1 URI_PATH 11 /5
            "\x01\x35"                             // uri-path_2             /5
            "\x01\x31"                             // uri-path_3             /1
            "\x48\x70\x6D\x69\x6E\x3D\x32\x30\x30" // URI_QUERY 15 pmin=200
            "\x09\x70\x6D\x61\x78\x3D\x34\x32\x30\x30" // URI_QUERY 15 pmax=4200
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_INF_OBSERVE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 27);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_con, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.min_period, 200);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_period, 4200);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_observe_composite_with_params) {
    uint8_t MSG[] =
            "\xD8"                                     // msg_len 13, tkl 8
            "\x28"                                     // extended length 53
            "\x05"                                     // FETCH
            "\x12\x34\x56\x78\x11\x11\x11\x11"         // token
            "\x61\x00"                                 // observe 6 = 0
            "\x97\x70\x6D\x69\x6E\x3D\x32\x30"         // URI_QUERY 15 pmin=20
            "\x07\x65\x70\x6D\x69\x6E\x3D\x31"         // URI_QUERY 15 epmin=1
            "\x07\x65\x70\x6D\x61\x78\x3D\x32"         // URI_QUERY 15 epmax=2
            "\x05\x63\x6F\x6E\x3D\x31"                 // URI_QUERY 15 con=1
            "\x09\x70\x6D\x61\x78\x3D\x31\x32\x30\x30" // URI_QUERY 15 pmax=1200
            "\xFF"                                     // payload marker
            "\x77\x44\x55\x33\x44\x55\x33\x33\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_INF_OBSERVE_COMP);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 53);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 10);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[54]);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_con, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_eval_period,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_eval_period,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.min_period, 20);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_period, 1200);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.min_eval_period, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_eval_period, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.con, 1);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_cancel_observation) {
    uint8_t MSG[] = "\x88"                             // msg_len 8, tkl 8
                    "\x01"                             // GET
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\x61\x01"                         // observe 6 = 1
                    "\x51\x35" // uri-path_1 URI_PATH 11 /5
                    "\x01\x35" // uri-path_2             /5
                    "\x01\x31" // uri-path_3             /1
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_INF_CANCEL_OBSERVE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 1);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_cancel_composite) {
    uint8_t MSG[] = "\xD8"                             // msg_len 13, tkl 8
                    "\x00"                             // extended length 13
                    "\x05"                             // FETCH
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\x61\x01"                         // observe 6 = 1
                    "\xFF"                             // payload marker
                    "\x77\x44\x55\x33\x44\x55\x33\x33\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_INF_CANCEL_OBSERVE_COMP);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 10);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[14]);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_write_partial) {
    uint8_t MSG[] = "\xA8"                             // msg_len 10, tkl 8
                    "\x02"                             // POST
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB2\x31\x35" // uri-path_1 URI_PATH 11 /15
                    "\x01\x32"     // uri-path_2             /2
                    "\x10"         // content_format 12 PLAINTEXT 0
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_PARTIAL_UPDATE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 10);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 15);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 3);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[17]);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_write_partial_with_resource_path) {
    uint8_t MSG[] = "\xD8"                             // msg_len 13, tkl 8
                    "\x00"                             // extended length 13
                    "\x02"                             // POST
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB2\x31\x35" // uri-path_1 URI_PATH    /15
                    "\x01\x32"     // uri-path_2             /2
                    "\x01\x34"     // uri-path_3             /4
                    "\x11\x3C"     // content_format 12 FORMAT_CBOR 60
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_PARTIAL_UPDATE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 15);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_CBOR);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 3);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[21]);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_write_attributes) {
    uint8_t MSG[] =
            "\xD8"                             // msg_len 13, tkl 8
            "\x4F"                             // extended length 92
            "\x03"                             // PUT
            "\x12\x34\x56\x78\x11\x11\x11\x11" // token
            "\xB2\x31\x35"                     // uri-path_1 URI_PATH 11 /15
            "\x01\x32"                         // uri-path_2             /2
            "\x02\x31\x32"                     // uri-path_3             /12
            "\x47\x70\x6D\x69\x6E\x3D\x32\x30" // URI_QUERY 15 pmin=20
            "\x07\x65\x70\x6D\x69\x6E\x3D\x31" // URI_QUERY 15 epmin=1
            "\x07\x65\x70\x6D\x61\x78\x3D\x32" // URI_QUERY 15 epmax=2
            "\x05\x63\x6F\x6E\x3D\x31"         // URI_QUERY 15 con=1
            "\x07\x67\x74\x3D\x32\x2E\x38\x35" // URI_QUERY 15 gt=2.85
            "\x09\x6C\x74\x3D\x33\x33\x33\x33\x2E\x38" // URI_QUERY 15 lt=3333.8
            "\x07\x73\x74\x3D\x2D\x30\x2E\x38"         // URI_QUERY 15 st=-0.8
            "\x06\x65\x64\x67\x65\x3D\x30"             // URI_QUERY 15 edge=0
            "\x0A\x68\x71\x6D\x61\x78\x3D\x37\x37\x37\x37" // URI_QUERY 15
                                                           // hqmax=7777
            "\x09\x70\x6D\x61\x78\x3D\x31\x32\x30\x30" // URI_QUERY 15 pmax=1200
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_ATTR);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 92);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 15);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 12);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);

    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_greater_than,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_less_than, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_step, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_eval_period,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_eval_period,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_edge, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_con, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_hqmax, true);

    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.min_period, 20);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_period, 1200);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.min_eval_period, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_eval_period, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.edge, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.con, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.hqmax, 7777);

    ANJ_UNIT_ASSERT_EQUAL(
            (int) (100 * out_data.attr.notification_attr.greater_than), 285);
    ANJ_UNIT_ASSERT_EQUAL(
            (int) (100 * out_data.attr.notification_attr.less_than), 333380);
    ANJ_UNIT_ASSERT_EQUAL((int) (100 * out_data.attr.notification_attr.step),
                          -80);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_write_composite) {
    uint8_t MSG[] = "\xD8"                             // msg_len 13, tkl 8
                    "\x00"                             // extended length 13
                    "\x07"                             // IPATCH
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xC1\x3C" // content_format 12 FORMAT_CBOR 60
                    "\xFF"     // payload marker
                    "\x77\x44\x55\x33\x44\x55\x33\x33\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_COMP);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 10);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[14]);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_execute) {
    uint8_t MSG[] = "\xD8"                             // msg_len 13, tkl 8
                    "\x07"                             // extended length 20
                    "\x02"                             // POST
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB2\x31\x35" // uri-path_1 URI_PATH 11 /15
                    "\x01\x32"     // uri-path_2             /2
                    "\x02\x31\x32" // uri-path_3             /12
                    "\x10"         // content_format 12 PLAIN-TEXT 0
                    "\xFF"         // payload marker
                    "\x77\x44\x55\x33\x44\x55\x33\x33\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_EXECUTE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 20);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 15);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 12);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 10);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[21]);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_create) {
    uint8_t MSG[] = "\xD8"                             // msg_len 13, tkl 8
                    "\x04"                             // extended length 17
                    "\x02"                             // POST
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB5\x33\x33\x36\x33\x39" // uri-path_1 URI_PATH 11 /33639
                    "\x11\x3C" // content_format 12 FORMAT_CBOR 60
                    "\xFF"     // payload marker
                    "\x76\x44\x55\x33\x44\x55\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_CREATE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 17);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 33639);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 8);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[20]);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_delete) {
    uint8_t MSG[] = "\xD8"                             // msg_len 13, tkl 8
                    "\x04"                             // extended length 17
                    "\x04"                             // DELETE
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB5\x33\x33\x36\x33\x39" // uri-path_1 URI_PATH 11 /33639
                    "\x01\x31"                 // uri-path_1 URI_PATH 11 /1
                    "\xFF"                     // payload marker
                    "\x76\x44\x55\x33\x44\x55\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_DELETE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 17);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 33639);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 8);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[20]);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_response) {
    uint8_t MSG[] = "\x08"                             // msg_len 0, tkl 8
                    "\x44"                             // CHANGED
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CHANGED);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_error_compromited_msg) {
    uint8_t MSG[] = "\xA4"             // msg_len 10, tkl 4
                    "\x43"             // VALID
                    "\x12\x34\x56\x78" // token
                    "\x51\x30"         // opt1
                    "\x53\x31\x32\x33" // opt2
                    "\xFF"             // payload marker
                    "\x78\x78\x78"     // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    uint8_t aux;
    // incorrect token length
    aux = MSG[0];
    MSG[0] = 0x53;
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));
    MSG[0] = aux;

    // no payload marker
    aux = MSG[12];
    MSG[12] = 0x11;
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));
    MSG[12] = aux;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_empty_message) {
    uint8_t MSG[] = "\x00" // msg_len 0, tkl 0
                    "\x00" // empty code 2.4
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_COAP_EMPTY_MSG);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_EMPTY);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_response_with_etag) {
    uint8_t MSG[] = "\x48"                             // msg_len 4, tkl 8
                    "\x44"                             // CHANGED
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\x43\x33\x33\x32"                 // etag 3 332
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.downloader_attr.etag.size, 3);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            "332", out_data.attr.downloader_attr.etag.bytes, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CHANGED);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_response_with_etag_and_size) {
    uint8_t MSG[] = "\x98"                             // msg_len 4, tkl 8
                    "\x44"                             // CHANGED
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\x43\x33\x33\x32"                 // etag 3 332
                    "\xD3\x0B\x31\x3c\x60"             // size2 3226720
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.downloader_attr.etag.size, 3);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            "332", out_data.attr.downloader_attr.etag.bytes, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.downloader_attr.total_size, 3226720);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CHANGED);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_response_with_location_path) {
    uint8_t MSG[] = "\x88"                             // msg_len 8, tkl 8
                    "\x41"                             // CREATED
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\x82\x72\x64"                     // LOCATION_PATH 8 /rd
                    "\x04\x35\x61\x33\x66"             // LOCATION_PATH 8 /5a3f
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }), 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CREATED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.location_path.location_len[0], 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.location_path.location_len[1], 4);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(out_data.location_path.location[0], "rd",
                                      2);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(out_data.location_path.location[1],
                                      "5a3f", 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.location_path.location_count, 2);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_error_to_long_uri) {
    uint8_t MSG[] = "\xD4"             // msg_len 13, tkl 4
                    "\x00"             // extended length 13
                    "\x01"             // GET
                    "\x12\x34\x56\x78" // token
                    "\xB1\x33"         // uri-path_1 URI_PATH 11 /3
                    "\x01\x33"         // uri-path_2             /3
                    "\x02\x31\x31"     // uri-path_3             /11
                    "\x02\x31\x31"     // uri-path_4             /11
                    "\x02\x31\x31"     // uri-path_5             /11
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_error_incorrect_post) {
    uint8_t MSG[] = "\x04"             // msg_len 0, tkl 4
                    "\x02"             // POST
                    "\x12\x34\x56\x78" // token
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_error_attr) {
    uint8_t MSG[] =
            "\x98"                                 // msg_len 9, tkl 8
            "\x03"                                 // PUT
            "\x12\x34\x56\x78\x11\x11\x11\x11"     // token
            "\xD7\x02\x70\x6D\x69\x6E\x3D\x6E\x30" // URI_QUERY 15 pmin=n0
            ;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_read_parsing_single_bytes) {
    uint8_t MSG[] = "\xD4"             // msg_len 13, tkl 4
                    "\x00"             // extended length 13
                    "\x01"             // GET
                    "\x12\x34\x56\x78" // token
                    "\xB1\x33"         // uri-path_1 URI_PATH 11 /3
                    "\x01\x33"         // uri-path_2             /3
                    "\x02\x31\x31"     // uri-path_3             /11
                    "\x02\x31\x31"     // uri-path_4             /11
                    "\x62\x01\x40"     // accept ACCEPT 17 SENML_ETCH_JSON 320
            ;

    size_t i = 1;
    size_t len = sizeof(MSG) - 1;
    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    for (; i < len - 1; i++) {
        ANJ_UNIT_ASSERT_TRUE(_anj_coap_decode_tcp(MSG, i * sizeof(MSG[0]),
                                                  &out_data, &offset)
                             == _ANJ_INF_COAP_TCP_INCOMPLETE_MESSAGE);
    }
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, i + sizeof(MSG[0]), &out_data, &offset));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 13);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 11);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[3], 11);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_SENML_ETCH_JSON);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes, ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78 }), 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_incorrect_message) {
    uint8_t INCORRECT_MSG[][21] = {
        "\x26\x5A\x39\x9D\x90\xF8\x9C\x12\x96\x09\x7A\xB2\xCE\x03\xEA\x65\xE3"
        "\x5E\x17\xD4",
        "\x87\xCD\x31\xE3\x5A\xA6\x09\x5C\x94\xD2\x22\x53\x40\xC8\xDA\x3C\xA2"
        "\x3A\xCA\x45",
        "\xE8\x67\x26\x52\x82\x5E\x88\x24\x39\xE3\x94\xBE\x9B\x31\xFB\x6F\x36"
        "\xF9\xA5\x9e",
        "\xFE\xFD\x74\x98\xD3\x44\x78\x2A\xEF\xFF\x3F\x28\x87\x19\x61\x7C\xAA"
        "\xBA\x77\xE8",
        "\x6F\xD8\x1A\xF1\x24\x27\x57\xA3\xE6\x95\xBB\x36\xAF\xE2\xE5\x65\x1A"
        "\x96\x1A\x81",
        "\xD9\x66\x84\x6A\x42\x9D\x5F\x83\x9E\x02\xD8\x08\x63\x19\x01\xB1\x9A"
        "\xD7\x82\x60",
        "\x9B\x5F\x66\xF5\x40\x91\x13\x80\x0E\xFF\x73\xC5\x50\x64\x23\xf3\x6E"
        "\x13\x57\xDc",
        "\xE4\x95\x17\xB8\x3E\x9F\xE6\xA8\xE4\x34\x89\x71\x84\x0A\xBF\x23\x90"
        "\x08\x11\xf7",
        "\x0F\xB8\x7B\x78\x63\x34\x24\xCA\x7E\x89\x76\xC1\x31\x92\x61\x48\x8F"
        "\x3B\x18\xAc",
        "\x81\xC4\x55\x4C\x36\x13\xD7\x57\x2E\x88\x78\x30\x35\xA8\xE6\x64\x9D"
        "\x5B\x66\xBe",
    };

    size_t len_incorrect_msg = sizeof(INCORRECT_MSG) / sizeof(*INCORRECT_MSG);
    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    for (size_t i = 0; i < len_incorrect_msg; i++) {
        ANJ_UNIT_ASSERT_FAILED(_anj_coap_decode_tcp(
                INCORRECT_MSG[i], len_incorrect_msg, &out_data, &offset));
    }
}

ANJ_UNIT_TEST(anj_decode_tcp, decode_compromited_message) {
    uint8_t MSG[] = "\xD8"                             // msg_len 13, tkl 8
                    "\x01"                             // extended length 14
                    "\x03"                             // PUT
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35"     // uri-path_1 URI_PATH 11 /5
                    "\x01\x30"     // uri-path_2             /0
                    "\x01\x31"     // uri-path_3             /1
                    "\x10"         // content_format 12 PLAINTEXT 0
                    "\xD1\x02\xEe" // BLOCK1 27 NUM:14 M:1 SZX:1024
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
            ;

    uint8_t temp = 0;
    size_t len = sizeof(MSG) - 1;
    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;

    // compromitation of msg_len
    temp = MSG[0];
    MSG[0] |= 0xDF;
    ANJ_UNIT_ASSERT_FAILED(_anj_coap_decode_tcp(MSG, len, &out_data, &offset));
    MSG[0] = temp;

    // compromitation of code
    temp = MSG[2];
    MSG[2] |= 0xFF;
    ANJ_UNIT_ASSERT_FAILED(_anj_coap_decode_tcp(MSG, len, &out_data, &offset));
    MSG[2] = temp;

    // compromitation of current options
    temp = MSG[17];
    MSG[17] |= 0x1F;
    ANJ_UNIT_ASSERT_FAILED(_anj_coap_decode_tcp(MSG, len, &out_data, &offset));
    MSG[17] = temp;
    MSG[18] |= 0xDF;
    ANJ_UNIT_ASSERT_FAILED(_anj_coap_decode_tcp(MSG, len, &out_data, &offset));
    ;
}

#define DATA300B DATA100B DATA100B DATA100B
ANJ_UNIT_TEST(anj_decode_tcp, decode_payload_extended_length_2bytes) {
    uint8_t MSG[] = "\xE8"                             // msg_len 14, tkl 8
                    "\x00\x28"                         // extended length 309
                    "\x02"                             // POST
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB5\x33\x33\x36\x33\x39" // uri-path_1 URI_PATH 11 /33639
                    "\x11\x3C" // content_format 12 FORMAT_CBOR 60
                    "\xFF"     // payload marker
            DATA300B;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));
    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_CREATE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length, 309);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 14);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 33639);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 300);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[21]);
}

#define DATA80kB \
    DATA10kB DATA10kB DATA10kB DATA10kB DATA10kB DATA10kB DATA10kB DATA10kB
ANJ_UNIT_TEST(anj_decode_tcp, decode_payload_extended_length_4bytes) {
    uint8_t MSG[] = "\xF8"                             // msg_len 15, tkl 8
                    "\x00\x00\x37\x7C"                 // extended length 80009
                    "\x02"                             // POST
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB5\x33\x33\x36\x33\x39" // uri-path_1 URI_PATH 11 /33639
                    "\x11\x3C" // content_format 12 FORMAT_CBOR 60
                    "\xFF"     // payload marker
            DATA80kB;

    _anj_coap_msg_t out_data = { 0 };
    size_t offset = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_tcp(MSG, sizeof(MSG) - 1, &out_data, &offset));
    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_CREATE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.extended_length,
                          80009);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.tcp.msg_len, 15);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 33639);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 80000);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[23]);
}

#pragma GCC diagnostic pop
