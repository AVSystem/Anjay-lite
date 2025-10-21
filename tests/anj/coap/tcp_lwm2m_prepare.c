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
#include <stdint.h>
#include <string.h>

#include <anj/defs.h>

#include "../../src/anj/coap/coap.h"
#include "../io/bigdata.h"

#include <anj_unit_test.h>

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_register) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_REGISTER;

    data.content_format = _ANJ_COAP_FORMAT_LINK_FORMAT;
    data.payload = (uint8_t *) "<1/1>";
    data.payload_size = 5;

    data.attr.register_attr.has_endpoint = true;
    data.attr.register_attr.has_lifetime = true;
    data.attr.register_attr.has_lwm2m_ver = true;
    data.attr.register_attr.has_Q = true;
    data.attr.register_attr.endpoint = "name";
    data.attr.register_attr.lifetime =
            anj_time_duration_new(120, ANJ_TIME_UNIT_S);
    data.attr.register_attr.lwm2m_ver = "1.2";
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] =
            "\xD8"                             // msg_len 13, tkl 8
            "\x19"                             // extended length 38
            "\x02"                             // POST
            "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
            "\xB2\x72\x64"                     // uri path /rd
            "\x11\x28" // content_format: application/link-format
            "\x37\x65\x70\x3D\x6E\x61\x6D\x65"         // uri-query ep=name
            "\x06\x6C\x74\x3D\x31\x32\x30"             // uri-query lt=120
            "\x09\x6C\x77\x6D\x32\x6D\x3D\x31\x2E\x32" // uri-query lwm2m=1.2
            "\x01\x51"                                 // uri-query Q
            "\xFF"                                     // payload marker
            "\x3C\x31\x2F\x31\x3E"                     // payload
            ;
    memcpy(&EXPECTED[3], data.token.bytes, 8);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 49);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_update) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_UPDATE;

    data.location_path.location[0] = "name";
    data.location_path.location_len[0] = 4;
    data.location_path.location_count = 1;

    data.attr.register_attr.has_sms_number = true;
    data.attr.register_attr.has_binding = true;
    data.attr.register_attr.binding = "T";
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\xD8" // msg_len 13, tkl 8
                         "\x00"
                         "\x02"                             // POST
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
                         "\xB4\x6E\x61\x6D\x65"             // uri path /name
                         "\x43\x62\x3D\x54"                 // uri-query b=T
                         "\x03\x73\x6D\x73"                 // uri-query sms
            ;
    memcpy(&EXPECTED[3], data.token.bytes, 8);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 24);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_deregister) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_DEREGISTER;

    data.location_path.location[0] = "name";
    data.location_path.location_len[0] = 4;
    data.location_path.location_count = 1;
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x58"                             // msg_len 8, tkl 8
                         "\x04"                             // DELETE
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
                         "\xb4\x6E\x61\x6D\x65"             // uri path /name
            ;
    memcpy(&EXPECTED[2], data.token.bytes, 8);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 15);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_bootstrap_request) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_BOOTSTRAP_REQ;

    data.attr.bootstrap_attr.has_endpoint = true;
    data.attr.bootstrap_attr.has_preferred_content_format = true;
    data.attr.bootstrap_attr.endpoint = "name";
    data.attr.bootstrap_attr.preferred_content_format = 60;
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\xD8" // msg_len 13, tkl 8
                         "\x05" // extended length 18
                         "\x02" // POST
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
                         "\xB2\x62\x73"                     // uri path /bs
                         "\x47\x65\x70\x3D\x6E\x61\x6D\x65" // uri-query ep=name
                         "\x06\x70\x63\x74\x3D\x36\x30"     // uri-query pct=60
            ;
    memcpy(&EXPECTED[3], data.token.bytes, 8);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 29);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_bootstrap_pack_request) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_BOOTSTRAP_PACK_REQ;

    data.accept = _ANJ_COAP_FORMAT_SENML_ETCH_JSON;
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\xA8"                             // msg_len 10, tkl 8
                         "\x01"                             // GET
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xB6\x62\x73\x70\x61\x63\x6B"     // uri path /bspack
                         "\x62\x01\x40"                     // accept /ETCH_JSON
            ;
    memcpy(&EXPECTED[2], data.token.bytes, 8);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 20);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_non_con_notify) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_INF_NON_CON_NOTIFY;
    data.token.size = 2;
    data.token.bytes[0] = 0x44;
    data.token.bytes[1] = 0x44;
    data.content_format = 0;
    data.observe_number = 0x2233;
    data.payload_size = 3;
    data.payload = (uint8_t *) "211";

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x82"          // msg_len 8, tkl 2
                         "\x45"          // CONTENT
                         "\x44\x44"      // token
                         "\x62\x22\x33"  // observe 0x2233
                         "\x60"          // content-format 0
                         "\xFF"          // payload marker
                         "\x32\x31\x31"; // payload

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 12);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_send) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_INF_CON_SEND;
    data.content_format = _ANJ_COAP_FORMAT_OPAQUE_STREAM;
    data.payload = (uint8_t *) "<1/1>";
    data.payload_size = 5;
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\xB8"                             // msg_len 11, tkl 8
                         "\x02"                             // POST
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb2\x64\x70"                     // uri path /dp
                         "\x11\x2A"              // content_format: octet-stream
                         "\xFF"                  // payload marker
                         "\x3c\x31\x2f\x31\x3e"; // payload
    memcpy(&EXPECTED[2], data.token.bytes,
           8); // copy token

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 21);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_con_notify) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_INF_CON_NOTIFY;
    data.token.size = 2;
    data.token.bytes[0] = 0x44;
    data.token.bytes[1] = 0x44;
    data.content_format = 0;
    data.observe_number = 0x2233;
    data.payload_size = 3;
    data.payload = (uint8_t *) "211";

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x82"         // msg_len 8, tkl 2
                         "\x45"         // CONTENT
                         "\x44\x44"     // token
                         "\x62\x22\x33" // observe 0x2233
                         "\x60"         // content-format 0
                         "\xFF"         // payload marker
                         "\x32\x31\x31" // payload
            ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 12);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_downloader_get) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_DOWNLOADER_GET;
    char *path = "/ab/cde/efgh";

    data.attr.downloader_attr.path[0] = &path[1];
    data.attr.downloader_attr.path_len[0] = 2;
    data.attr.downloader_attr.path[1] = &path[4];
    data.attr.downloader_attr.path_len[1] = 3;
    data.attr.downloader_attr.paths_count = 2;
    data.token.size = 2;
    data.token.bytes[0] = 0x44;
    data.token.bytes[1] = 0x44;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x72"             // msg_len 7, tkl 8
                         "\x01"             // GET
                         "\x44\x44"         // token
                         "\xb2\x61\x62"     // uri path /ab
                         "\x03\x63\x64\x65" // uri path /cde
            ;
    memcpy(&EXPECTED[2], data.token.bytes, 2); // copy token

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(EXPECTED) - 1);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_ack_notify) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_INF_INITIAL_NOTIFY;
    data.msg_code = ANJ_COAP_CODE_CONTENT;
    data.token.size = 2;
    data.token.bytes[0] = 0x44;
    data.token.bytes[1] = 0x44;
    data.content_format = 0;
    data.observe_number = 0x2233;
    data.payload_size = 3;
    data.payload = (uint8_t *) "211";

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x82"         // msg_len 8, tkl 2
                         "\x45"         // CONTENT
                         "\x44\x44"     // token
                         "\x62\x22\x33" // observe 0x2233
                         "\x60"         // content-format 0
                         "\xFF"         // payload marker
                         "\x32\x31\x31" // payload
            ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 12);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_response) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CREATED;

    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x03"         // msg_len 0, tkl 3
                         "\x41"         // CREATED
                         "\x11\x22\x33" // token
            ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 5);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_response_create_with_path) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CREATED;

    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;
    data.attr.create_attr.has_uri = true;
    data.attr.create_attr.oid = 1;
    data.attr.create_attr.iid = 2;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x43"         // msg_len 4, tkl 3
                         "\x41"         // CREATED
                         "\x11\x22\x33" // token
                         "\x81\x31"     // location-path /1
                         "\x01\x32";    // location-path /2
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 9);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_response_with_payload) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CONTENT;

    data.content_format = _ANJ_COAP_FORMAT_CBOR;
    data.payload_size = 5;
    data.payload = (uint8_t *) "00000";

    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x83"                 // msg_len 8, tkl 3
                         "\x45"                 // CONTENT
                         "\x11\x22\x33"         // token
                         "\xC1\x3C"             // content_format: cbor
                         "\xFF"                 // payload marker
                         "\x30\x30\x30\x30\x30" // payload
            ;
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 13);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_response_with_block) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CONTENT;

    data.payload_size = 5;
    data.payload = (uint8_t *) "00000";

    data.block.block_type = ANJ_OPTION_BLOCK_2;
    data.block.size = 512;
    data.block.number = 132;
    data.block.more_flag = true;

    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\xA3"                 // msg_len 10, tkl 3
                         "\x45"                 // CONTENT
                         "\x11\x22\x33"         // token
                         "\xC0"                 // CONTENT_FORMAT 12
                         "\xB2\x08\x4D"         // block2 23
                         "\xFF"                 // payload marker
                         "\x30\x30\x30\x30\x30" // payload
            ;
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 15);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_ping_with_custody) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_PING;
    data.signalling_opts.ping_pong.custody = true;
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x18"                             // msg_len 1, tkl 8
                         "\xE2"                             // PING
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
                         "\x20" // option delta 2, option len 0
            ;
    memcpy(&EXPECTED[2], data.token.bytes, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 11);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_ping_without_custody) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_PING;
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x08"                             // msg_len 0, tkl 8
                         "\xE2"                             // PING
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
            ;
    memcpy(&EXPECTED[2], data.token.bytes, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 10);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_pong_with_custody) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_PONG;
    data.signalling_opts.ping_pong.custody = true;
    // token is normally taken from request
    data.token.size = 8;
    for (size_t i = 0; i < 8; i++) {
        data.token.bytes[i] = i;
    }

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x18"                             // msg_len 1, tkl 8
                         "\xE3"                             // PONG
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
                         "\x20" // option delta 2, option len 0
            ;
    memcpy(&EXPECTED[2], data.token.bytes, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 11);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_pong_without_custody) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_PONG;
    // token is normally taken from request
    data.token.size = 8;
    for (size_t i = 0; i < 8; i++) {
        data.token.bytes[i] = i;
    }

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x08"                             // msg_len 0, tkl 8
                         "\xE3"                             // PONG
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
            ;
    memcpy(&EXPECTED[2], data.token.bytes, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 10);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_csm_with_block_wise_transfer) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_CSM;
    data.signalling_opts.csm.max_msg_size = 152;
    data.signalling_opts.csm.block_wise_transfer_capable = true;
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x38"                             // msg_len 3, tkl 8
                         "\xE1"                             // CSM
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
                         "\x21\x98" // option delta 2, option len 1
                         "\x20"     // option delta 2, option len 0
            ;
    memcpy(&EXPECTED[2], data.token.bytes, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 13);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_csm_without_block_wise_transfer) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_CSM;
    data.signalling_opts.csm.max_msg_size = 152;
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x28"                             // msg_len 2, tkl 8
                         "\xE1"                             // CSM
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
                         "\x21\x98" // option delta 2, option len 1
            ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 12);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_empty_response) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_EMPTY_MSG;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x00" // msg_len 0, tkl 0
                         "\x00" // EMPTY
            ;
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 2);
}

ANJ_UNIT_TEST(anj_prepare_tcp, prepare_error_buff_size) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_REGISTER;

    data.content_format = _ANJ_COAP_FORMAT_LINK_FORMAT;
    data.payload = (uint8_t *) "<1/1><1/1>";
    data.payload_size = 10;
    data.attr.register_attr.has_endpoint = true;
    data.attr.register_attr.has_lifetime = true;
    data.attr.register_attr.has_lwm2m_ver = true;
    data.attr.register_attr.has_Q = true;
    data.attr.register_attr.endpoint = "name";
    data.attr.register_attr.lifetime =
            anj_time_duration_new(120, ANJ_TIME_UNIT_S);
    data.attr.register_attr.lwm2m_ver = "1.2";
    data.token.size = 8;

    for (size_t i = 0; i < 54; i++) {
        ANJ_UNIT_ASSERT_FAILED(
                _anj_coap_encode_tcp(&data, buff, i, &out_msg_size));
    }

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, 54, &out_msg_size));
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 54);
}

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
ANJ_UNIT_TEST(anj_prepare_tcp, prepare_response_with_double_block) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CONTENT;
    data.payload_size = 5;
    data.payload = (uint8_t *) "00000";

    data.block.block_type = ANJ_OPTION_BLOCK_BOTH;
    data.block.size = 512;
    data.block.number = 132;

    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\xC3"                  // msg_len 12, tkl 3
                         "\x45"                  // CONTENT
                         "\x11\x22\x33"          // token
                         "\xC0"                  // CONTENT_FORMAT 12
                         "\xb1\x0D"              // block2 0 more
                         "\x42\x08\x45"          // block1 132
                         "\xFF"                  // payload marker
                         "\x30\x30\x30\x30\x30"; // payload

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 17);
}
#endif // ANJ_WITH_COMPOSITE_OPERATIONS

#define DATA300B DATA100B DATA100B DATA100B
ANJ_UNIT_TEST(anj_prepare_tcp, prepare_payload_extended_length_2bytes) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[400];
    size_t out_msg_size;

    data.operation = ANJ_OP_REGISTER;

    data.content_format = _ANJ_COAP_FORMAT_LINK_FORMAT;
    data.payload = (uint8_t *) DATA300B;
    data.payload_size = 300;

    data.attr.register_attr.has_endpoint = true;
    data.attr.register_attr.has_lifetime = true;
    data.attr.register_attr.has_lwm2m_ver = true;
    data.attr.register_attr.has_Q = true;
    data.attr.register_attr.endpoint = "name";
    data.attr.register_attr.lifetime =
            anj_time_duration_new(120, ANJ_TIME_UNIT_S);
    data.attr.register_attr.lwm2m_ver = "1.2";
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] =
            "\xE8"                             // msg_len 14, tkl 8
            "\x00\x40"                         // extended length 333
            "\x02"                             // POST
            "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
            "\xB2\x72\x64"                     // uri path /rd
            "\x11\x28" // content_format: application/link-format
            "\x37\x65\x70\x3D\x6E\x61\x6D\x65"         // uri-query ep=name
            "\x06\x6C\x74\x3D\x31\x32\x30"             // uri-query lt=120
            "\x09\x6C\x77\x6D\x32\x6D\x3D\x31\x2E\x32" // uri-query lwm2m=1.2
            "\x01\x51"                                 // uri-query Q
            "\xFF"                                     // payload marker
            DATA300B                                   // payload
            ;
    memcpy(&EXPECTED[4], data.token.bytes, 8);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 345);
}

#define DATA80kB \
    DATA10kB DATA10kB DATA10kB DATA10kB DATA10kB DATA10kB DATA10kB DATA10kB
ANJ_UNIT_TEST(anj_prepare_tcp, prepare_payload_extended_length_4bytes) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[81000];
    size_t out_msg_size;

    data.operation = ANJ_OP_REGISTER;

    data.content_format = _ANJ_COAP_FORMAT_LINK_FORMAT;
    data.payload = (uint8_t *) DATA80kB;
    data.payload_size = 80000;

    data.attr.register_attr.has_endpoint = true;
    data.attr.register_attr.has_lifetime = true;
    data.attr.register_attr.has_lwm2m_ver = true;
    data.attr.register_attr.has_Q = true;
    data.attr.register_attr.endpoint = "name";
    data.attr.register_attr.lifetime =
            anj_time_duration_new(120, ANJ_TIME_UNIT_S);
    data.attr.register_attr.lwm2m_ver = "1.2";
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_tcp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] =
            "\xF8"                             // msg_len 15, tkl 8
            "\x00\x00\x37\x94"                 // extended length 80033
            "\x02"                             // POST
            "\x00\x00\x00\x00\x00\x00\x00\x00" // place for a token
            "\xB2\x72\x64"                     // uri path /rd
            "\x11\x28" // content_format: application/link-format
            "\x37\x65\x70\x3D\x6E\x61\x6D\x65"         // uri-query ep=name
            "\x06\x6C\x74\x3D\x31\x32\x30"             // uri-query lt=120
            "\x09\x6C\x77\x6D\x32\x6D\x3D\x31\x2E\x32" // uri-query lwm2m=1.2
            "\x01\x51"                                 // uri-query Q
            "\xFF"                                     // payload marker
            DATA80kB                                   // payload
            ;
    memcpy(&EXPECTED[6], data.token.bytes, 8);

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 80047);
}

#pragma GCC diagnostic pop
