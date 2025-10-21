/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <assert.h>
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
    int read_counter;
    // write_payload_handler
    uint8_t buff[100];
    size_t buff_offset;
    bool last_block;
    int write_counter;
    uint8_t write_ret_val;
    // exchange_completion_handler
    const _anj_coap_msg_t *response;
    uint8_t result;
    int complete_counter;

    uint8_t ret_val;
} handlers_arg_t;

static uint8_t payload[20];

static uint8_t read_payload_handler(void *arg_ptr,
                                    uint8_t *buff,
                                    size_t buff_len,
                                    _anj_exchange_read_result_t *out_params) {
    handlers_arg_t *handlers_arg = (handlers_arg_t *) arg_ptr;
    out_params->payload_len = handlers_arg->out_payload_len;
    out_params->format = handlers_arg->out_format;
    if (handlers_arg->out_payload_len) {
        memcpy(buff, handlers_arg->out_payload, handlers_arg->out_payload_len);
    }
    handlers_arg->read_counter++;
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
    handlers_arg->write_counter++;
    return handlers_arg->write_ret_val;
}

static void exchange_completion_handler(void *arg_ptr,
                                        const _anj_coap_msg_t *response,
                                        int result) {
    handlers_arg_t *handlers_arg = (handlers_arg_t *) arg_ptr;
    handlers_arg->response = response;
    handlers_arg->result = result;
    handlers_arg->complete_counter++;
    // for server requests, response is always NULL
    assert(!response);
}

static void
verify_payload(uint8_t *expected, size_t expected_len, _anj_coap_msg_t *msg) {
    uint8_t out_buff[120];
    size_t out_msg_size = 0;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
            msg, out_buff, sizeof(out_buff), &out_msg_size));
    ASSERT_EQ_BYTES_SIZED(out_buff, expected, expected_len);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, expected_len);
}

#define TEST_INIT(Op, Result_code, Expected_state, With_payload,        \
                  With_block_write)                                     \
    _anj_coap_msg_t msg = {                                             \
        .operation = Op,                                                \
        .token = {                                                      \
            .size = 1,                                                  \
            .bytes = { 1 }                                              \
        },                                                              \
        .coap_binding_data = {                                          \
            .udp = {                                                    \
                .type = ANJ_COAP_UDP_TYPE_CONFIRMABLE,                  \
                .message_id = 0x3333,                                   \
            }                                                           \
        },                                                              \
        .uri = {                                                        \
            .uri_len = 1,                                               \
            .ids = { 1 }                                                \
        },                                                              \
        .payload = (uint8_t *) "1234567812345678",                      \
        .payload_size = With_payload ? 16 : 0,                          \
        .content_format = With_payload ? _ANJ_COAP_FORMAT_CBOR          \
                                       : _ANJ_COAP_FORMAT_NOT_DEFINED   \
    };                                                                  \
    if (With_block_write) {                                             \
        msg.block = (_anj_block_t) {                                    \
            .block_type = ANJ_OPTION_BLOCK_1,                           \
            .number = 0,                                                \
            .size = 16,                                                 \
            .more_flag = true                                           \
        };                                                              \
    }                                                                   \
    _anj_exchange_ctx_t ctx;                                            \
    _anj_exchange_init(&ctx);                                           \
    ASSERT_EQ(_anj_exchange_new_server_request(&ctx, Result_code, &msg, \
                                               &handlers, payload,      \
                                               sizeof(payload)),        \
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

// Test: Server sends Execute request.
// Server LwM2M              |      Client LwM2M
// ---------------------------------------------
// Confirmable Execute  ---->
//                            <---- 2.04 Changed
ANJ_UNIT_TEST(server_requests, execute_with_handlers) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 0;
    handlers_arg.ret_val = 0;
    TEST_INIT(ANJ_OP_DM_EXECUTE, ANJ_COAP_CODE_CHANGED,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, false, false);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    uint8_t expected[] = "\x61"          // ACK, tkl 1
                         "\x44"          // CHANGED
                         "\x33\x33\x01"; // msg id, token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(handlers_arg.read_counter, 1);
    ASSERT_EQ(handlers_arg.write_counter, 0);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
}

// Test: Read operation with single response.
// Server LwM2M         |           Client LwM2M
// ---------------------------------------------
// READ            ---->
//                       <----     2.05 Content
ANJ_UNIT_TEST(server_requests, read_operation) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 3;
    handlers_arg.out_payload = "123";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = 0;
    TEST_INIT(ANJ_OP_DM_READ, ANJ_COAP_CODE_CONTENT,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, false, false);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    uint8_t expected[] = "\x61"         // ACK, tkl 1
                         "\x45"         // Content
                         "\x33\x33\x01" // msg id, token
                         "\xC1\x3C"     // content_format: cbor
                         "\xFF"
                         "\x31\x32\x33";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(handlers_arg.read_counter, 1);
    ASSERT_EQ(handlers_arg.write_counter, 0);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
}

// Test: Read operation with single response and error returned from
// read_payload_handler.
// Server LwM2M         |           Client LwM2M
// ---------------------------------------------
// READ            ---->
//                       <---- 4.01 Unauthorized
ANJ_UNIT_TEST(server_requests, read_with_error) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 3;
    handlers_arg.out_payload = "123";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = ANJ_COAP_CODE_UNAUTHORIZED;
    TEST_INIT(ANJ_OP_DM_READ, ANJ_COAP_CODE_UNAUTHORIZED,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, false, false);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    uint8_t expected[] = "\x61"          // ACK, tkl 1
                         "\x81"          // Unauthorized
                         "\x33\x33\x01"; // msg id, token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(handlers_arg.read_counter, 0);
    ASSERT_EQ(handlers_arg.write_counter, 0);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, _ANJ_EXCHANGE_ERROR_REQUEST);
}

static _anj_coap_msg_t process_block_read(_anj_exchange_ctx_t *ctx,
                                          _anj_op_t op,
                                          bool block_transfer,
                                          uint8_t block_num,
                                          uint16_t msg_id) {
    _anj_coap_msg_t msg = {
        .operation = op,
        .token = {
            .size = 1,
            .bytes = { 2 }
        },
        .coap_binding_data = {
            .udp = {
                .type = ANJ_COAP_UDP_TYPE_CONFIRMABLE,
                .message_id = msg_id,
            }
        },
        .block = {
            .block_type = ANJ_OPTION_BLOCK_2,
            .number = block_num,
            .size = 16,
        },
        .uri = {
            .uri_len = 1,
            .ids = { 1 }
        }
    };
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(ctx), true);

    if (!block_transfer) {
        ASSERT_EQ(_anj_exchange_process(
                          ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
                  ANJ_EXCHANGE_STATE_FINISHED);
    }
    return msg;
}

// Test: Read operation with block response.
// Server LwM2M         |                    Client LwM2M
// ------------------------------------------------------
// READ            ---->
//                       <---- 2.05 Content block2 0 more
// READ block2 1   ---->
//                       <---- 2.05 Content block2 1 more
// READ block2 2   ---->
//                       <---- 2.05 Content block2 2
ANJ_UNIT_TEST(server_requests, read_operation_with_block) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
    TEST_INIT(ANJ_OP_DM_READ, ANJ_COAP_CODE_CONTENT,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, false, false);

    uint8_t expected[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x33\x33\x01" // msg id, token
            "\xC1\x3C"     // content_format: cbor
            "\xB1\x08"     // block2 0, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);

    msg = process_block_read(&ctx, ANJ_OP_DM_READ, true, 1, 0x2222);
    uint8_t expected2[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x22\x22\x02" // msg id, token
            "\xC1\x3C"     // content_format: cbor
            "\xB1\x18"     // block2 1, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    handlers_arg.ret_val = 0;
    msg = process_block_read(&ctx, ANJ_OP_DM_READ, false, 2, 0x2223);
    uint8_t expected3[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x22\x23\x02" // msg id, token
            "\xC1\x3C"     // content_format: cbor
            "\xB1\x20"     // block2 2, size 16
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    ASSERT_EQ(handlers_arg.read_counter, 3);
    ASSERT_EQ(handlers_arg.write_counter, 0);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
}

// Test: Observe operation with block response. The response is similar to the
// Read operation, but with the Observe option.
// Server LwM2M         |                    Client LwM2M
// ------------------------------------------------------
// Observe          ---->
//                       <---- 2.05 Content block2 0 more
// Observe block2 1 ---->
//                       <---- 2.05 Content block2 1
ANJ_UNIT_TEST(server_requests, observe_operation_with_block) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
    TEST_INIT(ANJ_OP_INF_OBSERVE, ANJ_COAP_CODE_CONTENT,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, false, false);

    uint8_t expected[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x33\x33\x01" // msg id, token
            "\x60"         // observe 0
            "\x61\x3C"     // content_format: cbor
            "\xB1\x08"     // block2 0, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    handlers_arg.ret_val = 0;
    msg = process_block_read(&ctx, ANJ_OP_INF_OBSERVE, false, 1, 0x2222);
    uint8_t expected2[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x22\x22\x02" // msg id, token
            "\x60"         // observe 0
            "\x61\x3C"     // content_format: cbor
            "\xB1\x10"     // block2 1, size 16
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    ASSERT_EQ(handlers_arg.read_counter, 2);
    ASSERT_EQ(handlers_arg.write_counter, 0);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
}

// Test: Read operation with block response, interrupted by error.
// Server LwM2M         |                    Client LwM2M
// ------------------------------------------------------
// READ            ---->
//                       <---- 2.05 Content block2 0 more
// READ block2 1   ---->
//                       <---- 4.01 Unauthorized
ANJ_UNIT_TEST(server_requests, read_operation_with_block_and_error) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
    TEST_INIT(ANJ_OP_DM_READ, ANJ_COAP_CODE_CONTENT,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, false, false);

    uint8_t expected[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x33\x33\x01" // msg id, token
            "\xC1\x3C"     // content_format: cbor
            "\xB1\x08"     // block2 0, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);

    handlers_arg.ret_val = ANJ_COAP_CODE_UNAUTHORIZED;
    msg = process_block_read(&ctx, ANJ_OP_DM_READ, false, 1, 0x2222);
    uint8_t expected2[] = "\x61"          // ACK, tkl 1
                          "\x81"          // Unauthorized
                          "\x22\x22\x02"; // msg id, token
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    ASSERT_EQ(handlers_arg.read_counter, 2);
    ASSERT_EQ(handlers_arg.write_counter, 0);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, _ANJ_EXCHANGE_ERROR_REQUEST);
}

// Test: Single Write operation.
// Server LwM2M         |           Client LwM2M
// ---------------------------------------------
// Write          ---->
//                       <----     2.04 Changed
ANJ_UNIT_TEST(server_requests, write_operation) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.ret_val = 0;
    TEST_INIT(ANJ_OP_DM_WRITE_REPLACE, ANJ_COAP_CODE_CHANGED,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, true, false);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    uint8_t expected[] = "\x61"          // ACK, tkl 1
                         "\x44"          // Changed
                         "\x33\x33\x01"; // msg id, token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(handlers_arg.read_counter, 1);
    ASSERT_EQ(handlers_arg.write_counter, 1);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
    ASSERT_EQ(handlers_arg.buff_offset, 16);
    ASSERT_EQ(handlers_arg.last_block, true);
    ASSERT_EQ_BYTES_SIZED(msg.payload, handlers_arg.buff,
                          handlers_arg.buff_offset);
}

static _anj_coap_msg_t process_block_write(_anj_exchange_ctx_t *ctx,
                                           bool block_transfer,
                                           uint8_t block_num,
                                           uint16_t msg_id,
                                           uint8_t token) {
    _anj_coap_msg_t msg = {
        .operation = ANJ_OP_DM_WRITE_REPLACE,
        .token = {
            .size = 1,
            .bytes = { token }
        },
        .coap_binding_data = {
            .udp = {
                .type = ANJ_COAP_UDP_TYPE_CONFIRMABLE,
                .message_id = msg_id,
            }
        },
        .block = {
            .block_type = ANJ_OPTION_BLOCK_1,
            .number = block_num,
            .size = 16,
            .more_flag = block_transfer
        },
        .content_format = _ANJ_COAP_FORMAT_CBOR,
        .uri = {
            .uri_len = 1,
            .ids = { 1 }
        },
        .payload = (uint8_t *) "1111111122222222",
        .payload_size = 16
    };
    // condition added for write_operation_with_block_and_interruption test case
    if (ctx->state == ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION) {
        ASSERT_EQ(_anj_exchange_process(
                          ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
                  ANJ_EXCHANGE_STATE_WAITING_MSG);
    }
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    if (!block_transfer) {
        ASSERT_EQ(_anj_exchange_process(
                          ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
                  ANJ_EXCHANGE_STATE_FINISHED);
    }
    return msg;
}

// Test: Write operation with block transfer.
// Server LwM2M             |                    Client LwM2M
// ----------------------------------------------------------
// WRITE block1 0 more ---->
//                          <---- 2.05 Continue block1 0 more
// WRITE block1 1 more ---->
//                          <---- 2.05 Continue block1 1 more
// WRITE block1 2      ---->
//                          <---- 2.05 Changed block1 2
ANJ_UNIT_TEST(server_requests, write_operation_with_block) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.ret_val = 0;
    TEST_INIT(ANJ_OP_DM_WRITE_REPLACE, ANJ_COAP_CODE_CHANGED,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, true, true);
    uint8_t expected[] = "\x61"          // ACK, tkl 1
                         "\x5F"          // Continue
                         "\x33\x33\x01"  // msg id, token
                         "\xd1\x0e\x08"; // block1 0 more
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(handlers_arg.last_block, false);

    msg = process_block_write(&ctx, true, 1, 0x2222, 2);
    uint8_t expected2[] = "\x61"          // ACK, tkl 1
                          "\x5F"          // Continue
                          "\x22\x22\x02"  // msg id, token
                          "\xd1\x0e\x18"; // block1 1 more
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
    ASSERT_EQ(handlers_arg.last_block, false);

    msg = process_block_write(&ctx, false, 2, 0x2223, 2);
    uint8_t expected3[] = "\x61"          // ACK, tkl 1
                          "\x44"          // Changed
                          "\x22\x23\x02"  // msg id, token
                          "\xd1\x0e\x20"; // block1 2
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    ASSERT_EQ(handlers_arg.read_counter, 1);
    ASSERT_EQ(handlers_arg.write_counter, 3);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
    ASSERT_EQ(handlers_arg.buff_offset, 48);
    ASSERT_EQ(handlers_arg.last_block, true);
    ASSERT_EQ_BYTES_SIZED(handlers_arg.buff,
                          "123456781234567811111111222222221111111122222222",
                          handlers_arg.buff_offset);
}

// Test: During write operation server sends Reset message,
// and we should ignore it.
// Server LwM2M             |                    Client LwM2M
// ----------------------------------------------------------
// WRITE block1 0 more ---->
//                          <---- 2.05 Continue block1 0 more
// Reset message       ---->
ANJ_UNIT_TEST(server_requests, write_operation_with_server_termintation) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.ret_val = 0;
    TEST_INIT(ANJ_OP_DM_WRITE_REPLACE, ANJ_COAP_CODE_CHANGED,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, true, true);
    uint8_t expected[] = "\x61"          // ACK, tkl 1
                         "\x5F"          // Continue
                         "\x33\x33\x01"  // msg id, token
                         "\xd1\x0e\x08"; // block1 0 more
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(handlers_arg.last_block, false);

    _anj_coap_msg_t reset_msg = {
        .operation = ANJ_OP_COAP_RESET,
        .token = {
            .size = 0,
        },
        .coap_binding_data = {
            .udp = {
                .message_id = 0x0000,
            }
        }
    };
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &reset_msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    ASSERT_EQ(handlers_arg.read_counter, 0);
    ASSERT_EQ(handlers_arg.write_counter, 1);
    ASSERT_EQ(handlers_arg.complete_counter, 0);
}

// Test: Write operation with block transfer, and interruption in the middle of
// the transfer.
// Server LwM2M             |                    Client LwM2M
// ----------------------------------------------------------
// WRITE block1 0 more ---->
//                          <---- 2.05 Continue block1 0 more
// WRITE block1 1 more ---->
//                          <---- 2.05 Continue block1 1 more
// ACK 2.05            ----> (ignored)
// WRITE block1 2      ---->
//                          <---- 2.05 Changed block1 2
ANJ_UNIT_TEST(server_requests, write_operation_with_block_and_interruption) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.ret_val = 0;
    TEST_INIT(ANJ_OP_DM_WRITE_REPLACE, ANJ_COAP_CODE_CHANGED,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, true, true);
    uint8_t expected[] = "\x61"          // ACK, tkl 1
                         "\x5F"          // Continue
                         "\x33\x33\x01"  // msg id, token
                         "\xd1\x0e\x08"; // block1 0 more
    verify_payload(expected, sizeof(expected) - 1, &msg);

    msg = process_block_write(&ctx, true, 1, 0x2222, 2);
    uint8_t expected2[] = "\x61"          // ACK, tkl 1
                          "\x5F"          // Continue
                          "\x22\x22\x02"  // msg id, token
                          "\xd1\x0e\x18"; // block1 1 more
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    _anj_coap_msg_t ack_to_ignore = {
        .operation = ANJ_OP_RESPONSE,
        .token = {
            .size = 1,
            .bytes = { 5 }
        },
        .coap_binding_data = {
            .udp = {
                .message_id = 0x5555,
            }
        },
    };
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &ack_to_ignore),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(&ctx), true);

    msg = process_block_write(&ctx, false, 2, 0x2223, 2);
    uint8_t expected3[] = "\x61"          // ACK, tkl 1
                          "\x44"          // Changed
                          "\x22\x23\x02"  // msg id, token
                          "\xd1\x0e\x20"; // block1 2
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    ASSERT_EQ(handlers_arg.read_counter, 1);
    ASSERT_EQ(handlers_arg.write_counter, 3);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
    ASSERT_EQ(handlers_arg.buff_offset, 48);
    ASSERT_EQ(handlers_arg.last_block, true);
    ASSERT_EQ_BYTES_SIZED(handlers_arg.buff,
                          "123456781234567811111111222222221111111122222222",
                          handlers_arg.buff_offset);
}

// Test: Read-Composite operation with single response. The clue here is that
// payload is present in the request and response.
// Server LwM2M         |          Client LwM2M
// --------------------------------------------
// READ-Composite  ---->
//                       <----     2.05 Content
ANJ_UNIT_TEST(server_requests, read_composite_operation) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 3;
    handlers_arg.out_payload = "123";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = 0;
    TEST_INIT(ANJ_OP_DM_READ_COMP, ANJ_COAP_CODE_CONTENT,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, true, false);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    uint8_t expected[] = "\x61"         // ACK, tkl 1
                         "\x45"         // Content
                         "\x33\x33\x01" // msg id, token
                         "\xC1\x3C"     // content_format: cbor
                         "\xFF"
                         "\x31\x32\x33";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(handlers_arg.read_counter, 1);
    ASSERT_EQ(handlers_arg.write_counter, 1);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
    ASSERT_EQ(handlers_arg.buff_offset, 16);
    ASSERT_EQ(handlers_arg.last_block, true);
    ASSERT_EQ_BYTES_SIZED(handlers_arg.buff, "1234567812345678",
                          handlers_arg.buff_offset);
}

static _anj_coap_msg_t
process_block_read_composite(_anj_exchange_ctx_t *ctx,
                             _anj_block_option_t block_type,
                             uint8_t block_num,
                             bool block_transfer,
                             uint16_t msg_id) {
    _anj_coap_msg_t msg = {
        .operation = ANJ_OP_DM_READ_COMP,
        .token = {
            .size = 1,
            .bytes = { 2 }
        },
        .coap_binding_data = {
            .udp = {
                .type = ANJ_COAP_UDP_TYPE_CONFIRMABLE,
                .message_id = msg_id,
            }
        },
        .block = {
            .block_type = block_type,
            .number = block_num,
            .size = 16,
            .more_flag = false
        },
        .content_format = _ANJ_COAP_FORMAT_CBOR,
        .uri = {
            .uri_len = 1,
            .ids = { 1 }
        },
        .payload = (block_type == ANJ_OPTION_BLOCK_1)
                           ? (uint8_t *) "1111111122222222"
                           : NULL,
        .payload_size = (block_type == ANJ_OPTION_BLOCK_1) ? 16 : 0,
    };
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(_anj_exchange_process(ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_ongoing_exchange(ctx), true);

    if (!block_transfer) {
        ASSERT_EQ(_anj_exchange_process(
                          ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
                  ANJ_EXCHANGE_STATE_FINISHED);
    }
    return msg;
}

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
// Test: Read-Composite operation with block transfer in both directions.
// First server sends 2 blocks, then client returns 2 blocks. Response with
// payload should be sent after last block1 message.
// Server LwM2M                      |                     Client LwM2M
// --------------------------------------------------------------------
// READ-Composite block1 0 more (payload) ---->
//                           <---- 2.05 Continue block1 0 more
// READ-Composite block1 1 (payload)      ---->
//                           <---- 2.05 Content block2 0 more, block1 1
//                           (                                 payload)
// READ-Composite block2 1                ---->
//                           <---- 2.05 Content block2 1 (payload)
ANJ_UNIT_TEST(server_requests, read_composite_operation_with_block) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "8888888877777777";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    TEST_INIT(ANJ_OP_DM_READ_COMP, ANJ_COAP_CODE_CONTENT,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, true, true);

    uint8_t expected[] = "\x61"          // ACK, tkl 1
                         "\x5F"          // Continue
                         "\x33\x33\x01"  // msg id, token
                         "\xd1\x0e\x08"; // block1 0 more
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(handlers_arg.last_block, false);

    ASSERT_EQ(handlers_arg.read_counter, 0);
    msg = process_block_read_composite(&ctx, ANJ_OPTION_BLOCK_1, 1, true,
                                       0x2222);
    uint8_t expected2[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x22\x22\x02" // msg id, token
            "\xC1\x3C"     // content_format: cbor
            "\xB1\x08"     // block2 0, size 16, more
            "\x41\x10"     // block1 1, size 16
            "\xFF"
            "\x38\x38\x38\x38\x38\x38\x38\x38\x37\x37\x37\x37\x37\x37\x37\x37";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    handlers_arg.ret_val = 0;
    handlers_arg.out_payload_len = 9;
    msg = process_block_read_composite(&ctx, ANJ_OPTION_BLOCK_2, 1, false,
                                       0x2223);
    uint8_t expected3[] = "\x61"         // ACK, tkl 1
                          "\x45"         // Content
                          "\x22\x23\x02" // msg id, token
                          "\xC1\x3C"     // content_format: cbor
                          "\xB1\x10"     // block2 1, size 16
                          "\xFF"
                          "\x38\x38\x38\x38\x38\x38\x38\x38\x37";
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    ASSERT_EQ(handlers_arg.read_counter, 2);
    ASSERT_EQ(handlers_arg.write_counter, 2);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
    ASSERT_EQ(handlers_arg.buff_offset, 32);
    ASSERT_EQ(handlers_arg.last_block, true);
    ASSERT_EQ_BYTES_SIZED(handlers_arg.buff, "12345678123456781111111122222222",
                          handlers_arg.buff_offset);
}
#endif // ANJ_WITH_COMPOSITE_OPERATIONS

// Test: Read operation with timeout after exchange max time.
ANJ_UNIT_TEST(server_requests, read_operation_with_timeout) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    _anj_coap_msg_t msg = {
        .operation = ANJ_OP_DM_READ,
        .token = {
            .size = 1,
            .bytes = { 1 }
        },
        .coap_binding_data = {
            .udp = {
                .type = ANJ_COAP_UDP_TYPE_CONFIRMABLE,
                .message_id = 0x3333,

            }
        },
        .uri = {
            .uri_len = 1,
            .ids = { 1 }
        },
        .payload = (uint8_t *) "1234567812345678",
        .payload_size = 0,
        .content_format = _ANJ_COAP_FORMAT_NOT_DEFINED
    };
    mock_time_reset();
    _anj_exchange_ctx_t ctx;
    _anj_exchange_init(&ctx);
    _anj_exchange_set_server_request_timeout(
            &ctx, anj_time_duration_new(10, ANJ_TIME_UNIT_S));
    ASSERT_EQ(_anj_exchange_new_server_request(&ctx, ANJ_COAP_CODE_CONTENT,
                                               &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    mock_time_advance(anj_time_duration_new(9, ANJ_TIME_UNIT_S)); // T0 + 9s
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    mock_time_advance(anj_time_duration_new(2, ANJ_TIME_UNIT_S)); // T0 + 11s
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NONE, &msg),
              ANJ_EXCHANGE_STATE_FINISHED);

    mock_time_reset();
    ASSERT_EQ(handlers_arg.read_counter, 1);
    ASSERT_EQ(handlers_arg.write_counter, 0);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, _ANJ_EXCHANGE_ERROR_TIMEOUT);
}

// Test: Notify operation with block transfer.
// Notify is LwM2M client initiated operation, but for block transfer server
// response with Read operation reqeust.
// Server LwM2M         |                    Client LwM2M
// ------------------------------------------------------
//                       <---- 2.05 Content block2 0 more (observe option)
// READ block2 1   ---->
//                       <---- 2.05 Content block2 1 more
// READ block2 2   ---->
//                       <---- 2.05 Content block2 2
ANJ_UNIT_TEST(server_requests, notify_operation_with_block) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_ctx_t ctx;
    _anj_exchange_init(&ctx);
    msg.operation = ANJ_OP_INF_NON_CON_NOTIFY;
    msg.observe_number = 2;
    msg.token.size = 1;
    msg.token.bytes[0] = 1;
    ASSERT_EQ(_anj_exchange_new_client_request(&ctx, &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    uint8_t expected[] =
            "\x51"         // NonConfirmable, tkl 1
            "\x45"         // Content
            "\x00\x00\x01" // msg id, token
            "\x61\x02"     // observe 2
            "\x61\x3C"     // content_format: cbor
            "\xB1\x08"     // block2 0, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";

    uint8_t out_buff[50];
    size_t out_msg_size = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
            &msg, out_buff, sizeof(out_buff), &out_msg_size));
    expected[2] = msg.coap_binding_data.udp.message_id >> 8;
    expected[3] = msg.coap_binding_data.udp.message_id;
    ASSERT_EQ_BYTES_SIZED(out_buff, expected, sizeof(expected) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(expected) - 1);

    msg = process_block_read(&ctx, ANJ_OP_DM_READ, true, 1, 0x2222);
    uint8_t expected2[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x22\x22\x02" // msg id, token
            "\xC1\x3C"     // content_format: cbor
            "\xB1\x18"     // block2 1, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    handlers_arg.ret_val = 0;
    // read composite is also recognized for notify operation
    msg = process_block_read(&ctx, ANJ_OP_DM_READ_COMP, false, 2, 0x2223);
    uint8_t expected3[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x22\x23\x02" // msg id, token
            "\xC1\x3C"     // content_format: cbor
            "\xB1\x20"     // block2 2, size 16
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    ASSERT_EQ(handlers_arg.read_counter, 3);
    ASSERT_EQ(handlers_arg.write_counter, 0);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
}

// Test: Read operation with block response, server by block option force the
// payload size.
// Server LwM2M         |                    Client LwM2M
// ------------------------------------------------------
// READ block size 16    ---->
//                       <---- 2.05 Content block2 0 more size 16
ANJ_UNIT_TEST(server_requests, read_operation_with_block_size_set) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    uint8_t payload[40];
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;

    _anj_coap_msg_t msg = {
        .operation = ANJ_OP_DM_READ,
        .token = {
            .size = 1,
            .bytes = { 1 }
        },
        .coap_binding_data = {
            .udp = {
                .message_id = 0x3333,

            }
        },
        .payload_size = 0,
        .block = (_anj_block_t) {
            .block_type = ANJ_OPTION_BLOCK_2,
            .number = 0,
            .size = 16,
            .more_flag = true
        }
    };
    _anj_exchange_ctx_t ctx;
    _anj_exchange_init(&ctx);
    ASSERT_EQ(_anj_exchange_new_server_request(&ctx, ANJ_COAP_CODE_CONTENT,
                                               &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    uint8_t expected[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x33\x33\x01" // msg id, token
            "\xC1\x3C"     // content_format: cbor
            "\xB1\x08"     // block2 0, size 16, more
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

// Test: Read operation, server by block option force the
// payload size, but client response in single message.
// Server LwM2M         |                    Client LwM2M
// ------------------------------------------------------
// READ block size 16    ---->
//                       <---- 2.05 Content
ANJ_UNIT_TEST(server_requests, read_operation_with_size_set) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    uint8_t payload[40];
    handlers_arg.out_payload_len = 2;
    handlers_arg.out_payload = "12";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = 0;

    _anj_coap_msg_t msg = {
        .operation = ANJ_OP_DM_READ,
        .token = {
            .size = 1,
            .bytes = { 1 }
        },
        .coap_binding_data = {
            .udp = {
                .message_id = 0x3333,

            }
        },
        .payload_size = 0,
        .block = (_anj_block_t) {
            .block_type = ANJ_OPTION_BLOCK_2,
            .number = 0,
            .size = 16,
            .more_flag = true
        }
    };
    _anj_exchange_ctx_t ctx;
    _anj_exchange_init(&ctx);
    ASSERT_EQ(_anj_exchange_new_server_request(&ctx, ANJ_COAP_CODE_CONTENT,
                                               &msg, &handlers, payload,
                                               sizeof(payload)),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    uint8_t expected[] = "\x61"         // ACK, tkl 1
                         "\x45"         // Content
                         "\x33\x33\x01" // msg id, token
                         "\xC1\x3C"     // content_format: cbor
                         "\xFF"
                         "\x31\x32";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

// Test: READ is processed, during waiting for next block, the new
// request is arriving. We should response with
// ANJ_COAP_CODE_SERVICE_UNAVAILABLE.
//
// Server LwM2M         |                    Client LwM2M
// ------------------------------------------------------
// READ            ---->
//                       <---- 2.05 Content block2 0 more
// READ block2 1   ---->
//                       <---- 2.05 Content block2 1 more
// New Request     ---->
//                       <---- ACK-5.03
// READ block2 2   ---->
//                       <---- 2.05 Content block2 2
ANJ_UNIT_TEST(server_requests, read_with_interruption) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.out_payload_len = 16;
    handlers_arg.out_payload = "1234567812345678";
    handlers_arg.out_format = _ANJ_COAP_FORMAT_CBOR;
    handlers_arg.ret_val = _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
    TEST_INIT(ANJ_OP_DM_READ, ANJ_COAP_CODE_CONTENT,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, false, false);

    msg = process_block_read(&ctx, ANJ_OP_DM_READ, true, 1, 0x2222);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    // additional request from server
    _anj_coap_msg_t new_req = msg;
    new_req.operation = ANJ_OP_DM_DISCOVER;
    new_req.token.size = 1;
    new_req.coap_binding_data.udp.message_id = 0x3334;
    new_req.token.bytes[0] = 1;
    new_req.msg_code = ANJ_COAP_CODE_GET;
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG, &new_req),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    uint8_t expected_ack[] =
            "\x61"         // ACK, tkl 1
            "\xA3\x33\x34" // ANJ_COAP_CODE_SERVICE_UNAVAILABLE, msg id
            "\x01";        // token
    verify_payload(expected_ack, sizeof(expected_ack) - 1, &new_req);

    handlers_arg.ret_val = 0;
    // make sure that message after interruption is processed correctly
    msg = process_block_read(&ctx, ANJ_OP_DM_READ, false, 2, 0x2223);
    uint8_t expected_last[] =
            "\x61"         // ACK, tkl 1
            "\x45"         // Content
            "\x22\x23\x02" // msg id, token
            "\xC1\x3C"     // content_format: cbor
            "\xB1\x20"     // block2 2, size 16
            "\xFF"
            "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
    verify_payload(expected_last, sizeof(expected_last) - 1, &msg);

    ASSERT_EQ(handlers_arg.read_counter, 3);
    ASSERT_EQ(handlers_arg.write_counter, 0);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
}

// Test: Write operation with block transfer, and interruption in the middle of
// the transfer.
// Server LwM2M             |                    Client LwM2M
// ----------------------------------------------------------
// WRITE block1 0 more ---->
//                          <---- 2.05 Continue block1 0 more
// WRITE block1 1 more ---->
//                          <---- 4.00 BAD_REQUEST
ANJ_UNIT_TEST(server_requests,
              write_operation_with_block_and_write_payload_error) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.ret_val = 0;
    TEST_INIT(ANJ_OP_DM_WRITE_REPLACE, ANJ_COAP_CODE_CHANGED,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, true, true);
    handlers_arg.write_ret_val = ANJ_COAP_CODE_BAD_REQUEST;
    msg = process_block_write(&ctx, true, 1, 0x2222, 2);
    uint8_t expected[] = "\x61"          // ACK, tkl 1
                         "\x80"          // BAD_REQUEST
                         "\x22\x22\x02"; // msg id, token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(handlers_arg.read_counter, 0);
    ASSERT_EQ(handlers_arg.write_counter, 2);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, _ANJ_EXCHANGE_ERROR_REQUEST);
}

// Test: Write operation with block transfer number mismatch.
// Server LwM2M             |                    Client LwM2M
// ----------------------------------------------------------
// WRITE block1 0 more ---->
//                          <---- 2.05 Continue block1 0 more
// WRITE block1 1 more ---->
//                          <---- 2.05 Continue block1 1 more
// WRITE block1 1 more ---->
// WRITE block1 2      ---->
//                          <---- 2.05 Changed block1 2
ANJ_UNIT_TEST(server_requests, write_operation_with_block_mismatch) {
    handlers_arg_t handlers_arg = { 0 };
    _anj_exchange_handlers_t handlers = {
        .arg = &handlers_arg,
        .write_payload = write_payload_handler,
        .read_payload = read_payload_handler,
        .completion = exchange_completion_handler
    };
    handlers_arg.ret_val = 0;
    TEST_INIT(ANJ_OP_DM_WRITE_REPLACE, ANJ_COAP_CODE_CHANGED,
              ANJ_EXCHANGE_STATE_MSG_TO_SEND, true, true);
    uint8_t expected[] = "\x61"          // ACK, tkl 1
                         "\x5F"          // Continue
                         "\x33\x33\x01"  // msg id, token
                         "\xd1\x0e\x08"; // block1 0 more
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ASSERT_EQ(handlers_arg.last_block, false);

    msg = process_block_write(&ctx, true, 1, 0x2222, 2);
    uint8_t expected2[] = "\x61"          // ACK, tkl 1
                          "\x5F"          // Continue
                          "\x22\x22\x02"  // msg id, token
                          "\xd1\x0e\x18"; // block1 1 more
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
    ASSERT_EQ(handlers_arg.last_block, false);

    _anj_coap_msg_t msg_with_mismatch = {
        .operation = ANJ_OP_DM_WRITE_REPLACE,
        .token = {
            .size = 1,
            .bytes = { 2 }
        },
        .coap_binding_data = {
            .udp = {
                .type = ANJ_COAP_UDP_TYPE_CONFIRMABLE,
                .message_id = 0x2221,
            }
        },
        .block = {
            .block_type = ANJ_OPTION_BLOCK_1,
            .number = 1,
            .size = 16,
            .more_flag = true
        },
        .content_format = _ANJ_COAP_FORMAT_CBOR,
        .uri = {
            .uri_len = 1,
            .ids = { 1 }
        },
        .payload = (uint8_t *) "1111111122222222",
        .payload_size = 16
    };
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &msg_with_mismatch),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    ASSERT_EQ(_anj_exchange_process(&ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &msg_with_mismatch),
              ANJ_EXCHANGE_STATE_WAITING_MSG);

    msg = process_block_write(&ctx, false, 2, 0x2223, 2);
    uint8_t expected3[] = "\x61"          // ACK, tkl 1
                          "\x44"          // Changed
                          "\x22\x23\x02"  // msg id, token
                          "\xd1\x0e\x20"; // block1 2
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    ASSERT_EQ(handlers_arg.read_counter, 1);
    ASSERT_EQ(handlers_arg.write_counter, 3);
    ASSERT_EQ(handlers_arg.complete_counter, 1);
    ASSERT_EQ(handlers_arg.result, 0);
    ASSERT_EQ(handlers_arg.buff_offset, 48);
    ASSERT_EQ(handlers_arg.last_block, true);
    ASSERT_EQ_BYTES_SIZED(handlers_arg.buff,
                          "123456781234567811111111222222221111111122222222",
                          handlers_arg.buff_offset);
}
