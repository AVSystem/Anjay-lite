/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/lwm2m_send.h>
#include <anj/utils.h>

#include "../../../../src/anj/coap/coap.h"
#include "../../../../src/anj/core/lwm2m_send.h"
#include "../../../../src/anj/exchange.h"
#include "../../../../src/anj/io/io.h"
#include "../mock/net_api_mock.h"
#include "../mock/time_api_mock.h"

#include <anj_unit_test.h>

// inner_mtu_value value will lead to block transfer for addtional objects in
// payload
#define _TEST_INIT(With_queue_mode, Queue_timeout)         \
    mock_time_reset();                                     \
    net_api_mock_t mock = { 0 };                           \
    net_api_mock_ctx_init(&mock);                          \
    mock.inner_mtu_value = 110;                            \
    anj_t anj;                                             \
    anj_configuration_t config = {                         \
        .endpoint_name = "name",                           \
        .queue_mode_enabled = With_queue_mode,             \
        .queue_mode_timeout = Queue_timeout                \
    };                                                     \
    ANJ_UNIT_ASSERT_SUCCESS(anj_core_init(&anj, &config)); \
    anj_dm_security_obj_t sec_obj;                         \
    anj_dm_security_obj_init(&sec_obj);                    \
    anj_dm_server_obj_t ser_obj;                           \
    anj_dm_server_obj_init(&ser_obj)

#define TEST_INIT() _TEST_INIT(false, ANJ_TIME_DURATION_ZERO)
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
        .iid = &iid,                             \
        .security_mode = ANJ_DM_SECURITY_NOSEC,  \
    };                                           \
    anj_dm_server_instance_init_t ser_inst = {   \
        .ssid = 2,                               \
        .lifetime = 150,                         \
        .binding = "U",                          \
        .iid = &iid                              \
    }

#define EXTENDED_INIT()     \
    TEST_INIT();            \
    INIT_BASIC_INSTANCES(); \
    ADD_INSTANCES()

#define EXTENDED_INIT_WITH_QUEUE_MODE(Queue_timeout) \
    TEST_INIT_WITH_QUEUE_MODE(Queue_timeout);        \
    INIT_BASIC_INSTANCES();                          \
    ADD_INSTANCES()

// token and message id are copied from request stored in anj.exchange_ctx
// correct response must contain the same token and message id as request
#define COPY_TOKEN_AND_MSG_ID(Msg, Token_size)                            \
    memcpy(&Msg[4], anj.exchange_ctx.base_msg.token.bytes, Token_size);   \
    Msg[2] = anj.exchange_ctx.base_msg.coap_binding_data.message_id >> 8; \
    Msg[3] = anj.exchange_ctx.base_msg.coap_binding_data.message_id & 0xFF

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
    mock.bytes_to_send = 500;                           \
    anj_core_step(&anj);                                \
    mock.bytes_to_send = 100;                           \
    ADD_RESPONSE(register_response);                    \
    anj_core_step(&anj);                                \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status, \
                          ANJ_CONN_STATUS_REGISTERED);  \
    anj_core_step(&anj);                                \
    mock.bytes_to_send = 0;                             \
    mock.bytes_sent = 0

#define HANDLE_UPDATE()                                              \
    mock.bytes_to_send = 500;                                        \
    anj_core_step(&anj);                                             \
    COPY_TOKEN_AND_MSG_ID(update, 8);                                \
    ANJ_UNIT_ASSERT_EQUAL(sizeof(update) - 1, mock.bytes_sent);      \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, update, \
                                      mock.bytes_sent);              \
    ADD_RESPONSE(update_response);                                   \
    anj_core_step(&anj);                                             \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,              \
                          ANJ_CONN_STATUS_REGISTERED);               \
    mock.bytes_sent = 0;                                             \
    mock.bytes_to_send = 0;                                          \
    anj_core_step(&anj);                                             \
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0)

static uint16_t g_send_id = 0;
static int g_result = 0;
static void *g_data = NULL;

static void
send_finished_handler(anj_t *anjay, uint16_t send_id, int result, void *data) {
    (void) anjay;
    g_send_id = send_id;
    g_result = result;
    g_data = data;
}

static anj_io_out_entry_t default_record_1 = {
    .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 9),
    .type = ANJ_DATA_TYPE_INT,
    .value.int_value = 42,
    .timestamp = 1705597224.0
};

static anj_io_out_entry_t default_record_2 = {
    .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 17),
    .type = ANJ_DATA_TYPE_STRING,
    .value.bytes_or_string.data = "demo_device",
    .timestamp = 1705597224.0
};

#ifdef ANJ_WITH_EXTERNAL_DATA
static bool opened;
static bool closed;

static char *ptr_for_callback = NULL;
static size_t ext_data_size = 0;
static size_t external_data_handler_call_count = 0;
static size_t external_data_handler_when_error = 0;
static int external_data_handler(void *buffer,
                                 size_t *inout_size,
                                 size_t offset,
                                 void *user_args) {
    (void) user_args;
    assert(&ptr_for_callback);
    ANJ_UNIT_ASSERT_TRUE(opened);
    external_data_handler_call_count++;
    if (external_data_handler_call_count == external_data_handler_when_error) {
        return -1;
    }
    size_t bytes_to_copy = ANJ_MIN(ext_data_size, *inout_size);
    memcpy(buffer, &ptr_for_callback[offset], bytes_to_copy);
    ext_data_size -= bytes_to_copy;
    *inout_size = bytes_to_copy;
    if (ext_data_size) {
        return ANJ_IO_NEED_NEXT_CALL;
    }
    return 0;
}

static int external_data_open(void *user_args) {
    (void) user_args;
    external_data_handler_call_count = 0;
    ANJ_UNIT_ASSERT_FALSE(opened);
    opened = true;

    return 0;
}

static bool closed;
static void external_data_close(void *user_args) {
    (void) user_args;
    ANJ_UNIT_ASSERT_FALSE(closed);
    closed = true;
}

static anj_io_out_entry_t default_record_3 = {
    .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 18),
    .type = ANJ_DATA_TYPE_EXTERNAL_BYTES,
    .value.external_data.get_external_data = external_data_handler,
    .value.external_data.open_external_data = external_data_open,
    .value.external_data.close_external_data = external_data_close
};

static anj_io_out_entry_t default_record_4 = {
    .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
    .type = ANJ_DATA_TYPE_INT,
    .value.int_value = 42
};
#endif // ANJ_WITH_EXTERNAL_DATA

// there is no Send reqeusts in the queue
#define FINAL_CHECK(Last_send_id, Last_result)      \
    ANJ_UNIT_ASSERT_EQUAL(anj.send_ctx.ids[0], 0);  \
    ANJ_UNIT_ASSERT_EQUAL(g_send_id, Last_send_id); \
    ANJ_UNIT_ASSERT_EQUAL(g_result, Last_result);

ANJ_UNIT_TEST(lwm2m_send, new_send_base_check) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    int mock_data;
    anj_send_request_t send_req_success = {
        .finished_handler = send_finished_handler,
        .data = &mock_data,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = 1,
        .records = &default_record_1
    };
    uint16_t send_id = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success, &send_id));
    ANJ_UNIT_ASSERT_EQUAL(send_id, 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.send_ctx.ids[0], 1);
    ANJ_UNIT_ASSERT_TRUE(anj.send_ctx.requests_queue[0] == &send_req_success);
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_abort(&anj, send_id));
    ANJ_UNIT_ASSERT_TRUE(g_data == &mock_data);
    FINAL_CHECK(1, ANJ_SEND_ERR_ABORT);
}

ANJ_UNIT_TEST(lwm2m_send, new_send_errors) {
    EXTENDED_INIT();

    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = 1,
        .records = &default_record_1
    };
    // not registered yet
    ANJ_UNIT_ASSERT_EQUAL(anj_send_new_request(&anj, &send_req, NULL),
                          ANJ_SEND_ERR_NOT_ALLOWED);

    PROCESS_REGISTRATION();

    send_req.finished_handler = NULL;
    ANJ_UNIT_ASSERT_EQUAL(anj_send_new_request(&anj, &send_req, NULL),
                          ANJ_SEND_ERR_DATA_NOT_VALID);
    send_req.finished_handler = send_finished_handler;
    send_req.records = NULL;
    ANJ_UNIT_ASSERT_EQUAL(anj_send_new_request(&anj, &send_req, NULL),
                          ANJ_SEND_ERR_DATA_NOT_VALID);
    send_req.records = &default_record_1;
    send_req.records_cnt = 0;
    ANJ_UNIT_ASSERT_EQUAL(anj_send_new_request(&anj, &send_req, NULL),
                          ANJ_SEND_ERR_DATA_NOT_VALID);
    send_req.records_cnt = 1;
    default_record_1.path = ANJ_MAKE_INSTANCE_PATH(1, 2);
    ANJ_UNIT_ASSERT_EQUAL(anj_send_new_request(&anj, &send_req, NULL),
                          ANJ_SEND_ERR_DATA_NOT_VALID);
    default_record_1.path = ANJ_MAKE_RESOURCE_PATH(3, 0, 9);

    ser_obj.server_instance.mute_send = true;
    anj_core_data_model_changed(&anj,
                                &ANJ_MAKE_RESOURCE_PATH(1, 1, 23),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    ANJ_UNIT_ASSERT_EQUAL(anj_send_new_request(&anj, &send_req, NULL),
                          ANJ_SEND_ERR_NOT_ALLOWED);
    ser_obj.server_instance.mute_send = false;
    anj_core_data_model_changed(&anj,
                                &ANJ_MAKE_RESOURCE_PATH(1, 1, 23),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);

    uint16_t send_id = 0;
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, &send_id));
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    ANJ_UNIT_ASSERT_EQUAL(anj_send_new_request(&anj, &send_req, NULL),
                          ANJ_SEND_ERR_NO_SPACE);
    ANJ_UNIT_ASSERT_EQUAL(send_id, 2);
}

ANJ_UNIT_TEST(lwm2m_send, send_id_overflow) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    anj_send_request_t send_req_success = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = 1,
        .records = &default_record_1
    };
    uint16_t send_id;
    anj.send_ctx.send_id_counter = UINT16_MAX - 2;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success, &send_id));
    ANJ_UNIT_ASSERT_EQUAL(send_id, UINT16_MAX - 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success, &send_id));
    ANJ_UNIT_ASSERT_EQUAL(send_id, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success, &send_id));
    ANJ_UNIT_ASSERT_EQUAL(send_id, 2);
}

ANJ_UNIT_TEST(lwm2m_send, send_abort_base_check) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();

    anj_send_request_t send_req_success_1 = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = 1,
        .records = &default_record_1
    };
    // identical content but different address in memory
    anj_send_request_t send_req_success_2 = send_req_success_1;
    anj_send_request_t send_req_success_3 = send_req_success_1;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success_1, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success_2, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success_3, NULL));

    ANJ_UNIT_ASSERT_EQUAL(anj_send_abort(&anj, 4),
                          ANJ_SEND_ERR_NO_REQUEST_FOUND);
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_abort(&anj, 2));
    ANJ_UNIT_ASSERT_TRUE(anj.send_ctx.requests_queue[0] == &send_req_success_1);
    ANJ_UNIT_ASSERT_EQUAL(anj.send_ctx.ids[0], 1);
    ANJ_UNIT_ASSERT_TRUE(anj.send_ctx.requests_queue[1] == &send_req_success_3);
    ANJ_UNIT_ASSERT_EQUAL(anj.send_ctx.ids[1], 3);
    ANJ_UNIT_ASSERT_EQUAL(anj.send_ctx.ids[2], 0);

    ANJ_UNIT_ASSERT_SUCCESS(anj_send_abort(&anj, ANJ_SEND_ID_ALL));
    for (int i = 0; i < 3; i++) {
        ANJ_UNIT_ASSERT_EQUAL(anj.send_ctx.ids[i], 0);
    }
}

static void send_finished_handler_with_abort_call(anj_t *anjay,
                                                  uint16_t send_id,
                                                  int result,
                                                  void *data) {
    (void) anjay;
    (void) send_id;
    (void) result;
    (void) data;
    ANJ_UNIT_ASSERT_EQUAL(anj_send_abort(anjay, ANJ_SEND_ID_ALL),
                          ANJ_SEND_ERR_ABORT);
}

ANJ_UNIT_TEST(lwm2m_send, abort_from_finish_handler) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_send_request_t send_req_success_1 = {
        .finished_handler = send_finished_handler_with_abort_call,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = 1,
        .records = &default_record_1
    };
    // identical content but different address in memory
    anj_send_request_t send_req_success_2 = send_req_success_1;
    anj_send_request_t send_req_success_3 = send_req_success_1;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success_1, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success_2, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_send_new_request(&anj, &send_req_success_3, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_abort(&anj, ANJ_SEND_ID_ALL));
    for (int i = 0; i < 3; i++) {
        ANJ_UNIT_ASSERT_EQUAL(anj.send_ctx.ids[i], 0);
    }
}

#define HANDLE_SEND(Send_request, Response)                                \
    mock.bytes_to_send = 500;                                              \
    anj_core_step(&anj);                                                   \
    COPY_TOKEN_AND_MSG_ID(Send_request, 8);                                \
    ANJ_UNIT_ASSERT_EQUAL(sizeof(Send_request) - 1, mock.bytes_sent);      \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, Send_request, \
                                      mock.bytes_sent);                    \
    ADD_RESPONSE(Response);                                                \
    mock.bytes_to_send = 0;                                                \
    anj_core_step(&anj);                                                   \
    ANJ_UNIT_ASSERT_EQUAL(anj.server_state.conn_status,                    \
                          ANJ_CONN_STATUS_REGISTERED);

static char send_response[] = "\x68"         // header v 0x01, Ack, tkl 8
                              "\x44\x00\x00" // Changed code 2.04
                              "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token

static char basic_send[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST 0x02, msg id
        "\x00\x00\x00\x00\x00\x00\x00\x00" // token
        "\xb2\x64\x70"                     // uri path /dp
        "\x11\x70"                         // content_format: senml-cbor
        "\xFF"
        "\x82\xa4"                                 // map(4)
        "\x21\x64/3/0"                             // base path
        "\x00\x62/9"                               // path
        "\x22\xfb\x41\xd9\x6a\x56\x4a\x00\x00\x00" // base time
        "\x02\x18\x2a"                             // value 42
        "\xa2"                                     // map(2)
        "\x00\x63/17"                              // path
        "\x03\x6b"
        "demo_device"; // string value

static char short_send[] = "\x48"         // Confirmable, tkl 8
                           "\x02\x00\x00" // POST 0x02, msg id
                           "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                           "\xb2\x64\x70"                     // uri path /dp
                           "\x11\x70" // content_format: senml-cbor
                           "\xFF"
                           "\x81\xa2"       // map(3)
                           "\x21\x66/3/0/9" // path
                           "\x02\x18\x2a";  // value 42

ANJ_UNIT_TEST(lwm2m_send, basic_send) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_io_out_entry_t records[] = { default_record_1, default_record_2 };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    HANDLE_SEND(basic_send, send_response);
    anj_core_server_obj_registration_update_trigger_executed(&anj);
    HANDLE_UPDATE();
    FINAL_CHECK(1, 0);
}

static char send_with_lifetime[] = "\x48"         // Confirmable, tkl 8
                                   "\x02\x00\x00" // POST 0x02, msg id
                                   "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                                   "\xb2\x64\x70" // uri path /dp
                                   "\x11\x70"     // content_format: senml-cbor
                                   "\xFF"
                                   "\x81\xa2"       // map(3)
                                   "\x21\x66/1/1/1" // path
                                   "\x02\x18\x96";  // value 150

ANJ_UNIT_TEST(lwm2m_send, basic_send_with_DM) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_res_value_t value;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_res_read(&anj, &ANJ_MAKE_RESOURCE_PATH(1, 1, 1), &value));
    anj_io_out_entry_t records[] = {
        {
            .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1),
            .type = ANJ_DATA_TYPE_INT,
            .value = value
        }
    };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = 1,
        .records = records
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    HANDLE_SEND(send_with_lifetime, send_response);
    anj_core_server_obj_registration_update_trigger_executed(&anj);
    HANDLE_UPDATE();
    FINAL_CHECK(1, 0);
}

ANJ_UNIT_TEST(lwm2m_send, two_sends_in_the_row) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_io_out_entry_t records[] = { default_record_1, default_record_2 };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };
    anj_io_out_entry_t short_record = default_record_1;
    short_record.timestamp = NAN;
    anj_send_request_t send_req_2 = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = 1,
        .records = &short_record
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req_2, NULL));
    HANDLE_SEND(basic_send, send_response);
    ANJ_UNIT_ASSERT_EQUAL(g_result, 0);
    ANJ_UNIT_ASSERT_EQUAL(g_send_id, 1);
    HANDLE_SEND(short_send, send_response);
    anj_core_server_obj_registration_update_trigger_executed(&anj);
    HANDLE_UPDATE();
    FINAL_CHECK(2, 0);
}

#ifdef ANJ_WITH_LWM2M12
static char lwm2m_cbor_send[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST 0x02, msg id
        "\x00\x00\x00\x00\x00\x00\x00\x00" // token
        "\xb2\x64\x70"                     // uri path /dp
        "\x12\x2D\x18"                     // content_format: lwm2m_cbor 11544
        "\xFF"
        "\xBF\x03\xBF\x00\xBF\x03\x18\x19\xFF\xFF\xFF"; // {3: {0: {3: 25}}}

ANJ_UNIT_TEST(lwm2m_send, send_with_lwm2m_cbor) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    static anj_io_out_entry_t lwm2m_cbor_record = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25,
    };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_LWM2M_CBOR,
        .records_cnt = 1,
        .records = &lwm2m_cbor_record
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    HANDLE_SEND(lwm2m_cbor_send, send_response);
    FINAL_CHECK(1, 0);
}

ANJ_UNIT_TEST(lwm2m_send, send_with_lwm2m_cbor_same_path) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    static anj_io_out_entry_t lwm2m_cbor_records[] = {
        {
            .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            .type = ANJ_DATA_TYPE_UINT,
            .value.uint_value = 25,
        },
        {
            .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            .type = ANJ_DATA_TYPE_UINT,
            .value.uint_value = 30,
        }
    };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_LWM2M_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(lwm2m_cbor_records),
        .records = lwm2m_cbor_records
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_send_new_request(&anj, &send_req, NULL),
                          ANJ_SEND_ERR_DATA_NOT_VALID);
}
#endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(lwm2m_send, abort_ongoing_send) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_io_out_entry_t records[] = { default_record_1, default_record_2 };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));

    mock.bytes_to_send = 500;
    anj_core_step(&anj);
    // wait for response
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_TRUE(_anj_exchange_ongoing_exchange(&anj.exchange_ctx));
    // abort when waiting for response
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_abort(&anj, ANJ_SEND_ID_ALL));
    FINAL_CHECK(1, ANJ_SEND_ERR_ABORT);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_FALSE(_anj_exchange_ongoing_exchange(&anj.exchange_ctx));

    anj_core_server_obj_registration_update_trigger_executed(&anj);
    HANDLE_UPDATE();

    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    mock.bytes_to_send = 0;
    anj_core_step(&anj);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_TRUE(_anj_exchange_ongoing_exchange(&anj.exchange_ctx));
    // abort when sending
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_abort(&anj, ANJ_SEND_ID_ALL));
    FINAL_CHECK(2, ANJ_SEND_ERR_ABORT);
    anj_core_step(&anj);
    ANJ_UNIT_ASSERT_FALSE(_anj_exchange_ongoing_exchange(&anj.exchange_ctx));

    anj_core_server_obj_registration_update_trigger_executed(&anj);
    HANDLE_UPDATE();

    // check if send is still working
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    HANDLE_SEND(basic_send, send_response);
    FINAL_CHECK(3, 0);
}

ANJ_UNIT_TEST(lwm2m_send, network_error) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_io_out_entry_t records[] = { default_record_1, default_record_2 };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));

    mock.call_result[ANJ_NET_FUN_SEND] = -14;
    // wait for close for the next anj_core_step call
    mock.call_result[ANJ_NET_FUN_CLOSE] = ANJ_NET_EINPROGRESS;
    anj_core_step(&anj);
    mock.call_result[ANJ_NET_FUN_SEND] = 0;
    mock.call_result[ANJ_NET_FUN_CLOSE] = 0;
    ANJ_UNIT_ASSERT_FALSE(_anj_exchange_ongoing_exchange(&anj.exchange_ctx));
    FINAL_CHECK(1, ANJ_SEND_ERR_NETWORK);

    PROCESS_REGISTRATION();
    // check if send is still working
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    HANDLE_SEND(basic_send, send_response);
    FINAL_CHECK(2, 0);
}

ANJ_UNIT_TEST(lwm2m_send, no_response_error) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    g_send_id = 0;
    anj_io_out_entry_t records[] = { default_record_1, default_record_2 };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));

    mock.bytes_to_send = 500;
    anj_core_step(&anj);
    mock_time_reset();
    // there is 4 retries in default config
    for (int i = 0; i < 5; i++) {
        mock_time_advance(anj_time_duration_new(100, ANJ_TIME_UNIT_S));
        ANJ_UNIT_ASSERT_EQUAL(g_send_id, 0);
        anj_core_step(&anj);
    }
    FINAL_CHECK(1, ANJ_SEND_ERR_TIMEOUT);
}

ANJ_UNIT_TEST(lwm2m_send, send_blocked_by_recv_einprogress) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    g_send_id = 0;
    anj_io_out_entry_t records[] = { default_record_1, default_record_2 };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));

    // allow to send data
    mock.bytes_to_send = 500;
    // record current send count
    int current_send_count = mock.call_count[ANJ_NET_FUN_SEND];
    // simulate blocking operation in the underlying socket
    mock.call_result[ANJ_NET_FUN_RECV] = ANJ_NET_EINPROGRESS;
    // process the tick where Update is due
    anj_core_step(&anj);
    // no send attempt happened due to ANJ_NET_EINPROGRESS on recv
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND],
                          current_send_count);

    // send request still in the queue
    ANJ_UNIT_ASSERT_EQUAL(anj.send_ctx.ids[0], 1);
}

ANJ_UNIT_TEST(lwm2m_send, send_with_io_ctx_error) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_io_out_entry_t invalid_record = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 9),
        .type = ANJ_DATA_TYPE_INT
    };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = 1,
        .records = &invalid_record
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    // change path to generate error
    invalid_record.path = ANJ_MAKE_INSTANCE_PATH(1, 2);

    // send request is not even sent
    mock.call_count[ANJ_NET_FUN_SEND] = 0;
    anj_core_step(&anj);
    // error leads to reregistration so there is one _anj_srv_conn_send call
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], 1);
    FINAL_CHECK(1, ANJ_SEND_ERR_INTERNAL);
}

static char send_error_response[] = "\x68"         // header v 0x01, Ack, tkl 8
                                    "\x80\x00\x00" // Bad Request code 4.00
                                    "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token

ANJ_UNIT_TEST(lwm2m_send, send_with_error_response) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_io_out_entry_t records[] = { default_record_1, default_record_2 };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    HANDLE_SEND(basic_send, send_error_response);
    anj_core_server_obj_registration_update_trigger_executed(&anj);
    HANDLE_UPDATE();
    FINAL_CHECK(1, ANJ_SEND_ERR_REJECTED);

    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    HANDLE_SEND(basic_send, send_response);
    FINAL_CHECK(2, 0);
}

static char send_with_data_model_block_1[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST 0x02, msg id
        "\x00\x00\x00\x00\x00\x00\x00\x00" // token
        "\xb2\x64\x70"                     // uri path /dp
        "\x11\x70"                         // content_format: senml-cbor
        "\xD1\x02\x0A"                     // block1 0, size 64, more
        "\xFF"
        "\x8e\xa2"       // map(3)
        "\x21\x66/3/0/9" // path
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18\x2a"   // value 42
        "\xa1"           // map(1)
        "\x02\x18";      // last value without last byte

static char send_with_data_model_block_2[] =
        "\x48"                             // Confirmable, tkl 8
        "\x02\x00\x00"                     // POST 0x02, msg id
        "\x00\x00\x00\x00\x00\x00\x00\x00" // token
        "\xb2\x64\x70"                     // uri path /dp
        "\x11\x70"                         // content_format: senml-cbor
        "\xD1\x02\x12"                     // block1 1, size 64
        "\xFF"
        "\x2a"; // last byte of the last value

static char send_response_block_1[] =
        "\x68"                             // ACK, tkl 1
        "\x5F"                             // Continue
        "\x00\x00"                         // msg id
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xd1\x0e\x0A";                    // block1 0, size 64, more

static char send_response_block_2[] =
        "\x68"                             // ACK, tkl 1
        "\x44\x00\x00"                     // Changed code 2.04
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\xd1\x0e\x12";                    // block1 1, size 64

ANJ_UNIT_TEST(lwm2m_send, send_with_block_transfer) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    static anj_io_out_entry_t record = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 9),
        .type = ANJ_DATA_TYPE_INT,
        .value.int_value = 42,
    };
    // create 14 identical records to force block transfer (in this tests single
    // block has size 64 bytes)
    anj_io_out_entry_t records[] = { record, record, record, record, record,
                                     record, record, record, record, record,
                                     record, record, record, record };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    HANDLE_SEND(send_with_data_model_block_1, send_response_block_1);
    HANDLE_SEND(send_with_data_model_block_2, send_response_block_2);

    anj_core_server_obj_registration_update_trigger_executed(&anj);
    HANDLE_UPDATE();
    FINAL_CHECK(1, 0);
}

ANJ_UNIT_TEST(lwm2m_send, mute_send_set_meantime) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_io_out_entry_t records[] = { default_record_1, default_record_2 };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));

    // mute send changed to true, abort all existing sends
    ser_obj.server_instance.mute_send = true;
    anj_core_data_model_changed(&anj,
                                &ANJ_MAKE_RESOURCE_PATH(1, 1, 23),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    anj_core_step(&anj);
    FINAL_CHECK(2, ANJ_SEND_ERR_ABORT);
}

static char basic_send_payload[] = "\x82\xa4"
                                   "\x21\x64/3/0"
                                   "\x00\x62/9"
                                   "\x22\xfb\x41\xd9\x6a\x56\x4a\x00\x00\x00"
                                   "\x02\x18\x2a"
                                   "\xa2"
                                   "\x00\x63/17"
                                   "\x03\x6b"
                                   "demo_device";

// check send_read_payload function to see if it works correctly for all
// possible paths
ANJ_UNIT_TEST(lwm2m_send, read_payload_check) {
    EXTENDED_INIT();
    PROCESS_REGISTRATION();
    anj_io_out_entry_t records[] = { default_record_1, default_record_2 };
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = ANJ_ARRAY_SIZE(records),
        .records = records
    };

    for (size_t payload_buff_size = 5;
         payload_buff_size < sizeof(basic_send_payload);
         payload_buff_size++) {
        ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL));
        _anj_exchange_handlers_t handlers = { 0 };
        _anj_coap_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        _anj_lwm2m_send_process(&anj, &handlers, &msg);
        ANJ_UNIT_ASSERT_EQUAL(msg.operation, ANJ_OP_INF_CON_SEND);

        uint8_t payload_buff[100] = { 0 };
        size_t total_len = 0;
        size_t out_len = 0;
        uint16_t format = 0;
        uint8_t result = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
        while (result == _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED) {
            out_len = 0;
            _anj_exchange_read_result_t params = { 0 };
            result = handlers.read_payload(handlers.arg,
                                           &payload_buff[total_len],
                                           payload_buff_size,
                                           &params);
            format = params.format;
            out_len = params.payload_len;
            total_len += out_len;
        }
        ANJ_UNIT_ASSERT_EQUAL(result, 0);
        ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
        ANJ_UNIT_ASSERT_EQUAL(total_len, sizeof(basic_send_payload) - 1);
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(payload_buff, basic_send_payload,
                                          total_len);
        handlers.completion(handlers.arg, 0, _ANJ_EXCHANGE_ERROR_TERMINATED);
    }
}

static anj_io_out_entry_t common_path_record_1 = {
    .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
    .type = ANJ_DATA_TYPE_INT,
    .value.int_value = 42,
    .timestamp = 1705597224.0
};

static anj_io_out_entry_t common_path_record_2 = {
    .path = ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
    .type = ANJ_DATA_TYPE_INT,
    .value.int_value = 42,
    .timestamp = 1705597223.0
};

// macro to check if common path is calculated correctly (find_common_path())
#define COMMON_PATH_CHECK(Path1, Path2, RecordCount, ExpectedBasePath)    \
    EXTENDED_INIT();                                                      \
    PROCESS_REGISTRATION();                                               \
    anj_io_out_entry_t records[] = { common_path_record_1,                \
                                     common_path_record_2 };              \
    records[0].path = Path1;                                              \
    records[1].path = Path2;                                              \
    anj_send_request_t send_req = {                                       \
        .finished_handler = send_finished_handler,                        \
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,             \
        .records_cnt = RecordCount,                                       \
        .records = records                                                \
    };                                                                    \
    ANJ_UNIT_ASSERT_SUCCESS(anj_send_new_request(&anj, &send_req, NULL)); \
    anj_core_step(&anj);                                                  \
    ANJ_UNIT_ASSERT_TRUE(anj_uri_path_equal(                              \
            &anj.anj_io.out_ctx.encoder.senml.base_path, &ExpectedBasePath));

ANJ_UNIT_TEST(lwm2m_send, common_path_resource_path) {
    COMMON_PATH_CHECK(ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
                      ANJ_MAKE_RESOURCE_PATH(3, 0, 0), 2,
                      ANJ_MAKE_RESOURCE_PATH(3, 0, 0));
}

ANJ_UNIT_TEST(lwm2m_send, common_path_instance_path) {
    COMMON_PATH_CHECK(ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
                      ANJ_MAKE_RESOURCE_PATH(3, 0, 2), 2,
                      ANJ_MAKE_INSTANCE_PATH(3, 0));
}

ANJ_UNIT_TEST(lwm2m_send, common_path_object_path) {
    COMMON_PATH_CHECK(ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
                      ANJ_MAKE_RESOURCE_PATH(3, 1, 1), 2,
                      ANJ_MAKE_OBJECT_PATH(3));
}

ANJ_UNIT_TEST(lwm2m_send, common_path_root_path) {
    COMMON_PATH_CHECK(ANJ_MAKE_RESOURCE_PATH(4, 0, 0),
                      ANJ_MAKE_RESOURCE_PATH(3, 1, 1), 2, ANJ_MAKE_ROOT_PATH());
}

ANJ_UNIT_TEST(lwm2m_send, common_path_resource_instance_path) {
    COMMON_PATH_CHECK(ANJ_MAKE_RESOURCE_INSTANCE_PATH(4, 0, 0, 0),
                      ANJ_MAKE_RESOURCE_INSTANCE_PATH(4, 0, 0, 0), 2,
                      ANJ_MAKE_RESOURCE_INSTANCE_PATH(4, 0, 0, 0));
}

ANJ_UNIT_TEST(lwm2m_send, common_path_resource_instance_path_2) {
    COMMON_PATH_CHECK(ANJ_MAKE_RESOURCE_INSTANCE_PATH(4, 2, 1, 0),
                      ANJ_MAKE_RESOURCE_INSTANCE_PATH(4, 2, 1, 1), 2,
                      ANJ_MAKE_RESOURCE_PATH(4, 2, 1));
}

ANJ_UNIT_TEST(lwm2m_send, common_path_single_path) {
    COMMON_PATH_CHECK(ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
                      ANJ_MAKE_RESOURCE_PATH(3, 1, 0), 1,
                      ANJ_MAKE_RESOURCE_PATH(3, 0, 0));
}

#ifdef ANJ_WITH_EXTERNAL_DATA
static void
verify_payload(char *expected, size_t expected_len, _anj_coap_msg_t *msg) {
    uint8_t out_buff[120];
    size_t out_msg_size = 0;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
            msg, out_buff, sizeof(out_buff), &out_msg_size));
#    define MSG_ID_OFFSET 2
    expected[MSG_ID_OFFSET] =
            (uint8_t) (msg->coap_binding_data.message_id >> 8);
    expected[MSG_ID_OFFSET + 1] = (uint8_t) msg->coap_binding_data.message_id;
#    define TOKEN_OFFSET 4
    memcpy(&expected[TOKEN_OFFSET], msg->token.bytes, msg->token.size);

    ASSERT_EQ_BYTES_SIZED(out_buff, expected, expected_len);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, expected_len);
}

ANJ_UNIT_TEST(lwm2m_send, send_external_opaque) {
#    define PREPARE_BEFORE_TEST()                                              \
        EXTENDED_INIT();                                                       \
        PROCESS_REGISTRATION();                                                \
                                                                               \
        anj_io_out_entry_t records[] = { default_record_3, default_record_4 }; \
        anj_send_request_t send_req_success = {                                \
            .finished_handler = send_finished_handler,                         \
            .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,              \
            .records_cnt = ANJ_ARRAY_SIZE(records),                            \
            .records = records                                                 \
        };                                                                     \
        uint16_t send_id = 0;                                                  \
        opened = false;                                                        \
        closed = false;                                                        \
        _anj_exchange_handlers_t handlers = { 0 };                             \
        _anj_coap_msg_t msg = { 0 };                                           \
        size_t buff_len = 32;

    // successfully send external string, string split between two messages
    {
        PREPARE_BEFORE_TEST()
        external_data_handler_when_error = 0;

        ptr_for_callback = "012345678901234567890123456789";
        ext_data_size = sizeof("012345678901234567890123456789") - 1;

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_send_new_request(&anj, &send_req_success, &send_id));

        _anj_lwm2m_send_process(&anj, &handlers, &msg);

        // preapare first block
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_client_request(
                                      &anj.exchange_ctx, &msg, &handlers,
                                      anj.payload_buffer, buff_len),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected[] = "\x48"         // Confirmable, tkl 8
                          "\x02\x00\x00" // POST 0x02, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // Token
                          "\xB2\x64\x70"                     // Path dp
                          "\x11\x70"     // content_format: senml_cbor
                          "\xD1\x02\x09" // block1 0 more
                          "\xFF"
                          "\x82\xA3"
                          "\x21\x64/3/0"
                          "\x00\x63/18"
                          "\x08"
                          "\x5f"
                          "\x4f"
                          "012345678901234"
                          "\x40";

        verify_payload(expected, sizeof(expected) - 1, &msg);
        ANJ_UNIT_ASSERT_FALSE(closed);

        // first block was sent
        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&anj.exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_WAITING_MSG);

        // get response and prepare another block
        msg.operation = ANJ_OP_RESPONSE;
        msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
        msg.msg_code = ANJ_COAP_CODE_CONTINUE;
        msg.payload_size = 0;
        msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&anj.exchange_ctx,
                                                    ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                    &msg),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected2[] = "\x48"         // Confirmable, tkl 8
                           "\x02\x00\x00" // POST 0x02, msg id
                           "\x00\x00\x00\x00\x00\x00\x00\x00" // Token
                           "\xB2\x64\x70"                     // Path dp
                           "\x11\x70"     // content_format: senml_cbor
                           "\xD1\x02\x11" // block1 1
                           "\xFF"
                           "\x4f"
                           "567890123456789"
                           "\xFF"
                           "\xA2"
                           "\x00\x62/1"
                           "\x02\x18\x2a";

        verify_payload(expected2, sizeof(expected2) - 1, &msg);
        ANJ_UNIT_ASSERT_TRUE(closed);

        // second block was sent
        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&anj.exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_WAITING_MSG);

        // get response
        msg.operation = ANJ_OP_RESPONSE;
        msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
        msg.msg_code = ANJ_COAP_CODE_CONTINUE;
        msg.payload_size = 0;
        msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&anj.exchange_ctx,
                                                    ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                    &msg),
                              ANJ_EXCHANGE_STATE_FINISHED);
    }
    // successfully send external string, whole string in first message
    {
        PREPARE_BEFORE_TEST()
        external_data_handler_when_error = 0;

        ptr_for_callback = "012345678901234";
        ext_data_size = sizeof("012345678901234") - 1;

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_send_new_request(&anj, &send_req_success, &send_id));

        _anj_lwm2m_send_process(&anj, &handlers, &msg);

        // preapare first block
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_client_request(
                                      &anj.exchange_ctx, &msg, &handlers,
                                      anj.payload_buffer, buff_len),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected[] = "\x48"         // Confirmable, tkl 8
                          "\x02\x00\x00" // POST 0x02, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // Token
                          "\xB2\x64\x70"                     // Path dp
                          "\x11\x70"     // content_format: senml_cbor
                          "\xD1\x02\x09" // block1 0 more
                          "\xFF"
                          "\x82\xA3"
                          "\x21\x64/3/0"
                          "\x00\x63/18"
                          "\x08"
                          "\x5f"
                          "\x4f"
                          "012345678901234\xFF";

        verify_payload(expected, sizeof(expected) - 1, &msg);
        ANJ_UNIT_ASSERT_TRUE(closed);

        // first block was sent
        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&anj.exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_WAITING_MSG);

        // get response and prepare another block
        msg.operation = ANJ_OP_RESPONSE;
        msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
        msg.msg_code = ANJ_COAP_CODE_CONTINUE;
        msg.payload_size = 0;
        msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&anj.exchange_ctx,
                                                    ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                    &msg),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected2[] = "\x48"         // Confirmable, tkl 8
                           "\x02\x00\x00" // POST 0x02, msg id
                           "\x00\x00\x00\x00\x00\x00\x00\x00" // Token
                           "\xB2\x64\x70"                     // Path dp
                           "\x11\x70"     // content_format: senml_cbor
                           "\xD1\x02\x11" // block1 1
                           "\xFF"
                           "\xA2"
                           "\x00\x62/1"
                           "\x02\x18\x2a";

        verify_payload(expected2, sizeof(expected2) - 1, &msg);

        // second block was sent
        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&anj.exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_WAITING_MSG);

        // get response
        msg.operation = ANJ_OP_RESPONSE;
        msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
        msg.msg_code = ANJ_COAP_CODE_CONTINUE;
        msg.payload_size = 0;
        msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&anj.exchange_ctx,
                                                    ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                    &msg),
                              ANJ_EXCHANGE_STATE_FINISHED);
    }
    // try send external string, exchange terminated
    {
        PREPARE_BEFORE_TEST()
        external_data_handler_when_error = 0;

        ptr_for_callback = "012345678901234567";
        ext_data_size = sizeof("012345678901234567") - 1;

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_send_new_request(&anj, &send_req_success, &send_id));

        _anj_lwm2m_send_process(&anj, &handlers, &msg);

        // preapare first block
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_client_request(
                                      &anj.exchange_ctx, &msg, &handlers,
                                      anj.payload_buffer, buff_len),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected[] = "\x48"         // Confirmable, tkl 8
                          "\x02\x00\x00" // POST 0x02, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // Token
                          "\xB2\x64\x70"                     // Path dp
                          "\x11\x70"     // content_format: senml_cbor
                          "\xD1\x02\x09" // block1 0 more
                          "\xFF"
                          "\x82\xA3"
                          "\x21\x64/3/0"
                          "\x00\x63/18"
                          "\x08"
                          "\x5f"
                          "\x4f"
                          "012345678901234\x40";

        verify_payload(expected, sizeof(expected) - 1, &msg);

        ANJ_UNIT_ASSERT_FALSE(closed);
        _anj_exchange_terminate(&anj.exchange_ctx,
                                _ANJ_EXCHANGE_ERROR_TERMINATED);
        ANJ_UNIT_ASSERT_TRUE(closed);
    }
    // try send external string, receive reset
    {
        PREPARE_BEFORE_TEST()
        external_data_handler_when_error = 0;

        ptr_for_callback = "012345678901234567";
        ext_data_size = sizeof("012345678901234567") - 1;

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_send_new_request(&anj, &send_req_success, &send_id));

        _anj_lwm2m_send_process(&anj, &handlers, &msg);

        // preapare first block
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_client_request(
                                      &anj.exchange_ctx, &msg, &handlers,
                                      anj.payload_buffer, buff_len),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected[] = "\x48"         // Confirmable, tkl 8
                          "\x02\x00\x00" // POST 0x02, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // Token
                          "\xB2\x64\x70"                     // Path dp
                          "\x11\x70"     // content_format: senml_cbor
                          "\xD1\x02\x09" // block1 0 more
                          "\xFF"
                          "\x82\xA3"
                          "\x21\x64/3/0"
                          "\x00\x63/18"
                          "\x08"
                          "\x5f"
                          "\x4f"
                          "012345678901234\x40";

        verify_payload(expected, sizeof(expected) - 1, &msg);
        ANJ_UNIT_ASSERT_FALSE(closed);

        // first block was sent
        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&anj.exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_WAITING_MSG);

        // get reset
        msg.operation = ANJ_OP_COAP_RESET;
        msg.msg_code = ANJ_COAP_CODE_EMPTY;
        msg.payload_size = 0;
        msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&anj.exchange_ctx,
                                                    ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                    &msg),
                              ANJ_EXCHANGE_STATE_FINISHED);

        ANJ_UNIT_ASSERT_TRUE(closed);
    }
    // try send external string, external data handler fails the first time it
    // is called
    {
        PREPARE_BEFORE_TEST()
        external_data_handler_when_error = 1;

        ptr_for_callback = "012345678901234567890123456789";
        ext_data_size = sizeof("012345678901234567890123456789") - 1;

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_send_new_request(&anj, &send_req_success, &send_id));

        _anj_lwm2m_send_process(&anj, &handlers, &msg);

        // preapare first block
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_client_request(
                                      &anj.exchange_ctx, &msg, &handlers,
                                      anj.payload_buffer, buff_len),
                              ANJ_EXCHANGE_STATE_FINISHED);

        ANJ_UNIT_ASSERT_TRUE(closed);
    }
    // try send external string, external data handler fails the second time it
    // is called
    {
        PREPARE_BEFORE_TEST()
        external_data_handler_when_error = 2;

        ptr_for_callback = "012345678901234567890123456789";
        ext_data_size = sizeof("012345678901234567890123456789") - 1;

        ANJ_UNIT_ASSERT_SUCCESS(
                anj_send_new_request(&anj, &send_req_success, &send_id));

        _anj_lwm2m_send_process(&anj, &handlers, &msg);

        // preapare first block
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_client_request(
                                      &anj.exchange_ctx, &msg, &handlers,
                                      anj.payload_buffer, buff_len),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
        char expected[] = "\x48"         // Confirmable, tkl 8
                          "\x02\x00\x00" // POST 0x02, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // Token
                          "\xB2\x64\x70"                     // Path dp
                          "\x11\x70"     // content_format: senml_cbor
                          "\xD1\x02\x09" // block1 0 more
                          "\xFF"
                          "\x82\xA3"
                          "\x21\x64/3/0"
                          "\x00\x63/18"
                          "\x08"
                          "\x5f"
                          "\x4f"
                          "012345678901234\x40";
        verify_payload(expected, sizeof(expected) - 1, &msg);
        ANJ_UNIT_ASSERT_FALSE(closed);

        // first block was sent
        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&anj.exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_WAITING_MSG);

        // get response, next external handler call will cause error
        msg.operation = ANJ_OP_RESPONSE;
        msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
        msg.msg_code = ANJ_COAP_CODE_CONTINUE;
        msg.payload_size = 0;
        msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&anj.exchange_ctx,
                                                    ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                    &msg),
                              ANJ_EXCHANGE_STATE_FINISHED);

        ANJ_UNIT_ASSERT_TRUE(closed);
    }
}
#endif // ANJ_WITH_EXTERNAL_DATA
