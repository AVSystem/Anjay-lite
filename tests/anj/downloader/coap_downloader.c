/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
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

#include "../../../src/anj/coap/coap.h"
#include "../../../src/anj/exchange.h"
#include "../../../src/anj/io/io.h"

#include "../mock/net_api_mock.h"
#include "../mock/time_api_mock.h"

#include <anj_unit_test.h>

static anj_coap_downloader_status_t g_set_connection_status =
        ANJ_COAP_DOWNLOADER_STATUS_INITIAL;
static uint8_t g_data[500] = { 0 };
static size_t g_data_len = 0;
static int g_callback_counter = 0;

static void coap_downloader_callback(void *arg,
                                     anj_coap_downloader_t *downloader,
                                     anj_coap_downloader_status_t conn_status,
                                     const uint8_t *data,
                                     size_t data_len) {
    (void) arg;
    (void) downloader;
    g_set_connection_status = conn_status;
    g_callback_counter++;
    if (data == NULL) {
        return;
    }
    memcpy(&g_data[g_data_len], data, data_len);
    g_data_len += data_len;
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
#define COPY_TOKEN_AND_MSG_ID(Msg, Token_size)                                \
    memcpy(&Msg[4], ctx.exchange_ctx.base_msg.token.bytes, Token_size);       \
    Msg[2] = ctx.exchange_ctx.base_msg.coap_binding_data.udp.message_id >> 8; \
    Msg[3] = ctx.exchange_ctx.base_msg.coap_binding_data.udp.message_id & 0xFF

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

static char request_3[] = "\x48"         // Confirmable, tkl 8
                          "\x01\x00\x00" // GET 0x01, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                          "\xb1\x61"                         // uri path /a
                          "\x02\x62\x63"                     // uri path /bc
                          "\x03\x64\x65\x66"                 // uri path /def
                          "\xc1\x20"; // block2 num 2, more false, size 16

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
    ANJ_UNIT_ASSERT_EQUAL(g_set_connection_status,
                          ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
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
            ctx.exchange_ctx.base_msg.coap_binding_data.udp.message_id >> 8;
    reset_response[3] =
            ctx.exchange_ctx.base_msg.coap_binding_data.udp.message_id & 0xFF;
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
