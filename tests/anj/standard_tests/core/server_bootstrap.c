/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/compat/net/anj_net_api.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>

#include "../../../../src/anj/core/server_bootstrap.h"

#include "../mock/net_api_mock.h"
#include "../mock/time_api_mock.h"

#include <anj_unit_test.h>

#define TEST_INIT()                                        \
    mock_time_reset();                                     \
    net_api_mock_t mock = { 0 };                           \
    net_api_mock_ctx_init(&mock);                          \
    mock.inner_mtu_value = 128;                            \
    anj_t anj;                                             \
    anj_configuration_t config = {                         \
        .endpoint_name = "name"                            \
    };                                                     \
    ANJ_UNIT_ASSERT_SUCCESS(anj_core_init(&anj, &config)); \
    anj_dm_security_obj_t sec_obj;                         \
    anj_dm_security_obj_init(&sec_obj);                    \
    anj_dm_server_obj_t ser_obj;                           \
    anj_dm_server_obj_init(&ser_obj)

#define ADD_INSTANCE_SECURITY()                           \
    anj_dm_security_instance_init_t sec_inst = {          \
        .server_uri = "coap://bootstrap-server.com:5693", \
        .bootstrap_server = true,                         \
        .security_mode = ANJ_DM_SECURITY_NOSEC,           \
    };                                                    \
    ANJ_UNIT_ASSERT_SUCCESS(                              \
            anj_dm_security_obj_add_instance(&sec_obj, &sec_inst))

#define ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS()                       \
    ADD_INSTANCE_SECURITY();                                              \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj)); \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &ser_obj))

ANJ_UNIT_TEST(server_bootstrap, bootstrap_init_success) {
    TEST_INIT();
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);

    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.server_uri,
                                 "bootstrap-server.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.port, "5693");
    ANJ_UNIT_ASSERT_EQUAL(anj.security_instance.type, ANJ_NET_BINDING_UDP);
}

ANJ_UNIT_TEST(server_bootstrap, bootstrap_init_no_security_object) {
    TEST_INIT();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
}

ANJ_UNIT_TEST(server_bootstrap, bootstrap_init_no_security_instance) {
    TEST_INIT();
    ADD_INSTANCE_SECURITY();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
}

ANJ_UNIT_TEST(server_bootstrap,
              bootstrap_init_delayed_by_client_hold_off_time) {
    TEST_INIT();
    anj_dm_security_instance_init_t sec_inst = {
        .server_uri = "coap://bootstrap-server.com:5693",
        .bootstrap_server = true,
        .security_mode = ANJ_DM_SECURITY_NOSEC,
        .client_hold_off_time = 10,
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &sec_inst));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &ser_obj));
    anj_core_step(&anj);

    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);

    mock_time_advance(anj_time_duration_new(11, ANJ_TIME_UNIT_S));
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_BOOTSTRAP_IN_PROGRESS);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.server_uri,
                                 "bootstrap-server.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(anj.security_instance.port, "5693");
    ANJ_UNIT_ASSERT_EQUAL(anj.security_instance.type, ANJ_NET_BINDING_UDP);
}

// correct response must contain the same token and message id as request
#define COPY_TOKEN_AND_MSG_ID(Response, SourceRequest) \
    memcpy(&Response[4], &SourceRequest[4], 8);        \
    Response[2] = SourceRequest[2];                    \
    Response[3] = SourceRequest[3]

#define RECEIVE_RESPONSE_FROM_LWM2M_SERVER(Response, SourceRequest) \
    COPY_TOKEN_AND_MSG_ID(Response, SourceRequest);                 \
    mock.bytes_to_recv = sizeof(Response) - 1;                      \
    mock.data_to_recv = (uint8_t *) Response

#define RECEIVE_REQUEST_FROM_LWM2M_SERVER(Request) \
    mock.bytes_to_recv = sizeof(Request) - 1;      \
    mock.data_to_recv = (uint8_t *) Request

static char expected_bootstrap[] =
        "\x48"                              // Confirmable, tkl 8
        "\x02\x00\x00"                      // POST, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"  // token
        "\xb2\x62\x73"                      // uri path /bs
        "\x47\x65\x70\x3d\x6e\x61\x6d\x65"  // uri-query: ep=name
        "\x07\x70\x63\x74\x3d\x31\x31\x32"; // uri-query: pct=112

static char response[] = "\x68"         // v 0x01, Ack, TKL=8
                         "\x44\x00\x00" // CHANGED code 2.4, msg_id
                         "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token

static char response_not_acceptable[] =
        "\x68"                              // v 0x01, Ack, TKL=8
        "\x86\x00\x00"                      // NOT ACCEPTABLE code 4.06, msg_id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token

static char expected_security_instance_write[] =
        "\x48"                             // Confirmable, tkl 8
        "\x03\x4b\xa8"                     // PUT, msg_id = 0x4ba8
        "\x4f\x54\x8a\x03\xf0\xfd\x88\xc0" // token
        "\xb1\x30"                         // uri path /0
        "\x01\x31"                         // uri path /1
        "\x11\x70"                         // content type senml+cbor
        "\xFF"                             // payload marker
        "\x84\xa3\x21\x65"
        "/0/1/"
        "\x00"
        "\x61\x30\x03\x76"
        "coap://server.com:5683"
        "\xa2\x00\x61\x32\x02\x03"
        "\xa2\x00\x61\x31\x04\xf4"
        "\xa2\x00\x62\x31\x30\x02\x01";

static char expected_security_instance_write_retransmission[] =
        "\x48"                             // Confirmable, tkl 8
        "\x03\x4b\xa8"                     // PUT, msg_id = 0x4ba8
        "\x4f\x54\x8a\x03\xf0\xfd\x88\xc0" // token
        "\xb1\x30"                         // uri path /0
        "\x01\x32"                         // uri path /2
        "\x11\x70"                         // content type senml+cbor
        "\xFF"                             // payload marker
        "\x84\xa3\x21\x65"
        "/0/1/"
        "\x00"
        "\x61\x30\x03\x76"
        "coap://server.org:5683"
        "\xa2\x00\x61\x32\x02\x03"
        "\xa2\x00\x61\x31\x04\xf4"
        "\xa2\x00\x62\x31\x30\x02\x01";

static char expected_server_instance_write[] =
        "\x48"                             // Confirmable, tkl 8
        "\x03\x4b\xb0"                     // PUT, msg_id = 0x4bb0
        "\x4f\x54\x8a\x04\xf0\xfd\x88\xc0" // token
        "\xb1\x31"                         // uri path /1
        "\x01\x32"                         // uri path /2
        "\x11\x70"                         // content type senml+cbor
        "\xFF"                             // payload marker
        "\x84\xa3\x21\x65"
        "/1/2/"
        "\x00"
        "\x61\x30\x02\x01"
        "\xa2\x00\x61\x31\x02\x18\x3c"
        "\xa2\x00\x61\x36\x04\xf4"
        "\xa2\x00\x61\x37\x03\x61\x55";

static char expected_bootstrap_finish[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x4b\xb8"                     // POST, msg_id = 0x4bb8
        "\x4f\x54\x8a\x05\xf0\xfd\x88\xc0" // token
        "\xb2\x62\x73";                    // uri path /bs

#define SEND_BOOTSTRAP_REQUEST()                                       \
    anj_core_step(&anj);                                               \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                \
                          ANJ_CONN_STATUS_BOOTSTRAPPING);              \
    COPY_TOKEN_AND_MSG_ID(expected_bootstrap, mock.send_data_buffer);  \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,           \
                                      expected_bootstrap,              \
                                      sizeof(expected_bootstrap) - 1); \
                                                                       \
    RECEIVE_RESPONSE_FROM_LWM2M_SERVER(response, expected_bootstrap);  \
    anj_core_step(&anj);                                               \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                \
                          ANJ_CONN_STATUS_BOOTSTRAPPING)

#define RECEIVE_SECURITY_OBJECT_WRITE()                                  \
    RECEIVE_REQUEST_FROM_LWM2M_SERVER(expected_security_instance_write); \
    anj_core_step(&anj);                                                 \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                  \
                          ANJ_CONN_STATUS_BOOTSTRAPPING);                \
                                                                         \
    anj_core_step(&anj);                                                 \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                  \
                          ANJ_CONN_STATUS_BOOTSTRAPPING);                \
    COPY_TOKEN_AND_MSG_ID(response, expected_security_instance_write);   \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(                                   \
            mock.send_data_buffer, response, sizeof(response) - 1)

#define RECEIVE_SERVER_OBJECT_WRITE()                                  \
    RECEIVE_REQUEST_FROM_LWM2M_SERVER(expected_server_instance_write); \
    anj_core_step(&anj);                                               \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                \
                          ANJ_CONN_STATUS_BOOTSTRAPPING);              \
                                                                       \
    anj_core_step(&anj);                                               \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                \
                          ANJ_CONN_STATUS_BOOTSTRAPPING);              \
    COPY_TOKEN_AND_MSG_ID(response, expected_server_instance_write);   \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(                                 \
            mock.send_data_buffer, response, sizeof(response) - 1)
/**
 * Below Bootstrap mimic is a minimum version which consists of four steps:
 * 1. Send Bootstrap request
 * 2. Receive Boostrap Write with Security Object Instance
 * 3. Receive Boostrap Write with Server Object Instance
 * 4. Receive Bootstrap Finish
 */
#define MIMIC_BOOTSTRAP()                                                      \
    anj_core_step(&anj);                                                       \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                        \
                          ANJ_CONN_STATUS_BOOTSTRAPPING);                      \
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CONNECTED);         \
                                                                               \
    /* allow sending data */                                                   \
    mock.bytes_to_send = 100;                                                  \
                                                                               \
    /* Step 1. Send Bootstrap request */                                       \
    SEND_BOOTSTRAP_REQUEST();                                                  \
                                                                               \
    /* Step 2. Receive Boostrap Write with Security Object Instance */         \
    RECEIVE_SECURITY_OBJECT_WRITE();                                           \
                                                                               \
    /* Step 3. Receive Boostrap Write with Server Object Instance */           \
    RECEIVE_SERVER_OBJECT_WRITE();                                             \
                                                                               \
    /* Step 4. Receive Bootstrap Finish */                                     \
    /* if below function is not used, Register Request will overwrite response \
    / to Bootstrap Finish */                                                   \
    net_api_mock_dont_overwrite_buffer(anj.connection_ctx.net_ctx);            \
    RECEIVE_REQUEST_FROM_LWM2M_SERVER(expected_bootstrap_finish);              \
    anj_core_step(&anj);                                                       \
                                                                               \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                        \
                          ANJ_CONN_STATUS_REGISTERING);                        \
    COPY_TOKEN_AND_MSG_ID(response, mock.send_data_buffer);                    \
    /* Register Request was dropped by mock and in buffer is Bootstrap Finish  \
     */                                                                        \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(                                         \
            mock.send_data_buffer, response, sizeof(response) - 1)

ANJ_UNIT_TEST(server_bootstrap, mimic_bootstrap) {
    TEST_INIT();
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    MIMIC_BOOTSTRAP();
}

ANJ_UNIT_TEST(server_bootstrap, mimic_bootstrap_exceeds_lifetime) {
    TEST_INIT();
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CONNECTED);

    // allow sending data
    mock.bytes_to_send = 100;

    // Step 1. Send Bootstrap request
    SEND_BOOTSTRAP_REQUEST();

    // Step 2. Absence of the following request from server exceeds lifetime
    mock_time_advance(
            anj_time_duration_add(anj.bootstrap_ctx.bootstrap_lifetime,
                                  anj_time_duration_new(1, ANJ_TIME_UNIT_S)));
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_ERROR);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);
}

ANJ_UNIT_TEST(server_bootstrap,
              mimic_bootstrap_exceeds_lifetime_with_retry_success) {
    TEST_INIT();
    // reinit with additional configuration
    config.bootstrap_retry_count = 1;
    config.bootstrap_retry_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S);
    anj_core_init(&anj, &config);
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CONNECTED);

    // allow sending data
    mock.bytes_to_send = 100;

    // Step 1. Send Bootstrap request
    SEND_BOOTSTRAP_REQUEST();

    // Step 2. Absence of the following request from server exceeds lifetime
    mock_time_advance(
            anj_time_duration_add(anj.bootstrap_ctx.bootstrap_lifetime,
                                  anj_time_duration_new(1, ANJ_TIME_UNIT_S)));
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);

    mock_time_advance(anj_time_duration_new(6, ANJ_TIME_UNIT_S));
    MIMIC_BOOTSTRAP();
}

ANJ_UNIT_TEST(server_bootstrap, lifetime_check_from_last_request) {
    TEST_INIT();
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CONNECTED);

    // allow sending data
    mock.bytes_to_send = 100;

    // Send Bootstrap request
    SEND_BOOTSTRAP_REQUEST();

    // Advance some time
    mock_time_advance(anj_time_duration_new(100, ANJ_TIME_UNIT_S));

    // Receive Security Object Write
    RECEIVE_SECURITY_OBJECT_WRITE();

    // Above Request from server should reset timeout so we can wait
    // anj.bootstrap_ctx.bootstrap_lifetime seconds from now
    mock_time_advance(
            anj_time_duration_sub(anj.bootstrap_ctx.bootstrap_lifetime,
                                  anj_time_duration_new(1, ANJ_TIME_UNIT_S)));
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_BOOTSTRAP_IN_PROGRESS);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
}

ANJ_UNIT_TEST(server_bootstrap, data_model_validation_error) {
    TEST_INIT();
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CONNECTED);

    // allow sending data
    mock.bytes_to_send = 100;

    // mimic Bootstrap but skip Security Object Write to simulate error
    SEND_BOOTSTRAP_REQUEST();
    RECEIVE_SERVER_OBJECT_WRITE();
    RECEIVE_REQUEST_FROM_LWM2M_SERVER(expected_bootstrap_finish);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_ERROR);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
    COPY_TOKEN_AND_MSG_ID(response_not_acceptable, mock.send_data_buffer);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer,
                                      response_not_acceptable,
                                      sizeof(response_not_acceptable) - 1);
}

ANJ_UNIT_TEST(server_bootstrap, connection_failure_no_retry) {
    TEST_INIT();
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    net_api_mock_force_connection_failure();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_ERROR);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);
}

ANJ_UNIT_TEST(server_bootstrap, connection_failure_and_retry_failed) {
    TEST_INIT();
    // reinit with additional configuration
    config.bootstrap_retry_count = 1;
    config.bootstrap_retry_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S);
    anj_core_init(&anj, &config);
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    net_api_mock_force_connection_failure();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);

    net_api_mock_force_connection_failure();
    mock_time_advance(anj_time_duration_new(6, ANJ_TIME_UNIT_S));
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_ERROR);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);
}

ANJ_UNIT_TEST(server_bootstrap, connection_failure_and_retry_success) {
    TEST_INIT();
    // reinit with additional configuration
    config.bootstrap_retry_count = 1;
    config.bootstrap_retry_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S);
    anj_core_init(&anj, &config);
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    net_api_mock_force_connection_failure();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);

    mock_time_advance(anj_time_duration_new(6, ANJ_TIME_UNIT_S));
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_BOOTSTRAP_IN_PROGRESS);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CONNECTED);
}

ANJ_UNIT_TEST(server_bootstrap, send_failure_no_retry) {
    TEST_INIT();
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    net_api_mock_force_send_failure();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_ERROR);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);
}

ANJ_UNIT_TEST(server_bootstrap, send_failure_and_retry_failed) {
    TEST_INIT();
    // reinit with additional configuration
    config.bootstrap_retry_count = 1;
    config.bootstrap_retry_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S);
    anj_core_init(&anj, &config);
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    net_api_mock_force_send_failure();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);

    net_api_mock_force_send_failure();
    mock_time_advance(anj_time_duration_new(6, ANJ_TIME_UNIT_S));
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_ERROR);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);
}

ANJ_UNIT_TEST(server_bootstrap, send_failure_and_retry_success) {
    TEST_INIT();
    // reinit with additional configuration
    config.bootstrap_retry_count = 1;
    config.bootstrap_retry_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S);
    anj_core_init(&anj, &config);
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    net_api_mock_force_send_failure();
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);

    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_WAITING);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CLOSED);

    mock_time_advance(anj_time_duration_new(6, ANJ_TIME_UNIT_S));
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.details.bootstrap.bootstrap_state,
                          _ANJ_SRV_BOOTSTRAP_STATE_BOOTSTRAP_IN_PROGRESS);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL(mock.state, ANJ_NET_SOCKET_STATE_CONNECTED);
}

static char CoAP_PING[] = "\x40\x00\x12\x34"; // Confirmable, tkl 0, empty msg
static char RST_response[] = "\x70"           // ACK, tkl 0
                             "\x00\x12\x34";  // empty msg

ANJ_UNIT_TEST(server_bootstrap, CoAP_ping) {
    TEST_INIT();
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    // allow sending data
    mock.bytes_to_send = 100;
    SEND_BOOTSTRAP_REQUEST();
    mock.bytes_to_recv = sizeof(CoAP_PING) - 1;
    mock.data_to_recv = (uint8_t *) CoAP_PING;
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            mock.send_data_buffer, RST_response, sizeof(RST_response) - 1);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,
                          ANJ_CONN_STATUS_BOOTSTRAPPING);
}

#ifdef ANJ_WITH_CACHE
ANJ_UNIT_TEST(server_bootstrap, retransmission) {
    TEST_INIT();
    ADD_SECURITY_INSTANCE_AND_INSTALL_OBJECTS();
    anj_core_step(&anj);

    // allow sending data
    mock.bytes_to_send = 100;

    // Send Bootstrap request
    SEND_BOOTSTRAP_REQUEST();

    // Receive Security Object Write
    RECEIVE_REQUEST_FROM_LWM2M_SERVER(expected_security_instance_write);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(response, expected_security_instance_write);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            mock.send_data_buffer, response, sizeof(response) - 1);

    mock.bytes_sent = 0;

    memset(mock.send_data_buffer, 0, 100);
    // retransmission - serving another security instance create request
    // would result in error
    RECEIVE_REQUEST_FROM_LWM2M_SERVER(
            expected_security_instance_write_retransmission);
    anj_core_step(&anj);
    COPY_TOKEN_AND_MSG_ID(response,
                          expected_security_instance_write_retransmission);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            mock.send_data_buffer, response, sizeof(response) - 1);
}
#endif // ANJ_WITH_CACHE
