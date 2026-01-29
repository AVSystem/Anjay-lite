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
#include <stdlib.h>
#include <string.h>

#define ANJ_UNIT_ENABLE_SHORT_ASSERTS
#include <anj/anj_config.h>
#include <anj/defs.h>

#include "../../src/anj/coap/coap.h"
#include "../../src/anj/exchange.h"

#include "../mock/time_api_mock.h"

#include <anj_unit_test.h>

typedef struct {
    // read_payload_handler
    size_t out_payload_len;
    char *out_payload;
    uint16_t out_format;
    // write_payload_handler
    uint8_t buff[100];
    size_t buff_offset;
    bool last_block;
    // exchange_completion_handler
    const _anj_coap_msg_t *response;
    uint8_t result;
    int complete_counter;

    int counter;
    uint8_t ret_val;
} handlers_arg_t;

static uint8_t read_payload_handler(void *arg_ptr,
                                    uint8_t *buff,
                                    size_t buff_len,
                                    _anj_exchange_read_result_t *out_params) {
    (void) buff_len;
    handlers_arg_t *handlers_arg = (handlers_arg_t *) arg_ptr;
    out_params->payload_len = handlers_arg->out_payload_len;
    out_params->format = handlers_arg->out_format;
    if (handlers_arg->out_payload_len) {
        memcpy(buff, handlers_arg->out_payload, handlers_arg->out_payload_len);
    }
    handlers_arg->counter++;
    return handlers_arg->ret_val;
}

static uint8_t write_payload_handler(void *arg_ptr,
                                     uint8_t *buff,
                                     size_t buff_len,
                                     bool last_block) {
    handlers_arg_t *handlers_arg = (handlers_arg_t *) arg_ptr;
    memcpy(handlers_arg->buff + handlers_arg->buff_offset, buff, buff_len);
    handlers_arg->buff_offset += buff_len;
    handlers_arg->last_block = last_block;
    handlers_arg->counter++;
    return handlers_arg->ret_val;
}

static void exchange_completion_handler(void *arg_ptr,
                                        const _anj_coap_msg_t *response,
                                        int result) {
    handlers_arg_t *handlers_arg = (handlers_arg_t *) arg_ptr;
    handlers_arg->response = response;
    handlers_arg->result = result;
    handlers_arg->complete_counter++;
}

// no need to initialize MID - it can be randomized, and in verify_payload()
// we overwrite MID and token in serialized message
#define TEST_INIT()                      \
    mock_time_reset();                   \
    uint8_t payload[20];                 \
    _anj_coap_msg_t msg;                 \
    handlers_arg_t handlers_arg = { 0 }; \
    memset(&msg, 0, sizeof(msg));        \
    _anj_exchange_ctx_t ctx;             \
    _anj_exchange_init(&ctx);

static void
verify_payload(uint8_t *expected, size_t expected_len, _anj_coap_msg_t *msg) {
    uint8_t out_buff[120];
    size_t out_msg_size = 0;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
            msg, out_buff, sizeof(out_buff), &out_msg_size));
#define MSG_ID_OFFSET 2
    expected[MSG_ID_OFFSET] =
            (uint8_t) (msg->coap_binding_data.message_id >> 8);
    expected[MSG_ID_OFFSET + 1] = (uint8_t) msg->coap_binding_data.message_id;
#define TOKEN_OFFSET 4
    memcpy(&expected[TOKEN_OFFSET], msg->token.bytes, msg->token.size);

    ASSERT_EQ_BYTES_SIZED(out_buff, expected, expected_len);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, expected_len);
}

// Test: exchange API for non-confirmable send should call read_payload_handler
// once, and after sending the message, the exchange should be finished.
// Client LwM2M              |         Server LwM2M
// ------------------------------------------------
// Non-Confirmable SEND ---->
//
ANJ_UNIT_TEST(client_requests, non_confirmable_send) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "123";
    handlers_arg.out_payload_len = 3;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_INF_NON_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(handlers_arg.counter, 1);

    uint8_t expected[] = "\x58"         // NonConfirmable, tkl 8
                         "\x02\x00\x00" // POST 0x02, msg id
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb2\x64\x70"                     // uri path /dp
                         "\x11\x3C" // content_format: cbor
                         "\xFF"
                         "\x31\x32\x33";

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

// Test: exchange API for Confirmable send should call read_payload_handler
// once, and after sending the message and getting the ACK, the exchange should
// be finished.
// Client LwM2M          |   Server LwM2M
// --------------------------------------
// Confirmable SEND ---->
//                         <---- ACK 2.04
ANJ_UNIT_TEST(client_requests, confirmable_send) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "345";
    handlers_arg.out_payload_len = 3;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_INF_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CHANGED;
    response.payload_size = 0;
    handlers_arg.out_payload_len = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(handlers_arg.counter, 1);

    uint8_t expected[] = "\x48"         // Confirmable, tkl 8
                         "\x02\x00\x00" // POST 0x02, msg id
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb2\x64\x70"                     // uri path /dp
                         "\x11\x3C" // content_format: cbor
                         "\xFF"
                         "\x33\x34\x35";

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

static _anj_coap_msg_t process_send_response(_anj_exchange_ctx_t *ctx,
                                             _anj_coap_msg_t *msg) {
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_NONE, msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    _anj_coap_msg_t response = *msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CONTINUE;
    response.payload_size = 0;
    response.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    return response;
}

// Test: exchange API for Non-Confirmable send should call read_payload_handler
// three times, and after sending the last block and getting the ACK, the
// exchange should be finished. Due to block transfer the operation will be of
// confirmable type. Payload buffer len is set to 20, but its size should be
// reduced to 16. Message type should be changed to Confirmable. Atomic variant
// (server processes request after obtaining all blocks). Client LwM2M |  Server
// LwM2M
// -------------------------------------------------
// Confirmable SEND block1 0 more ---->
//                                    <---- ACK 2.31 block1 0 more
// Confirmable SEND block1 1 more ---->
//                                    <---- ACK 2.31 block1 1 more
// Confirmable SEND block1 2      ---->
//                                    <---- ACK 2.04 block1 2
ANJ_UNIT_TEST(client_requests, send_with_block_transfer_atomic) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    msg.operation = ANJ_OP_INF_NON_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x01"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x64\x70"                     // uri path /dp
            "\x11\x3C"                         // content_format: cbor
            "\xd1\x02\x08"                     // block1 0, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_send_response(&ctx, &msg);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected2[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x64\x70"                     // uri path /dp
            "\x11\x3C"                         // content_format: cbor
            "\xd1\x02\x18"                     // block1 1, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_send_response(&ctx, &msg);

    handlers_arg.out_payload_len = 8;
    handlers_arg.ret_val = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected3[] = "\x48"         // Confirmable, tkl 8
                          "\x02\x00\x00" // POST 0x02, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                          "\xb2\x64\x70"                     // uri path /dp
                          "\x11\x3C"     // content_format: cbor
                          "\xd1\x02\x20" // block1 2, size 16
                          "\xFF"
                          "\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    msg = process_send_response(&ctx, &msg);
    msg.msg_code = ANJ_COAP_CODE_CHANGED;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(handlers_arg.counter, 3);
}

// Test: exchange API for Non-Confirmable send should call read_payload_handler
// three times, and after sending the last block and getting the ACK, the
// exchange should be finished. Due to block transfer the operation will be of
// confirmable type. Payload buffer len is set to 20, but its size should be
// reduced to 16. Message type should be changed to Confirmable. Stateless
// variant (server processes each block separately). Client LwM2M |  Server
// LwM2M
// -------------------------------------------------
// Confirmable SEND block1 0 more ---->
//                                    <---- ACK 2.04 block1 0
// Confirmable SEND block1 1 more ---->
//                                    <---- ACK 2.04 block1 1
// Confirmable SEND block1 2      ---->
//                                    <---- ACK 2.04 block1 2
ANJ_UNIT_TEST(client_requests, send_with_block_transfer_process_stateless) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    msg.operation = ANJ_OP_INF_NON_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x01"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x64\x70"                     // uri path /dp
            "\x11\x3C"                         // content_format: cbor
            "\xd1\x02\x08"                     // block1 0, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_send_response(&ctx, &msg);
    msg.msg_code = ANJ_COAP_CODE_CHANGED;
    msg.block.more_flag = false;

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected2[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x64\x70"                     // uri path /dp
            "\x11\x3C"                         // content_format: cbor
            "\xd1\x02\x18"                     // block1 1, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_send_response(&ctx, &msg);
    msg.msg_code = ANJ_COAP_CODE_CHANGED;
    msg.block.more_flag = false;

    handlers_arg.out_payload_len = 8;
    handlers_arg.ret_val = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected3[] = "\x48"         // Confirmable, tkl 8
                          "\x02\x00\x00" // POST 0x02, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                          "\xb2\x64\x70"                     // uri path /dp
                          "\x11\x3C"     // content_format: cbor
                          "\xd1\x02\x20" // block1 2, size 16
                          "\xFF"
                          "\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    msg = process_send_response(&ctx, &msg);
    msg.msg_code = ANJ_COAP_CODE_CHANGED;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(handlers_arg.counter, 3);
}

// Note: Block1 size negotiation is not supported
// ANJ_UNIT_TEST(client_requests, send_with_block_transfer_size_negotiation) {
//     TEST_INIT();
//     _anj_exchange_handlers_t handlers = {
//         .read_payload = read_payload_handler,
//         .arg = &handlers_arg
//     };
//     handlers_arg.out_payload = "12345678123456781234567812345678";
//     handlers_arg.out_payload_len = 32;
//     handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
//     handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

//     msg.operation = ANJ_OP_INF_NON_CON_SEND;

//     ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers,
//     payload,
//                                                 40),
//                 ANJ_EXCHANGE_STATE_MSG_TO_SEND);
//     uint8_t expected[] =
//             "\x48"                             // Confirmable, tkl 8
//             "\x02\x00\x01"                     // POST 0x02, msg id
//             "\x00\x00\x00\x00\x00\x00\x00\x00" // token
//             "\xb2\x64\x70"                     // uri path /dp
//             "\x11\x3C"                         // content_format: cbor
//             "\xd1\x02\x09"                     // block1 0, size 32, more
//             "\xFF"
//             "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38"
//             "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
//     verify_payload(expected, sizeof(expected) - 1, &msg);
//     ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

//     msg = process_send_response(&ctx, &msg);
//     msg.block.size = 16; // server changed block size

//     ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
//                 ANJ_EXCHANGE_STATE_MSG_TO_SEND);
//     uint8_t expected2[] =
//             "\x48"                             // Confirmable, tkl 8
//             "\x02\x00\x00"                     // POST 0x02, msg id
//             "\x00\x00\x00\x00\x00\x00\x00\x00" // token
//             "\xb2\x64\x70"                     // uri path /dp
//             "\x11\x3C"                         // content_format: cbor
//             "\xd1\x02\x28"                     // block1 2, size 16, more
//             "\xFF"
//             "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38"
//             "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
//     verify_payload(expected2, sizeof(expected2) - 1, &msg);
//     ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

//     msg = process_send_response(&ctx, &msg);

//     handlers_arg.out_payload_len = 8;
//     handlers_arg.ret_val = 0;
//     ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
//                 ANJ_EXCHANGE_STATE_MSG_TO_SEND);
//     uint8_t expected3[] = "\x48"         // Confirmable, tkl 8
//                             "\x02\x00\x00" // POST 0x02, msg id
//                             "\x00\x00\x00\x00\x00\x00\x00\x00" // token
//                             "\xb2\x64\x70"                     // uri path
//                             /dp
//                             "\x11\x3C"     // content_format: cbor
//                             "\xd1\x02\x30" // block1 3, size 16
//                             "\xFF"
//                             "\x31\x32\x33\x34\x35\x36\x37\x38";
//     verify_payload(expected3, sizeof(expected3) - 1, &msg);

//     msg = process_send_response(&ctx, &msg);
//     msg.msg_code = ANJ_COAP_CODE_CHANGED;
//     ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
//                 ANJ_EXCHANGE_STATE_FINISHED);

//     ASSERT_EQ(handlers_arg.counter, 3);
// }

// Test: Send operation with block transfer and separate response.
// Separate response can be sent after each block transfer.
// Client LwM2M                      |  Server LwM2M
// -------------------------------------------------
// Confirmable SEND block1 0 more ---->
//                                    <---- Empty msg
//                                    <---- Con 2.31 block1 0 more
// Empty msg                      ---->
// Confirmable SEND block1 1 more ---->
//                                    <---- ACK 2.31 block1 1 more
// Confirmable SEND block1 2      ---->
//                                    <---- Empty msg
//                                    <---- Con 2.04 block1 2
// Empty msg                      ---->
ANJ_UNIT_TEST(client_requests, send_with_block_transfer_separate_response) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    msg.operation = ANJ_OP_INF_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    msg = process_send_response(&ctx, &msg);

    // empty_msg from server
    msg.operation = ANJ_OP_COAP_EMPTY_MSG;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_CONFIRMABLE;
    msg.operation = ANJ_OP_RESPONSE;
    // Con response with empty response
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(msg.operation, ANJ_OP_COAP_EMPTY_MSG);

    // next Confirmable Send
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x64\x70"                     // uri path /dp
            "\x11\x3C"                         // content_format: cbor
            "\xd1\x02\x18"                     // block1 1, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    // Server ACK and rest of the message
    handlers_arg.out_payload_len = 8;
    handlers_arg.ret_val = 0;
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.payload_size = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    msg = process_send_response(&ctx, &msg);
    msg.operation = ANJ_OP_COAP_EMPTY_MSG;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_CONFIRMABLE;
    msg.msg_code = ANJ_COAP_CODE_CHANGED;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    uint8_t empty[] = "\x60"          // ACK, tkl 0
                      "\x00\x00\x00"; // empty msg
    verify_payload(empty, sizeof(empty) - 1, &msg);
}

// Test: Confirmable SEND with block transfer during
// preparing the second block should be cancelled.
// Client LwM2M                       |  Server LwM2M
// --------------------------------------------------
// Confirmable SEND block1 0 more ---->
//                                    <---- ACK 2.31 block1 0 more
ANJ_UNIT_TEST(client_requests, send_with_read_payload_error) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    msg.operation = ANJ_OP_INF_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x64\x70"                     // uri path /dp
            "\x11\x3C"                         // content_format: cbor
            "\xd1\x02\x08"                     // block1 0, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_send_response(&ctx, &msg);
    handlers_arg.ret_val = -44;

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);
}

// Test: Confirmable SEND with block transfer should
// be cancelled because of Reset message.
// Client LwM2M                       |  Server LwM2M
// --------------------------------------------------
// Confirmable SEND block1 0 more ---->
//                                    <---- CoAP Reset
ANJ_UNIT_TEST(client_requests, send_with_server_reset) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
    msg.operation = ANJ_OP_INF_CON_SEND;
    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    msg = process_send_response(&ctx, &msg);
    msg.operation = ANJ_OP_COAP_RESET;

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);
}

// Test: Confirmable Send is processed, during waiting for ACK, the 2 new
// message are arriving. For first request we should response with
// ANJ_COAP_CODE_SERVICE_UNAVAILABLE, second message that is not a request
// should be ignored.
//
// Client LwM2M          |                Server LwM2M
// ---------------------------------------------------
// Confirmable SEND ---->
//                       <---- Confirmable Request
// ACK-5.03         ---->
//                       <---- Non-Confirmable Request
// ignore
//                       <---- ACK 2.04
ANJ_UNIT_TEST(client_requests, confirmable_send_with_interruptions) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "345";
    handlers_arg.out_payload_len = 3;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_INF_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    // first interruption, response for the request
    _anj_coap_msg_t server_request = {
        .operation = ANJ_OP_DM_READ,
        .msg_code = ANJ_COAP_CODE_GET,
        .payload_size = 0,
        .token = {
            .size = 1,
            .bytes = { 1 }
        },
        .coap_binding_data = {
            .type = ANJ_COAP_UDP_TYPE_CONFIRMABLE,
            .message_id = 0x3333,
        }
    };
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &server_request),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected_interrupt_response[] =
            "\x61"         // ACK, tkl 1
            "\xA3\x33\x33" // ANJ_COAP_CODE_SERVICE_UNAVAILABLE, msg id
            "\x01";        // token
    verify_payload(expected_interrupt_response,
                   sizeof(expected_interrupt_response) - 1, &server_request);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    // ignore non-request message
    _anj_coap_msg_t client_ack = {
        .operation = ANJ_OP_RESPONSE,
        .msg_code = ANJ_COAP_CODE_VALID,
        .payload_size = 0,
        .token = {
            .size = 1,
            .bytes = { 1 }
        },
        .coap_binding_data = {
            .type = ANJ_COAP_UDP_TYPE_CONFIRMABLE,
            .message_id = 0x3334,
        }
    };
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &client_ack),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CHANGED;
    response.payload_size = 0;
    handlers_arg.out_payload_len = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(handlers_arg.counter, 1);

    uint8_t expected[] = "\x48"         // Confirmable, tkl 8
                         "\x02\x00\x00" // POST 0x02, msg id
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb2\x64\x70"                     // uri path /dp
                         "\x11\x3C" // content_format: cbor
                         "\xFF"
                         "\x33\x34\x35";

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

// Test: Update operation without paylaoad
// defined.
// Client LwM2M          |         Server LwM2M
// --------------------------------------------
// UPDATE ---->
//                               <---- ACK 2.04
ANJ_UNIT_TEST(client_requests, update_operation_no_payload) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload_len = 0;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_UPDATE;
    msg.location_path.location[0] = "name";
    msg.location_path.location_len[0] = 4;
    msg.location_path.location_count = 1;
    msg.attr.register_attr.has_binding = true;
    msg.attr.register_attr.binding = "U";

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CHANGED;
    response.payload_size = 0;
    handlers_arg.out_payload_len = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(handlers_arg.counter, 1);

    uint8_t expected[] = "\x48"         // Confirmable, tkl 8
                         "\x02\x00\x00" // POST 0x02, msg id
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb4\x6e\x61\x6d\x65"             // uri path /name
                         "\x43\x62\x3d\x55";                // uri-query b=U

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

// Test: Update operation with separate response.
// Client LwM2M          |         Server LwM2M
// --------------------------------------------
// UPDATE ---->
//                               <---- Empty msg
//                               <---- Con 2.04
// Empty msg --->
ANJ_UNIT_TEST(client_requests, update_operation_separate_response) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg
    };
    msg.operation = ANJ_OP_UPDATE;
    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_COAP_EMPTY_MSG;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    response.operation = ANJ_OP_RESPONSE;
    response.msg_code = ANJ_COAP_CODE_CHANGED;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);

    uint8_t empty[] = "\x60"          // ACK, tkl 0
                      "\x00\x00\x00"; // empty msg
    verify_payload(empty, sizeof(empty) - 1, &response);
}

// Test: BootstrapPack-Request, only client request that uses
// write_payload_handler.
// Client LwM2M               |               Server LwM2M
// --------------------------------------------------------
// BootstrapPack-Request ---->
//                             <---- ACK 2.05 with payload
ANJ_UNIT_TEST(client_requests, bootstrap_pack_request) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler
    };
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_BOOTSTRAP_PACK_REQ;
    msg.accept = _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CONTENT;
    response.payload_size = 4;
    response.payload = (uint8_t *) "pack";

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(handlers_arg.counter, 1);
    ASSERT_EQ(handlers_arg.last_block, true);
    ASSERT_EQ(handlers_arg.buff_offset, 4);
    ASSERT_EQ_BYTES_SIZED(response.payload, handlers_arg.buff,
                          handlers_arg.buff_offset);

    uint8_t expected[] = "\x48"         // Confirmable, tkl 8
                         "\x01\x00\x00" // GET 0x01, msg id
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb6\x62\x73\x70\x61\x63\x6b"     // uri path /bspack
                         "\x62\x01\x42";                    // accept /ETCH_CBOR

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

static _anj_coap_msg_t process_bootstrap_pack_response(_anj_exchange_ctx_t *ctx,
                                                       _anj_coap_msg_t *msg,
                                                       size_t block_number,
                                                       bool more_flag) {
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    _anj_coap_msg_t response = *msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CONTENT;
    response.content_format = _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;
    response.accept = _ANJ_COAP_FORMAT_NOT_DEFINED;
    response.payload_size = 16;
    response.payload = (uint8_t *) "12345678123456781234567812345678";
    response.block.block_type = ANJ_OPTION_BLOCK_2;
    response.block.size = 16;
    response.block.number = block_number;
    response.block.more_flag = more_flag;
    return response;
}

// Test: BootstrapPack-Request with block transfer and multiple
// write_payload_handler calls.
// Client LwM2M               |                               Server LwM2M
// -----------------------------------------------------------------------
// BootstrapPack-Request ---->
//                             <---- ACK 2.05 with paylaoad block2 0, more
// GET block2 1 ---->
//                             <---- ACK 2.05 with paylaoad block2 1, more
// GET block2 2 ---->
//                             <---- ACK 2.05 with paylaoad block1 2
ANJ_UNIT_TEST(client_requests, bootstrap_pack_request_with_block_transfer) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler
    };
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_BOOTSTRAP_PACK_REQ;
    msg.accept = _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected[] = "\x48"         // Confirmable, tkl 8
                         "\x01\x00\x00" // GET 0x01, msg id
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb6\x62\x73\x70\x61\x63\x6b"     // uri path /bspack
                         "\x62\x01\x42";                    // accept /ETCH_CBOR
    verify_payload(expected, sizeof(expected) - 1, &msg);

    msg = process_bootstrap_pack_response(&ctx, &msg, 0, true);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    uint8_t expected2[] = "\x48"         // Confirmable, tkl 8
                          "\x01\x00\x00" // GET 0x01, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                          "\xb6\x62\x73\x70\x61\x63\x6b"     // uri path /bspack
                          "\x62\x01\x42" // accept /ETCH_CBOR
                          "\x61\x10";    // block2 1, size 16
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_bootstrap_pack_response(&ctx, &msg, 1, true);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected3[] = "\x48"         // Confirmable, tkl 8
                          "\x01\x00\x00" // GET 0x01, msg id
                          "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                          "\xb6\x62\x73\x70\x61\x63\x6b"     // uri path /bspack
                          "\x62\x01\x42" // accept /ETCH_CBOR
                          "\x61\x20";    // block2 2, size 16
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_bootstrap_pack_response(&ctx, &msg, 2, false);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(handlers_arg.counter, 3);
    ASSERT_EQ(handlers_arg.buff_offset, 48);
    ASSERT_EQ_BYTES_SIZED("1234567812345678123456781234567812345678123456781234"
                          "56781234567812345678123456781234567812345678",
                          handlers_arg.buff, handlers_arg.buff_offset);
}

// Test: Register operation with single message payload, and
// exchange_completion_handler.
// Client LwM2M          |         Server LwM2M
// --------------------------------------------
// REGISTER ---->
//                               <---- ACK 2.01
ANJ_UNIT_TEST(client_requests, register_operation) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload_len = 2;
    handlers_arg.out_payload = "12";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_LINK_FORMAT;
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_REGISTER;
    msg.attr.register_attr.has_endpoint = true;
    msg.attr.register_attr.has_lifetime = true;
    msg.attr.register_attr.endpoint = "name";
    msg.attr.register_attr.lifetime =
            anj_time_duration_new(120, ANJ_TIME_UNIT_S);

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CREATED;
    response.payload_size = 0;
    handlers_arg.out_payload_len = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(handlers_arg.counter, 1);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
    ASSERT_TRUE(handlers_arg.response == &response);

    uint8_t expected[] = "\x48"         // Confirmable, tkl 8
                         "\x02\x00\x00" // POST 0x02, msg id
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\xb2\x72\x64"                     // uri path /rd
                         "\x11\x28" // content_format: application/link-format
                         "\x37\x65\x70\x3d\x6e\x61\x6d\x65" // uri-query ep=name
                         "\x06\x6c\x74\x3d\x31\x32\x30"     // uri-query lt=120
                         "\xFF"
                         "\x31\x32";

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

// Test: Register operation with 4.00 in response.
// Client LwM2M          |         Server LwM2M
// --------------------------------------------
// REGISTER ---->
//                               <---- ACK 4.00
ANJ_UNIT_TEST(client_requests, register_with_cancel) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload_len = 2;
    handlers_arg.out_payload = "12";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_LINK_FORMAT;
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_REGISTER;
    msg.attr.register_attr.has_endpoint = true;
    msg.attr.register_attr.has_lifetime = true;
    msg.attr.register_attr.endpoint = "name";
    msg.attr.register_attr.lifetime =
            anj_time_duration_new(120, ANJ_TIME_UNIT_S);

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_BAD_REQUEST;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, _ANJ_EXCHANGE_ERROR_SERVER_RESPONSE);
    ASSERT_TRUE(handlers_arg.response == &response);
}

static _anj_coap_msg_t process_register_response(_anj_exchange_ctx_t *ctx,
                                                 _anj_coap_msg_t *msg) {
    // remove register attr to make sure that it is filled in the next request
    memset(&msg->attr.register_attr, 0, sizeof(msg->attr.register_attr));
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_NONE, msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);

    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_NONE, msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    _anj_coap_msg_t response = *msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CONTINUE;
    response.payload_size = 0;
    response.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    return response;
}
// Test: Register operation with block transfer - each message has to contain
// all register_attr options.
// Client LwM2M               |         Server LwM2M
// -------------------------------------------------
// REGISTER block1 0 more ---->
//                             <---- ACK 2.31 block1 0 more
// REGISTER block1 1 more ---->
//                             <---- ACK 2.31 block1 1 more
// REGISTER block1 2      ---->
//                             <---- ACK 2.01 block1 2
ANJ_UNIT_TEST(client_requests, register_operation_with_block) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "1111111122222222";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_LINK_FORMAT;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    msg.operation = ANJ_OP_REGISTER;
    msg.attr.register_attr.has_endpoint = true;
    msg.attr.register_attr.has_lifetime = true;
    msg.attr.register_attr.endpoint = "name";
    msg.attr.register_attr.lifetime =
            anj_time_duration_new(120, ANJ_TIME_UNIT_S);

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x72\x64"                     // uri path /rd
            "\x11\x28" // content_format: application/link-format
            "\x37\x65\x70\x3d\x6e\x61\x6d\x65" // uri-query ep=name
            "\x06\x6c\x74\x3d\x31\x32\x30"     // uri-query lt=120
            "\xc1\x08"                         // block1 0, size 16, more
            "\xFF"
            "\x31\x31\x31\x31\x31\x31\x31\x31\x32\x32\x32\x32\x32\x32\x32\x32";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_register_response(&ctx, &msg);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(handlers_arg.complete_counter, 0);
    uint8_t expected2[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x72\x64"                     // uri path /rd
            "\x11\x28" // content_format: application/link-format
            "\x37\x65\x70\x3d\x6e\x61\x6d\x65" // uri-query ep=name
            "\x06\x6c\x74\x3d\x31\x32\x30"     // uri-query lt=120
            "\xc1\x18"                         // block1 1, size 16, more
            "\xFF"
            "\x31\x31\x31\x31\x31\x31\x31\x31\x32\x32\x32\x32\x32\x32\x32\x32";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_register_response(&ctx, &msg);
    handlers_arg.ret_val = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(handlers_arg.complete_counter, 0);
    uint8_t expected3[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x72\x64"                     // uri path /rd
            "\x11\x28" // content_format: application/link-format
            "\x37\x65\x70\x3d\x6e\x61\x6d\x65" // uri-query ep=name
            "\x06\x6c\x74\x3d\x31\x32\x30"     // uri-query lt=120
            "\xc1\x20"                         // block1 2, size 16,
            "\xFF"
            "\x31\x31\x31\x31\x31\x31\x31\x31\x32\x32\x32\x32\x32\x32\x32\x32";
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    msg = process_register_response(&ctx, &msg);
    handlers_arg.ret_val = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
    ASSERT_TRUE(handlers_arg.response == &msg);
}

// Test: Update operation with sending timeout, which results in the closure of
// the exchange.
ANJ_UNIT_TEST(client_requests, update_operation_with_send_timeout) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .completion = exchange_completion_handler,
        .arg = &handlers_arg
    };

    msg.operation = ANJ_OP_UPDATE;
    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);

    anj_time_duration_t delta =
            anj_time_monotonic_diff(ctx.send_confirmation_timeout_timestamp,
                                    anj_time_monotonic_now());
    mock_time_advance(delta);
    mock_time_tick(ANJ_TIME_UNIT_S);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, _ANJ_EXCHANGE_ERROR_TIMEOUT);
}

// Test: Update operation with _anj_exchange_terminate call, which results in
// the closure of the exchange.
ANJ_UNIT_TEST(client_requests, update_operation_with_termination) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .completion = exchange_completion_handler,
        .arg = &handlers_arg
    };

    msg.operation = ANJ_OP_UPDATE;
    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    _anj_exchange_terminate(&ctx, _ANJ_EXCHANGE_ERROR_TERMINATED);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, _ANJ_EXCHANGE_ERROR_TERMINATED);
}

// Test: Update operation with retransmision.
ANJ_UNIT_TEST(client_requests, update_operation_with_2_retransmision) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg
    };
    msg.operation = ANJ_OP_UPDATE;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    for (int i = 0; i < 2; i++) {
        anj_time_duration_t delta =
                anj_time_monotonic_diff(ctx.timeout_timestamp,
                                        anj_time_monotonic_now());
        mock_time_advance(delta);
        mock_time_tick(ANJ_TIME_UNIT_S);

        ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
                  ANJ_EXCHANGE_STATE_MSG_TO_SEND);
        ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
                  ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
        ASSERT_EQ(_anj_exchange_process(
                          &ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
                  ANJ_EXCHANGE_STATE_WAITING_MSG);
    }
    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CHANGED;
    response.payload_size = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);
}

// Test: Update operation with retransmision and service unavailable interrupt.
ANJ_UNIT_TEST(
        client_requests,
        update_operation_with_2_retransmision_service_unavailable_interrupt) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    msg.operation = ANJ_OP_UPDATE;
    handlers_arg.result = 0;
    handlers_arg.out_payload_len = 2;
    handlers_arg.out_payload = "12";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    _anj_coap_msg_t first_reg = msg;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    // first retransmision
    anj_time_duration_t delta =
            anj_time_monotonic_diff(ctx.timeout_timestamp,
                                    anj_time_monotonic_now());
    mock_time_advance(delta);
    mock_time_tick(ANJ_TIME_UNIT_S);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    // server request to interrupt the exchange
    _anj_coap_msg_t server_req = {
        .operation = ANJ_OP_DM_DISCOVER,
        .payload_size = 0,
        .token = {
            .size = 2,
            .bytes = { 1, 2 }
        },
        .coap_binding_data = {
            .type = ANJ_COAP_UDP_TYPE_CONFIRMABLE,
            .message_id = 0x4444,
        }
    };
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &server_req),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(server_req.msg_code, ANJ_COAP_CODE_SERVICE_UNAVAILABLE);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    // second retransmision
    delta = anj_time_monotonic_diff(ctx.timeout_timestamp,
                                    anj_time_monotonic_now());
    mock_time_advance(delta);
    mock_time_tick(ANJ_TIME_UNIT_S);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    // we want to make sure that second retransmision is identical to the first
    // request, response with ANJ_COAP_CODE_SERVICE_UNAVAILABLE is empty so
    // payload shoud be the same
    uint8_t first_req_buff[40];
    size_t first_req_size;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(&first_reg, first_req_buff,
                                                 sizeof(first_req_buff),
                                                 &first_req_size));
    uint8_t final_req_buff[40];
    size_t final_req_size;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
            &msg, final_req_buff, sizeof(final_req_buff), &final_req_size));
    ASSERT_EQ(first_req_size, final_req_size);
    // msg id is different
    final_req_buff[2] = first_req_buff[2];
    final_req_buff[3] = first_req_buff[3];
    ASSERT_EQ_BYTES_SIZED(first_req_buff, final_req_buff, first_req_size);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CHANGED;
    response.payload_size = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);
}

// Test: Update operation with retransmision fail.
ANJ_UNIT_TEST(client_requests, update_operation_with_retransmision_fail) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg
    };
    msg.operation = ANJ_OP_UPDATE;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    _anj_coap_msg_t base_msg = msg;
    uint8_t req_buff[40];
    size_t req_size;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(&base_msg, req_buff,
                                                 sizeof(req_buff), &req_size));

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    for (int i = 0; i < 4; i++) {
        anj_time_duration_t delta =
                anj_time_monotonic_diff(ctx.timeout_timestamp,
                                        anj_time_monotonic_now());
        mock_time_advance(delta);
        mock_time_tick(ANJ_TIME_UNIT_S);

        ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
                  ANJ_EXCHANGE_STATE_MSG_TO_SEND);
        ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
                  ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
        ASSERT_EQ(_anj_exchange_process(
                          &ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
                  ANJ_EXCHANGE_STATE_WAITING_MSG);
        // make sure that retransmisition is the same as the first request
        uint8_t final_req_buff[40];
        size_t final_req_size;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
                &msg, final_req_buff, sizeof(final_req_buff), &final_req_size));
        ASSERT_EQ(req_size, final_req_size);
        ASSERT_EQ_BYTES_SIZED(req_buff, final_req_buff, req_size);
    }
    anj_time_duration_t delta =
            anj_time_monotonic_diff(ctx.timeout_timestamp,
                                    anj_time_monotonic_now());
    mock_time_advance(delta);
    mock_time_tick(ANJ_TIME_UNIT_S);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);
}

// Test: exchange API for non-confirmable notify should call
// read_payload_handler once, and after sending the message, the exchange should
// be finished. Client LwM2M              |         Server LwM2M
// ------------------------------------------------
// Non-Confirmable Notify ---->
//
ANJ_UNIT_TEST(client_requests, non_confirmable_notify) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "123";
    handlers_arg.out_payload_len = 3;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_INF_NON_CON_NOTIFY;
    _anj_coap_token_t *token = &msg.token;
    token->size = 8;
    msg.observe_number = 1;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(handlers_arg.counter, 1);

    uint8_t expected[] = "\x58"         // NonConfirmable, tkl 8
                         "\x45\x00\x00" // Content, msg id
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\x61\x01"                         // observe 1
                         "\x61\x3C" // content_format: cbor
                         "\xFF"
                         "\x31\x32\x33";

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

// Test: exchange API for non-confirmable notify should call
// read_payload_handler once, and after sending the message, the exchange should
// be finished. Client LwM2M      |         Server LwM2M
// ------------------------------------------------
// Confirmable Notify ---->
//                          <---- Empty message
ANJ_UNIT_TEST(client_requests, confirmable_notify) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "123";
    handlers_arg.out_payload_len = 3;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_INF_CON_NOTIFY;
    _anj_coap_token_t *token = &msg.token;
    token->size = 8;
    msg.observe_number = 1;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_COAP_EMPTY_MSG;
    token = &response.token;
    token->size = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(handlers_arg.counter, 1);

    uint8_t expected[] = "\x48"         // Confirmable, tkl 8
                         "\x45\x00\x00" // Content, msg id
                         "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                         "\x61\x01"                         // observe 1
                         "\x61\x3C" // content_format: cbor
                         "\xFF"
                         "\x31\x32\x33";

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

// Test: Send operation with block transfer and separate response.
// After first Empty msg, server try to send new request.
// Client LwM2M                      |  Server LwM2M
// -------------------------------------------------
// Confirmable SEND block1 0 more ---->
//                                    <---- Empty msg
//                                    <---- Confirmable Request
// ACK-5.03                       ---->
//                                    <---- Con 2.31 block1 0 more
// Empty msg                      ---->
// Confirmable SEND block1 1 more ---->
//                                    <---- ACK 2.31 block1 1 more
// Confirmable SEND block1 2      ---->
//                                    <---- Empty msg
//                                    <---- Con 2.04 block1 2
// Empty msg                      ---->
ANJ_UNIT_TEST(client_requests,
              send_with_block_transfer_separate_response_and_interruption) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    msg.operation = ANJ_OP_INF_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    msg = process_send_response(&ctx, &msg);

    // empty_msg from server
    msg.operation = ANJ_OP_COAP_EMPTY_MSG;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    // additional request from server
    _anj_coap_msg_t new_req = msg;
    new_req.operation = ANJ_OP_DM_READ;
    new_req.token.size = 1;
    new_req.coap_binding_data.message_id = 0x3333;
    new_req.token.bytes[0] = 1;
    new_req.msg_code = ANJ_COAP_CODE_GET;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &new_req),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected_ack[] =
            "\x61"         // ACK, tkl 1
            "\xA3\x33\x33" // ANJ_COAP_CODE_SERVICE_UNAVAILABLE, msg id
            "\x01";        // token
    verify_payload(expected_ack, sizeof(expected_ack) - 1, &new_req);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &new_req),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    // Con response with empty response
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_CONFIRMABLE;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(msg.operation, ANJ_OP_COAP_EMPTY_MSG);

    // next Confirmable Send
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x64\x70"                     // uri path /dp
            "\x11\x3C"                         // content_format: cbor
            "\xd1\x02\x18"                     // block1 1, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    // Server ACK and rest of the message
    handlers_arg.out_payload_len = 8;
    handlers_arg.ret_val = 0;
    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.payload_size = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    msg = process_send_response(&ctx, &msg);
    msg.operation = ANJ_OP_COAP_EMPTY_MSG;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_CONFIRMABLE;
    msg.msg_code = ANJ_COAP_CODE_CHANGED;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    uint8_t empty[] = "\x60"          // ACK, tkl 0
                      "\x00\x00\x00"; // empty msg
    verify_payload(empty, sizeof(empty) - 1, &msg);
}

ANJ_UNIT_TEST(client_requests, set_udp_tx_params) {
    _anj_exchange_ctx_t ctx;
    _anj_exchange_init(&ctx);
    anj_exchange_udp_tx_params_t default_params =
            ANJ_EXCHANGE_UDP_TX_PARAMS_DEFAULT;
    ASSERT_TRUE(anj_time_duration_eq(ctx.tx_params.ack_timeout,
                                     default_params.ack_timeout));
    ASSERT_EQ(ctx.tx_params.ack_random_factor,
              default_params.ack_random_factor);
    ASSERT_EQ(ctx.tx_params.max_retransmit, default_params.max_retransmit);

    anj_exchange_udp_tx_params_t test_params = {
        .ack_random_factor = 10,
        .ack_timeout = anj_time_duration_new(1100, ANJ_TIME_UNIT_MS),
        .max_retransmit = 12
    };
    // random factor must be >= 1
    anj_exchange_udp_tx_params_t err_params = {
        .ack_random_factor = 0.1,
        .ack_timeout = anj_time_duration_new(11111, ANJ_TIME_UNIT_MS),
        .max_retransmit = 111
    };
    ASSERT_OK(_anj_exchange_set_udp_tx_params(&ctx, &test_params));
    ASSERT_FAIL(_anj_exchange_set_udp_tx_params(&ctx, &err_params));
    ASSERT_TRUE(anj_time_duration_eq(ctx.tx_params.ack_timeout,
                                     test_params.ack_timeout));
    ASSERT_EQ(ctx.tx_params.ack_random_factor, test_params.ack_random_factor);
    ASSERT_EQ(ctx.tx_params.max_retransmit, test_params.max_retransmit);
}

// Test: Register operation with block transfer number mismatch.
// Client LwM2M               |         Server LwM2M
// -------------------------------------------------
// REGISTER block1 0 more ---->
//                             <---- ACK 2.31 block1 2 more
//                             <---- ACK 2.31 block1 0 more
// REGISTER block1 1      ---->
//                             <---- ACK 2.01 block1 1

ANJ_UNIT_TEST(client_requests, register_with_block_number_mismatch) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "1111111122222222";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_LINK_FORMAT;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    msg.operation = ANJ_OP_REGISTER;
    msg.attr.register_attr.has_endpoint = true;
    msg.attr.register_attr.has_lifetime = true;
    msg.attr.register_attr.endpoint = "name";
    msg.attr.register_attr.lifetime =
            anj_time_duration_new(120, ANJ_TIME_UNIT_S);

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);
    ASSERT_EQ(_anj_exchange_get_state(&ctx),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);

    msg = process_register_response(&ctx, &msg);
    msg.block.number = 2;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(handlers_arg.complete_counter, 0);
    ASSERT_EQ(handlers_arg.result, 0);
    msg.block.number = 0;
    handlers_arg.ret_val = 0;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
}

ANJ_UNIT_TEST(client_requests, bootstrap_pack_request_write_payload_error) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.ret_val = 0;

    msg.operation = ANJ_OP_BOOTSTRAP_PACK_REQ;
    msg.accept = _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.msg_code = ANJ_COAP_CODE_CONTENT;
    response.payload_size = 4;
    response.payload = (uint8_t *) "pack";

    handlers_arg.ret_val = ANJ_COAP_CODE_NOT_ACCEPTABLE;

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    _anj_exchange_terminate(&ctx, _ANJ_EXCHANGE_ERROR_TERMINATED);
    ASSERT_EQ(handlers_arg.complete_counter, 1);

    ASSERT_EQ(handlers_arg.result, _ANJ_EXCHANGE_ERROR_REQUEST);
}

// Test: SEND operation with read_payload error, no message should be sent.
ANJ_UNIT_TEST(client_requests, send_read_payload_error) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler,
        .arg = &handlers_arg
    };
    handlers_arg.ret_val = ANJ_COAP_CODE_BAD_REQUEST;
    msg.operation = ANJ_OP_INF_NON_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(handlers_arg.counter, 1);
    ASSERT_EQ(handlers_arg.result, _ANJ_EXCHANGE_ERROR_REQUEST);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
}

// Test: Register operation with separate response.
// Client LwM2M          |         Server LwM2M
// --------------------------------------------
// REGISTER ---->
//                               <---- Empty msg
//                               <---- Con 2.01
// Empty msg --->
static char location_path[10];
static char register_response[] = "\x48"         // header v 0x01, Con, tkl 8
                                  "\x41\x00\x00" // CREATED code 2.1
                                  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" // token
                                  "\x82\x72\x64"; // location-path /rd

static void register_completion_handler(void *arg_ptr,
                                        const _anj_coap_msg_t *response,
                                        int result) {
    handlers_arg_t *handlers_arg = (handlers_arg_t *) arg_ptr;
    handlers_arg->response = response;
    handlers_arg->result = result;
    handlers_arg->complete_counter++;
    memcpy(location_path,
           response->location_path.location[0],
           response->location_path.location_len[0]);
}

ANJ_UNIT_TEST(client_requests, register_operation_separate_response) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .completion = register_completion_handler
    };
    msg.operation = ANJ_OP_REGISTER;
    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t ack = msg;
    ack.operation = ANJ_OP_COAP_EMPTY_MSG;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &ack),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response;
    // copy msg id and token
    register_response[2] = msg.coap_binding_data.message_id >> 8;
    register_response[3] = msg.coap_binding_data.message_id & 0xFF;
    memcpy(&register_response[4], msg.token.bytes, msg.token.size);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_decode_udp((uint8_t *) register_response,
                                                 sizeof(register_response) - 1,
                                                 &response));
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);

    uint8_t empty[] = "\x60"          // ACK, tkl 0
                      "\x00\x00\x00"; // empty msg
    verify_payload(empty, sizeof(empty) - 1, &response);

    ANJ_UNIT_ASSERT_EQUAL(handlers_arg.complete_counter, 1);
    ANJ_UNIT_ASSERT_EQUAL(handlers_arg.result, 0);
    ANJ_UNIT_ASSERT_EQUAL(location_path[0], 'r');
    ANJ_UNIT_ASSERT_EQUAL(location_path[1], 'd');
    ANJ_UNIT_ASSERT_EQUAL(location_path[2], '\0');
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), false);
}

// Test: Update operation with separate response, but empty message is not
// received.
// Client LwM2M          |         Server LwM2M
// --------------------------------------------
// UPDATE ---->
//                               <---- Con 2.04
// Empty msg --->
ANJ_UNIT_TEST(client_requests,
              update_operation_separate_response_lack_of_empty_msg) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg
    };
    msg.operation = ANJ_OP_UPDATE;
    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.msg_code = ANJ_COAP_CODE_CHANGED;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);

    uint8_t empty[] = "\x60"          // ACK, tkl 0
                      "\x00\x00\x00"; // empty msg
    verify_payload(empty, sizeof(empty) - 1, &response);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), false);
}

// Test: Send operation with block transfer and separate response.
// Empty message is received after Con response.
// Client LwM2M                      |  Server LwM2M
// -------------------------------------------------
// Confirmable SEND block1 0 more ---->
//                                    <---- Con 2.31 block1 0 more
// Empty msg                      ---->
//                                    <---- Empty msg
// Confirmable SEND block1 1      ---->
//                                    <---- ACK 2.04 block1 1
ANJ_UNIT_TEST(client_requests,
              send_with_block_transfer_separate_response_wrong_order) {
    TEST_INIT();
    _anj_exchange_handlers_t handlers = {
        .read_payload = read_payload_handler,
        .arg = &handlers_arg
    };
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    msg.operation = ANJ_OP_INF_CON_SEND;

    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               20),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    handlers_arg.ret_val = 0;
    _anj_coap_msg_t first_send = msg;
    msg = process_send_response(&ctx, &msg);

    msg.coap_binding_data.type = ANJ_COAP_UDP_TYPE_CONFIRMABLE;
    // CON response - send empty message
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    // check that empty message is sent
    uint8_t empty[] = "\x60"          // ACK, tkl 0
                      "\x00\x00\x00"; // empty msg
    verify_payload(empty, sizeof(empty) - 1, &msg);

    // right after empty message, send next block
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected[] =
            "\x48"                             // Confirmable, tkl 8
            "\x02\x00\x00"                     // POST 0x02, msg id
            "\x00\x00\x00\x00\x00\x00\x00\x00" // token
            "\xb2\x64\x70"                     // uri path /dp
            "\x11\x3C"                         // content_format: cbor
            "\xd1\x02\x10"                     // block1 1, size 16
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    // We don't check for incoming messages until here, so now we get empty
    // message from previous block, we should ignore it.
    first_send.operation = ANJ_OP_COAP_EMPTY_MSG;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &first_send),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    // Now ACK response will end the exchange.
    _anj_coap_msg_t response = msg;
    response.operation = ANJ_OP_RESPONSE;
    response.coap_binding_data.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    response.msg_code = ANJ_COAP_CODE_CHANGED;
    response.payload_size = 0;
    response.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &response),
              ANJ_EXCHANGE_STATE_FINISHED);
}
