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

#define ANJ_UNIT_ENABLE_SHORT_ASSERTS
#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/device_object.h>
#include <anj/dm/server_object.h>

#include "../../../src/anj/coap/coap.h"
#include "../../../src/anj/core/register.h"
#include "../../../src/anj/dm/dm_io.h"
#include "../../../src/anj/exchange.h"

#include <anj_unit_test.h>

static void
verify_payload(char *expected, size_t expected_len, _anj_coap_msg_t *msg) {
    uint8_t out_buff[500];
    size_t out_msg_size = 0;
    _anj_coap_msg_t msg_cpy = *msg;
    msg_cpy.coap_binding_data.udp.message_id = 0x1111;
    msg_cpy.token.size = 1;
    msg_cpy.token.bytes[0] = 0x01;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
            &msg_cpy, out_buff, sizeof(out_buff), &out_msg_size));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(out_buff, expected, expected_len);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, expected_len);
}

#define TEST_INIT(Anj, Exchange_ctx)                                        \
    anj_t Anj = { 0 };                                                      \
    anj_dm_device_obj_t Device_obj;                                         \
    _anj_dm_initialize(&Anj);                                               \
    anj_dm_device_object_init_t Device_init = { 0 };                        \
    ASSERT_OK(anj_dm_device_obj_install(&Anj, &Device_obj, &Device_init));  \
    anj_dm_server_obj_t Server_obj;                                         \
    anj_dm_server_obj_init(&Server_obj);                                    \
    anj_dm_server_instance_init_t Server_inst_1 = {                         \
        .ssid = 1,                                                          \
        .binding = "U",                                                     \
        .lifetime = 1                                                       \
    };                                                                      \
    ASSERT_OK(anj_dm_server_obj_add_instance(&Server_obj, &Server_inst_1)); \
    ASSERT_OK(anj_dm_server_obj_install(&Anj, &Server_obj));                \
    _anj_exchange_init(&Exchange_ctx);                                      \
    _anj_register_ctx_init(&Anj);

#define NEW_REQUEST(Msg, Exchange_ctx, Exchange_handlers, Payload)          \
    ASSERT_EQ(_anj_exchange_new_client_request(&Exchange_ctx, &Msg,         \
                                               &Exchange_handlers, Payload, \
                                               sizeof(Payload)),            \
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);                              \
    ASSERT_EQ(_anj_exchange_process(&Exchange_ctx,                          \
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,   \
                                    &Msg),                                  \
              ANJ_EXCHANGE_STATE_WAITING_MSG);

// update and deregister should use location path from register response
ANJ_UNIT_TEST(register, base_register_update_deregister) {
    _anj_exchange_ctx_t exchange_ctx;
    uint8_t payload[100];
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers;

    TEST_INIT(anj, exchange_ctx);

    /* REGISTER MESSAGE WITH RESPONSE HANDLING */

    // register attributes encoding is tested in anj tests
    _anj_attr_register_t attr = {
        .has_endpoint = true,
        .endpoint = "name",
        .has_lifetime = true,
        .lifetime = anj_time_duration_new(1, ANJ_TIME_UNIT_S),
        .has_lwm2m_ver = true,
        .lwm2m_ver = _ANJ_LWM2M_VERSION_STR
    };
    _anj_register_register(&anj, &attr, &msg, &exchange_handlers);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);

    NEW_REQUEST(msg, exchange_ctx, exchange_handlers, payload);

    char expected_register[] =
            "\x41"             // Confirmable, tkl 1
            "\x02\x11\x11\x01" // POST, msg_id token
            "\xb2\x72\x64"     // uri path /rd
            "\x11\x28"         // content_format: application/link-format
            "\x37\x65\x70\x3d\x6e\x61\x6d\x65"         // uri-query ep=name
            "\x04\x6c\x74\x3d\x31"                     // uri-query lt=1
            "\x09\x6c\x77\x6d\x32\x6d\x3d\x31\x2e\x32" // uri-query lwm2m=1.2
            "\xFF"
            "</1>;ver=1.2,</1/0>,</3>;ver=1.0,</3/0>";
    verify_payload(expected_register, sizeof(expected_register) - 1, &msg);
    // register message response with 2 location paths
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_CREATED;
    msg.payload_size = 0;
    msg.location_path.location_count = 2;
    msg.location_path.location[0] = "dd";
    msg.location_path.location_len[0] = 2;
    msg.location_path.location[1] = "eee";
    msg.location_path.location_len[1] = 3;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_FINISHED);

    /* UPDATE MESSAGE WITH RESPONSE HANDLING */
    memset(&msg, 0, sizeof(msg));
    _anj_register_update(&anj, NULL, false, &msg, &exchange_handlers);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);

    NEW_REQUEST(msg, exchange_ctx, exchange_handlers, payload);
    // update message with 2 location paths from previous response
    char expected_update[] = "\x41"              // Confirmable, tkl 1
                             "\x02\x11\x11\x01"  // POST, msg_id token
                             "\xb2\x64\x64"      // uri path /dd
                             "\x03\x65\x65\x65"; // uri path /eee
    verify_payload(expected_update, sizeof(expected_update) - 1, &msg);
    // empty response
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_CHANGED;
    msg.payload_size = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_FINISHED);

    /* DEREGISTER MESSAGE WITH RESPONSE HANDLING */
    memset(&msg, 0, sizeof(msg));
    _anj_register_deregister(&anj, &msg, &exchange_handlers);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);
    NEW_REQUEST(msg, exchange_ctx, exchange_handlers, payload);
    // deregister message with 2 location paths from previous response
    char expected_deregister[] = "\x41"              // Confirmable, tkl 1
                                 "\x04\x11\x11\x01"  // DELETE, msg_id token
                                 "\xb2\x64\x64"      // uri path /dd
                                 "\x03\x65\x65\x65"; // uri path /eee
    verify_payload(expected_deregister, sizeof(expected_deregister) - 1, &msg);
    // empty response
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_DELETED;
    msg.payload_size = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_FINISHED);
}

ANJ_UNIT_TEST(register, exchange_failed) {
    _anj_exchange_ctx_t exchange_ctx;
    uint8_t payload[100];
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers;

    TEST_INIT(anj, exchange_ctx);
    _anj_attr_register_t attr = {
        .has_endpoint = true,
        .endpoint = "name",
        .has_lifetime = true,
        .lifetime = 1,
        .has_lwm2m_ver = true,
        .lwm2m_ver = _ANJ_LWM2M_VERSION_STR
    };
    _anj_register_register(&anj, &attr, &msg, &exchange_handlers);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);

    NEW_REQUEST(msg, exchange_ctx, exchange_handlers, payload);
    // register message is checked in previous test
    // cancel the exchange to simulate exchange failure
    _anj_exchange_terminate(&exchange_ctx, _ANJ_EXCHANGE_ERROR_NETWORK);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_ERROR);
}

ANJ_UNIT_TEST(register, location_path_too_long) {
    _anj_exchange_ctx_t exchange_ctx;
    uint8_t payload[100];
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers;

    TEST_INIT(anj, exchange_ctx);

    _anj_attr_register_t attr = {
        .has_endpoint = true,
        .endpoint = "name",
        .has_lifetime = true,
        .lifetime = 1,
        .has_lwm2m_ver = true,
        .lwm2m_ver = _ANJ_LWM2M_VERSION_STR
    };
    _anj_register_register(&anj, &attr, &msg, &exchange_handlers);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);
    NEW_REQUEST(msg, exchange_ctx, exchange_handlers, payload);

    // register message response with 2 location paths
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_CREATED;
    msg.payload_size = 0;
    msg.location_path.location_count = 1;
    msg.location_path.location[0] = "dddeee";
    // allowed location path length is 5, check CMakeLists.txt
    msg.location_path.location_len[0] = 6;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_ERROR);
}

// block transfers are tested more detailed in exchange and dm_integration
// tests
ANJ_UNIT_TEST(register, register_with_block_transfer) {
    _anj_exchange_ctx_t exchange_ctx;
    // 32 bytes means that payload will be split into 2 blocks
    uint8_t payload[32];
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers;

    TEST_INIT(anj, exchange_ctx);

    _anj_attr_register_t attr = {
        .has_endpoint = true,
        .endpoint = "name",
    };
    _anj_register_register(&anj, &attr, &msg, &exchange_handlers);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);

    // first block request
    NEW_REQUEST(msg, exchange_ctx, exchange_handlers, payload);
    char expected_register_1[] =
            "\x41"             // Confirmable, tkl 1
            "\x02\x11\x11\x01" // POST, msg_id token
            "\xb2\x72\x64"     // uri path /rd
            "\x11\x28"         // content_format: application/link-format
            "\x37\x65\x70\x3d\x6e\x61\x6d\x65" // uri-query ep=name
            "\xc1\x09"                         // block1 0, size 32, more
            "\xFF"
            "</1>;ver=1.2,</1/0>,</3>;ver=1.0";
    verify_payload(expected_register_1, sizeof(expected_register_1) - 1, &msg);

    // first block response
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_CONTINUE;
    msg.payload_size = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    // second block request
    char expected_register_2[] =
            "\x41"             // Confirmable, tkl 1
            "\x02\x11\x11\x01" // POST, msg_id token
            "\xb2\x72\x64"     // uri path /rd
            "\x11\x28"         // content_format: application/link-format
            "\x37\x65\x70\x3d\x6e\x61\x6d\x65" // uri-query ep=name
            "\xc1\x11"                         // block1 1, size 32
            "\xFF"
            ",</3/0>";
    verify_payload(expected_register_2, sizeof(expected_register_2) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_WAITING_MSG);
    // second block response
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_CREATED;
    msg.payload_size = 0;
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);
    ASSERT_EQ(_anj_exchange_process(&exchange_ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_FINISHED);
}

// after first block cancel the exchange and confirm that register API calls
// data model and operation is finished
ANJ_UNIT_TEST(register, block_transfer_with_error) {
    _anj_exchange_ctx_t exchange_ctx;
    // 32 bytes means that payload will be split into 2 blocks
    uint8_t payload[32];
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers;

    TEST_INIT(anj, exchange_ctx);

    _anj_attr_register_t attr = {
        .has_endpoint = true,
        .endpoint = "name",
    };
    _anj_register_register(&anj, &attr, &msg, &exchange_handlers);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);

    // first block request
    NEW_REQUEST(msg, exchange_ctx, exchange_handlers, payload);
    ASSERT_TRUE(anj.dm.op_in_progress);
    _anj_exchange_terminate(&exchange_ctx, _ANJ_EXCHANGE_ERROR_NETWORK);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_ERROR);
    ASSERT_FALSE(anj.dm.op_in_progress);
}

ANJ_UNIT_TEST(register, update_with_data_model_payload) {
    _anj_exchange_ctx_t exchange_ctx;
    uint8_t payload[100];
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers;

    TEST_INIT(anj, exchange_ctx);

    anj.register_ctx.location_path[0][0] = 'd';
    anj.register_ctx.location_path_len[0] = 1;
    _anj_register_update(&anj, NULL, true, &msg, &exchange_handlers);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);

    NEW_REQUEST(msg, exchange_ctx, exchange_handlers, payload);
    // update message with 2 location paths from previous response
    char expected_update[] =
            "\x41"             // Confirmable, tkl 1
            "\x02\x11\x11\x01" // POST, msg_id token
            "\xb1\x64"         // uri path /d
            "\x11\x28"         // content_format: application/link-format
            "\xFF"
            "</1>;ver=1.2,</1/0>,</3>;ver=1.0,</3/0>";
    verify_payload(expected_update, sizeof(expected_update) - 1, &msg);
    // empty response
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_CHANGED;
    msg.payload_size = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_FINISHED);
}

ANJ_UNIT_TEST(register, update_with_lifetime) {
    _anj_exchange_ctx_t exchange_ctx;
    uint8_t payload[100];
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers;

    TEST_INIT(anj, exchange_ctx);

    anj.register_ctx.location_path[0][0] = 'd';
    anj.register_ctx.location_path_len[0] = 1;

    anj_time_duration_t lifetime = anj_time_duration_new(2, ANJ_TIME_UNIT_S);
    _anj_register_update(&anj, &lifetime, false, &msg, &exchange_handlers);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_IN_PROGRESS);

    NEW_REQUEST(msg, exchange_ctx, exchange_handlers, payload);
    // update message with 2 location paths from previous response
    char expected_update[] = "\x41"                  // Confirmable, tkl 1
                             "\x02\x11\x11\x01"      // POST, msg_id token
                             "\xb1\x64"              // uri path /d
                             "\x44\x6c\x74\x3d\x32"; // uri-query lt=2
    verify_payload(expected_update, sizeof(expected_update) - 1, &msg);
    // empty response
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_CHANGED;
    msg.payload_size = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(_anj_register_operation_status(&anj),
              _ANJ_REGISTER_OPERATION_FINISHED);
}
