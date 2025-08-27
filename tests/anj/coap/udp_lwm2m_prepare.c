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
#include <string.h>

#include <anj/defs.h>

#include "../../src/anj/coap/coap.h"
#include "../../src/anj/coap/udp_header.h"

#include <anj_unit_test.h>

ANJ_UNIT_TEST(anj_prepare_udp, prepare_register) {
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
    data.attr.register_attr.lifetime = 120;
    data.attr.register_attr.lwm2m_ver = "1.2";

    data.coap_binding_data.udp.message_id = 0x01;
    data.token.size = 8;

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x01"                     // POST 0x02, msg id 0001
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token not filled
            "\xb2\x72\x64"                     // uri path /rd
            "\x11\x28" // content_format: application/link-format
            "\x37\x65\x70\x3d\x6e\x61\x6d\x65"         // uri-query ep=name
            "\x06\x6c\x74\x3d\x31\x32\x30"             // uri-query lt=120
            "\x09\x6c\x77\x6d\x32\x6d\x3d\x31\x2e\x32" // uri-query lwm2m=1.2
            "\x01\x51"                                 // uri-query Q
            "\xFF"
            "\x3c\x31\x2f\x31\x3e";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 50);
    ANJ_UNIT_ASSERT_TRUE((out_msg_size - data.payload_size)
                         <= calculated_msg_size);
    // each uri-query option has potentially 4 bytes long header
    ANJ_UNIT_ASSERT_TRUE(22 + (out_msg_size - data.payload_size)
                         >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_update) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_UPDATE;
    data.location_path.location[0] = "name";
    data.location_path.location_len[0] = 4;
    data.location_path.location_count = 1;

    data.attr.register_attr.has_sms_number = true;
    data.attr.register_attr.has_binding = true;
    data.attr.register_attr.binding = "U";

    data.coap_binding_data.udp.message_id = 0x02;
    data.token.size = 8;

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x48"         // Confirmable, tkl 8
                         "\x02\x00\x02" // POST 0x02, msg id 0002
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token not filled
                         "\xb4\x6e\x61\x6d\x65"             // uri path /name
                         "\x43\x62\x3d\x55"                 // uri-query b=U
                         "\x03\x73\x6d\x73";                // uri-query sms

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 25);
    ANJ_UNIT_ASSERT_TRUE(out_msg_size <= calculated_msg_size);
    // each uri-query option has potentially 4 bytes long header
    ANJ_UNIT_ASSERT_TRUE(17 + out_msg_size >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_deregister) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_DEREGISTER;
    data.location_path.location[0] = "name";
    data.location_path.location_len[0] = 4;
    data.location_path.location_count = 1;
    data.coap_binding_data.udp.message_id = 0x03;
    data.token.size = 8;

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x48"         // Confirmable, tkl 8
                         "\x04\x00\x03" // DELETE 0x04, msg id 0003
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb4\x6e\x61\x6d\x65";            // uri path /name

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 17);
    ANJ_UNIT_ASSERT_TRUE(out_msg_size <= calculated_msg_size);
    ANJ_UNIT_ASSERT_TRUE(15 + out_msg_size >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_bootstrap_request) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_BOOTSTRAP_REQ;

    data.attr.bootstrap_attr.has_endpoint = true;
    data.attr.bootstrap_attr.has_preferred_content_format = true;
    data.attr.bootstrap_attr.endpoint = "name";
    data.attr.bootstrap_attr.preferred_content_format = 60;
    data.coap_binding_data.udp.message_id = 0x04;
    data.token.size = 8;

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x48"         // Confirmable, tkl 8
                         "\x02\x00\x04" // POST 0x02, msg id 0004
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb2\x62\x73"                     // uri path /bs
                         "\x47\x65\x70\x3d\x6e\x61\x6d\x65" // uri-query ep=name
                         "\x06\x70\x63\x74\x3d\x36\x30";    // uri-query pct=60

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 30);
    ANJ_UNIT_ASSERT_TRUE(out_msg_size <= calculated_msg_size);
    ANJ_UNIT_ASSERT_TRUE(15 + out_msg_size >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_bootstrap_pack_request) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_BOOTSTRAP_PACK_REQ;
    data.accept = _ANJ_COAP_FORMAT_SENML_ETCH_JSON;
    data.coap_binding_data.udp.message_id = 0x05;
    data.token.size = 8;

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x48"         // Confirmable, tkl 8
                         "\x01\x00\x05" // GET 0x01, msg id 0005
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb6\x62\x73\x70\x61\x63\x6b"     // uri path /bspack
                         "\x62\x01\x40"                     // accept /ETCH_JSON
            ;
    memcpy(&EXPECTED[4], data.token.bytes,
           8); // copy token

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 22);
    ANJ_UNIT_ASSERT_TRUE(out_msg_size <= calculated_msg_size);
    ANJ_UNIT_ASSERT_TRUE(15 + out_msg_size >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_non_con_notify) {
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

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x52"         // NonConfirmable, tkl 2
                         "\x45\x00\x00" // CONTENT 2.5 msg id not filled
                         "\x44\x44"     // token
                         "\x62\x22\x33" // observe 0x2233
                         "\x60"         // content-format 0
                         "\xFF"
                         "\x32\x31\x31";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 14);
    ANJ_UNIT_ASSERT_TRUE((out_msg_size - data.payload_size)
                         <= calculated_msg_size);
    ANJ_UNIT_ASSERT_TRUE(15 + (out_msg_size - data.payload_size)
                         >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_send) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_INF_CON_SEND;
    data.content_format = _ANJ_COAP_FORMAT_OPAQUE_STREAM;
    data.payload = (uint8_t *) "<1/1>";
    data.payload_size = 5;
    data.token.size = 8;

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x48"         // Confirmable, tkl 8
                         "\x02\x00\x00" // POST 0x02, msg id not filled
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token not filled
                         "\xb2\x64\x70"                     // uri path /dp
                         "\x11\x2A" // content_format: octet-stream
                         "\xFF"
                         "\x3c\x31\x2f\x31\x3e";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 23);
    ANJ_UNIT_ASSERT_TRUE((out_msg_size - data.payload_size)
                         <= calculated_msg_size);
    ANJ_UNIT_ASSERT_TRUE(15 + (out_msg_size - data.payload_size)
                         >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_non_con_send) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_INF_NON_CON_SEND;
    data.content_format = _ANJ_COAP_FORMAT_OPAQUE_STREAM;
    data.payload = (uint8_t *) "<1/1>";
    data.payload_size = 5;
    data.token.size = 8;

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x58"         // NonConfirmable, tkl 8
                         "\x02\x00\x00" // POST 0x02, msg id not set
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb2\x64\x70"                     // uri path /dp
                         "\x11\x2A" // content_format: octet-stream
                         "\xFF"
                         "\x3c\x31\x2f\x31\x3e";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 23);
    ANJ_UNIT_ASSERT_TRUE((out_msg_size - data.payload_size)
                         <= calculated_msg_size);
    ANJ_UNIT_ASSERT_TRUE(15 + (out_msg_size - data.payload_size)
                         >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_con_notify) {
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

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x42"         // Confirmable, tkl 2
                         "\x45\x00\x00" // CONTENT 2.5 msg id not set
                         "\x44\x44"     // token
                         "\x62\x22\x33" // observe 0x2233
                         "\x60"         // content-format 0
                         "\xFF"
                         "\x32\x31\x31";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 14);
    ANJ_UNIT_ASSERT_TRUE((out_msg_size - data.payload_size)
                         <= calculated_msg_size);
    ANJ_UNIT_ASSERT_TRUE(15 + (out_msg_size - data.payload_size)
                         >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_downloader_get) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_DOWNLOADER_GET;
    char *path = "/ab/cde/efgh";

    data.attr.downloader_attr.path[0] = &path[1];
    data.attr.downloader_attr.path_len[0] = 2;
    data.attr.downloader_attr.path[1] = &path[4];
    data.attr.downloader_attr.path_len[1] = 3;
    data.attr.downloader_attr.path[2] = &path[8];
    data.attr.downloader_attr.path_len[2] = 4;
    data.attr.downloader_attr.paths_count = 3;
    data.token.size = 2;
    data.token.bytes[0] = 0x44;
    data.token.bytes[1] = 0x44;
    data.coap_binding_data.udp.message_id = 0x0A;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x42"                 // Confirmable, tkl 2
                         "\x01\x00\x0A"         // GET 0x01, msg id 0010
                         "\x44\x44"             // token
                         "\xb2\x61\x62"         // uri path /ab
                         "\x03\x63\x64\x65"     // uri path /cde
                         "\x04\x65\x66\x67\x68" // uri path /efgh
            ;
    memcpy(&EXPECTED[4], data.token.bytes, 2); // copy token

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(EXPECTED) - 1);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_downloader_get_empty_path) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_DOWNLOADER_GET;
    char *path = "/ab//efgh";

    data.attr.downloader_attr.path[0] = &path[1];
    data.attr.downloader_attr.path_len[0] = 2;
    data.attr.downloader_attr.path[1] = NULL; // empty path segment
    data.attr.downloader_attr.path_len[1] = 0;
    data.attr.downloader_attr.path[2] = &path[5];
    data.attr.downloader_attr.path_len[2] = 4;
    data.attr.downloader_attr.paths_count = 3;
    data.token.size = 2;
    data.token.bytes[0] = 0x44;
    data.token.bytes[1] = 0x44;
    data.coap_binding_data.udp.message_id = 0x0A;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x42"                 // Confirmable, tkl 2
                         "\x01\x00\x0A"         // GET 0x01, msg id 0010
                         "\x44\x44"             // token
                         "\xb2\x61\x62"         // uri path /ab
                         "\x00"                 // uri path /
                         "\x04\x65\x66\x67\x68" // uri path /efgh
            ;
    memcpy(&EXPECTED[4], data.token.bytes, 2); // copy token

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(EXPECTED) - 1);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_ack_notify) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_INF_INITIAL_NOTIFY;
    data.msg_code = ANJ_COAP_CODE_CONTENT;
    data.token.size = 2;
    data.token.bytes[0] = 0x44;
    data.token.bytes[1] = 0x44;
    data.coap_binding_data.udp.message_id = 0x2222;
    data.content_format = 0;
    data.observe_number = 0x2233;
    data.payload_size = 3;
    data.payload = (uint8_t *) "211";

    size_t calculated_msg_size = _anj_coap_calculate_msg_header_max_size(&data);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x62"         // ACK, tkl 2
                         "\x45\x22\x22" // CONTENT 2.5 msg id 2222
                         "\x44\x44"     // token
                         "\x62\x22\x33" // observe 0x2233
                         "\x60"         // content-format 0
                         "\xFF"
                         "\x32\x31\x31";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 14);
    ANJ_UNIT_ASSERT_TRUE((out_msg_size - data.payload_size)
                         <= calculated_msg_size);
    ANJ_UNIT_ASSERT_TRUE(15 + (out_msg_size - data.payload_size)
                         >= calculated_msg_size);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_response) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CREATED;
    // msd_id and token are normally taken from request
    data.coap_binding_data.udp.message_id = 0x2222;
    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x63"          // ACK, tkl 3
                         "\x41\x22\x22"  // CREATED 0x41
                         "\x11\x22\x33"; // token

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 7);
    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_calculate_msg_header_max_size(&data), 13);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_response_create) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CREATED;
    // msd_id and token are normally taken from request
    data.coap_binding_data.udp.message_id = 0x2222;
    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;
    data.attr.create_attr.has_uri = true;
    data.attr.create_attr.oid = 1;
    data.attr.create_attr.iid = 2;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x63"         // ACK, tkl 3
                         "\x41\x22\x22" // CREATED 0x41
                         "\x11\x22\x33" // token
                         "\x81\x31"     // location-path /1
                         "\x01\x32";    // location-path /2

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 11);
    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_calculate_msg_header_max_size(&data), 25);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_response_create_max_path) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CREATED;
    // msd_id and token are normally taken from request
    data.coap_binding_data.udp.message_id = 0x2222;
    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;
    data.attr.create_attr.has_uri = true;
    data.attr.create_attr.oid = 12345;
    data.attr.create_attr.iid = 17890;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x63"                      // ACK, tkl 3
                         "\x41\x22\x22"              // CREATED 0x41
                         "\x11\x22\x33"              // token
                         "\x85\x31\x32\x33\x34\x35"  // location-path /12345
                         "\x05\x31\x37\x38\x39\x30"; // location-path /17890

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 19);
}

// check if options are encoded correctly for oid=0, iid=0
ANJ_UNIT_TEST(anj_prepare_udp, prepare_response_create_empty_path) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CREATED;
    // msd_id and token are normally taken from request
    data.coap_binding_data.udp.message_id = 0x2222;
    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;
    data.attr.create_attr.has_uri = true;
    data.attr.create_attr.oid = 0;
    data.attr.create_attr.iid = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x63"         // ACK, tkl 3
                         "\x41\x22\x22" // CREATED 0x41
                         "\x11\x22\x33" // token
                         "\x81\x30"     // location-path /0
                         "\x01\x30";    // location-path /0

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 11);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_response_with_payload) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CONTENT;
    data.content_format = _ANJ_COAP_FORMAT_CBOR;
    data.payload_size = 5;
    data.payload = (uint8_t *) "00000";

    data.coap_binding_data.udp.message_id = 0x2222;
    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x63"         // ACK, tkl 3
                         "\x45\x22\x22" // CONTENT 0x45
                         "\x11\x22\x33" // token
                         "\xC1\x3C"     // content_format: cbor
                         "\xFF"
                         "\x30\x30\x30\x30\x30";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 15);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_response_with_block) {
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

    data.coap_binding_data.udp.message_id = 0x2222;
    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x63"         // ACK, tkl 3
                         "\x45\x22\x22" // CONTENT 0x45
                         "\x11\x22\x33" // token
                         "\xC0"         // CONTENT_FORMAT 12
                         "\xb2\x08\x4D" // block2 23
                         "\xFF"
                         "\x30\x30\x30\x30\x30";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 17);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_reset) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_RESET;
    data.coap_binding_data.udp.message_id = 0x2222;
    // this value should be ignored - data.operation = ANJ_OP_COAP_RESET makes
    // _anj_coap_encode_udp() overwrite the token size to 0
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x70"          // ACK, tkl 0
                         "\x00\x22\x22"; // empty msg

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 4);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_ping) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_PING_UDP;
    // this value should be ignored - data.operation = ANJ_OP_COAP_PING_UDP
    // makes _anj_coap_encode_udp() overwrite the token size to 0
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] =
            "\x40\x00\x00\x00"; // Confirmable, tkl 0, empty msg, msg id not set

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 4);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_empty_response) {
    _anj_coap_msg_t data = { 0 };
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_COAP_EMPTY_MSG;
    data.coap_binding_data.udp.message_id = 0x2222;
    // this value should be ignored - data.operation = ANJ_OP_COAP_EMPTY_MSG
    // makes _anj_coap_encode_udp() overwrite the token size to 0
    data.token.size = 8;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x60\x00\x22\x22"; // ACK, tkl 0, empty msg, token

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 4);
}

ANJ_UNIT_TEST(anj_prepare_udp, prepare_error_buff_size) {
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
    data.attr.register_attr.lifetime = 120;
    data.attr.register_attr.lwm2m_ver = "1.2";
    data.token.size = 8;

    for (size_t i = _ANJ_COAP_UDP_HEADER_LENGTH + 1; i < 55; i++) {
        ANJ_UNIT_ASSERT_FAILED(
                _anj_coap_encode_udp(&data, buff, i, &out_msg_size));
    }
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, 55, &out_msg_size));
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 55);
}

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
ANJ_UNIT_TEST(anj_prepare_udp, prepare_response_with_double_block) {
    _anj_coap_msg_t data;
    memset(&data, 0, sizeof(_anj_coap_msg_t));
    uint8_t buff[100];
    size_t out_msg_size;

    data.operation = ANJ_OP_RESPONSE;
    data.msg_code = ANJ_COAP_CODE_CONTENT;
    data.payload_size = 5;
    data.payload = (uint8_t *) "00000";

    data.block.block_type = ANJ_OPTION_BLOCK_BOTH;
    data.block.size = 512;
    data.block.number = 132;

    data.coap_binding_data.udp.message_id = 0x2222;
    data.token.size = 3;
    data.token.bytes[0] = 0x11;
    data.token.bytes[1] = 0x22;
    data.token.bytes[2] = 0x33;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_encode_udp(&data, buff, sizeof(buff), &out_msg_size));

    uint8_t EXPECTED[] = "\x63"         // ACK, tkl 3
                         "\x45\x22\x22" // CONTENT 0x45
                         "\x11\x22\x33" // token
                         "\xC0"         // CONTENT_FORMAT 12
                         "\xb1\x0D"     // block2 0 more
                         "\x42\x08\x45" // block1 132
                         "\xFF"
                         "\x30\x30\x30\x30\x30";

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, 19);
}
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
