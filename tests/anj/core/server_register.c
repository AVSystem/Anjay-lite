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

#include <anj/compat/net/anj_net_api.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>

#include "../../src/anj/exchange.h"

#include "../mock/net_api_mock.h"
#include "../mock/time_api_mock.h"

#include <anj_unit_test.h>

// inner_mtu_value value will lead to block transfer for additional objects in
// payload
#define TEST_INIT()                                        \
    set_mock_time(0);                                      \
    net_api_mock_t mock = { 0 };                           \
    net_api_mock_ctx_init(&mock);                          \
    mock.inner_mtu_value = 102;                            \
    anj_t anj;                                             \
    anj_configuration_t config = {                         \
        .endpoint_name = "name"                            \
    };                                                     \
    ANJ_UNIT_ASSERT_SUCCESS(anj_core_init(&anj, &config)); \
    anj_dm_security_obj_t sec_obj;                         \
    anj_dm_security_obj_init(&sec_obj);                    \
    anj_dm_server_obj_t ser_obj;                           \
    anj_dm_server_obj_init(&ser_obj)

#define ADD_INSTANCES()                                                   \
    ANJ_UNIT_ASSERT_SUCCESS(                                              \
            anj_dm_security_obj_add_instance(&sec_obj, &sec_inst));       \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj)); \
    ANJ_UNIT_ASSERT_SUCCESS(                                              \
            anj_dm_server_obj_add_instance(&ser_obj, &ser_inst));         \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &ser_obj))

#define INIT_BASIC_INSTANCES()                   \
    const anj_iid_t iid = 1;                     \
    anj_dm_security_instance_init_t sec_inst = { \
        .server_uri = "coap://server.com:5683",  \
        .ssid = 2,                               \
        .iid = &iid                              \
    };                                           \
    anj_dm_server_instance_init_t ser_inst = {   \
        .ssid = 2,                               \
        .lifetime = 10,                          \
        .binding = "U",                          \
        .iid = &iid                              \
    }

#define EXTENDED_INIT()     \
    TEST_INIT();            \
    INIT_BASIC_INSTANCES(); \
    ADD_INSTANCES()

ANJ_UNIT_TEST(server_register, register_init_success) {
    EXTENDED_INIT();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.server_uri,
                                 "server.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.port, "5683");
    ANJ_UNIT_ASSERT_EQUAL(anj.security_instance.type, ANJ_NET_BINDING_UDP);
    ANJ_UNIT_ASSERT_EQUAL(anj.security_instance.iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_instance.bootstrap_on_registration_failure,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_instance.lifetime, 10);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_instance.ssid, 2);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_instance.iid, 1);
    anj_communication_retry_res_t retry_res =
            ANJ_COMMUNICATION_RETRY_RES_DEFAULT;
    ANJ_UNIT_ASSERT_EQUAL(anj.server_instance.retry_res.retry_timer,
                          retry_res.retry_timer);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_instance.retry_res.retry_count,
                          retry_res.retry_count);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_instance.retry_res.seq_delay_timer,
                          retry_res.seq_delay_timer);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_instance.retry_res.seq_retry_count,
                          retry_res.seq_retry_count);
}

ANJ_UNIT_TEST(server_register, register_init_no_objects) {
    TEST_INIT();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
}

ANJ_UNIT_TEST(server_register, register_init_no_instances) {
    TEST_INIT();
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &ser_obj));
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
}

ANJ_UNIT_TEST(server_register, register_init_different_ssid) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    ser_inst.ssid = 1;
    ADD_INSTANCES();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
}

ANJ_UNIT_TEST(server_register, register_init_ipv6) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    sec_inst.server_uri = "coap://[2001:db8::1]:222";
    ADD_INSTANCES();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.server_uri,
                                 "[2001:db8::1]");
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.port, "222");
}

ANJ_UNIT_TEST(server_register, register_init_ipv6_no_port) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    sec_inst.server_uri = "coap://[2001:db8::1]/";
    ADD_INSTANCES();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.server_uri,
                                 "[2001:db8::1]");
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.port, "5683");
}

ANJ_UNIT_TEST(server_register, register_init_no_port) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    sec_inst.server_uri = "coap://server.com/";
    ADD_INSTANCES();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.server_uri,
                                 "server.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.port, "5683");
}

ANJ_UNIT_TEST(server_register, register_init_empty_port) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    sec_inst.server_uri = "coap://server.com:";
    ADD_INSTANCES();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
}

ANJ_UNIT_TEST(server_register, register_init_coaps) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    sec_inst.server_uri = "coaps://server.com:123";
    ADD_INSTANCES();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.server_uri,
                                 "server.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.port, "123");
}

// token and message id are copied from request stored in anj.exchange_ctx
// correct response must contain the same token and message id as request
#define COPY_TOKEN_AND_MSG_ID(Msg)                                            \
    memcpy(&Msg[4], anj.exchange_ctx.base_msg.token.bytes, 8);                \
    Msg[2] = anj.exchange_ctx.base_msg.coap_binding_data.udp.message_id >> 8; \
    Msg[3] = anj.exchange_ctx.base_msg.coap_binding_data.udp.message_id & 0xFF

#define CHECK_LOCATION_PATHS()                                      \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(                              \
            anj.register_ctx.location_path[0], "rd", strlen("rd")); \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(                              \
            anj.register_ctx.location_path[1], "5a3f", strlen("5a3f"))

#define ADD_RESPONSE(Response)                 \
    COPY_TOKEN_AND_MSG_ID(Response);           \
    mock.bytes_to_recv = sizeof(Response) - 1; \
    mock.data_to_recv = (uint8_t *) Response

static char expected_register[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xb2\x72\x64"                     // uri path /rd
        "\x11\x28" // content_format: application/link-format
        "\x37\x65\x70\x3d\x6e\x61\x6d\x65"         // uri-query ep=name
        "\x05\x6c\x74\x3d\x31\x30"                 // uri-query lt=10
        "\x09\x6c\x77\x6d\x32\x6d\x3d\x31\x2e\x32" // uri-query lwm2m=1.2
        "\x03\x62\x3D\x55"                         // uri-query b=U
        "\xFF"
        "</1>;ver=1.2,</1/1>";

static char response[] = "\x68"         // header v 0x01, Ack, tkl 8
                         "\x41\x00\x00" // CREATED code 2.1
                         "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
                         "\x82\x72\x64"                     // location-path /rd
                         "\x04\x35\x61\x33\x66"; // location-path 8 /5a3f

ANJ_UNIT_TEST(server_register, register_no_delays) {
    EXTENDED_INIT();

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // allow to send data
    mock.bytes_to_send = 100;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    COPY_TOKEN_AND_MSG_ID(expected_register);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register,
                                      sizeof(expected_register) - 1);

    // provide response
    ADD_RESPONSE(response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_LOCATION_PATHS();

    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.hostname, "server.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.port, "5683");
}

ANJ_UNIT_TEST(server_register, register_net_again) {
    EXTENDED_INIT();

    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EAGAIN;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    mock.bytes_to_send = 100;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    COPY_TOKEN_AND_MSG_ID(expected_register);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register,
                                      sizeof(expected_register) - 1);

    anj_core_step(&anj);

    ADD_RESPONSE(response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_LOCATION_PATHS();

    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 2);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], 2);
    // additional recv in _anj_reg_session_process
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_RECV], 3 + 1);
}

ANJ_UNIT_TEST(server_register, register_block_transfer) {
    EXTENDED_INIT();

    anj_dm_obj_inst_t inst_x[2] = { 0 };
    inst_x[0].iid = 1;
    inst_x[1].iid = 2;
    anj_dm_handlers_t handlers = { 0 };
    anj_dm_obj_t obj_x = {
        .max_inst_count = 2,
        .insts = inst_x,
        .oid = 9999,
        .handlers = &handlers
    };
    anj_dm_add_obj(&anj, &obj_x);

    char expected_register_block_1[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST, msg_id
            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
            "\xb2\x72\x64"                     // uri path /rd
            "\x11\x28" // content_format: application/link-format
            "\x37\x65\x70\x3d\x6e\x61\x6d\x65"         // uri-query ep=name
            "\x05\x6c\x74\x3d\x31\x30"                 // uri-query lt=10
            "\x09\x6c\x77\x6d\x32\x6d\x3d\x31\x2e\x32" // uri-query lwm2m=1.2
            "\x03\x62\x3D\x55"                         // uri-query b=U
            "\xc1\x09" // block1 0, size 32, more
            "\xFF"
            "</1>;ver=1.2,</1/1>,</9999>,</99";
    char expected_register_block_2[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST, msg_id
            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
            "\xb2\x72\x64"                     // uri path /rd
            "\x11\x28" // content_format: application/link-format
            "\x37\x65\x70\x3d\x6e\x61\x6d\x65"         // uri-query ep=name
            "\x05\x6c\x74\x3d\x31\x30"                 // uri-query lt=10
            "\x09\x6c\x77\x6d\x32\x6d\x3d\x31\x2e\x32" // uri-query lwm2m=1.2
            "\x03\x62\x3D\x55"                         // uri-query b=U
            "\xc1\x11"                                 // block1 1, size 32
            "\xFF"
            "99/1>,</9999/2>";
    char response_block_1[] = "\x68"                             // ACK, tkl 1
                              "\x5F"                             // Continue
                              "\x00\x00"                         // msg id
                              "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
                              "\xd1\x0e\x09"; // block1 0, size 32, more

    char response_block_2[] = "\x68"     // header v 0x01, Ack, tkl 8
                              "\x41"     // CREATED code 2.1
                              "\x00\x00" // msg id
                              "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
                              "\x82\x72\x64"         // location-path /rd
                              "\x04\x35\x61\x33\x66" // location-path /5a3f
                              "\xd1\x06\x10";        // block1 1, size 32

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // allow to send data
    mock.bytes_to_send = 200;
    anj_core_step(&anj);
    mock.bytes_to_send = 0;
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    COPY_TOKEN_AND_MSG_ID(expected_register_block_1);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register_block_1,
                                      sizeof(expected_register_block_1) - 1);

    // provide first block response
    ADD_RESPONSE(response_block_1);
    anj_core_step(&anj);

    // allow to send data
    mock.bytes_to_send = 200;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    COPY_TOKEN_AND_MSG_ID(expected_register_block_2);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register_block_2,
                                      sizeof(expected_register_block_2) - 1);

    // provide second block response
    ADD_RESPONSE(response_block_2);
    anj_core_step(&anj);

    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_LOCATION_PATHS();
}

ANJ_UNIT_TEST(server_register, register_error_response) {
    EXTENDED_INIT();

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // first register request
    mock.bytes_to_send = 100;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(expected_register) - 1, mock.bytes_sent);
    mock.bytes_sent = 0;

    // provide error response
    static char response_not_allowed[] =
            "\x68"                              // header v 0x01, Ack, tkl 8
            "\x80\x00\x00"                      // Bad Request 4.0, msg id
            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token
    ADD_RESPONSE(response_not_allowed);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // next register request will be sent in 60 seconds
    uint64_t actual_time = 50;
    set_mock_time(actual_time);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);

    // second register request
    set_mock_time_advance(&actual_time, 20);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(expected_register);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register,
                                      sizeof(expected_register) - 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    mock.bytes_sent = 0;

    // second fail
    ADD_RESPONSE(response_not_allowed);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // next register request will be sent in 120 seconds
    set_mock_time_advance(&actual_time, 110);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);

    // third register request
    set_mock_time_advance(&actual_time, 20);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(expected_register);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register,
                                      sizeof(expected_register) - 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    mock.bytes_sent = 0;

    // third fail
    ADD_RESPONSE(response_not_allowed);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // next register request will be sent in 240 seconds
    set_mock_time_advance(&actual_time, 230);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);

    // fourth register try - this time not allow to connect
    set_mock_time_advance(&actual_time, 20);
    mock.call_result[ANJ_NET_FUN_CONNECT] = -20;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;

    // next register request will be sent in 480 seconds
    set_mock_time_advance(&actual_time, 479);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);

    // check if hostname and port are correct provided in next connection calls
    mock.hostname[0] = '\0';
    mock.port[0] = '\0';

    // fifth register try - finally success
    set_mock_time_advance(&actual_time, 2);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(expected_register);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register,
                                      sizeof(expected_register) - 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    // correct response
    ADD_RESPONSE(response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_LOCATION_PATHS();
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.hostname, "server.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.port, "5683");

    // seq retry count is not increased so cleanup should not be called
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 0);
}

ANJ_UNIT_TEST(server_register, register_fail_network_errors) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    // seq_retry_count will be increased after first fail
    anj_communication_retry_res_t comm_retry_res;
    ser_inst.comm_retry_res = &comm_retry_res;
    ser_inst.comm_retry_res->retry_count = 1;
    ser_inst.comm_retry_res->retry_timer = 10;
    ser_inst.comm_retry_res->seq_delay_timer = 1000;
    ser_inst.comm_retry_res->seq_retry_count = 2;
    ADD_INSTANCES();

    // no connection
    mock.call_result[ANJ_NET_FUN_CONNECT] = -20;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // seq_retry_count was increased - cleanup should be called
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);

    // next register attempt will be in 1000 seconds
    uint64_t actual_time = 999;
    set_mock_time(actual_time);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;

    // second attempt - send error
    set_mock_time_advance(&actual_time, 2);
    anj_core_step(&anj);
    anj_core_step(&anj);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    mock.call_result[ANJ_NET_FUN_SEND] = -14;
    anj_core_step(&anj);
    // Registration failed - no more retries. It falls back to Bootstrap,
    // but there is no adequate LwM2M Security Object instance prepared.
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);

    // register failed - cleanup should be called
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 2);
}

ANJ_UNIT_TEST(server_register, register_location_path_error) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    // seq_retry_count will be increased after first fail
    anj_communication_retry_res_t comm_retry_res;
    ser_inst.comm_retry_res = &comm_retry_res;
    ser_inst.comm_retry_res->retry_count = 1;
    ser_inst.comm_retry_res->retry_timer = 10;
    ser_inst.comm_retry_res->seq_delay_timer = 100;
    ser_inst.comm_retry_res->seq_retry_count = 1;
    bool bootstrap_on_registration_failure = false;
    ser_inst.bootstrap_on_registration_failure =
            &bootstrap_on_registration_failure;
    ADD_INSTANCES();

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // allow to send data
    mock.bytes_to_send = 100;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    COPY_TOKEN_AND_MSG_ID(expected_register);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register,
                                      sizeof(expected_register) - 1);

    static char response_six_location_paths[] =
            "\x68"                             // header v 0x01, Ack, tkl 8
            "\x41\x00\x00"                     // CREATED code 2.1
            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
            "\x82\x72\x64"                     // location-path /rd
            "\x04\x35\x61\x33\x66"             // location-path 8 /5a3f
            "\x02\x33\x61"                     // location-path 8 /3a
            "\x02\x34\x61"                     // location-path 8 /4a
            "\x02\x35\x61"                     // location-path 8 /5a
            "\x02\x36\x61"                     // location-path 8 /6a
            "\x02\x37\x61";                    // location-path 8 /7a
    ADD_RESPONSE(response_six_location_paths);
    anj_core_step(&anj);
    // incorrect message is ignored still in REGISTERING state
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
}

ANJ_UNIT_TEST(server_register, register_corrupted_coap_msg) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    // seq_retry_count will be increased after first fail
    anj_communication_retry_res_t comm_retry_res;
    ser_inst.comm_retry_res = &comm_retry_res;
    ser_inst.comm_retry_res->retry_count = 2;
    ser_inst.comm_retry_res->retry_timer = 10;
    ser_inst.comm_retry_res->seq_delay_timer = 100;
    ser_inst.comm_retry_res->seq_retry_count = 1;
    ADD_INSTANCES();

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // allow to send data
    mock.bytes_to_send = 100;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    COPY_TOKEN_AND_MSG_ID(expected_register);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register,
                                      sizeof(expected_register) - 1);

    static char response_2_last_bytes_missed[] =
            "\x68"                             // header v 0x01, Ack, tkl 8
            "\x41\x00\x00"                     // CREATED code 2.1
            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
            "\x82\x72\x64"                     // location-path /rd
            "\x04\x35\x61";                    // location-path 8 /5a3f
    ADD_RESPONSE(response_2_last_bytes_missed);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // next register request will be sent in 10 seconds
    uint64_t actual_time = 11;
    set_mock_time(actual_time);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(expected_register);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      expected_register,
                                      sizeof(expected_register) - 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ADD_RESPONSE(response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_LOCATION_PATHS();
}

ANJ_UNIT_TEST(server_register, register_retransmisions) {
    EXTENDED_INIT();
    anj_exchange_udp_tx_params_t test_params = {
        .max_retransmit = 2,
        .ack_random_factor = 1.01,
        .ack_timeout_ms = 5000
    };
    _anj_exchange_set_udp_tx_params(&anj.exchange_ctx, &test_params);

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    uint64_t actual_time = _ANJ_EXCHANGE_COAP_PROCESSING_DELAY_MS / 1000 + 1;
    set_mock_time(actual_time);
    // there is no send ACK, exchange is cancel and second register attempt
    // starts
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_get_state(&anj.exchange_ctx),
                          ANJ_EXCHANGE_STATE_FINISHED);
    anj_core_step(&anj);

    // next register request will be sent in 60 seconds
    set_mock_time_advance(&actual_time, 61);
    anj_core_step(&anj);

    // second register request
    mock.bytes_to_send = 100;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(expected_register) - 1, mock.bytes_sent);
    mock.bytes_sent = 0;
    mock.bytes_to_send = 0;

    // response timeout is set to 5000 ms
    set_mock_time_advance(&actual_time, 6);
    anj_core_step(&anj);

    // request is send again
    mock.bytes_to_send = 100;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(expected_register) - 1, mock.bytes_sent);

    ADD_RESPONSE(response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_LOCATION_PATHS();
}

ANJ_UNIT_TEST(server_register, register_random_request) {
    EXTENDED_INIT();

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // first register request
    mock.bytes_to_send = 100;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(expected_register) - 1, mock.bytes_sent);
    mock.bytes_sent = 0;

    // provide LwM2M server request, exchange API must ignore it
    static char server_read_request[] =
            "\x48"         // header v 0x01, Confirmable, tkl 1
            "\x01\x12\x12" // GET code 0.1, msg id 1212
            "\x12\x12\x12\x12\x12\x12\x12\x12" // token
            "\xB1\x33"                         // uri-path_1 URI_PATH 11 /3
            "\x01\x30";                        // uri-path_2             /0
    mock.bytes_to_recv = sizeof(server_read_request) - 1;
    mock.data_to_recv = (uint8_t *) server_read_request;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ADD_RESPONSE(response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_LOCATION_PATHS();
}
