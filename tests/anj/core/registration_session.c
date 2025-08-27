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
#include <stdio.h>
#include <string.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/utils.h>

#include "../mock/net_api_mock.h"
#include "../mock/time_api_mock.h"

#include <anj_unit_test.h>

static anj_conn_status_t g_conn_status;
static void
conn_status_cb(void *arg, anj_t *anj, anj_conn_status_t conn_status) {
    (void) arg;
    (void) anj;
    g_conn_status = conn_status;
}

// inner_mtu_value value will lead to block transfer for addtional objects in
// payload
#define _TEST_INIT(With_queue_mode, Queue_timeout)         \
    set_mock_time(0);                                      \
    net_api_mock_t mock = { 0 };                           \
    net_api_mock_ctx_init(&mock);                          \
    mock.bytes_to_send = 100;                              \
    mock.inner_mtu_value = 110;                            \
    anj_t anj;                                             \
    anj_configuration_t config = {                         \
        .endpoint_name = "name",                           \
        .queue_mode_enabled = With_queue_mode,             \
        .queue_mode_timeout_ms = Queue_timeout,            \
        .connection_status_cb = conn_status_cb,            \
    };                                                     \
    ANJ_UNIT_ASSERT_SUCCESS(anj_core_init(&anj, &config)); \
    anj_dm_security_obj_t sec_obj;                         \
    anj_dm_security_obj_init(&sec_obj);                    \
    anj_dm_server_obj_t ser_obj;                           \
    anj_dm_server_obj_init(&ser_obj)

#define TEST_INIT() _TEST_INIT(false, 0)
#define TEST_INIT_WITH_QUEUE_MODE(Queue_timeout) _TEST_INIT(true, Queue_timeout)

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
        .lifetime = 150,                         \
        .binding = "U",                          \
        .iid = &iid                              \
    }

#define INIT_BASIC_BOOTSTRAP_INSTANCE()                   \
    anj_dm_security_instance_init_t boot_sec_inst = {     \
        .server_uri = "coap://bootstrap-server.com:5693", \
        .bootstrap_server = true,                         \
        .security_mode = ANJ_DM_SECURITY_NOSEC,           \
    };                                                    \
    ANJ_UNIT_ASSERT_SUCCESS(                              \
            anj_dm_security_obj_add_instance(&sec_obj, &boot_sec_inst))

#define EXTENDED_INIT()     \
    TEST_INIT();            \
    INIT_BASIC_INSTANCES(); \
    ADD_INSTANCES()

#define EXTENDED_INIT_WITH_BOOTSTRAP() \
    TEST_INIT();                       \
    INIT_BASIC_INSTANCES();            \
    INIT_BASIC_BOOTSTRAP_INSTANCE();   \
    ADD_INSTANCES()

#define EXTENDED_INIT_WITH_QUEUE_MODE(Queue_timeout) \
    TEST_INIT_WITH_QUEUE_MODE(Queue_timeout);        \
    INIT_BASIC_INSTANCES();                          \
    ADD_INSTANCES()

// token and message id are copied from request stored in anj.exchange_ctx
// correct response must contain the same token and message id as request
#define COPY_TOKEN_AND_MSG_ID(Msg, Token_size)                                \
    memcpy(&Msg[4], anj.exchange_ctx.base_msg.token.bytes, Token_size);       \
    Msg[2] = anj.exchange_ctx.base_msg.coap_binding_data.udp.message_id >> 8; \
    Msg[3] = anj.exchange_ctx.base_msg.coap_binding_data.udp.message_id & 0xFF

#define ADD_RESPONSE(Response)                 \
    COPY_TOKEN_AND_MSG_ID(Response, 8);        \
    mock.bytes_to_recv = sizeof(Response) - 1; \
    mock.data_to_recv = (uint8_t *) Response

static char register_response[] =
        "\x68"                             // header v 0x01, Ack, tkl 8
        "\x41\x00\x00"                     // CREATED code 2.1
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\x82\x72\x64"                     // location-path /rd
        "\x04\x35\x61\x33\x66";            // location-path 8 /5a3f

static char update_with_lifetime[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xb2\x72\x64"                     // uri path /rd
        "\x04\x35\x61\x33\x66"             // uri path /5a3f
        "\x46\x6c\x74\x3d\x31\x30\x30";    // uri-query lt=100

static char update[] = "\x48"                             // Confirmable, tkl 8
                       "\x02\x00\x00"                     // POST, msg_id
                       "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
                       "\xb2\x72\x64"                     // uri path /rd
                       "\x04\x35\x61\x33\x66";            // uri path /5a3f

static char update_response[] = "\x68"         // header v 0x01, Ack, tkl 8
                                "\x44\x00\x00" // Changed code 2.04
                                "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token

// in second anj_core_step registration will be finished and there is first
// iteration of REGISTERED state
#define PROCESS_REGISTRATION()                          \
    anj_core_step(&anj);                                \
    ADD_RESPONSE(register_response);                    \
    anj_core_step(&anj);                                \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status, \
                          ANJ_CONN_STATUS_REGISTERED);  \
    anj_core_step(&anj);                                \
    mock.bytes_sent = 0

#define HANDLE_UPDATE(Request)                                        \
    anj_core_step(&anj);                                              \
    COPY_TOKEN_AND_MSG_ID(Request, 8);                                \
    ANJ_UNIT_ASSERT_EQUAL(sizeof(Request) - 1, mock.bytes_sent);      \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, Request, \
                                      mock.bytes_sent);               \
    ADD_RESPONSE(update_response);                                    \
    anj_core_step(&anj);                                              \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,               \
                          ANJ_CONN_STATUS_REGISTERED);                \
    mock.bytes_sent = 0;                                              \
    anj_core_step(&anj);                                              \
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0)

#define MAX_TRANSMIT_WAIT 93
ANJ_UNIT_TEST(registration_session, lifetime_check) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    // nothing changed
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);

    // lifetime changed - new update should be sent
    ser_obj.server_instance.lifetime = 100;
    anj_core_data_model_changed(&anj,
                                &ANJ_MAKE_RESOURCE_PATH(1, 1, 1),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    HANDLE_UPDATE(update_with_lifetime);

    // next update should be sent after 50 seconds
    uint64_t actual_time = 49;
    set_mock_time(actual_time);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    set_mock_time_advance(&actual_time, 2);
    HANDLE_UPDATE(update);

    // lifetime changed - new update should be sent
    ser_obj.server_instance.lifetime = 500;
    // change payload to 500
    update_with_lifetime[24] = '5';
    anj_core_data_model_changed(&anj,
                                &ANJ_MAKE_RESOURCE_PATH(1, 1, 1),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    HANDLE_UPDATE(update_with_lifetime);
    // change payload to 100
    update_with_lifetime[24] = '1';

    // next update should be sent after 500-MAX_TRANSMIT_WAIT
    set_mock_time_advance(&actual_time, (500 - MAX_TRANSMIT_WAIT - 1));
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    set_mock_time_advance(&actual_time, 2);
    HANDLE_UPDATE(update);
}

ANJ_UNIT_TEST(registration_session, inifinite_lifetime) {
    EXTENDED_INIT();
    ser_obj.server_instance.lifetime = 0;
    PROCESS_REGISTRATION();
    // there is no update possible
    uint64_t actual_time = 10000;
    set_mock_time(actual_time);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    // lifetime changed - new update should be sent
    ser_obj.server_instance.lifetime = 100;
    anj_core_data_model_changed(&anj,
                                &ANJ_MAKE_RESOURCE_PATH(1, 1, 1),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    HANDLE_UPDATE(update_with_lifetime);
    // next update should be sent after 50 seconds
    set_mock_time_advance(&actual_time, 51);
    HANDLE_UPDATE(update);
}

ANJ_UNIT_TEST(registration_session, update_trigger) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    anj_core_server_obj_registration_update_trigger_executed(&anj);
    HANDLE_UPDATE(update);
    // lifetime is 150 so next update should be sent after 75 seconds
    uint64_t actual_time = 74;
    set_mock_time(actual_time);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    set_mock_time_advance(&actual_time, 2);
    HANDLE_UPDATE(update);
}

static char update_with_data_model[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xb2\x72\x64"                     // uri path /rd
        "\x04\x35\x61\x33\x66"             // uri path /5a3f
        "\x11\x28" // content_format: application/link-format
        "\xFF"
        "</1>;ver=1.2,</1/1>";

ANJ_UNIT_TEST(registration_session, update_with_data_model) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    // first update is without data model
    uint64_t actual_time = 76;
    set_mock_time(actual_time);
    HANDLE_UPDATE(update);

    anj_core_data_model_changed(&anj, &ANJ_MAKE_INSTANCE_PATH(1, 3),
                                ANJ_CORE_CHANGE_TYPE_ADDED);
    HANDLE_UPDATE(update_with_data_model);
}

static char update_with_data_model_block_1[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xb2\x72\x64"                     // uri path /rd
        "\x04\x35\x61\x33\x66"             // uri path /5a3f
        "\x11\x28"     // content_format: application/link-format
        "\xD1\x02\x0A" // block1 0, size 64, more
        "\xFF"
        "</1>;ver=1.2,</1/1>,</9900>,</9901>,</9902>,</9903>,</9904>,</99";
static char update_with_data_model_block_2[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xb2\x72\x64"                     // uri path /rd
        "\x04\x35\x61\x33\x66"             // uri path /5a3f
        "\x11\x28"     // content_format: application/link-format
        "\xD1\x02\x12" // block1 1, size 64
        "\xFF"
        "05>,</9906>";
static char update_response_block_1[] =
        "\x68"                             // ACK, tkl 1
        "\x5F"                             // Continue
        "\x00\x00"                         // msg id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xd1\x0e\x0A";                    // block1 0, size 64, more

static char update_response_block_2[] =
        "\x68"                             // ACK, tkl 1
        "\x44\x00\x00"                     // Changed code 2.04
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xd1\x0e\x12";                    // block1 1, size 64

ANJ_UNIT_TEST(registration_session, update_with_block_data_model) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    // first update is without data model
    uint64_t actual_time = 76;
    set_mock_time(actual_time);
    HANDLE_UPDATE(update);

    // anj_core_data_model_changed is called in anj_dm_add_obj
    anj_dm_obj_t obj_x[7] = { 0 };
    for (int i = 0; i < 7; i++) {
        obj_x[i].oid = 9900 + i;
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &(obj_x[i])));
    }

    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update_with_data_model_block_1, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      update_with_data_model_block_1,
                                      mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update_with_data_model_block_1) - 1,
                          mock.bytes_sent);
    ADD_RESPONSE(update_response_block_1);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update_with_data_model_block_2, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      update_with_data_model_block_2,
                                      mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update_with_data_model_block_2) - 1,
                          mock.bytes_sent);
    ADD_RESPONSE(update_response_block_2);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);

    // next update doesn't contain data model
    set_mock_time_advance(&actual_time, 76);
    HANDLE_UPDATE(update);
}

ANJ_UNIT_TEST(registration_session, update_retransmissions) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    // seq_retry_count will be increased after first fail
    anj_communication_retry_res_t comm_retry_res = {
        .retry_count = 3,
        .retry_timer = 10,
        .seq_retry_count = 1
    };
    ser_inst.comm_retry_res = &comm_retry_res;
    ADD_INSTANCES();
    anj_core_data_model_changed(&anj, &ANJ_MAKE_OBJECT_PATH(1),
                                ANJ_CORE_CHANGE_TYPE_ADDED);
    PROCESS_REGISTRATION();

    anj_core_server_obj_registration_update_trigger_executed(&anj);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, update,
                                      mock.bytes_sent);
    mock.bytes_sent = 0;

    // first retry - because of resonse timeout
    uint64_t actual_time = 12;
    set_mock_time(actual_time);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, update,
                                      mock.bytes_sent);
    mock.bytes_sent = 0;

    // second retry
    set_mock_time_advance(&actual_time, 12);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, update,
                                      mock.bytes_sent);
    mock.bytes_sent = 0;
    ADD_RESPONSE(update_response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
}

ANJ_UNIT_TEST(registration_session, update_connection_error) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    anj_core_server_obj_registration_update_trigger_executed(&anj);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, update,
                                      mock.bytes_sent);
    // error response always leads to reregistration
    mock.bytes_to_send = 0;
    mock.call_result[ANJ_NET_FUN_RECV] = -14;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    mock.call_result[ANJ_NET_FUN_RECV] = 0;

    // after registration next update is successful
    mock.bytes_to_send = 100;
    PROCESS_REGISTRATION();
    uint64_t actual_time = 100;
    set_mock_time(actual_time);
    HANDLE_UPDATE(update);
}

static char error_response[] = "\x68"         // header v 0x01, Ack, tkl 8
                               "\x80\x00\x00" // Bad Request code 4.00
                               "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token

ANJ_UNIT_TEST(registration_session, update_error_response) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    anj_core_server_obj_registration_update_trigger_executed(&anj);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, update,
                                      mock.bytes_sent);
    // error response always leads to reregistration
    ADD_RESPONSE(error_response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);

    // after registration next update is successful
    PROCESS_REGISTRATION();
    uint64_t actual_time = 100;
    set_mock_time(actual_time);
    HANDLE_UPDATE(update);
}

#define ADD_REQUEST(Request)                  \
    mock.bytes_to_recv = sizeof(Request) - 1; \
    mock.data_to_recv = (uint8_t *) Request

#define CHECK_RESPONSE(Respone)                                       \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, Respone, \
                                      mock.bytes_sent);               \
    ANJ_UNIT_ASSERT_EQUAL(sizeof(Respone) - 1, mock.bytes_sent)

static char read_request[] = "\x42"         // header v 0x01, Confirmable
                             "\x01\x45\x45" // GET code 0.1, msg id
                             "\x12\x34"     // token
                             "\xB1\x31"     //  URI_PATH 11 /1
                             "\x01\x31"     //              /1
                             "\x01\x31"     //              /1
                             "\x60";        // accept 17 plaintext

static char read_response[] = "\x62"         // ACK, tkl 3
                              "\x45\x45\x45" // CONTENT 0x45
                              "\x12\x34"     // token
                              "\xC0"         // content_format: plaintext
                              "\xFF"
                              "\x31\x35\x30"; // 150

static char write_request[] = "\x42"          // header v 0x01, Confirmable
                              "\x03\x47\x24"  // PUT code 0.1
                              "\x12\x34"      // token
                              "\xB1\x31"      // uri-path_1 URI_PATH 11 /1
                              "\x01\x31"      // uri-path_2             /1
                              "\x01\x32"      // uri-path_3             /2
                              "\x10"          // content_format 12 PLAINTEXT 0
                              "\xFF"          // payload marker
                              "\x31\x30\x30"; // payload

static char write_response[] = "\x62"         // ACK, tkl 3
                               "\x44\x47\x24" // CHANGED code 2.4
                               "\x12\x34";    // token

ANJ_UNIT_TEST(registration_session, server_requests) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    // read request
    ADD_REQUEST(read_request);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_RESPONSE(read_response);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    // write request
    ADD_REQUEST(write_request);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_RESPONSE(write_response);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    // write request with lifetime in result after write request update with
    // lifetime is immediately sent
    write_request[11] = 0x31; // change uri-path_3 to /1
    write_request[2] = 0x48;  // change MID
    ADD_REQUEST(write_request);
    anj_core_step(&anj);
    // lifetime changed next update contains lifetime
    HANDLE_UPDATE(update_with_lifetime);
}

static char read_ivalid_path[] = "\x62"         // ACK, tkl 3
                                 "\x84\x45\x45" // NOT_FOUND 0x84
                                 "\x12\x34";    // token

static char write_request_no_payload[] = "\x42" // header v 0x01, Confirmable
                                         "\x03\x47\x24" // PUT code 0.1
                                         "\x12\x34"     // token
                                         "\xB1\x31" // uri-path_1 URI_PATH 11 /1
                                         "\x01\x31" // uri-path_2             /1
                                         "\x01\x31" // uri-path_3             /1
                                         "\x10" // content_format 12 PLAINTEXT 0
                                         "\xFF";

ANJ_UNIT_TEST(registration_session, server_requests_error_handling) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    // invalid_path - change uri-path_1 to /2
    read_request[7] = 0x32;
    ADD_REQUEST(read_request);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_RESPONSE(read_ivalid_path);
    // set uri-path_1 to /1 again
    read_request[7] = 0x31;
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    // invalid - write_response is used as random ACK - message should be
    // ignored
    mock.bytes_sent = 0;
    ADD_REQUEST(write_response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    // malformed request - message should be ignored
    mock.bytes_sent = 0;
    ADD_REQUEST(write_request_no_payload);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    // read request - ACK timeout
    // change msgid to not reveice message from cache
    read_request[2] = 0x46;
    ADD_REQUEST(read_request);
    mock.bytes_to_send = 0;
    anj_core_step(&anj);
    uint64_t actual_time = 5;
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    set_mock_time(actual_time);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    mock.bytes_to_send = 100;

    read_request[2] = 0x45;
}

ANJ_UNIT_TEST(registration_session, server_requests_network_error) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    // network error - reregistration is expected
    mock.bytes_to_send = 0;
    ADD_REQUEST(read_request);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    mock.call_result[ANJ_NET_FUN_SEND] = -14;
    mock.state = ANJ_NET_SOCKET_STATE_CLOSED;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
}

static char observe_request[] =
        "\x42"         // header v 0x01, Confirmable, tkl 8
        "\x01\x11\x21" // GET code 0.1
        "\x56\x78"     // token
        "\x60"         // observe 6 = 0
        "\x51\x31"     // uri-path_1 URI_PATH 11 /1
        "\x01\x31"     // uri-path_2 /1
        "\x01\x35"     // uri-path_3 /5
        "\x48\x70\x6d\x69\x6e\x3d\x31\x30\x30"  // URI_QUERY 15 pmin=100
        "\x08\x70\x6d\x61\x78\x3d\x33\x30\x30"; // URI_QUERY 15 pmax=300

static char observe_response[] =
        "\x62"         // ACK, tkl 2
        "\x45\x11\x21" // CONTENT 2.5 msg id 2222
        "\x56\x78"     // token
        "\x60"         // observe 6 = 0
        "\x62\x2D\x18" // content-format 11544 lwm2mcobr
        "\xFF"
        "\xBF\x01\xBF\x01\xBF\x05\x19\x03\x20\xFF\xFF\xFF"; // 800

static char notification[] =
        "\x52"         // NonConfirmable, tkl 2
        "\x45\x00\x00" // CONTENT 2.5 msg id 0006
        "\x56\x78"     // token
        "\x61\x01"     // observe 0x01
        "\x62\x2D\x18" // content-format 11544 lwm2mcobr
        "\xFF"
        "\xBF\x01\xBF\x01\xBF\x05\x18\xC8\xFF\xFF\xFF"; // 200

// copy msg id
#define CHECK_NOTIFY(Notification)              \
    Notification[2] = mock.send_data_buffer[2]; \
    Notification[3] = mock.send_data_buffer[3]; \
    CHECK_RESPONSE(Notification)

ANJ_UNIT_TEST(registration_session, observations) {
    TEST_INIT();
    INIT_BASIC_INSTANCES();
    ser_inst.lifetime = 1000;
    ser_inst.disable_timeout = 800;
    ser_inst.default_notification_mode = 0;
    ADD_INSTANCES();
    PROCESS_REGISTRATION();

    ADD_REQUEST(observe_request);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_RESPONSE(observe_response);
    // observation should be added
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].ssid, 2);

    uint64_t actual_time = 101;
    set_mock_time(actual_time);
    mock.bytes_sent = 0;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);

    // change disable_timeout value to see if notification is sent
    // pmin is 100 and time is 101 so notification should be sent
    ser_obj.server_instance.disable_timeout = 200;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    anj_core_data_model_changed(&anj,
                                &ANJ_MAKE_RESOURCE_PATH(1, 1, 5),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_NOTIFY(notification);
    mock.bytes_sent = 0;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    // change time to > pmax
    set_mock_time_advance(&actual_time, 301);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    // second notification - increase observe option
    notification[7] = 2;
    CHECK_NOTIFY(notification);
    notification[7] = 1;
    mock.bytes_sent = 0;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);

    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    anj_core_server_obj_registration_update_trigger_executed(&anj);
    HANDLE_UPDATE(update);

    // change time to > pmax to force notification - send error leads to
    // reregistration
    set_mock_time_advance(&actual_time, 301);
    mock.bytes_to_send = 0;
    mock.call_result[ANJ_NET_FUN_SEND] = -14;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    // observation should be removed
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].ssid, 0);
}

static char deregistrer[] = "\x48"         // Confirmable, tkl 8
                            "\x04\x00\x00" // DELETE, msg_id
                            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
                            "\xb2\x72\x64"                     // uri path /rd
                            "\x04\x35\x61\x33\x66";            // uri path /5a3f

static char deregistrer_response[] =
        "\x68"                              // header v 0x01, Ack, tkl 8
        "\x42\x00\x00"                      // Code=Deleted
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token

#define HANDLE_DEREGISTER(Response)                                       \
    anj_core_step(&anj);                                                  \
    COPY_TOKEN_AND_MSG_ID(deregistrer, 8);                                \
    ANJ_UNIT_ASSERT_EQUAL(sizeof(deregistrer) - 1, mock.bytes_sent);      \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, deregistrer, \
                                      mock.bytes_sent);                   \
    ADD_RESPONSE(Response);                                               \
    anj_core_step(&anj);                                                  \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                   \
                          ANJ_CONN_STATUS_SUSPENDED);                     \
    mock.bytes_sent = 0;                                                  \
    anj_core_step(&anj);                                                  \
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0)

ANJ_UNIT_TEST(registration_session, server_disable_by_server) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    // simulate execute on /1/0/4
    anj_core_server_obj_disable_executed(&anj, 5);
    HANDLE_DEREGISTER(deregistrer_response);

    // pass 6s and check if Anjay exits the suspended state
    set_mock_time(6);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
}

ANJ_UNIT_TEST(registration_session, server_disable_failed_deregister) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    // simulate execute on /1/0/4
    anj_core_server_obj_disable_executed(&anj, 5);
    HANDLE_DEREGISTER(error_response);

    // pass 6s and check if Anjay exits the suspended state
    set_mock_time(6);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
}

ANJ_UNIT_TEST(registration_session, server_disable_by_user_with_timeout) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    anj_core_disable_server(&anj, 5);
    HANDLE_DEREGISTER(deregistrer_response);

    // pass 5s and check if Anjay exits the suspended state
    set_mock_time(5);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
}

ANJ_UNIT_TEST(registration_session, server_disable_by_user_with_enable) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    anj_core_disable_server(&anj, 5);

    HANDLE_DEREGISTER(deregistrer_response);

    // pass 2s and enable server manually
    set_mock_time(2);
    anj_core_restart(&anj);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
}

ANJ_UNIT_TEST(registration_session, server_disable_twice) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    // simulate execute on /1/0/4
    anj_core_server_obj_disable_executed(&anj, 50);

    HANDLE_DEREGISTER(deregistrer_response);

    uint64_t mock_time_s = 45;
    set_mock_time(mock_time_s);
    // anjay should leave suspended state after 5 seconds but we add additional
    // 10 seconds
    ANJ_UNIT_ASSERT_EQUAL(anj_core_next_step_time(&anj), 5 * 1000);
    anj_core_disable_server(&anj, 10 * 1000);
    ANJ_UNIT_ASSERT_EQUAL(anj_core_next_step_time(&anj), 10 * 1000);

    // update time and check if Anjay exits the suspended state
    set_mock_time_advance(&mock_time_s, 5);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_SUSPENDED);
    set_mock_time_advance(&mock_time_s, 6);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
}

ANJ_UNIT_TEST(registration_session, queue_mode_check_set_timeout) {
    EXTENDED_INIT_WITH_QUEUE_MODE(50000);
    uint64_t actual_time = 10000;
    set_mock_time(actual_time / 1000);
    PROCESS_REGISTRATION();
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.registered.queue_start_time,
                          actual_time + 50000);
}

ANJ_UNIT_TEST(registration_session, queue_mode_check_default_timeout) {
    EXTENDED_INIT_WITH_QUEUE_MODE(0);
    uint64_t actual_time = 10000;
    set_mock_time(actual_time / 1000);
    PROCESS_REGISTRATION();
    // default timeout is 93 seconds: MAX_TRANSMIT_WAIT
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.registered.queue_start_time,
                          actual_time + (93 * 1000));
}

ANJ_UNIT_TEST(registration_session, queue_mode_basic_check) {
    // lifetime is set to 150 so next update should be sent after 75 seconds
    // queue mode timeout is 50 seconds so after 50 seconds queue mode should be
    // started
    EXTENDED_INIT_WITH_QUEUE_MODE((50 * 1000));
    PROCESS_REGISTRATION();

    uint64_t actual_time_s = 45;
    set_mock_time(actual_time_s);
    ANJ_UNIT_ASSERT_EQUAL(anj_core_next_step_time(&anj), 0);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);

    // client try to close connection before entering queue mode
    set_mock_time_advance(&actual_time_s, 10);
    mock.call_result[ANJ_NET_FUN_CLOSE] = ANJ_NET_EAGAIN;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_ENTERING_QUEUE_MODE);
    ANJ_UNIT_ASSERT_EQUAL(anj_core_next_step_time(&anj), 0);

    // close connection is successful - queue mode is started
    mock.call_result[ANJ_NET_FUN_CLOSE] = 0;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);

    // lifetime is 150 so next update should be sent after 75 seconds, 55
    // seconds already passed
    ANJ_UNIT_ASSERT_EQUAL(anj_core_next_step_time(&anj), (75 - 55) * 1000);

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    set_mock_time_advance(&actual_time_s, 25);
    // it's update time - first client will try to open connection
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EAGAIN;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    HANDLE_UPDATE(update);

    // after 10 seconds we are still in registered state
    // 40 second before next queue mode
    set_mock_time_advance(&actual_time_s, 10);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.registered.queue_start_time
                                  / 1000,
                          actual_time_s + 40);

    // read request extend queue mode time
    ADD_REQUEST(read_request);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_RESPONSE(read_response);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.registered.queue_start_time
                                  / 1000,
                          actual_time_s + 50);

    // close connection is successful - queue mode is started immediately
    set_mock_time_advance(&actual_time_s, 51);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    // next anj_core_step() is called with big delay, but still everything is ok
    // and update is sent
    set_mock_time_advance(&actual_time_s, 3000);
    HANDLE_UPDATE(update);
}

ANJ_UNIT_TEST(registration_session, queue_mode_force_update) {
    // lifetime is set to 150 so next update should be sent after 75 seconds
    // queue mode timeout is 50 seconds so after 50 seconds queue mode should be
    // started
    EXTENDED_INIT_WITH_QUEUE_MODE((50 * 1000));
    PROCESS_REGISTRATION();

    uint64_t actual_time_s = 55;
    set_mock_time(actual_time_s);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    ANJ_UNIT_ASSERT_EQUAL(anj_core_next_step_time(&anj), 20 * 1000);
    anj_core_request_update(&anj);
    HANDLE_UPDATE(update);
}

ANJ_UNIT_TEST(registration_session, queue_mode_notifications) {
    TEST_INIT_WITH_QUEUE_MODE((50 * 1000));
    INIT_BASIC_INSTANCES();
    // set longer lifetime to avoid update before notification
    ser_inst.lifetime = 300;
    ser_inst.disable_timeout = 800;
    ADD_INSTANCES();
    PROCESS_REGISTRATION();

    // pmin/pmax are set to 100/300 so notification should be sent after 100
    // seconds
    ADD_REQUEST(observe_request);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_RESPONSE(observe_response);

    // enter queue mode - notification shouldn't be sent yet
    uint64_t actual_time = 60;
    set_mock_time(actual_time);
    mock.bytes_sent = 0;
    ser_obj.server_instance.disable_timeout = 200;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    anj_core_data_model_changed(&anj,
                                &ANJ_MAKE_RESOURCE_PATH(1, 1, 5),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);

    // pmin is set to 100 so notification should be sent after 100 seconds
    // 60 seconds already passed
    ANJ_UNIT_ASSERT_EQUAL(anj_core_next_step_time(&anj), (100 - 60) * 1000);

    // notification should be sent
    set_mock_time_advance(&actual_time, 50);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_NOTIFY(notification);

    // enter queue mode after 50 seconds
    set_mock_time_advance(&actual_time, 40);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    set_mock_time_advance(&actual_time, 20);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);

    // we are 170 seconds after start
    // update should be sent after 300 - MAX_TRANSMIT_WAIT = 207 seconds
    set_mock_time_advance(&actual_time, 30);
    anj_core_step(&anj);
    // liftime is set to 300 so next update should be sent after 207 seconds
    // 200 seconds already passed
    ANJ_UNIT_ASSERT_EQUAL(anj_core_next_step_time(&anj), (207 - 200) * 1000);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    set_mock_time_advance(&actual_time, 10);
    ANJ_UNIT_ASSERT_EQUAL(anj_core_next_step_time(&anj), 0);
    HANDLE_UPDATE(update);
}

static char observe_request_no_attributes[] =
        "\x42"         // header v 0x01, Confirmable, tkl 8
        "\x01\x11\x21" // GET code 0.1
        "\x56\x78"     // token
        "\x60"         // observe 6 = 0
        "\x51\x31"     // uri-path_1 URI_PATH 11 /1
        "\x01\x31"     // uri-path_2 /1
        "\x01\x35";    // uri-path_3 /5

ANJ_UNIT_TEST(registration_session, queue_mode_notifications_no_attributes) {
    TEST_INIT_WITH_QUEUE_MODE((50 * 1000));
    INIT_BASIC_INSTANCES();
    // set longer lifetime to avoid update before notification
    ser_inst.lifetime = 300;
    ser_inst.disable_timeout = 800;
    ADD_INSTANCES();
    PROCESS_REGISTRATION();

    ADD_REQUEST(observe_request_no_attributes);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_RESPONSE(observe_response);

    // enter queue mode - notification shouldn't be sent yet
    uint64_t actual_time = 60;
    set_mock_time(actual_time);
    mock.bytes_sent = 0;
    ser_obj.server_instance.disable_timeout = 200;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);
    // no pmin is set so notification should be immediately sent after change
    anj_core_data_model_changed(&anj,
                                &ANJ_MAKE_RESOURCE_PATH(1, 1, 5),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_NOTIFY(notification);
}

ANJ_UNIT_TEST(registration_session, queue_mode_connection_error) {
    EXTENDED_INIT_WITH_QUEUE_MODE((50 * 1000));
    PROCESS_REGISTRATION();
    ANJ_UNIT_ASSERT_EQUAL(g_conn_status, ANJ_CONN_STATUS_REGISTERED);

    uint64_t actual_time_s = 45;
    set_mock_time(actual_time_s);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    set_mock_time_advance(&actual_time_s, 10);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    ANJ_UNIT_ASSERT_EQUAL(g_conn_status, ANJ_CONN_STATUS_QUEUE_MODE);

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    set_mock_time_advance(&actual_time_s, 25);
    // it's update time - first client will try to open connection
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EAGAIN;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    // connection error leads to reregistration
    mock.call_result[ANJ_NET_FUN_CONNECT] = -888;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    ANJ_UNIT_ASSERT_EQUAL(g_conn_status, ANJ_CONN_STATUS_REGISTERING);
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    // first attempt to reregister failed because of network error
    // (log from INFO [server] [../src/anj/core/server_register.c:
    // `Registration retry no. 1 will start with 60s delay`)
    // so we need to wait at least 60 seconds to see next attempt
    set_mock_time_advance(&actual_time_s, 70);
    PROCESS_REGISTRATION();
    ANJ_UNIT_ASSERT_EQUAL(g_conn_status, ANJ_CONN_STATUS_REGISTERED);
}

ANJ_UNIT_TEST(registration_session, queue_mode_update_error) {
    EXTENDED_INIT_WITH_QUEUE_MODE((50 * 1000));
    PROCESS_REGISTRATION();

    uint64_t actual_time_s = 55;
    set_mock_time(actual_time_s);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);

    set_mock_time_advance(&actual_time_s, 25);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, update,
                                      mock.bytes_sent);
    // error response always leads to reregistration
    ADD_RESPONSE(error_response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    PROCESS_REGISTRATION();
    // after registration next update is successful
    // because it's already update time we skip queue mode
    set_mock_time_advance(&actual_time_s, 100);
    HANDLE_UPDATE(update);
}

ANJ_UNIT_TEST(registration_session, queue_mode_entering_queue_mode_error) {
    EXTENDED_INIT_WITH_QUEUE_MODE((50 * 1000));
    PROCESS_REGISTRATION();

    uint64_t actual_time_s = 45;
    set_mock_time(actual_time_s);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);

    // client try to close connection before entering queue mode
    set_mock_time_advance(&actual_time_s, 10);
    mock.net_eagain_calls = 1;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_ENTERING_QUEUE_MODE);

    // close connection error leads to reregistration
    mock.call_result[ANJ_NET_FUN_CLOSE] = -888;
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EAGAIN;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    // next connection attempt is successful
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    PROCESS_REGISTRATION();
}

static char bootstrap_request_trigger[] = "\x42" // header v 0x01, Confirmable
                                          "\x02\x11\x55" // POST code 0.2
                                          "\x12\x77"     // token
                                          "\xB1\x31"     //  URI_PATH 11 /1
                                          "\x01\x31"     //              /1
                                          "\x01\x39";    //              /9
static char execute_response[] = "\x62"                  // ACK, tkl 3
                                 "\x44\x11\x55"          // Changed code 2.04
                                 "\x12\x77";             // token

ANJ_UNIT_TEST(registration_session, bootstrap_trigger) {
    EXTENDED_INIT();
    // first try will fail because we are not registered yet
    anj_core_server_obj_bootstrap_request_trigger_executed(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.bootstrap_request_triggered, false);
    PROCESS_REGISTRATION();

    mock.call_result[ANJ_NET_FUN_CLEANUP] = ANJ_NET_EAGAIN;
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EAGAIN;
    mock.bytes_to_send = 0;
    // bootstrap request trigger should be executed
    // then response shuld be sent
    // in next step connection should be closed, with cleanup
    ADD_REQUEST(bootstrap_request_trigger);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 0);
    mock.bytes_to_send = 500;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_RESPONSE(execute_response);
    mock.call_result[ANJ_NET_FUN_CLEANUP] = 0;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 2);
    // bootstrap should start here, but we don't have valid configuration
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
}

ANJ_UNIT_TEST(registration_session, shutdown) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    static const int cleanup_retries = 5;
    mock.call_result[ANJ_NET_FUN_CLEANUP] = ANJ_NET_EAGAIN;
    for (int i = 0; i < cleanup_retries; ++i) {
        ANJ_UNIT_ASSERT_EQUAL(anj_core_shutdown(&anj), ANJ_NET_EAGAIN);
        ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], i + 1);
    }
    mock.call_result[ANJ_NET_FUN_CLEANUP] = 0;
    ANJ_UNIT_ASSERT_EQUAL(anj_core_shutdown(&anj), 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP],
                          cleanup_retries + 1);
    // after shutdown we should be able to restart client
    ANJ_UNIT_ASSERT_SUCCESS(anj_core_init(&anj, &config));
    sec_obj.installed = false;
    ser_obj.installed = false;
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &ser_obj));
    PROCESS_REGISTRATION();
    ANJ_UNIT_ASSERT_EQUAL(anj_core_shutdown(&anj), 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP],
                          cleanup_retries + 2);
}

#define HANDLE_DEREGISTER_WITH_REGISTRATION()                             \
    anj_core_step(&anj);                                                  \
    COPY_TOKEN_AND_MSG_ID(deregistrer, 8);                                \
    ANJ_UNIT_ASSERT_EQUAL(sizeof(deregistrer) - 1, mock.bytes_sent);      \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, deregistrer, \
                                      mock.bytes_sent);                   \
    ADD_RESPONSE(deregistrer_response);                                   \
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EAGAIN;               \
    anj_core_step(&anj);                                                  \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                   \
                          ANJ_CONN_STATUS_REGISTERING);

ANJ_UNIT_TEST(registration_session, restart) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_core_restart(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 0);
    // send deregister and then close the connection with cleanup
    // after restart we should be able to register again
    HANDLE_DEREGISTER_WITH_REGISTRATION();
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    PROCESS_REGISTRATION();
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);
}

ANJ_UNIT_TEST(registration_session, restart_from_queue_mode) {
    EXTENDED_INIT_WITH_QUEUE_MODE((50 * 1000));
    PROCESS_REGISTRATION();

    uint64_t actual_time_s = 51;
    set_mock_time(actual_time_s);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 1);
    anj_core_restart(&anj);
    // first we want to connect to the server
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EAGAIN;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 2);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    // then we start the standard deregistration process
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    HANDLE_DEREGISTER_WITH_REGISTRATION();
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    PROCESS_REGISTRATION();
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);
    // 1 register, 2 when leaving queue mode, 2 when reregistering
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 5);
}

ANJ_UNIT_TEST(registration_session, suspend_from_queue_mode) {
    EXTENDED_INIT_WITH_QUEUE_MODE((50 * 1000));
    PROCESS_REGISTRATION();

    uint64_t actual_time_s = 51;
    set_mock_time(actual_time_s);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 1);
    anj_core_disable_server(&anj, 10);
    // first we want to connect to the server
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EAGAIN;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 2);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    // then we start the standard deregistration process
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    HANDLE_DEREGISTER(deregistrer_response);
    // important check - cleanup shouldn't be called
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 0);
    // 1 register, 2 when leaving queue mode
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 3);
}

ANJ_UNIT_TEST(registration_session, suspend_from_queue_mode_with_open_error) {
    EXTENDED_INIT_WITH_QUEUE_MODE((50 * 1000));
    PROCESS_REGISTRATION();

    uint64_t actual_time_s = 51;
    set_mock_time(actual_time_s);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    anj_core_disable_server(&anj, 10);
    // error during queue mode doesn't lead to reregistration but still to
    // suspend
    mock.call_result[ANJ_NET_FUN_CONNECT] = -1;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_SUSPENDED);
}

static char expected_bootstrap[] =
        "\x48"                              // Confirmable, tkl 8
        "\x02\x00\x00"                      // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"  // token
        "\xb2\x62\x73"                      // uri path /bs
        "\x47\x65\x70\x3d\x6e\x61\x6d\x65"  // uri-query: ep=name
        "\x07\x70\x63\x74\x3d\x31\x31\x32"; // uri-query: pct=112

#define HANDLE_DEREGISTER_WITH_BOOTSTRAP()                                \
    anj_core_step(&anj);                                                  \
    COPY_TOKEN_AND_MSG_ID(deregistrer, 8);                                \
    ANJ_UNIT_ASSERT_EQUAL(sizeof(deregistrer) - 1, mock.bytes_sent);      \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, deregistrer, \
                                      mock.bytes_sent);                   \
    ADD_RESPONSE(deregistrer_response);                                   \
    anj_core_step(&anj);                                                  \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                   \
                          ANJ_CONN_STATUS_BOOTSTRAPPING);                 \
    COPY_TOKEN_AND_MSG_ID(expected_bootstrap, 8);                         \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,              \
                                      expected_bootstrap,                 \
                                      sizeof(expected_bootstrap) - 1);

ANJ_UNIT_TEST(registration_session, suspend_from_bootstrap) {
    EXTENDED_INIT_WITH_BOOTSTRAP();
    PROCESS_REGISTRATION();
    // force bootstrap
    anj_core_request_bootstrap(&anj);
    HANDLE_DEREGISTER_WITH_BOOTSTRAP();
    anj_core_disable_server(&anj, 10);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_SUSPENDED);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 2);
}

#define PROCESS_BOOTSTRAP()                                  \
    anj_core_step(&anj);                                     \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,      \
                          ANJ_CONN_STATUS_BOOTSTRAPPING);    \
    COPY_TOKEN_AND_MSG_ID(expected_bootstrap, 8);            \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, \
                                      expected_bootstrap,    \
                                      sizeof(expected_bootstrap) - 1);

ANJ_UNIT_TEST(registration_session, restart_from_bootstrap) {
    EXTENDED_INIT_WITH_BOOTSTRAP();
    PROCESS_REGISTRATION();
    anj_core_request_bootstrap(&anj);
    HANDLE_DEREGISTER_WITH_BOOTSTRAP();
    // restart should lead to registration
    anj_core_restart(&anj);
    PROCESS_REGISTRATION();
}

ANJ_UNIT_TEST(registration_session, restart_from_suspend) {
    EXTENDED_INIT_WITH_BOOTSTRAP();
    PROCESS_REGISTRATION();
    anj_core_request_bootstrap(&anj);
    HANDLE_DEREGISTER_WITH_BOOTSTRAP();
    anj_core_disable_server(&anj, 10);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_SUSPENDED);
    anj_core_restart(&anj);
    PROCESS_REGISTRATION();
}

ANJ_UNIT_TEST(registration_session, bootstrap_from_suspend) {
    EXTENDED_INIT_WITH_BOOTSTRAP();
    PROCESS_REGISTRATION();
    anj_core_disable_server(&anj, 10);
    HANDLE_DEREGISTER(deregistrer_response);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_SUSPENDED);
    anj_core_request_bootstrap(&anj);
    PROCESS_BOOTSTRAP();
}

ANJ_UNIT_TEST(registration_session, bootstrap_from_suspend_from_bootstrap) {
    // stop bootstrap and start again - to see if everything is cleaned up
    // properly
    EXTENDED_INIT_WITH_BOOTSTRAP();
    PROCESS_REGISTRATION();
    anj_core_request_bootstrap(&anj);
    HANDLE_DEREGISTER_WITH_BOOTSTRAP();
    anj_core_disable_server(&anj, 10);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_SUSPENDED);
    anj_core_request_bootstrap(&anj);
    PROCESS_BOOTSTRAP();
}

ANJ_UNIT_TEST(registration_session, bootstrap_from_registering) {
    EXTENDED_INIT_WITH_BOOTSTRAP();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    anj_core_request_bootstrap(&anj);
    PROCESS_BOOTSTRAP();
}

ANJ_UNIT_TEST(registration_session, bootstrap_and_suspend_from_registering) {
    EXTENDED_INIT_WITH_BOOTSTRAP();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERING);
    // we trigger bootstrap and then suspend, but bootstrap has higher priority
    // so we should end up in bootstrapping state
    anj_core_request_bootstrap(&anj);
    anj_core_disable_server(&anj, 10);
    PROCESS_BOOTSTRAP();
}

ANJ_UNIT_TEST(registration_session, queue_mode_with_reuse_port_error) {
    EXTENDED_INIT_WITH_QUEUE_MODE((50 * 1000));
    PROCESS_REGISTRATION();

    uint64_t actual_time_s = 51;
    set_mock_time(actual_time_s);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_QUEUE_MODE);
    mock.call_result[ANJ_NET_FUN_REUSE_LAST_PORT] = -35;
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EAGAIN;
    set_mock_time_advance(&actual_time_s, 25);
    // reconnect failed because of reuse port error, so we process new
    // registration
    anj_core_step(&anj);
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    PROCESS_REGISTRATION();
}

static char CoAP_PING[] = "\x40\x00\x01\x01"; // Confirmable, tkl 0, empty msg
static char RST_response[] = "\x70"           // ACK, tkl 0
                             "\x00\x01\x01";  // empty msg

ANJ_UNIT_TEST(registration_session, CoAP_ping) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    ADD_REQUEST(CoAP_PING);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
    CHECK_RESPONSE(RST_response);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
}

static char write_mute_send_enable_request[] =
        "\x42"         // header v 0x01, Confirmable, TKL=2
        "\x02\x47\x26" // POST code 0.2, MID 0x4726
        "\x56\x78"     // token
        "\xB1\x31"     // uri-path /1
        "\x01\x31"     // uri-path /1
        "\x12\x2d\x16" // content format TLV
        "\xFF"         // payload marker
        "\xc1\x17"     // TLV resource 23
        "\x01";        // true

static char write_mute_send_enable_response[] =
        "\x62"      // Header: Ver=1, Type=2 (ACK), TKL=2
        "\x44"      // Code: 2.04 Changed
        "\x47\x26"  // Message ID: 0x4726
        "\x56\x78"; // Token: 0x5678

static char write_mute_send_disable_request[] =
        "\x42"         // header v 0x01, Confirmable, TKL=2
        "\x02\x47\x26" // POST code 0.2, MID 0x4726
        "\x56\x78"     // token
        "\xB1\x31"     // uri-path /1
        "\x01\x31"     // uri-path /1
        "\x12\x2d\x16" // content format TLV
        "\xFF"         // payload marker
        "\xc1\x17"     // TLV resource 23
        "\x00";        // false

static char execute_request[] =
        "\x42"         // Header: Ver=1, Type=0 (CON), TKL=2
        "\x02\x47\x25" // Code=EXECUTE (0.05), MID=0x4725
        "\x12\x34"     // Token
        "\xB1\x33"     // Uri-Path: /3
        "\x01\x30"     // Uri-Path: /0
        "\x01\x34";    // Uri-Path: /4

static char execute_ack_response[] = "\x62"      // Ver=1, Type=2 (ACK), TKL=2
                                     "\x44"      // Code=2.04 (Changed)
                                     "\x47\x25"  // Message ID = 0x4725
                                     "\x12\x34"; // Token = 0x1234

static int g_reboot_execute_counter;

static void reboot_cb(void *arg, anj_t *anj) {
    (void) anj;
    (void) arg;
    g_reboot_execute_counter++;
}

#define ADD_DEVICE_OBJECT()                                               \
    char serial_number[] = "53r141-number";                               \
    anj_dm_device_obj_t device_obj;                                       \
    anj_dm_device_object_init_t dev_obj_init = {                          \
        .reboot_cb = reboot_cb,                                           \
        .serial_number = serial_number                                    \
    };                                                                    \
    ANJ_UNIT_ASSERT_SUCCESS(                                              \
            anj_dm_device_obj_install(&anj, &device_obj, &dev_obj_init)); \
    g_reboot_execute_counter = 0

/**
 * Send request and expect responses as follows:
 * - send execute request
 * - expect execution and response
 * - send same execute request
 * - expect no execution but same response
 */
ANJ_UNIT_TEST(registration_session, retransmission_latest_execute) {
    EXTENDED_INIT();
    ADD_DEVICE_OBJECT();
    PROCESS_REGISTRATION();

    ADD_REQUEST(execute_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(execute_ack_response);
    ASSERT_EQ(g_reboot_execute_counter, 1);

    // clear response buffer
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    ADD_REQUEST(execute_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(execute_ack_response);
    // counter not incremented - request was not executed, only responded to
    ASSERT_EQ(g_reboot_execute_counter, 1);
}

/**
 * Send request and expect responses as follows:
 * - send write request
 * - expect writing and response
 * - send wite request with same MID but other payload
 * - expect no writing but same response
 */
ANJ_UNIT_TEST(registration_session, retransmission_latest_write) {
    EXTENDED_INIT();
    ADD_DEVICE_OBJECT();
    PROCESS_REGISTRATION();

    ASSERT_FALSE(anj.server_instance.mute_send);
    ADD_REQUEST(write_mute_send_enable_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_mute_send_enable_response);
    ASSERT_TRUE(anj.server_instance.mute_send);

    // clear response buffer
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    ADD_REQUEST(write_mute_send_disable_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_mute_send_enable_response);
    // Mute Send stays enabled, as for first request
    ASSERT_TRUE(anj.server_instance.mute_send);
}

/**
 * Send request and expect responses as follows:
 * - send execute request
 * - expect execution and response
 * - send write request
 * - expect writing and response
 * - send same execute request
 * - expect no execution and no response
 */
ANJ_UNIT_TEST(registration_session, retransmission_non_latest_execute) {
    EXTENDED_INIT();
    ADD_DEVICE_OBJECT();
    PROCESS_REGISTRATION();

    ADD_REQUEST(execute_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(execute_ack_response);
    ASSERT_EQ(g_reboot_execute_counter, 1);

    // clear response buffer
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    ASSERT_FALSE(anj.server_instance.mute_send);
    ADD_REQUEST(write_mute_send_enable_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_mute_send_enable_response);
    ASSERT_TRUE(anj.server_instance.mute_send);

    // clear response buffer
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    ADD_REQUEST(execute_request);
    anj_core_step(&anj);
    // no response
    ASSERT_EQ(mock.bytes_sent, 0);
    ASSERT_EQ(g_reboot_execute_counter, 1);
}

/**
 * Send request and expect responses as follows:
 * - send write request
 * - expect writing and response
 * - send execute request
 * - expect execution and response
 * - send same write request
 * - expect no writing and no response
 */
ANJ_UNIT_TEST(registration_session, retransmission_non_latest_write) {
    EXTENDED_INIT();
    ADD_DEVICE_OBJECT();
    PROCESS_REGISTRATION();

    ASSERT_FALSE(anj.server_instance.mute_send);
    ADD_REQUEST(write_mute_send_enable_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_mute_send_enable_response);
    ASSERT_TRUE(anj.server_instance.mute_send);

    // clear response buffer
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    ADD_REQUEST(execute_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(execute_ack_response);
    ASSERT_EQ(g_reboot_execute_counter, 1);

    // clear response buffer
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    ADD_REQUEST(write_mute_send_disable_request);
    anj_core_step(&anj);
    // no response
    ASSERT_EQ(mock.bytes_sent, 0);
    // Mute Send stays enabled, as for first request
    ASSERT_TRUE(anj.server_instance.mute_send);
}

static char read_serial_number_request[] =
        "\x42"         // Header: Ver=1, Type=0 (CON), TKL=2
        "\x01\x47\x27" // Code: 0.01 (GET), MID: 0x4727
        "\x56\x78"     // Token: 0x5678
        "\xB1\x33"     // Uri-Path: "3"
        "\x01\x30"     // Uri-Path: "0"
        "\x01\x32"     // Uri-Path: "2"
        "\x61\x00";    // Accept: 0 (text/plain)

static char read_serial_number_response[] =
        "\x62"     // ACK
        "\x45"     // 2.05 Content
        "\x47\x27" // MID
        "\x56\x78" // Token
        "\xC0"     // Content-Format = 0 (text/plain)
        "\xFF"     // payload marker
        "53r141-number";

static char read_serial_number_service_unavailable[] =
        "\x62"      // ACK
        "\xA3"      // 5.03 Service Unavailable
        "\x47\x27"  // MID
        "\x56\x78"; // Token

/**
 * Send read request twice and verify that the responses are the same
 */
ANJ_UNIT_TEST(registration_session, retransmission_read) {
    EXTENDED_INIT();
    ADD_DEVICE_OBJECT();
    PROCESS_REGISTRATION();

    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_response);

    // change serial number value
    snprintf(serial_number, ANJ_ARRAY_SIZE(serial_number), "new-value");

    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    // retransmission
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_response);
}

/**
 * - receive read request
 * - handle it
 * - open another exchange
 * - receive same read request
 * - handle it regardles of being in exchange as we have a ready response to
 * send right away
 */
ANJ_UNIT_TEST(registration_session, send_cached_during_another_exchange) {
    EXTENDED_INIT();
    ADD_DEVICE_OBJECT();
    PROCESS_REGISTRATION();

    // receive read request and respond properly
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_response);

    // change serial number value
    snprintf(serial_number, ANJ_ARRAY_SIZE(serial_number), "new-value");

    // add an object to force a payload in Update message
    anj_dm_obj_t obj_x = {
        .oid = 9900
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_x));
    anj_core_step(&anj);
    ASSERT_NE(mock.bytes_sent, 0);

    // clear buffer with update
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    // send the same request and see if it's handled same way again
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_response);

    // receive response to the update request
    ADD_RESPONSE(update_response);
    anj_core_step(&anj);

    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
}

/**
 * - open an exchange and wait for response
 * - receive read request and respond with 5.03
 * - recevie response and close the exchange
 * - receive same read request
 * - send cached 5.03 again
 */
ANJ_UNIT_TEST(registration_session, cache_service_unavailable) {
    EXTENDED_INIT();
    ADD_DEVICE_OBJECT();
    PROCESS_REGISTRATION();

    // advance time to trigger update
    set_mock_time(76);
    anj_core_step(&anj);
    ASSERT_NE(mock.bytes_sent, 0);

    // clear buffer with update
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    // receive read request and respond with 5.03
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_service_unavailable);

    // receive response to the update request
    ADD_RESPONSE(update_response);
    anj_core_step(&anj);

    // clear buffer with update
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    // send the same request and see if it's handled same way again
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_service_unavailable);
}

static char read_path_that_dont_exists[] =
        "\x42"         // Header: Ver=1, Type=0 (CON), TKL=2
        "\x01\x47\x27" // Code: 0.01 (GET), MID: 0x4727
        "\x56\x78"     // Token: 0x5678
        "\xB1\x39"     // Uri-Path: "9"
        "\x01\x39"     // Uri-Path: "9"
        "\x01\x39"     // Uri-Path: "9"
        "\x61\x00";    // Accept: 0 (text/plain)

static char read_path_that_dont_exists_response[] = "\x62"     // ACK
                                                    "\x84"     // 4.04 Not found
                                                    "\x47\x27" // MID
                                                    "\x56\x78"; // Token

ANJ_UNIT_TEST(registration_session, cache_response_with_error) {
    EXTENDED_INIT();
    ADD_DEVICE_OBJECT();
    PROCESS_REGISTRATION();

    ADD_REQUEST(read_path_that_dont_exists);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_path_that_dont_exists_response);

    // This time request for existing path, but with the same msgid as previous
    // request
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_path_that_dont_exists_response);
}

static char ret_update_with_data_model_block_1[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xb2\x72\x64"                     // uri path /rd
        "\x04\x35\x61\x33\x66"             // uri path /5a3f
        "\x11\x28"     // content_format: application/link-format
        "\xD1\x02\x0A" // block1 0, size 64, more
        "\xFF"
        "</1>;ver=1.2,</1/1>,</3>;ver=1.0,</3/0>,</9900>,</9901>,</9902>,";

static char ret_update_with_data_model_block_2[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xb2\x72\x64"                     // uri path /rd
        "\x04\x35\x61\x33\x66"             // uri path /5a3f
        "\x11\x28"     // content_format: application/link-format
        "\xD1\x02\x1A" // block1 1, size 64, more
        "\xFF"
        "</9903>,</9904>,</9905>,</9906>,</9907>,</9908>,</9909>,</9910>,";

static char ret_update_with_data_model_block_3[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xb2\x72\x64"                     // uri path /rd
        "\x04\x35\x61\x33\x66"             // uri path /5a3f
        "\x11\x28"     // content_format: application/link-format
        "\xD1\x02\x22" // block1 2, size 64
        "\xFF"
        "</9911>";

static char ret_update_response_block_2[] =
        "\x68"                             // ACK, tkl 1
        "\x5F\x00\x00"                     // Continue
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xd1\x0e\x1A";                    // block1 1, size 64, more

static char ret_update_response_block_3[] =
        "\x68"                             // ACK, tkl 1
        "\x44\x00\x00"                     // Changed code 2.04
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xd1\x0e\x22";                    // block1 2, size 64

ANJ_UNIT_TEST(registration_session,
              upstream_block_request_retransmission_previous_downlink_request) {
    EXTENDED_INIT();
    ADD_DEVICE_OBJECT();
    PROCESS_REGISTRATION();

    // receive read request and respond properly
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_response);

    // change serial number value
    snprintf(serial_number, ANJ_ARRAY_SIZE(serial_number), "new-value");

    // anj_core_data_model_changed is called in anj_dm_add_obj
    anj_dm_obj_t obj_x[12] = { 0 };
    for (int i = 0; i < 12; i++) {
        obj_x[i].oid = 9900 + i;
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &(obj_x[i])));
    }

    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(ret_update_with_data_model_block_1, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      ret_update_with_data_model_block_1,
                                      mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(ret_update_with_data_model_block_1) - 1,
                          mock.bytes_sent);

    // send the same request and see if it's handled same way again
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_response);

    ADD_RESPONSE(update_response_block_1);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(ret_update_with_data_model_block_2, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      ret_update_with_data_model_block_2,
                                      mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(ret_update_with_data_model_block_2) - 1,
                          mock.bytes_sent);

    // send the same request and see if it's handled same way again
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_response);

    ADD_RESPONSE(ret_update_response_block_2);
    anj_core_step(&anj);

    COPY_TOKEN_AND_MSG_ID(ret_update_with_data_model_block_3, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      ret_update_with_data_model_block_3,
                                      mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(ret_update_with_data_model_block_3) - 1,
                          mock.bytes_sent);

    // send the same request and see if it's handled same way again
    ADD_REQUEST(read_serial_number_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(read_serial_number_response);

    ADD_RESPONSE(ret_update_response_block_3);
    anj_core_step(&anj);

    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
}

ANJ_UNIT_TEST(registration_session, retransmission_eagain) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    ASSERT_FALSE(anj.server_instance.mute_send);
    ADD_REQUEST(write_mute_send_enable_request);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_mute_send_enable_response);
    ASSERT_TRUE(anj.server_instance.mute_send);

    // anj_core_data_model_changed is called in anj_dm_add_obj
    anj_dm_obj_t obj_x[7] = { 0 };
    for (int i = 0; i < 7; i++) {
        obj_x[i].oid = 9900 + i;
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &(obj_x[i])));
    }

    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update_with_data_model_block_1, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      update_with_data_model_block_1,
                                      mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update_with_data_model_block_1) - 1,
                          mock.bytes_sent);

    // send request with equal msg id, _anj_server_send will return eagain
    mock.bytes_to_send = 0;
    mock.call_result[ANJ_NET_FUN_SEND] = ANJ_NET_EAGAIN;
    ADD_REQUEST(write_mute_send_disable_request);
    anj_core_step(&anj);
    // previous value
    mock.bytes_to_send = 100;
    mock.call_result[ANJ_NET_FUN_SEND] = 0;
    anj_core_step(&anj);
    CHECK_RESPONSE(write_mute_send_enable_response);
    ASSERT_TRUE(anj.server_instance.mute_send);

    ADD_RESPONSE(update_response_block_1);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(update_with_data_model_block_2, 8);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      update_with_data_model_block_2,
                                      mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update_with_data_model_block_2) - 1,
                          mock.bytes_sent);
    ADD_RESPONSE(update_response_block_2);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_REGISTERED);
}

static char res_buff[100] = { 0 };
static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) anj;
    (void) obj;
    if (iid == 0 && rid == 0) {
        return anj_dm_write_string_chunked(value, res_buff, sizeof(res_buff),
                                           NULL);
    }
    return 0;
}

static anj_dm_obj_t obj_111 = {
    .oid = 111,
    .insts = &(anj_dm_obj_inst_t) {
        .iid = 0,
        .res_count = 1,
        .resources =
                (anj_dm_res_t[])
                        {
                            {
                                .rid = 0,
                                .operation = ANJ_DM_RES_W,
                                .type = ANJ_DATA_TYPE_STRING
                            }
                        }
    },
    .max_inst_count = 1,
    .handlers = &(anj_dm_handlers_t) {
        .res_write = res_write
    }
};

static char block_write_request_1[] =
        "\x42"              // header v 0x01, Confirmable
        "\x03\x47\x24"      // PUT code 0.1
        "\x12\x34"          // token
        "\xB3\x31\x31\x31"  // uri-path_1 URI_PATH 11 /111
        "\x01\x30"          // uri-path_2             /0
        "\x01\x30"          // uri-path_3             /0
        "\x10"              // content_format 12 PLAINTEXT 0
        "\xD1\x02\x08"      // block1 0, size 16, more
        "\xFF"              // payload marker
        "very long string"; // payload

static char block_write_request_2[] =
        "\x42"              // header v 0x01, Confirmable
        "\x03\x47\x25"      // PUT code 0.1
        "\x12\x34"          // token
        "\xB3\x31\x31\x31"  // uri-path_1 URI_PATH 11 /111
        "\x01\x30"          // uri-path_2             /0
        "\x01\x30"          // uri-path_3             /0
        "\x10"              // content_format 12 PLAINTEXT 0
        "\xD1\x02\x18"      // block1 1, size 16, more
        "\xFF"              // payload marker
        " split into thre"; // payload

static char block_write_request_3[] =
        "\x42"             // header v 0x01, Confirmable
        "\x03\x47\x26"     // PUT code 0.1
        "\x12\x34"         // token
        "\xB3\x31\x31\x31" // uri-path_1 URI_PATH 11 /111
        "\x01\x30"         // uri-path_2             /0
        "\x01\x30"         // uri-path_3             /0
        "\x10"             // content_format 12 PLAINTEXT 0
        "\xD1\x02\x20"     // block1 2, size 16
        "\xFF"             // payload marker
        "e requests";      // payload

static char write_response_1[] = "\x62"          // ACK, tkl 3
                                 "\x5F\x47\x24"  // Continue
                                 "\x12\x34"      // token
                                 "\xd1\x0e\x08"; // block1 0, size 16, more

static char write_response_2[] = "\x62"          // ACK, tkl 3
                                 "\x5F\x47\x25"  // Continue
                                 "\x12\x34"      // token
                                 "\xd1\x0e\x18"; // block1 1, size 16, more

static char write_response_3[] = "\x62"          // ACK, tkl 3
                                 "\x44\x47\x26"  // CHANGED code 2.4
                                 "\x12\x34"      // token
                                 "\xd1\x0e\x20"; // block1 2, size 16

ANJ_UNIT_TEST(registration_session, retransmission_write_block) {
    EXTENDED_INIT();
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_111));
    PROCESS_REGISTRATION();

    ADD_REQUEST(block_write_request_1);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_response_1);

    // clear buffer with response
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    // retransmission of first block
    ADD_REQUEST(block_write_request_1);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_response_1);

    // clear buffer with response
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    // retransmission of first block, sent return eagain
    mock.bytes_to_send = 0;
    mock.call_result[ANJ_NET_FUN_SEND] = ANJ_NET_EAGAIN;
    ADD_REQUEST(block_write_request_1);
    anj_core_step(&anj);
    ASSERT_EQ(mock.bytes_sent, 0);
    // previous value
    mock.bytes_to_send = 100;
    mock.call_result[ANJ_NET_FUN_SEND] = 0;
    anj_core_step(&anj);
    CHECK_RESPONSE(write_response_1);

    ADD_REQUEST(block_write_request_2);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_response_2);

    // clear buffer with response
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    // retransmission of first block, no response since it is not latest cached
    // one
    ADD_REQUEST(block_write_request_1);
    anj_core_step(&anj);
    ASSERT_EQ(mock.bytes_sent, 0);

    // retransmission of second block
    ADD_REQUEST(block_write_request_2);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_response_2);

    ADD_REQUEST(block_write_request_3);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_response_3);

    // clear buffer with response
    mock.bytes_sent = 0;
    memset(mock.send_data_buffer, 0, 100);

    // retransmission of third block
    ADD_REQUEST(block_write_request_3);
    anj_core_step(&anj);
    CHECK_RESPONSE(write_response_3);

    ANJ_UNIT_ASSERT_EQUAL_BYTES(res_buff,
                                "very long string split into three requests");

    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
}
