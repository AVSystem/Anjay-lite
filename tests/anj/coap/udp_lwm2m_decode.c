/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>

#include <anj/defs.h>

#include "../../src/anj/coap/coap.h"

#include <anj_unit_test.h>

ANJ_UNIT_TEST(anj_decode_udp, decode_read) {
    uint8_t MSG[] = "\x44"             // header v 0x01, Confirmable, tkl 4
                    "\x01\x21\x37"     // GET code 0.1, msg id 3721
                    "\x12\x34\x56\x78" // token
                    "\xB1\x33"         // uri-path_1 URI_PATH 11 /3
                    "\x01\x33"         // uri-path_2             /3
                    "\x02\x31\x31"     // uri-path_3             /11
                    "\x02\x31\x31"     // uri-path_4             /11
                    "\x62\x01\x40"     // accept ACCEPT 17 SENML_ETCH_JSON 320
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 11);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[3], 11);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_SENML_ETCH_JSON);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x2137);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes, ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78 }), 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_write_replace) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x03\x37\x21" // PUT code 0.1
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35"     // uri-path_1 URI_PATH 11 /5
                    "\x01\x30"     // uri-path_2             /0
                    "\x01\x31"     // uri-path_3             /1
                    "\x10"         // content_format 12 PLAINTEXT 0
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_REPLACE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 3);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[20]);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_write_replace_with_block) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x03\x37\x21" // PUT code 0.1
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35"     // uri-path_1 URI_PATH 11 /5
                    "\x01\x30"     // uri-path_2             /0
                    "\x01\x31"     // uri-path_3             /1
                    "\x10"         // content_format 12 PLAINTEXT 0
                    "\xd1\x02\xee" // BLOCK1 27 NUM:14 M:1 SZX:1024
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_REPLACE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 3);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[23]);
    ANJ_UNIT_ASSERT_EQUAL(out_data.block.block_type, ANJ_OPTION_BLOCK_1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.block.size, 1024);
    ANJ_UNIT_ASSERT_EQUAL(out_data.block.more_flag, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.block.number, 14);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_discover) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x01\x37\x21" // GET code 0.1
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35" // uri-path_1 URI_PATH 11 /5
                    "\x01\x35" // uri-path_2             /5
                    "\x61\x28" // accept ACCEPT 17 LINK_FORMAT 40
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_DISCOVER);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_LINK_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.discover_attr.has_depth, false);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_discover_with_depth) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x01\x37\x21" // GET code 0.1
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB1\x35" // uri-path_1 URI_PATH 11 /5
                    "\x01\x35" // uri-path_2             /5
                    "\x47\x64\x65\x70\x74\x68\x3d\x32" // URI_QUERY 15 depth=2
                    "\x21\x28" // accept ACCEPT 17 LINK_FORMAT 40
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_DISCOVER);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_LINK_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.discover_attr.has_depth, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.discover_attr.depth, 2);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_bootstrap_finish) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x02\x37\x21" // POST code 0.2
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB2\x62\x73" // uri-path_1 URI_PATH 11 /bs
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_BOOTSTRAP_FINISH);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_read_composite) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x05\x37\x21" // FETCH code 0.5
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xC0"                     // content_format 12 PLAINTEXT 0
                    "\x52\x2D\x17"             // accept 17 LWM2M_JSON 11543
                    "\xFF"                     // payload marker
                    "\x33\x44\x55\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_READ_COMP);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_OMA_LWM2M_JSON);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 6);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[17]);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_observe_with_pmin_pmax) {
    uint8_t MSG[] =
            "\x48"         // header v 0x01, Confirmable, tkl 8
            "\x01\x37\x21" // GET code 0.1
            "\x12\x34\x56\x78\x11\x11\x11\x11"     // token
            "\x61\x00"                             // observe 6 = 0
            "\x51\x35"                             // uri-path_1 URI_PATH 11 /5
            "\x01\x35"                             // uri-path_2 /5
            "\x01\x31"                             // uri-path_3 /1
            "\x48\x70\x6d\x69\x6e\x3d\x32\x30\x30" // URI_QUERY 15 pmin=200
            "\x09\x70\x6d\x61\x78\x3d\x34\x32\x30\x30" // URI_QUERY 15 pmax=4200
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_INF_OBSERVE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format,
                          _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_con, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.min_period, 200);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_period, 4200);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_observe_composite_with_params) {
    uint8_t MSG[] =
            "\x48"         // header v 0x01, Confirmable, tkl 8
            "\x05\x37\x21" // FETCH code 0.5
            "\x12\x34\x56\x78\x11\x11\x11\x11"         // token
            "\x61\x00"                                 // observe 6 = 0
            "\x97\x70\x6d\x69\x6e\x3d\x32\x30"         // URI_QUERY 15 pmin=20
            "\x07\x65\x70\x6d\x69\x6e\x3d\x31"         // URI_QUERY 15 epmin=1
            "\x07\x65\x70\x6d\x61\x78\x3d\x32"         // URI_QUERY 15 epmax=2
            "\x05\x63\x6f\x6e\x3d\x31"                 // URI_QUERY 15 con=1
            "\x09\x70\x6d\x61\x78\x3d\x31\x32\x30\x30" // URI_QUERY 15 pmax=1200
            "\xFF"                                     // payload marker
            "\x77\x44\x55\x33\x44\x55\x33\x33\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_INF_OBSERVE_COMP);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 10);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[55]);
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

ANJ_UNIT_TEST(anj_decode_udp, decode_cancel_observation) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x01\x37\x21" // GET code 0.1
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\x61\x01"                         // observe 6 = 1
                    "\x51\x35" // uri-path_1 URI_PATH 11 /5
                    "\x01\x35" // uri-path_2             /5
                    "\x01\x31" // uri-path_3             /1
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_INF_CANCEL_OBSERVE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 5);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 1);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_cancel_composite) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x05\x37\x21" // FETCH code 0.5
                    "\x12\x34\x56\x78\x11\x11\x11\x11"         // token
                    "\x61\x01"                                 // observe 6 = 1
                    "\xFF"                                     // payload marker
                    "\x77\x44\x55\x33\x44\x55\x33\x33\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_INF_CANCEL_OBSERVE_COMP);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 10);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[15]);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_write_partial) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x02\x37\x21" // POST code 0.2
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB2\x31\x35" // uri-path_1 URI_PATH 11 /15
                    "\x01\x32"     // uri-path_2             /2
                    "\x10"         // content_format 12 PLAINTEXT 0
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_PARTIAL_UPDATE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 15);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 3);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[19]);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_write_partial_with_resource_path) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x02\x37\x21" // POST code 0.2
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB2\x31\x35" // uri-path_1 URI_PATH 11 /15
                    "\x01\x32"     // uri-path_2             /2
                    "\x01\x34"     // uri-path_3             /4
                    "\x11\x3C"     // content_format 12 FORMAT_CBOR 60
                    "\xFF"         // payload marker
                    "\x33\x44\x55" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_PARTIAL_UPDATE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 15);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.accept, _ANJ_COAP_FORMAT_NOT_DEFINED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 3);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[22]);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_write_attributes) {
    uint8_t MSG[] =
            "\x48"         // header v 0x01, Confirmable, tkl 8
            "\x03\x37\x21" // PUT code 0.3
            "\x12\x34\x56\x78\x11\x11\x11\x11" // token
            "\xB2\x31\x35"                     // uri-path_1 URI_PATH 11 /15
            "\x01\x32"                         // uri-path_2             /2
            "\x02\x31\x32"                     // uri-path_3             /12
            "\x47\x70\x6d\x69\x6e\x3d\x32\x30" // URI_QUERY 15 pmin=20
            "\x07\x65\x70\x6d\x69\x6e\x3d\x31" // URI_QUERY 15 epmin=1
            "\x07\x65\x70\x6d\x61\x78\x3d\x32" // URI_QUERY 15 epmax=2
            "\x05\x63\x6f\x6e\x3d\x31"         // URI_QUERY 15 con=1
            "\x07\x67\x74\x3d\x32\x2e\x38\x35" // URI_QUERY 15 gt=2.85
            "\x09\x6c\x74\x3d\x33\x33\x33\x33\x2e\x38" // URI_QUERY 15 lt=3333.8
            "\x07\x73\x74\x3d\x2D\x30\x2e\x38"         // URI_QUERY 15 st=-0.8
            "\x06\x65\x64\x67\x65\x3d\x30"             // URI_QUERY 15 edge=0
            "\x0A\x68\x71\x6d\x61\x78\x3d\x37\x37\x37\x37" // URI_QUERY 15
                                                           // hqmax=7777
            "\x09\x70\x6d\x61\x78\x3d\x31\x32\x30\x30" // URI_QUERY 15 pmax=1200
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_ATTR);
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

ANJ_UNIT_TEST(anj_decode_udp, decode_write_composite) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x07\x37\x21" // IPATCH code 0.7
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xC1\x3C" // content_format 12 FORMAT_CBOR 60
                    "\xFF"     // payload marker
                    "\x77\x44\x55\x33\x44\x55\x33\x33\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_WRITE_COMP);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 10);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[15]);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_execute) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x02\x37\x21" // POST code 0.2
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB2\x31\x35" // uri-path_1 URI_PATH 11 /15
                    "\x01\x32"     // uri-path_2             /2
                    "\x02\x31\x32" // uri-path_3             /12
                    "\x10"         // content_format 12 PLAIN-TEXT 0
                    "\xFF"         // payload marker
                    "\x77\x44\x55\x33\x44\x55\x33\x33\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_EXECUTE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 15);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[2], 12);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_PLAINTEXT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 10);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[22]);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_create) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x02\x37\x21" // POST code 0.2
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB5\x33\x33\x36\x33\x39" // uri-path_1 URI_PATH 11 /33639
                    "\x11\x3C" // content_format 12 FORMAT_CBOR 60
                    "\xFF"     // payload marker
                    "\x76\x44\x55\x33\x44\x55\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_CREATE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 33639);
    ANJ_UNIT_ASSERT_EQUAL(out_data.content_format, _ANJ_COAP_FORMAT_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 8);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[21]);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_delete) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Confirmable, tkl 8
                    "\x04\x37\x21" // DELETE code 0.4
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\xB5\x33\x33\x36\x33\x39" // uri-path_1 URI_PATH 11 /33639
                    "\x01\x31"                 // uri-path_1 URI_PATH 11 /1
                    "\xFF"                     // payload marker
                    "\x76\x44\x55\x33\x44\x55\x33\x33" // payload
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_DM_DELETE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.uri_len, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[0], 33639);
    ANJ_UNIT_ASSERT_EQUAL(out_data.uri.ids[1], 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.payload_size, 8);
    ANJ_UNIT_ASSERT_EQUAL((intptr_t) out_data.payload, (intptr_t) &MSG[21]);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_response) {
    uint8_t MSG[] = "\x68"         // header v 0x01, Ack, tkl 8
                    "\x44\x37\x21" // CHANGED code 2.4
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.type,
                          ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CHANGED);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_ping) {
    uint8_t MSG[] = "\x40\x00\x00\x00"; // Confirmable, tkl 0, empty msg

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_COAP_PING_UDP);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.type,
                          ANJ_COAP_UDP_TYPE_CONFIRMABLE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_EMPTY);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_reset_message) {
    uint8_t MSG[] = "\x70"          // ACK, tkl 0
                    "\x00\x22\x22"; // empty msg

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_COAP_RESET);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.type,
                          ANJ_COAP_UDP_TYPE_RESET);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_EMPTY);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_empty_message) {
    uint8_t MSG[] = "\x60"         // header v 0x01, Ack
                    "\x00\x37\x21" // empty code 2.4
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_COAP_EMPTY_MSG);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.type,
                          ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_EMPTY);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_con_response) {
    uint8_t MSG[] = "\x48"         // header v 0x01, Con, tkl 8
                    "\x44\x37\x21" // CHANGED code 2.4
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.type,
                          ANJ_COAP_UDP_TYPE_CONFIRMABLE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CHANGED);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_non_con_response) {
    uint8_t MSG[] = "\x58"         // header v 0x01, Con, tkl 8
                    "\x44\x37\x21" // CHANGED code 2.4
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.type,
                          ANJ_COAP_UDP_TYPE_NON_CONFIRMABLE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CHANGED);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_response_with_etag) {
    uint8_t MSG[] = "\x68"         // header v 0x01, Ack, tkl 8
                    "\x44\x37\x21" // CHANGED code 2.4
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\x43\x33\x33\x32"                 // etag 3 332
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.downloader_attr.etag.size, 3);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            "332", out_data.attr.downloader_attr.etag.bytes, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.downloader_attr.total_size, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CHANGED);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_response_with_etag_and_size) {
    uint8_t MSG[] = "\x68"         // header v 0x01, Ack, tkl 8
                    "\x44\x37\x21" // CHANGED code 2.4
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\x43\x33\x33\x32"                 // etag 3 332
                    "\xD3\x0B\x31\x3c\x60"             // size2 3226720
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.downloader_attr.etag.size, 3);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            "332", out_data.attr.downloader_attr.etag.bytes, 3);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.downloader_attr.total_size, 3226720);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CHANGED);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_response_with_location_path) {
    uint8_t MSG[] = "\x68"         // header v 0x01, Ack, tkl 8
                    "\x41\x37\x21" // CREATED code 2.1
                    "\x12\x34\x56\x78\x11\x11\x11\x11" // token
                    "\x82\x72\x64"                     // LOCATION_PATH 8 /rd
                    "\x04\x35\x61\x33\x66"             // LOCATION_PATH 8 /5a3f
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.coap_binding_data.udp.message_id, 0x3721);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.token.bytes,
            ((uint8_t[]){ 0x12, 0x34, 0x56, 0x78, 0x11, 0x11, 0x11, 0x11 }),
            8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.token.size, 8);
    ANJ_UNIT_ASSERT_EQUAL(out_data.operation, ANJ_OP_RESPONSE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.msg_code, ANJ_COAP_CODE_CREATED);
    ANJ_UNIT_ASSERT_EQUAL(out_data.location_path.location_len[0], 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.location_path.location_len[1], 4);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.location_path.location[0], "rd", 2);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            out_data.location_path.location[1], "5a3f", 4);
    ANJ_UNIT_ASSERT_EQUAL(out_data.location_path.location_count, 2);
}

ANJ_UNIT_TEST(anj_decode_udp, decode_error_compromited_msg) {
    uint8_t MSG[] = "\x54"             // header v 0x01, Non-confirmable, tkl 4
                    "\x43\x21\x37"     // code 2.3
                    "\x12\x34\x56\x78" // token
                    "\x51\x30"         // opt 1
                    "\x53\x31\x32\x33" // opt 2
                    "\xFF"             // payload marker
                    "\x78\x78\x78"     // payload
            ;

    _anj_coap_msg_t out_data = { 0 };

    uint8_t aux;
    // incorrect version number
    aux = MSG[0];
    MSG[0] = 0x14;
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));
    MSG[0] = aux;

    // incorrect token length
    aux = MSG[0];
    MSG[0] = 0x53;
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));
    MSG[0] = aux;

    // no payload marker
    aux = MSG[14];
    MSG[14] = 0x11;
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));
    MSG[14] = aux;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));
}

ANJ_UNIT_TEST(anj_decode_udp, decode_error_too_long_uri) {
    uint8_t MSG[] = "\x44"             // header v 0x01, Confirmable, tkl 4
                    "\x01\x21\x37"     // GET code 0.1, msg id 3721
                    "\x12\x34\x56\x78" // token
                    "\xB1\x33"         // uri-path_1 URI_PATH 11 /3
                    "\x01\x33"         // uri-path_2             /3
                    "\x02\x31\x31"     // uri-path_3             /11
                    "\x02\x31\x31"     // uri-path_4             /11
                    "\x02\x31\x31"     // uri-path_5             /11
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));
}

ANJ_UNIT_TEST(anj_decode_udp, decode_error_incorrect_post) {
    uint8_t MSG[] = "\x44"             // header v 0x01, Confirmable, tkl 4
                    "\x02\x21\x37"     // POST code 0.2, msg id 3721
                    "\x12\x34\x56\x78" // token
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));
}

ANJ_UNIT_TEST(anj_decode_udp, decode_error_attr) {
    uint8_t MSG[] =
            "\x48"         // header v 0x01, Confirmable, tkl 8
            "\x03\x37\x21" // PUT code 0.3
            "\x12\x34\x56\x78\x11\x11\x11\x11"     // token
            "\xd7\x02\x70\x6d\x69\x6e\x3d\x6e\x30" // URI_QUERY 15 pmin=n0
            ;

    _anj_coap_msg_t out_data = { 0 };
    ANJ_UNIT_ASSERT_FAILED(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));
}
