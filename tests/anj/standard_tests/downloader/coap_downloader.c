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

#include <anj/coap_downloader.h>
#include <anj/compat/net/anj_net_api.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/utils.h>

#include "../../../../src/anj/coap/coap.h"
#include "../../../../src/anj/exchange.h"
#include "../../../../src/anj/io/io.h"

#include "../mock/net_api_mock.h"
#include "../mock/time_api_mock.h"

#include <anj_unit_test.h>

static anj_coap_downloader_status_t g_set_connection_status =
        ANJ_COAP_DOWNLOADER_STATUS_INITIAL;
static uint8_t g_data[500] = { 0 };
static size_t g_data_len = 0;
static int g_callback_counter = 0;

typedef enum {
    DATA_CALLBACK_ACTION_NONE,
    DATA_CALLBACK_ACTION_SUSPEND,
    DATA_CALLBACK_ACTION_TERMINATE
} data_callback_action_t;

static data_callback_action_t g_data_callback_action =
        DATA_CALLBACK_ACTION_NONE;
static int g_data_callback_action_result = -1;

static void coap_downloader_callback(void *arg,
                                     anj_coap_downloader_t *downloader,
                                     anj_coap_downloader_status_t conn_status,
                                     const uint8_t *data,
                                     size_t data_len) {
    (void) arg;
    g_set_connection_status = conn_status;
    g_callback_counter++;
    if (data == NULL) {
        return;
    }
    memcpy(&g_data[g_data_len], data, data_len);
    g_data_len += data_len;

    data_callback_action_t action = g_data_callback_action;
    g_data_callback_action = DATA_CALLBACK_ACTION_NONE;
    switch (action) {
    case DATA_CALLBACK_ACTION_SUSPEND:
        g_data_callback_action_result = anj_coap_downloader_suspend(downloader);
        break;
    case DATA_CALLBACK_ACTION_TERMINATE:
        anj_coap_downloader_terminate(downloader);
        g_data_callback_action_result = 0;
        break;
    default:
        break;
    }
}

#define TEST_INIT()                                               \
    mock_time_reset();                                            \
    net_api_mock_t mock = { 0 };                                  \
    net_api_mock_ctx_init(&mock);                                 \
    mock.inner_mtu_value = 100;                                   \
    anj_coap_downloader_t ctx;                                    \
    anj_coap_downloader_configuration_t config = {                \
        .event_cb = coap_downloader_callback,                     \
        .event_cb_arg = NULL,                                     \
    };                                                            \
    g_data_len = 0;                                               \
    memset(g_data, 0, sizeof(g_data));                            \
    g_callback_counter = 0;                                       \
    g_data_callback_action = DATA_CALLBACK_ACTION_NONE;           \
    g_data_callback_action_result = -1;                           \
    g_set_connection_status = ANJ_COAP_DOWNLOADER_STATUS_INITIAL; \
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_init(&ctx, &config));

#define TEST_INIT_RETRY()                                         \
    mock_time_reset();                                            \
    net_api_mock_t mock = { 0 };                                  \
    net_api_mock_ctx_init(&mock);                                 \
    mock.inner_mtu_value = 100;                                   \
    anj_coap_downloader_t ctx;                                    \
    anj_coap_downloader_configuration_t config = {                \
        .event_cb = coap_downloader_callback,                     \
        .event_cb_arg = NULL,                                     \
        .retry_count = 3,                                         \
        .retry_delay = anj_time_duration_new(10, ANJ_TIME_UNIT_S) \
    };                                                            \
    g_data_len = 0;                                               \
    memset(g_data, 0, sizeof(g_data));                            \
    g_callback_counter = 0;                                       \
    g_data_callback_action = DATA_CALLBACK_ACTION_NONE;           \
    g_data_callback_action_result = -1;                           \
    g_set_connection_status = ANJ_COAP_DOWNLOADER_STATUS_INITIAL; \
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_init(&ctx, &config));

#define BASE_URI "coap://test_uri.com:5683/a/bc/def"
#define BASE_URI_2 "coap://uri_turi.com:333/a/bc/def"
#define BASE_URI_3 "coaps://test_uri.com:5683/a/bc/def"

#define START_DOWNLOAD(uri)                                              \
    g_callback_counter = 0;                                              \
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_start(&ctx, uri, NULL)); \
    anj_coap_downloader_step(&ctx);                                      \
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,                       \
                          ANJ_COAP_DOWNLOADER_STATUS_STARTING);          \
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);

// token and message id are copied from request stored in ctx.exchange_ctx
// correct response must contain the same token and message id as request
#define SET_MSG_ID(Msg, Msg_id)                  \
    do {                                         \
        uint16_t msg_id__ = (uint16_t) (Msg_id); \
        (Msg)[2] = (uint8_t) (msg_id__ >> 8);    \
        (Msg)[3] = (uint8_t) (msg_id__ & 0xFF);  \
    } while (0)

#define COPY_MSG_ID(Msg) \
    SET_MSG_ID(Msg, ctx.exchange_ctx.base_msg.coap_binding_data.message_id)

#define COPY_TOKEN(Msg, Token_size) \
    memcpy(&(Msg)[4], ctx.exchange_ctx.base_msg.token.bytes, Token_size)

#define COPY_TOKEN_AND_MSG_ID(Msg, Token_size) \
    do {                                       \
        COPY_TOKEN(Msg, Token_size);           \
        COPY_MSG_ID(Msg);                      \
    } while (0)

#define ADD_RESPONSE(Response)                 \
    COPY_TOKEN_AND_MSG_ID(Response, 8);        \
    mock.bytes_to_recv = sizeof(Response) - 1; \
    mock.data_to_recv = (uint8_t *) Response

#define HANDLE_REQUEST(Request, Response)                             \
    mock.bytes_to_send = 500;                                         \
    g_callback_counter = 0;                                           \
    anj_coap_downloader_step(&ctx);                                   \
    COPY_TOKEN_AND_MSG_ID(Request, 8);                                \
    ANJ_UNIT_ASSERT_EQUAL(sizeof(Request) - 1, mock.bytes_sent);      \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, Request, \
                                      mock.bytes_sent);               \
    ADD_RESPONSE(Response);                                           \
    mock.bytes_to_send = 0;                                           \
    anj_coap_downloader_step(&ctx);                                   \
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,                    \
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);

static char response_1[] = "\x68"         // header v 0x01, Ack, tkl 8
                           "\x45\x00\x00" // Content code 2.05
                           "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
                           "\x41\xA2"                         // etag 0xA2
                           "\xd1\x06\x08" // block2 num 0, more true, size 16
                           "\xFF"         // payload marker
                           "\x01\x02\x03\x04\x05\x06\x07\x08"
                           "\x11\x12\x13\x14\x15\x16\x17\x18";

static char request_1[] = "\x48"         // Confirmable, tkl 8
                          "\x01\x00\x00" // GET 0x01, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                          "\xb1\x61"                         // uri path /a
                          "\x02\x62\x63"                     // uri path /bc
                          "\x03\x64\x65\x66";                // uri path /def

static char response_2[] = "\x68"         // header v 0x01, Ack, tkl 8
                           "\x45\x00\x00" // Content code 2.05
                           "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
                           "\x41\xA2"                         // etag 0xA2
                           "\xd1\x06\x18" // block2 num 1, more true, size 16
                           "\xFF"         // payload marker
                           "\x21\x22\x23\x24\x25\x26\x27\x28"
                           "\x31\x32\x33\x34\x35\x36\x37\x38";

static char request_2[] = "\x48"         // Confirmable, tkl 8
                          "\x01\x00\x00" // GET 0x01, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                          "\xb1\x61"                         // uri path /a
                          "\x02\x62\x63"                     // uri path /bc
                          "\x03\x64\x65\x66"                 // uri path /def
                          "\xc1\x10"; // block2 num 1, more false, size 16

static char response_3[] = "\x68"         // header v 0x01, Ack, tkl 8
                           "\x45\x00\x00" // Content code 2.05
                           "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
                           "\x41\xA2"                         // etag 0xA2
                           "\xd1\x06\x20" // block2 num 2, more false, size 16
                           "\xFF"         // payload marker
                           "\x41\x42\x43\x44\x45\x46\x47\x48";

static char response_3_etag_mismatch[] =
        "\x68"                             // header v 0x01, Ack, tkl 8
        "\x45\x00\x00"                     // Content code 2.05
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\x41\xA3"                         // etag 0xA3
        "\xd1\x06\x20"                     // block2 num 2, more false, size 16
        "\xFF"                             // payload marker
        "\x41\x42\x43\x44\x45\x46\x47\x48";

static char request_3[] = "\x48"         // Confirmable, tkl 8
                          "\x01\x00\x00" // GET 0x01, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                          "\xb1\x61"                         // uri path /a
                          "\x02\x62\x63"                     // uri path /bc
                          "\x03\x64\x65\x66"                 // uri path /def
                          "\xc1\x20"; // block2 num 2, more false, size 16

static char response_2_too_short[] =
        "\x68"                             // header v 0x01, Ack, tkl 8
        "\x45\x00\x00"                     // Content code 2.05
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\x41\xA2"                         // etag 0xA2
        "\xd1\x06\x18"                     // block2 num 1, more true, size 16
        "\xFF"                             // payload marker
        "\x21\x22\x23\x24\x25\x26\x27\x28";

static char response_2_changed_szx[] =
        "\x68"                             // header v 0x01, Ack, tkl 8
        "\x45\x00\x00"                     // Content code 2.05
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\x41\xA2"                         // etag 0xA2
        "\xd1\x06\x1D"                     // block2 num 1, more true, size 32
        "\xFF"                             // payload marker
        "\x21\x22\x23\x24\x25\x26\x27\x28"
        "\x31\x32\x33\x34\x35\x36\x37\x38";

static char response_created[] = "\x68"         // header v 0x01, Ack, tkl 8
                                 "\x41\x00\x00" // Created code 2.01
                                 "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token

static char server_empty_ack[] = "\x60"          // ACK, tkl 0
                                 "\x00\x00\x00"; // empty msg

static char separate_response_final[] =
        "\x48"                             // header v 0x01, Confirmable, tkl 8
        "\x45\x00\x00"                     // Content code 2.05
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\x41\xA2"                         // etag 0xA2
        "\xFF"                             // payload marker
        "\x51\x52\x53\x54\x55\x56\x57\x58";

static char separate_response_1[] =
        "\x48"                             // header v 0x01, Confirmable, tkl 8
        "\x45\x00\x00"                     // Content code 2.05
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
        "\x41\xA2"                         // etag 0xA2
        "\xd1\x06\x08"                     // block2 num 0, more true, size 16
        "\xFF"                             // payload marker
        "\x01\x02\x03\x04\x05\x06\x07\x08"
        "\x11\x12\x13\x14\x15\x16\x17\x18";

#define FINAL_CHECK()                                                     \
    g_callback_counter = 0;                                               \
    anj_coap_downloader_step(&ctx);                                       \
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,                        \
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);          \
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);                         \
    anj_coap_downloader_step(&ctx);                                       \
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,                        \
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHED);           \
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);                         \
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 40);                                \
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(g_data,                             \
                                      "\x01\x02\x03\x04\x05\x06\x07\x08"  \
                                      "\x11\x12\x13\x14\x15\x16\x17\x18"  \
                                      "\x21\x22\x23\x24\x25\x26\x27\x28"  \
                                      "\x31\x32\x33\x34\x35\x36\x37\x38"  \
                                      "\x41\x42\x43\x44\x45\x46\x47\x48", \
                                      g_data_len);

#define ASSERT_RETRY_SCHEDULED(Previous_status, Attempts)                    \
    do {                                                                     \
        ANJ_UNIT_ASSERT_TRUE(                                                \
                ctx.status == ANJ_COAP_DOWNLOADER_STATUS_FINISHING           \
                || ctx.status                                                \
                           == ANJ_COAP_DOWNLOADER_STATUS_WAITING_FOR_RETRY); \
        g_callback_counter = 0;                                              \
        if (ctx.status == ANJ_COAP_DOWNLOADER_STATUS_FINISHING) {            \
            anj_coap_downloader_step(&ctx);                                  \
        }                                                                    \
        ANJ_UNIT_ASSERT_EQUAL(ctx.status,                                    \
                              ANJ_COAP_DOWNLOADER_STATUS_WAITING_FOR_RETRY); \
        ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status, Previous_status);     \
        ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);                        \
        ANJ_UNIT_ASSERT_EQUAL(ctx.current_retry_attempts, Attempts);         \
        ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx), 0);       \
    } while (0)

#define WAIT_FOR_RETRY_AND_RESTART()                                         \
    do {                                                                     \
        g_callback_counter = 0;                                              \
        anj_coap_downloader_step(&ctx);                                      \
        ANJ_UNIT_ASSERT_EQUAL(ctx.status,                                    \
                              ANJ_COAP_DOWNLOADER_STATUS_WAITING_FOR_RETRY); \
        ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,                       \
                              ANJ_COAP_DOWNLOADER_STATUS_WAITING_FOR_RETRY); \
        ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);                        \
        mock_time_advance(anj_time_duration_new(10, ANJ_TIME_UNIT_S));       \
        anj_coap_downloader_step(&ctx);                                      \
        ANJ_UNIT_ASSERT_EQUAL(ctx.status,                                    \
                              ANJ_COAP_DOWNLOADER_STATUS_RETRYING);          \
        ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,                       \
                              ANJ_COAP_DOWNLOADER_STATUS_WAITING_FOR_RETRY); \
        ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);                        \
        anj_coap_downloader_step(&ctx);                                      \
        ANJ_UNIT_ASSERT_EQUAL(ctx.status,                                    \
                              ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);       \
        ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,                       \
                              ANJ_COAP_DOWNLOADER_STATUS_RETRYING);          \
        ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);                        \
    } while (0)

#define COMPLETE_SUSPEND()                                           \
    do {                                                             \
        ANJ_UNIT_ASSERT_EQUAL(ctx.status,                            \
                              ANJ_COAP_DOWNLOADER_STATUS_FINISHING); \
        g_callback_counter = 0;                                      \
        anj_coap_downloader_step(&ctx);                              \
        ANJ_UNIT_ASSERT_EQUAL(ctx.status,                            \
                              ANJ_COAP_DOWNLOADER_STATUS_SUSPENDED); \
        ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);                \
        anj_coap_downloader_step(&ctx);                              \
        ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,               \
                              ANJ_COAP_DOWNLOADER_STATUS_SUSPENDED); \
        ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);                \
    } while (0)

#define RESUME_DOWNLOAD()                                              \
    do {                                                               \
        ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_resume(&ctx));     \
        ANJ_UNIT_ASSERT_EQUAL(ctx.status,                              \
                              ANJ_COAP_DOWNLOADER_STATUS_RESUMING);    \
        g_callback_counter = 0;                                        \
        anj_coap_downloader_step(&ctx);                                \
        ANJ_UNIT_ASSERT_EQUAL(ctx.status,                              \
                              ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING); \
        ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,                 \
                              ANJ_COAP_DOWNLOADER_STATUS_RESUMING);    \
        ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);                  \
    } while (0)

ANJ_UNIT_TEST(coap_downloader, basic_download) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.hostname, "test_uri.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.port, "5683");
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

ANJ_UNIT_TEST(coap_downloader, basic_download_two_in_the_row) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.hostname, "test_uri.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.port, "5683");
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();

    g_data_len = 0;
    memset(g_data, 0, sizeof(g_data));
    g_callback_counter = 0;
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

ANJ_UNIT_TEST(coap_downloader,
              basic_download_two_in_the_row_with_different_uri_and_etag) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.hostname, "test_uri.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.port, "5683");
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();

    g_data_len = 0;
    memset(g_data, 0, sizeof(g_data));
    g_callback_counter = 0;
    START_DOWNLOAD(BASE_URI_2);
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.hostname, "uri_turi.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.port, "333");
    response_1[13] = 0x11; // change etag
    response_2[13] = 0x11; // change etag
    response_3[13] = 0x11; // change etag
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
    // restore etag for other tests
    response_1[13] = 0xA2;
    response_2[13] = 0xA2;
    response_3[13] = 0xA2;
}

ANJ_UNIT_TEST(coap_downloader, basic_download_with_recv_eagain) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.hostname, "test_uri.com");
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.port, "5683");
    HANDLE_REQUEST(request_1, response_1);

    // simulate EAGAIN during receive, if bytes_to_recv==0, mock will return
    // EAGAIN
    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    // in every anj_coap_downloader_step call we will try to receive data
    // but we will get EAGAIN until we set bytes_to_recv>0
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ADD_RESPONSE(response_2);
    mock.bytes_to_send = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);

    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

ANJ_UNIT_TEST(coap_downloader, second_start_fails) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_start(&ctx, BASE_URI, NULL),
                          ANJ_COAP_DOWNLOADER_ERR_IN_PROGRESS);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: starting a new download while the current one is waiting for a retry
// is rejected. The retry state and attempt counter remain unchanged.
// Client downloader             | Network / Server
// ------------------------------------------------
// connect() fails
// close connection
// wait for retry
// start new download -> ERR_IN_PROGRESS
ANJ_UNIT_TEST(coap_downloader, start_while_waiting_for_retry_fails) {
    TEST_INIT_RETRY();
    net_api_mock_force_connection_failure();
    START_DOWNLOAD(BASE_URI);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_STARTING, 1);

    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_start(&ctx, BASE_URI_2, NULL),
                          ANJ_COAP_DOWNLOADER_ERR_IN_PROGRESS);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status,
                          ANJ_COAP_DOWNLOADER_STATUS_WAITING_FOR_RETRY);
    ANJ_UNIT_ASSERT_EQUAL(ctx.current_retry_attempts, 1);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx), 0);
}

// Test: starting a new download after the retry delay expires, but before the
// retry connection is established, is rejected. The retry state and attempt
// counter remain unchanged.
// Client downloader             | Network / Server
// ------------------------------------------------
// connect() fails
// close connection
// retry delay expires
// start new download -> ERR_IN_PROGRESS
ANJ_UNIT_TEST(coap_downloader, start_while_retrying_fails) {
    TEST_INIT_RETRY();
    net_api_mock_force_connection_failure();
    START_DOWNLOAD(BASE_URI);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_STARTING, 1);

    anj_coap_downloader_step(&ctx);
    mock_time_advance(anj_time_duration_new(10, ANJ_TIME_UNIT_S));
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_RETRYING);

    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_start(&ctx, BASE_URI_2, NULL),
                          ANJ_COAP_DOWNLOADER_ERR_IN_PROGRESS);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_RETRYING);
    ANJ_UNIT_ASSERT_EQUAL(ctx.current_retry_attempts, 1);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx), 0);
}

// Test: starting a new download while a suspended transfer is being resumed is
// rejected. The retained progress and the resume operation remain unchanged.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// suspend download
// resume download
// start new download -> ERR_IN_PROGRESS
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, start_while_resuming_fails) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_resume(&ctx));
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_RESUMING);

    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_start(&ctx, BASE_URI_2, NULL),
                          ANJ_COAP_DOWNLOADER_ERR_IN_PROGRESS);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_RESUMING);
    ANJ_UNIT_ASSERT_EQUAL(ctx.next_block2_number, 1);

    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_RESUMING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);

    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

ANJ_UNIT_TEST(coap_downloader, invalid_uri) {
    TEST_INIT();
    // invalid prefix
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_start(
                                  &ctx, "coapss://test_uri.com:5683/a/bc/def",
                                  NULL),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_URI);
    // empty host
    ANJ_UNIT_ASSERT_EQUAL(
            anj_coap_downloader_start(&ctx, "coap://:5683/a/bc/def", NULL),
            ANJ_COAP_DOWNLOADER_ERR_INVALID_URI);
    // invalid port
    ANJ_UNIT_ASSERT_EQUAL(
            anj_coap_downloader_start(&ctx, "coap://test_uri.com:abcd/a/bc/def",
                                      NULL),
            ANJ_COAP_DOWNLOADER_ERR_INVALID_URI);
    // missing path
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_start(&ctx, "", NULL),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_URI);
    // missing net_config while coaps uri
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_start(&ctx, BASE_URI_3, NULL),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_CONFIGURATION);
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_start(&ctx, BASE_URI, NULL));
}

ANJ_UNIT_TEST(coap_downloader, basic_download_with_net_again_inprogress) {
    TEST_INIT();
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EINPROGRESS;
    mock.call_result[ANJ_NET_FUN_CLEANUP] = ANJ_NET_EINPROGRESS;
    START_DOWNLOAD(BASE_URI);
    // one additional step to handle ANJ_NET_EINPROGRESS
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    anj_coap_downloader_step(&ctx);
    //    HANDLE_REQUEST(request_1, response_1);
    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_1, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_1) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_1,
                                      mock.bytes_sent);
    ADD_RESPONSE(response_1);
    mock.bytes_to_send = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    g_callback_counter = 0;
    // we don't leave FINISHING until connection is closed
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    mock.call_result[ANJ_NET_FUN_CLEANUP] = 0;
    anj_coap_downloader_step(&ctx);
    // second call to call coap_downloader_callback
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHED);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
}

ANJ_UNIT_TEST(coap_downloader, termination) {
    TEST_INIT();

    START_DOWNLOAD(BASE_URI);
    anj_coap_downloader_terminate(&ctx);
    anj_coap_downloader_step(&ctx);
    // second call to call coap_downloader_callback
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TERMINATED);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 3);

    // if there is no download in progress, termination has no effect
    g_callback_counter = 0;
    anj_coap_downloader_terminate(&ctx);
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);

    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    g_callback_counter = 0;
    anj_coap_downloader_terminate(&ctx);
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
    // one block was served
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TERMINATED);
}

ANJ_UNIT_TEST(coap_downloader, network_error_send) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 3);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_NETWORK);
}

ANJ_UNIT_TEST(coap_downloader, network_error_send_next_try_succeeds) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 3);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_NETWORK);

    // start new download, this time send will succeed
    g_data_len = 0;
    memset(g_data, 0, sizeof(g_data));
    g_callback_counter = 0;
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

ANJ_UNIT_TEST(coap_downloader, network_error_connect) {
    TEST_INIT();
    net_api_mock_force_connection_failure();
    START_DOWNLOAD(BASE_URI);
    anj_coap_downloader_step(&ctx);
    // connection is not established, but we still want to cleanup network ctx
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 3);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_NETWORK);
}

static char response_bad_request[] =
        "\x68"                              // header v 0x01, Ack, tkl 8
        "\x80\x00\x00"                      // bad request code 4.00
        "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // token

ANJ_UNIT_TEST(coap_downloader, invalid_server_response) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_bad_request);

    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_RESPONSE);
}

uint8_t reset_response[] = "\x70"          // ACK, tkl 0
                           "\x00\x22\x22"; // empty msg

ANJ_UNIT_TEST(coap_downloader, reset_response) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    // simulate reset response - there is no token so there are more code here
    mock.bytes_to_send = 500;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_2, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_2) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_2,
                                      mock.bytes_sent);
    reset_response[2] =
            ctx.exchange_ctx.base_msg.coap_binding_data.message_id >> 8;
    reset_response[3] =
            ctx.exchange_ctx.base_msg.coap_binding_data.message_id & 0xFF;
    mock.bytes_to_recv = sizeof(reset_response) - 1;
    mock.data_to_recv = (uint8_t *) reset_response;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);

    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_RESPONSE);
}

ANJ_UNIT_TEST(coap_downloader, etag_mismatch) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    response_2[13] = 0xA3; // change ETag in response_2
    HANDLE_REQUEST(request_2, response_2);
    response_2[13] = 0xA2; // restore original ETag

    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_ETAG_MISMATCH);
}

ANJ_UNIT_TEST(coap_downloader, retransmission) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);

    // send request_3, but do not receive response
    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_3, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_3) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_3,
                                      mock.bytes_sent);

    // nothing yet happened
    mock.bytes_sent = 0;
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(mock.bytes_sent, 0);

    // now we should retransmit request_3
    mock_time_advance(anj_time_duration_new(3, ANJ_TIME_UNIT_S));
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

ANJ_UNIT_TEST(coap_downloader, too_many_paths_error) {
    TEST_INIT();

    // ANJ_COAP_DOWNLOADER_MAX_PATHS_NUMBER exceeded
    START_DOWNLOAD("coap://test_uri.com:5683/a/bc/def/sss");
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 3);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INTERNAL);

    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

#define TEST_INIT_NO_RETRANSMISSION()                             \
    mock_time_reset();                                            \
    net_api_mock_t mock = { 0 };                                  \
    net_api_mock_ctx_init(&mock);                                 \
    mock.inner_mtu_value = 100;                                   \
    anj_coap_downloader_t ctx;                                    \
    anj_exchange_udp_tx_params_t udp_tx_params = {                \
        .max_retransmit = 0,                                      \
        .ack_timeout = anj_time_duration_new(1, ANJ_TIME_UNIT_S), \
        .ack_random_factor = 2.0,                                 \
    };                                                            \
    anj_coap_downloader_configuration_t config = {                \
        .event_cb = coap_downloader_callback,                     \
        .event_cb_arg = NULL,                                     \
        .udp_tx_params = &udp_tx_params,                          \
    };                                                            \
    g_data_len = 0;                                               \
    g_callback_counter = 0;                                       \
    g_set_connection_status = ANJ_COAP_DOWNLOADER_STATUS_INITIAL; \
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_init(&ctx, &config));

ANJ_UNIT_TEST(coap_downloader, exchange_timeout) {
    TEST_INIT_NO_RETRANSMISSION();
    START_DOWNLOAD(BASE_URI);

    mock.bytes_to_send = 500;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_1, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_1) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_1,
                                      mock.bytes_sent);
    anj_coap_downloader_step(&ctx);

    // timeout the exchange
    mock_time_advance(anj_time_duration_new(2, ANJ_TIME_UNIT_S));
    anj_coap_downloader_step(&ctx); // timeout here
    anj_coap_downloader_step(&ctx); // finishing here
    anj_coap_downloader_step(&ctx); // failed here
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 3);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TIMEOUT);
}

ANJ_UNIT_TEST(coap_downloader, msg_buffer_too_small) {
    TEST_INIT_NO_RETRANSMISSION();
    START_DOWNLOAD(BASE_URI);
    // we don't have to provide bigger response, just set error code
    mock.call_result[ANJ_NET_FUN_RECV] = ANJ_NET_EMSGSIZE;

    // network integration layer of anj_core will drop this response
    // this is not intuitive, but it is ok for standard connection
    // so we can accept it here

    mock.bytes_to_send = 500;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_1, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_1) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_1,
                                      mock.bytes_sent);
    ADD_RESPONSE(response_1);
    mock.bytes_to_send = 0;
    anj_coap_downloader_step(&ctx);

    // timeout the exchange
    mock_time_advance(anj_time_duration_new(2, ANJ_TIME_UNIT_S));
    anj_coap_downloader_step(&ctx); // timeout here
    anj_coap_downloader_step(&ctx); // finishing here
    anj_coap_downloader_step(&ctx); // failed here
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 3);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TIMEOUT);
}

ANJ_UNIT_TEST(coap_downloader, additional_ignored_message) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    // add response_1 second time - it should be ignored: block number mismatch
    ADD_RESPONSE(response_1);
    mock.bytes_to_send = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);

    // add response_1 third time but with different token - it also should be
    // ignored: token mismatch
    ADD_RESPONSE(response_1);
    response_1[4]++; // change token
    mock.bytes_to_send = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);

    ADD_RESPONSE(response_2);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);

    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: download is retried after an initial connection failure and the next
// attempt completes normally.
// Client downloader             | Network / Server
// ------------------------------------------------
// connect() fails
// close connection
// retry delay expires
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, retry) {
    TEST_INIT_RETRY();
    net_api_mock_force_connection_failure();
    START_DOWNLOAD(BASE_URI);
    // connection is not established, but we still want to cleanup network ctx
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_STARTING, 1);
    WAIT_FOR_RETRY_AND_RESTART();
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: a connection cleanup failure after receiving the final block finishes
// the download with a network error without scheduling a retry.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
// close connection fails
// finish with ERR_NETWORK without retry
ANJ_UNIT_TEST(coap_downloader,
              cleanup_failure_after_completed_download_does_not_retry) {
    TEST_INIT_RETRY();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);

    ANJ_UNIT_ASSERT_EQUAL(ctx.next_block2_number, 2);

    mock.call_result[ANJ_NET_FUN_CLEANUP] = -1;
    g_callback_counter = 0;

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_NETWORK);
    ANJ_UNIT_ASSERT_EQUAL(ctx.current_retry_attempts, 0);

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);

    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 40);
}

// Test: retry starts from the first block that has not been fully downloaded.
// Blocks 0 and 1 are already delivered to the application. Sending request for
// block 2 fails, so the next exchange resumes from block 2 instead of block 0.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ----X send failure
// close connection
// retry delay expires
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, retry_resumes_from_last_downloaded_block) {
    TEST_INIT_RETRY();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 32);

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING, 1);
    WAIT_FOR_RETRY_AND_RESTART();

    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_3, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_3) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_3,
                                      mock.bytes_sent);

    ADD_RESPONSE(response_3);
    mock.bytes_to_send = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    FINAL_CHECK();
}

// Test: download fails after all retry attempts are used.
// The first attempt fails while connecting. The following attempts fail while
// sending the request, so the retry budget is exhausted.
// Client downloader             | Network / Server
// ------------------------------------------------
// connect() fails
// close connection
// retry delay expires
// GET ----X send failure
// close connection
// retry delay expires
// GET ----X send failure
// close connection
ANJ_UNIT_TEST(coap_downloader, retry_fails) {
    TEST_INIT_RETRY();
    net_api_mock_force_connection_failure();
    START_DOWNLOAD(BASE_URI);
    // connection is not established, but we still want to cleanup network ctx
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_STARTING, 1);
    WAIT_FOR_RETRY_AND_RESTART();

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_RETRYING, 2);
    WAIT_FOR_RETRY_AND_RESTART();

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);

    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_NETWORK);
}

// Test: user termination during a retry attempt aborts the download without
// scheduling another retry.
// Client downloader             | Network / Server
// ------------------------------------------------
// connect() fails
// close connection
// retry delay expires
// connect succeeds
// terminate download
// close connection
ANJ_UNIT_TEST(coap_downloader, retry_aborted) {
    TEST_INIT_RETRY();
    net_api_mock_force_connection_failure();
    START_DOWNLOAD(BASE_URI);
    // connection is not established, but we still want to cleanup network ctx
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_STARTING, 1);
    WAIT_FOR_RETRY_AND_RESTART();
    anj_coap_downloader_terminate(&ctx);
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TERMINATED);
}

// Test: user termination while waiting for a retry aborts the download without
// starting another attempt. The download finishes with the terminated error.
// Client downloader             | Network / Server
// ------------------------------------------------
// connect() fails
// close connection
// wait for retry
// terminate download
// finish with ERR_TERMINATED
ANJ_UNIT_TEST(coap_downloader, termination_while_waiting_for_retry) {
    TEST_INIT_RETRY();
    net_api_mock_force_connection_failure();
    START_DOWNLOAD(BASE_URI);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_STARTING, 1);

    anj_coap_downloader_terminate(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TERMINATED);

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
}

// Test: user termination after the retry delay expires, but before the retry
// connection is established, aborts the download. The download finishes with
// the terminated error instead of starting another attempt.
// Client downloader             | Network / Server
// ------------------------------------------------
// connect() fails
// close connection
// retry delay expires
// terminate download
// finish with ERR_TERMINATED
ANJ_UNIT_TEST(coap_downloader, termination_while_retrying) {
    TEST_INIT_RETRY();
    net_api_mock_force_connection_failure();
    START_DOWNLOAD(BASE_URI);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_STARTING, 1);

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_WAITING_FOR_RETRY);
    mock_time_advance(anj_time_duration_new(10, ANJ_TIME_UNIT_S));
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_RETRYING);

    g_callback_counter = 0;
    anj_coap_downloader_terminate(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TERMINATED);

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
}

// Test: retry is not continued when the resumed block has a different ETag.
// Blocks 0 and 1 are downloaded with ETag A2. After a send failure, retry asks
// for block 2, but the server responds with ETag A3, so already written data is
// no longer guaranteed to belong to the same resource representation.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more, ETag A2
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more, ETag A2
// GET block2 2 ----X send failure
// close connection
// retry delay expires
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2, ETag A3
ANJ_UNIT_TEST(coap_downloader, retry_with_etag_mismatch) {
    TEST_INIT_RETRY();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 32);

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING, 1);
    WAIT_FOR_RETRY_AND_RESTART();

    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_3, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_3) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_3,
                                      mock.bytes_sent);

    ADD_RESPONSE(response_3_etag_mismatch);
    mock.bytes_to_send = 0;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 32);

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_ETAG_MISMATCH);
}

// Test: an old block received after retry resume is ignored.
// Blocks 0 and 1 are already delivered to the application. Retry asks for block
// 2, but the server first repeats block 1. That repeated block must not be
// delivered again; the transfer continues when block 2 arrives.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ----X send failure
// close connection
// retry delay expires
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 1 more
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, retry_with_old_block) {
    TEST_INIT_RETRY();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 32);

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING, 1);
    WAIT_FOR_RETRY_AND_RESTART();

    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_3, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_3) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_3,
                                      mock.bytes_sent);

    ADD_RESPONSE(response_2);
    mock.bytes_to_send = 0;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 32);

    ADD_RESPONSE(response_3);
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 40);
    FINAL_CHECK();
}

// Test: the server cannot change Block2 SZX in the middle of a transfer.
// The first response fixes Block2 size to 16 bytes. The next response reports
// block 1 with size 32, which would make the block number ambiguous relative to
// the bytes already written.
// Client downloader             | Server
// ----------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more, size 16
// GET block2 1 size 16 -------->
//                               <---- ACK 2.05 block2 1 more, size 32
ANJ_UNIT_TEST(coap_downloader, server_changes_szx) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_2, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_2) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_2,
                                      mock.bytes_sent);

    ADD_RESPONSE(response_2_changed_szx);
    mock.bytes_to_send = 0;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);

    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INTERNAL);
}

// Test: non-final Block2 response must contain exactly SZX bytes of payload.
// The server responds with Block2 size 16 and more flag set, but sends only 8
// payload bytes. This response is malformed and must fail the exchange.
// Client downloader             | Server
// ----------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more, 8 bytes
ANJ_UNIT_TEST(coap_downloader, non_final_block2_payload_too_short) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_2, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_2) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_2,
                                      mock.bytes_sent);

    ADD_RESPONSE(response_2_too_short);
    mock.bytes_to_send = 0;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);

    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INTERNAL);
}

// TODO: Should work after fixing a bug in EMB#5742:
// https://gitlab.avsystem.com/iot/embedded/EMB/-/work_items/5742
// Test: downloader handles a separate response.
// The server first acknowledges the CON GET with an Empty ACK. The actual
// content arrives later as a separate CON response, so the client must send an
// Empty ACK for that response before the exchange is complete.
// Client downloader             | Server
// ----------------------------------------------
// GET -------------------------->
//                               <---- Empty ACK
//                               <---- CON 2.05 Content
// Empty ACK ------------------->
// ANJ_UNIT_TEST(coap_downloader, separate_response) {
//     TEST_INIT();
//     START_DOWNLOAD(BASE_URI);
//
//     mock.bytes_to_send = 500;
//     g_callback_counter = 0;
//     anj_coap_downloader_step(&ctx);
//     COPY_TOKEN_AND_MSG_ID(request_1, 8);
//     ANJ_UNIT_ASSERT_EQUAL(sizeof(request_1) - 1, mock.bytes_sent);
//     ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_1,
//                                       mock.bytes_sent);
//
//     COPY_MSG_ID(server_empty_ack);
//     mock.bytes_to_recv = sizeof(server_empty_ack) - 1;
//     mock.data_to_recv = (uint8_t *) server_empty_ack;
//     mock.bytes_to_send = 0;
//     anj_coap_downloader_step(&ctx);
//     ANJ_UNIT_ASSERT_EQUAL(ctx.status,
//     ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
//     ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
//     ANJ_UNIT_ASSERT_EQUAL(g_data_len, 0);
//
//     uint16_t separate_mid =
//             (uint16_t)
//             (ctx.exchange_ctx.base_msg.coap_binding_data.message_id
//                         + 1);
//     COPY_TOKEN(separate_response_final, 8);
//     SET_MSG_ID(separate_response_final, separate_mid);
//     mock.bytes_to_recv = sizeof(separate_response_final) - 1;
//     mock.data_to_recv = (uint8_t *) separate_response_final;
//     // Force the required Empty ACK to be sent by the next step.
//     mock.bytes_to_send = 0;
//     mock.bytes_sent = 0;
//     g_callback_counter = 0;
//     anj_coap_downloader_step(&ctx);
//     ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
//                           ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
//     ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);
//     ANJ_UNIT_ASSERT_EQUAL(g_data_len, 8);
//     ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
//             g_data, "\x51\x52\x53\x54\x55\x56\x57\x58", g_data_len);
//
//     char expected_empty_ack[] = "\x60"          // ACK, tkl 0
//                                 "\x00\x00\x00"; // empty msg
//     SET_MSG_ID(expected_empty_ack, separate_mid);
//     mock.bytes_to_send = 500;
//     g_callback_counter = 0;
//     anj_coap_downloader_step(&ctx);
//     ANJ_UNIT_ASSERT_EQUAL(sizeof(expected_empty_ack) - 1, mock.bytes_sent);
//     ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
//             mock.send_data_buffer, expected_empty_ack, mock.bytes_sent);
//     ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
//
//     anj_coap_downloader_step(&ctx);
//     ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
//                           ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
//     ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);
//     anj_coap_downloader_step(&ctx);
//     ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
//                           ANJ_COAP_DOWNLOADER_STATUS_FINISHED);
//     ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
//     ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx), 0);
// }

// Test: downloader handles separate response for the first Block2 response and
// then continues the remaining block transfer.
// The first content block is delivered as a separate CON response. After the
// client ACKs it, the downloader sends the next Block2 request and completes
// the rest of the transfer normally.
// Client downloader             | Server
// ----------------------------------------------
// GET -------------------------->
//                               <---- Empty ACK
//                               <---- CON 2.05 block2 0 more
// Empty ACK ------------------->
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, separate_response_with_block_transfer) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);

    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_1, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_1) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_1,
                                      mock.bytes_sent);

    COPY_MSG_ID(server_empty_ack);
    mock.bytes_to_recv = sizeof(server_empty_ack) - 1;
    mock.data_to_recv = (uint8_t *) server_empty_ack;
    mock.bytes_to_send = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);

    uint16_t separate_mid =
            (uint16_t) (ctx.exchange_ctx.base_msg.coap_binding_data.message_id
                        + 1);
    COPY_TOKEN(separate_response_1, 8);
    SET_MSG_ID(separate_response_1, separate_mid);
    mock.bytes_to_recv = sizeof(separate_response_1) - 1;
    mock.data_to_recv = (uint8_t *) separate_response_1;
    mock.bytes_to_send = 0;
    mock.bytes_sent = 0;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);

    char expected_empty_ack[] = "\x60"          // ACK, tkl 0
                                "\x00\x00\x00"; // empty msg
    SET_MSG_ID(expected_empty_ack, separate_mid);
    mock.bytes_to_send = 500;
    g_callback_counter = 0;
    net_api_mock_dont_overwrite_buffer(ctx.connection_ctx.net_ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(expected_empty_ack) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, expected_empty_ack,
                                      mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);

    mock.dont_overwrite_buffer = false;
    mock.bytes_sent = 0;
    mock.bytes_to_send = 500;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_2, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_2) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_2,
                                      mock.bytes_sent);

    ADD_RESPONSE(response_2);
    mock.bytes_to_send = 0;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 32);

    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: suspension from a data callback handling a separate response is
// deferred until exchange processing returns. The next Block2 request is not
// sent, while resume continues from the committed block.
// Client downloader             | Server
// ----------------------------------------------
// GET -------------------------->
//                               <---- Empty ACK
//                               <---- CON 2.05 block2 0 more
// suspend from data callback
// TODO: Empty ACK -------------->
// close connection
// resume download
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_from_separate_response_callback) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);

    mock.bytes_to_send = 500;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_1, 8);

    COPY_MSG_ID(server_empty_ack);
    mock.bytes_to_recv = sizeof(server_empty_ack) - 1;
    mock.data_to_recv = (uint8_t *) server_empty_ack;
    mock.bytes_to_send = 0;
    anj_coap_downloader_step(&ctx);

    uint16_t separate_mid =
            (uint16_t) (ctx.exchange_ctx.base_msg.coap_binding_data.message_id
                        + 1);
    COPY_TOKEN(separate_response_1, 8);
    SET_MSG_ID(separate_response_1, separate_mid);
    mock.bytes_to_recv = sizeof(separate_response_1) - 1;
    mock.data_to_recv = (uint8_t *) separate_response_1;
    mock.bytes_sent = 0;
    g_data_callback_action = DATA_CALLBACK_ACTION_SUSPEND;
    anj_coap_downloader_step(&ctx);

    ANJ_UNIT_ASSERT_SUCCESS(g_data_callback_action_result);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(ctx.next_block2_number, 1);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);

    // // TODO: Should work after fixing a bug in EMB#5742:
    // https://gitlab.avsystem.com/iot/embedded/EMB/-/work_items/5742.
    // The separate CON response should be acknowledged before suspension
    // cleanup. This block currently fails because no Empty ACK is sent. char
    // expected_empty_ack[] = "\x60"          // ACK, tkl 0
    //                             "\x00\x00\x00"; // empty msg
    // SET_MSG_ID(expected_empty_ack, separate_mid);
    // mock.bytes_to_send = 500;
    // anj_coap_downloader_step(&ctx);
    // ANJ_UNIT_ASSERT_EQUAL(sizeof(expected_empty_ack) - 1, mock.bytes_sent);
    // ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
    //         mock.send_data_buffer, expected_empty_ack, mock.bytes_sent);

    COMPLETE_SUSPEND();
    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: retry attempt counter is reset after any payload is successfully
// written.
// Each send failure increments the retry counter, but receiving a later payload
// block confirms progress and resets the counter back to zero.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ----X send failure
// close connection
// retry delay expires
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ----X send failure
// close connection
// retry delay expires
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, retry_limit_reset) {
    TEST_INIT_RETRY();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    ANJ_UNIT_ASSERT_EQUAL(ctx.current_retry_attempts, 0);

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING, 1);
    ANJ_UNIT_ASSERT_EQUAL(ctx.current_retry_attempts, 1);

    WAIT_FOR_RETRY_AND_RESTART();
    HANDLE_REQUEST(request_2, response_2);
    ANJ_UNIT_ASSERT_EQUAL(ctx.current_retry_attempts, 0);

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING, 1);
    ANJ_UNIT_ASSERT_EQUAL(ctx.current_retry_attempts, 1);

    WAIT_FOR_RETRY_AND_RESTART();
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: a download may be suspended before the first downloader step, so no
// connection or request is created until an explicit resume.
// Client downloader             | Network / Server
// ------------------------------------------------
// start download
// suspend download
// resume download
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_before_first_step) {
    TEST_INIT();

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_start(&ctx, BASE_URI, NULL));
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_STARTING);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], 0);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], 0);

    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: suspending while an asynchronous connection attempt is in progress
// cancels that attempt. Resuming creates a new connection and starts from the
// first block because no payload has been delivered yet.
// Client downloader             | Network / Server
// ------------------------------------------------
// connect() in progress
// suspend download
// close connection
// resume download
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_during_connect) {
    TEST_INIT();

    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EINPROGRESS;
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_start(&ctx, BASE_URI, NULL));
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_STARTING);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_STARTING);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 1);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;
    COMPLETE_SUSPEND();
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], 0);

    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: suspending after the connection has been established but before the
// first request is sent keeps the initial transfer state in the downloader.
// Resume on the same instance starts with a regular GET rather than a Block2
// request.
// Client downloader             | Network / Server
// ------------------------------------------------
// connect
// suspend before GET
// resume download
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_before_first_request) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], 0);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    RESUME_DOWNLOAD();

    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: a suspended blockwise transfer resumes from the first block that has
// not yet been delivered to the application. Previously delivered data must not
// be repeated.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// suspend download
// resume download
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_after_block_and_resume) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    RESUME_DOWNLOAD();

    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: suspension requested from the data callback is deferred until the
// currently delivered block is committed. No request for the next block is
// sent before cleanup, and resume continues without repeating payload.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// suspend from data callback
// resume download
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_from_data_callback) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);

    g_data_callback_action = DATA_CALLBACK_ACTION_SUSPEND;
    HANDLE_REQUEST(request_1, response_1);
    ANJ_UNIT_ASSERT_SUCCESS(g_data_callback_action_result);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(ctx.next_block2_number, 1);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], 1);

    COMPLETE_SUSPEND();
    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: suspension requested while delivering the final block resolves as a
// successful download. There is no remaining block from which the transfer
// could later be resumed.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
// suspend from final data callback
// finish download
ANJ_UNIT_TEST(coap_downloader, suspend_from_final_data_callback) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);

    g_data_callback_action = DATA_CALLBACK_ACTION_SUSPEND;
    HANDLE_REQUEST(request_3, response_3);
    ANJ_UNIT_ASSERT_SUCCESS(g_data_callback_action_result);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);

    FINAL_CHECK();
}

// Test: termination requested from the data callback is deferred until control
// returns from exchange processing. The next block is not requested and the
// exchange reaches a reusable finished state during cleanup.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// terminate from data callback
// close connection
ANJ_UNIT_TEST(coap_downloader, terminate_from_data_callback) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);

    g_data_callback_action = DATA_CALLBACK_ACTION_TERMINATE;
    HANDLE_REQUEST(request_1, response_1);
    ANJ_UNIT_ASSERT_SUCCESS(g_data_callback_action_result);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TERMINATED);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], 1);

    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);
    ANJ_UNIT_ASSERT_FALSE(_anj_exchange_ongoing_exchange(&ctx.exchange_ctx));

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 2);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], 1);
}

// Test: suspending an exchange that is waiting for a response preserves the
// request block as not delivered. A late response from the canceled exchange is
// ignored, and the resumed exchange requests and delivers that block once.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
// suspend download
// resume download
// GET block2 1 ---------------->
//                               <---- old ACK block2 1 (ignored)
//                               <---- new ACK block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_while_waiting_for_response) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    mock.bytes_to_send = 500;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_2, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_2) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_2,
                                      mock.bytes_sent);
    COPY_TOKEN_AND_MSG_ID(response_2, 8);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    RESUME_DOWNLOAD();

    mock.bytes_sent = 0;
    mock.bytes_to_send = 500;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_2, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_2) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_2,
                                      mock.bytes_sent);

    mock.bytes_to_recv = sizeof(response_2) - 1;
    mock.data_to_recv = (uint8_t *) response_2;
    mock.bytes_to_send = 0;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);

    ADD_RESPONSE(response_2);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 32);

    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: resume is rejected until asynchronous connection cleanup finishes.
// Once cleanup has completed, resume may be requested without an additional
// network operation in between.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// suspend download
// close() in progress
// resume rejected
// close() in progress
// resume rejected
// close succeeds
// resume download
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, resume_waits_for_suspend_cleanup) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    mock.call_result[ANJ_NET_FUN_CLEANUP] = ANJ_NET_EINPROGRESS;
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));

    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_resume(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_resume(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);

    mock.call_result[ANJ_NET_FUN_CLEANUP] = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_SUSPENDED);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 3);

    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_SUSPENDED);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 1);

    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: explicit suspension is not treated as a network failure and never
// schedules an automatic retry, even if downloader retries are enabled.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// suspend download
// retry delay expires (no traffic)
// resume download
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_does_not_schedule_retry) {
    TEST_INIT_RETRY();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    int connect_calls = mock.call_count[ANJ_NET_FUN_CONNECT];
    int send_calls = mock.call_count[ANJ_NET_FUN_SEND];

    mock_time_advance(anj_time_duration_new(30, ANJ_TIME_UNIT_S));
    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_SUSPENDED);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], connect_calls);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SEND], send_calls);

    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: suspending while waiting for an automatic retry cancels the retry. Time
// advancing past the retry deadline causes no traffic; only explicit resume on
// the same downloader instance reconnects and continues from the retained
// block.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ----X send failure
// close connection
// wait for retry
// suspend download
// retry delay expires (no traffic)
// resume download
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_while_waiting_for_retry) {
    TEST_INIT_RETRY();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING, 1);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    int connect_calls = mock.call_count[ANJ_NET_FUN_CONNECT];

    mock_time_advance(anj_time_duration_new(20, ANJ_TIME_UNIT_S));
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], connect_calls);

    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: suspension requested after the retry delay expires, but before the
// reconnecting step executes, prevents that reconnect from taking place.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ----X send failure
// close connection
// retry delay expires
// suspend while restarting
// resume download
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_while_restarting) {
    TEST_INIT_RETRY();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    net_api_mock_force_send_failure();
    anj_coap_downloader_step(&ctx);
    ASSERT_RETRY_SCHEDULED(ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING, 1);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status,
                          ANJ_COAP_DOWNLOADER_STATUS_WAITING_FOR_RETRY);
    mock_time_advance(anj_time_duration_new(10, ANJ_TIME_UNIT_S));
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_RETRYING);
    int connect_calls = mock.call_count[ANJ_NET_FUN_CONNECT];

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], connect_calls);

    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: a resume operation can itself be suspended before it performs network
// activity. Since RESUMING has not yet been reported, returning to SUSPENDED
// does not emit a duplicate callback. A second resume uses the retained
// transfer progress and completes normally.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// suspend download
// resume download
// suspend while resuming
// resume download
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_while_resuming) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_resume(&ctx));
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_RESUMING);
    int connect_calls = mock.call_count[ANJ_NET_FUN_CONNECT];

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_SUSPENDED);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_SUSPENDED);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], connect_calls);

    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: suspend is idempotent. Repeating it while suspension cleanup is already
// in progress neither discards transfer progress retained by the downloader nor
// turns suspension into another error condition.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// suspend download
// suspend download again
// suspend already suspended download again
// resume download
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, suspend_is_idempotent) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));

    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}

// Test: suspend and resume reject inactive states without changing downloader
// state. Finishing a successful transfer cannot be converted into a suspended
// transfer.
// Client downloader             | Network / Server
// ------------------------------------------------
// suspend before start (rejected)
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
// suspend while finishing (rejected)
// suspend after finish (rejected)
ANJ_UNIT_TEST(coap_downloader, suspend_rejected_in_inactive_states) {
    TEST_INIT();

    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_suspend(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_resume(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_INITIAL);

    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_suspend(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx), 0);

    FINAL_CHECK();
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_suspend(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_resume(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHED);
}

// Test: termination in progress and an ordinary failed download cannot be
// changed into a suspended download. Termination also invalidates transfer
// progress retained by the downloader for resume.
// Client downloader             | Network / Server
// ------------------------------------------------
// start download
// terminate download
// suspend while finishing (rejected)
// close connection
// suspend after failure (rejected)
// resume after failure (rejected)
ANJ_UNIT_TEST(coap_downloader, suspend_rejected_for_terminated_download) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);

    anj_coap_downloader_terminate(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_suspend(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TERMINATED);

    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_suspend(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_resume(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_TERMINATED);
}

// Test: transfer progress retained for resume belongs to the current downloader
// instance. Initializing the downloader again discards that progress, so the
// previous transfer can no longer be resumed.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more, ETag A2
// suspend download
// initialize downloader again
// resume rejected
ANJ_UNIT_TEST(coap_downloader, resume_unavailable_after_reinitialization) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_init(&ctx, &config));
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_INITIAL);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_resume(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
}

// Test: a resumed transfer validates the ETag retained by the downloader before
// delivering any new payload. If the resource changed while suspended, the
// transfer fails and cannot be resumed again.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET -------------------------->
//                               <---- ACK 2.05 block2 0 more, ETag A2
// suspend and resume
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more, ETag A3
// fail without delivering block 1
ANJ_UNIT_TEST(coap_downloader, resume_with_etag_mismatch) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();
    RESUME_DOWNLOAD();

    mock.bytes_to_send = 500;
    anj_coap_downloader_step(&ctx);
    COPY_TOKEN_AND_MSG_ID(request_2, 8);
    ANJ_UNIT_ASSERT_EQUAL(sizeof(request_2) - 1, mock.bytes_sent);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, request_2,
                                      mock.bytes_sent);

    response_2[13] = (char) 0xA3;
    ADD_RESPONSE(response_2);
    mock.bytes_to_send = 0;
    g_callback_counter = 0;
    anj_coap_downloader_step(&ctx);
    response_2[13] = (char) 0xA2;
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
    ANJ_UNIT_ASSERT_EQUAL(g_callback_counter, 0);
    ANJ_UNIT_ASSERT_EQUAL(g_data_len, 16);

    anj_coap_downloader_step(&ctx);
    anj_coap_downloader_step(&ctx);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FAILED);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_get_error(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_ETAG_MISMATCH);
    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_resume(&ctx),
                          ANJ_COAP_DOWNLOADER_ERR_INVALID_STATE);
}

// Test: starting an unrelated download on an instance containing a suspended
// transfer is rejected. The retained progress remains unchanged and the
// suspended transfer can still be resumed.
// Client downloader             | Network / Server
// ------------------------------------------------
// GET resource A -------------->
//                               <---- ACK 2.05 block2 0 more, ETag A2
// suspend resource A
// start resource B -> ERR_IN_PROGRESS
// resume resource A
// GET block2 1 ---------------->
//                               <---- ACK 2.05 block2 1 more
// GET block2 2 ---------------->
//                               <---- ACK 2.05 block2 2
ANJ_UNIT_TEST(coap_downloader, start_while_suspended_fails) {
    TEST_INIT();
    START_DOWNLOAD(BASE_URI);
    HANDLE_REQUEST(request_1, response_1);

    ANJ_UNIT_ASSERT_SUCCESS(anj_coap_downloader_suspend(&ctx));
    COMPLETE_SUSPEND();

    ANJ_UNIT_ASSERT_EQUAL(anj_coap_downloader_start(&ctx, BASE_URI_2, NULL),
                          ANJ_COAP_DOWNLOADER_ERR_IN_PROGRESS);
    ANJ_UNIT_ASSERT_EQUAL(ctx.status, ANJ_COAP_DOWNLOADER_STATUS_SUSPENDED);
    ANJ_UNIT_ASSERT_EQUAL(ctx.next_block2_number, 1);
    ANJ_UNIT_ASSERT_EQUAL(ctx.server_block2_size, 16);
    ANJ_UNIT_ASSERT_EQUAL(ctx.etag.size, 1);

    RESUME_DOWNLOAD();
    HANDLE_REQUEST(request_2, response_2);
    HANDLE_REQUEST(request_3, response_3);
    FINAL_CHECK();
}
