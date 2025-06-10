/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <anj/compat/time.h>
#include <anj/defs.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#include "coap/coap.h"
#include "exchange.h"
#include "utils.h"

#define exchange_log(...) anj_log(exchange, __VA_ARGS__)

static uint8_t
default_read_payload_handler(void *arg_ptr,
                             uint8_t *buff,
                             size_t buff_len,
                             _anj_exchange_read_result_t *out_params) {
    (void) arg_ptr;
    (void) buff;
    (void) buff_len;
    out_params->format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    return 0;
}

static uint8_t default_write_payload_handler(void *arg_ptr,
                                             uint8_t *buff,
                                             size_t buff_len,
                                             bool last_block) {
    (void) arg_ptr;
    (void) buff;
    (void) buff_len;
    (void) last_block;
    return 0;
}

static void default_exchange_completion_handler(void *arg_ptr,
                                                const _anj_coap_msg_t *response,
                                                int result) {
    (void) arg_ptr;
    (void) response;
    (void) result;
}

static void set_default_handlers(_anj_exchange_handlers_t *handlers) {
    if (!handlers->completion) {
        handlers->completion = default_exchange_completion_handler;
    }
    if (!handlers->read_payload) {
        handlers->read_payload = default_read_payload_handler;
    }
    if (!handlers->write_payload) {
        handlers->write_payload = default_write_payload_handler;
    }
}

static _anj_exchange_state_t finalize_exchange(_anj_exchange_ctx_t *ctx,
                                               const _anj_coap_msg_t *msg,
                                               int result) {
    ctx->handlers.completion(ctx->handlers.arg, msg, result);
    ctx->state = ANJ_EXCHANGE_STATE_FINISHED;
    ctx->block_transfer = false;
    ctx->request_prepared = false;
    return ctx->state;
}

static void exchange_param_init(_anj_exchange_ctx_t *ctx) {
    ctx->retry_count = 0;
    ctx->block_number = 0;
    // RFC 7252 "The initial timeout is set to a random number between
    // ACK_TIMEOUT and (ACK_TIMEOUT * ACK_RANDOM_FACTOR)"
    double ack_timeout_ms = (double) ctx->tx_params.ack_timeout_ms;

    if (ctx->server_request) {
        ctx->timeout_ms = ctx->server_exchange_timeout;
    } else {
        // calculate timeout for the first message, comply with RFC 7252 4.2
        double random_factor = ((double) _anj_rand32_r(&ctx->timeout_rand_seed)
                                / (double) UINT32_MAX)
                               * (ctx->tx_params.ack_random_factor - 1.0);
        ctx->timeout_ms = (uint64_t) (ack_timeout_ms * (random_factor + 1.0));
    }
    ctx->timeout_timestamp_ms = anj_time_real_now() + ctx->timeout_ms;
    ctx->send_ack_timeout_timestamp_ms =
            anj_time_real_now() + _ANJ_EXCHANGE_COAP_PROCESSING_DELAY_MS;
}

static void reset_exchange_params(_anj_exchange_ctx_t *ctx) {
    ctx->timeout_timestamp_ms = anj_time_real_now() + ctx->timeout_ms;
    ctx->retry_count = 0;
}

static bool timeout_occurred(uint64_t timeout_timestamp_ms) {
    return anj_time_real_now() >= timeout_timestamp_ms;
}

static void handle_send_ack(_anj_exchange_ctx_t *ctx,
                            _anj_exchange_event_t event) {
    // no retransmission if the message is not sent in the allowed time
    if (timeout_occurred(ctx->send_ack_timeout_timestamp_ms)) {
        exchange_log(L_ERROR, "sending timeout occurred");
        finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_TIMEOUT);
    } else if (event == ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION) {
        if (!ctx->confirmable && !ctx->block_transfer) {
            finalize_exchange(ctx, NULL,
                              ctx->msg_code >= ANJ_COAP_CODE_BAD_REQUEST
                                      ? ctx->msg_code
                                      : 0);
            exchange_log(L_TRACE, "exchange finished");
        } else {
            ctx->state = ANJ_EXCHANGE_STATE_WAITING_MSG;
            exchange_log(L_TRACE, "message sent, waiting for response");
        }
    }
}

static void handle_server_response(_anj_exchange_ctx_t *ctx,
                                   _anj_coap_msg_t *in_out_msg) {
    if (in_out_msg->operation == ANJ_OP_COAP_EMPTY_MSG) {
        if (ctx->base_msg.operation == ANJ_OP_INF_CON_NOTIFY) {
            finalize_exchange(ctx, in_out_msg, 0);
            return;
        } else {
            exchange_log(
                    L_DEBUG,
                    "empty message received, waiting for separate response");
            ctx->separate_response = true;
            reset_exchange_params(ctx);
            return;
        }
    }
    if (in_out_msg->operation == ANJ_OP_COAP_RESET) {
        exchange_log(L_WARNING, "received CoAP RESET message");
        // cancel the transaction, msg_code is not important in this case, use
        // of ANJ_COAP_CODE_BAD_REQUEST will result in deletion of observations
        // when handling notifications
        finalize_exchange(ctx, NULL, ANJ_COAP_CODE_BAD_REQUEST);
        return;
    }

    if (!_anj_tokens_equal(&in_out_msg->token, &ctx->base_msg.token)) {
        if (in_out_msg->msg_code > ANJ_COAP_CODE_IPATCH) {
            exchange_log(L_INFO, "token mismatch, ignoring the message");
            return;
        }
        // response only for requests
        exchange_log(L_INFO,
                     "token mismatch, response for request with "
                     "ANJ_COAP_CODE_SERVICE_UNAVAILABLE");
        goto send_service_unavailable;
    }

    if (ctx->block_number != in_out_msg->block.number) {
        exchange_log(L_WARNING, "block number mismatch, ignoring");
        return;
    }

    if (in_out_msg->msg_code >= ANJ_COAP_CODE_BAD_REQUEST) {
        exchange_log(L_ERROR, "received error response: %" PRIu8,
                     in_out_msg->msg_code);
        finalize_exchange(ctx, NULL, in_out_msg->msg_code);
        return;
    }
    if (in_out_msg->block.block_type != ANJ_OPTION_BLOCK_NOT_DEFINED) {
        exchange_log(L_DEBUG, "next block received, block number: %" PRIu32,
                     in_out_msg->block.number);
    }

    // HACK: The server must reset the more_flag for the last BLOCK1 ACK message
    //       to prevent sending the next block. To avoid errors, we check the
    //       block_transfer flag (controlled internally) instead of the
    //       more_flag for BLOCK1.
    ctx->block_transfer = (in_out_msg->block.block_type == ANJ_OPTION_BLOCK_2
                           && in_out_msg->block.more_flag)
                          || (in_out_msg->block.block_type == ANJ_OPTION_BLOCK_1
                              && ctx->block_transfer);

    ctx->base_msg.payload_size = 0;
    // BootstrapPack-Request is only LwM2M Client request that contains payload
    // in response
    if (in_out_msg->payload_size) {
        uint8_t result = ctx->handlers.write_payload(ctx->handlers.arg,
                                                     in_out_msg->payload,
                                                     in_out_msg->payload_size,
                                                     !ctx->block_transfer);
        ctx->base_msg.block = (_anj_block_t) {
            .more_flag = false,
            .number = ++ctx->block_number,
            .block_type = ANJ_OPTION_BLOCK_2,
            .size = ctx->block_size
        };
        if (result) {
            exchange_log(L_ERROR,
                         "error while writing payload: %" PRIu8
                         ", cancel exchange",
                         result);
            finalize_exchange(ctx, NULL, result);
            return;
        }
    } else if (ctx->block_transfer) {
        _anj_exchange_read_result_t read_result = { 0 };
        uint8_t result =
                ctx->handlers.read_payload(ctx->handlers.arg, ctx->payload_buff,
                                           ctx->block_size, &read_result);
        ctx->base_msg.payload_size = read_result.payload_len;
        ctx->base_msg.content_format = read_result.format;
        ctx->base_msg.block = (_anj_block_t) {
            .number = ++ctx->block_number,
            .block_type = ANJ_OPTION_BLOCK_1,
            .size = ctx->block_size
        };
        if (result == _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED) {
            ctx->block_transfer = true;
            ctx->base_msg.block.more_flag = true;
        } else if (!result) {
            ctx->block_transfer = false;
            ctx->base_msg.block.more_flag = false;
        } else {
            exchange_log(L_ERROR,
                         "error while reading payload: %" PRIu8
                         ", cancel exchange",
                         result);
            finalize_exchange(ctx, NULL, result);
            return;
        }
    }

    if (ctx->block_transfer || ctx->base_msg.payload_size != 0) {
        if (!ctx->request_prepared && ctx->separate_response) {
            ctx->request_prepared = true;
            goto send_empty_ack;
        }
        ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
        reset_exchange_params(ctx);
        _anj_coap_init_coap_udp_credentials(&ctx->base_msg);
        *in_out_msg = ctx->base_msg;
        return;
    } else {
        if (ctx->separate_response) {
            ctx->confirmable = false;
            goto send_empty_ack;
        }
        exchange_log(L_TRACE, "exchange finished");
        finalize_exchange(ctx, in_out_msg, 0);
        return;
    }

send_empty_ack:
    ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
    in_out_msg->operation = ANJ_OP_COAP_EMPTY_MSG;
    in_out_msg->block.block_type = ANJ_OPTION_BLOCK_NOT_DEFINED;
    return;

send_service_unavailable:
    ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
    in_out_msg->operation = ANJ_OP_RESPONSE;
    in_out_msg->msg_code = ANJ_COAP_CODE_SERVICE_UNAVAILABLE;
    in_out_msg->payload_size = 0;
    in_out_msg->block.block_type = ANJ_OPTION_BLOCK_NOT_DEFINED;
    return;
}

static void handle_server_request(_anj_exchange_ctx_t *ctx,
                                  _anj_coap_msg_t *in_out_msg) {
    uint8_t response_code = ANJ_COAP_CODE_EMPTY;
    // For block transfer, token in next request don't have to be the same
    // that's why we don't check token equality, but operation type and block
    // number. In case of notify operation and block transfer, the server
    // responds with a READ or READ composite operation.
    if (in_out_msg->operation != ctx->op
            && !(ctx->op == ANJ_OP_INF_NON_CON_NOTIFY
                 && (in_out_msg->operation == ANJ_OP_DM_READ
                     || in_out_msg->operation == ANJ_OP_DM_READ_COMP))) {
        if (in_out_msg->operation >= ANJ_OP_RESPONSE) {
            // According to the specification, the Server cannot respond with a
            // Reset message to the ACK: "Rejecting an Acknowledgement or Reset
            // message is effected by silently ignoring it."
            exchange_log(L_WARNING, "invalid operation, ignoring the message");
            return;
        }
        // response only for requests
        exchange_log(L_INFO,
                     "different request, response for request with "
                     "ANJ_COAP_CODE_SERVICE_UNAVAILABLE");
        goto send_service_unavailable;
    }

    // message_id and token are the same, so we are facing retransmission of the
    // same request, we should send the same response as before
    if (ctx->base_msg.coap_binding_data.udp.message_id
                    == in_out_msg->coap_binding_data.udp.message_id
            && _anj_tokens_equal(&ctx->base_msg.token, &in_out_msg->token)) {
        exchange_log(L_INFO,
                     "Retransmission detected, sending previous response");
        *in_out_msg = ctx->base_msg;
        ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
        reset_exchange_params(ctx);
        return;
    }

    ctx->block_number++;
    if (ctx->block_number != in_out_msg->block.number) {
        exchange_log(L_WARNING, "block number mismatch, ignoring");
        ctx->block_number--;
        return;
    }
    exchange_log(L_DEBUG, "next block received, block number: %" PRIu32,
                 in_out_msg->block.number);

    if (in_out_msg->payload_size != 0) {
        if (!in_out_msg->block.more_flag) { // last block
            ctx->block_transfer = false;
            ctx->block_number = 0;
        }
        uint8_t result = ctx->handlers.write_payload(ctx->handlers.arg,
                                                     in_out_msg->payload,
                                                     in_out_msg->payload_size,
                                                     !ctx->block_transfer);
        if (result) {
            exchange_log(L_ERROR, "error while writing payload: %" PRIu8,
                         result);
            response_code = result;
            goto send_response;
        } else {
            response_code = ctx->block_transfer ? ANJ_COAP_CODE_CONTINUE
                                                : ctx->msg_code;
        }
    }

    size_t payload_size = 0;
    // ANJ_COAP_CODE_CONTINUE means that server is still sending payload, we
    // want to read payload after last write block is received
    if (response_code != ANJ_COAP_CODE_CONTINUE) {
        in_out_msg->payload = ctx->payload_buff;
        _anj_exchange_read_result_t read_result = { 0 };
        uint8_t result =
                ctx->handlers.read_payload(ctx->handlers.arg, ctx->payload_buff,
                                           ctx->block_size, &read_result);
        payload_size = read_result.payload_len;
        in_out_msg->content_format = read_result.format;
        if (read_result.with_create_path) {
            in_out_msg->attr.create_attr.has_uri = true;
            in_out_msg->attr.create_attr.oid = read_result.created_oid;
            in_out_msg->attr.create_attr.iid = read_result.created_iid;
        }

        if (result == _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED) {
            ctx->block_transfer = true;
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
            if (in_out_msg->block.block_type == ANJ_OPTION_BLOCK_1) {
                in_out_msg->block.block_type = ANJ_OPTION_BLOCK_BOTH;
                in_out_msg->block.size = ctx->block_size;
            } else
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
            {
                in_out_msg->block = (_anj_block_t) {
                    .more_flag = true,
                    .number = ctx->block_number,
                    .block_type = ANJ_OPTION_BLOCK_2,
                    .size = ctx->block_size
                };
            }
        } else if (result) {
            exchange_log(L_ERROR, "error while reading payload: %" PRIu8,
                         result);
            response_code = (uint8_t) result;
            goto send_response;
        } else {
            in_out_msg->block.more_flag = false;
            ctx->block_transfer = false;
        }
        if (payload_size) {
            response_code = ANJ_COAP_CODE_CONTENT;
        }
    }

send_response:
    ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
    reset_exchange_params(ctx);
    in_out_msg->operation =
            (in_out_msg->operation == ANJ_OP_INF_OBSERVE
             || in_out_msg->operation == ANJ_OP_INF_OBSERVE_COMP)
                    ? ANJ_OP_INF_INITIAL_NOTIFY
                    : ANJ_OP_RESPONSE;
    if (response_code >= ANJ_COAP_CODE_BAD_REQUEST) {
        in_out_msg->payload_size = 0;
        in_out_msg->block.block_type = ANJ_OPTION_BLOCK_NOT_DEFINED;
        ctx->block_transfer = false;
        // error code for completion handler
        ctx->msg_code = response_code;
    } else {
        in_out_msg->payload_size = payload_size;
    }
    in_out_msg->msg_code = response_code;
    // store the response in case of retransmission
    ctx->base_msg = *in_out_msg;
    return;

send_service_unavailable:
    ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
    in_out_msg->operation = ANJ_OP_RESPONSE;
    in_out_msg->payload_size = 0;
    in_out_msg->msg_code = ANJ_COAP_CODE_SERVICE_UNAVAILABLE;
    in_out_msg->block.block_type = ANJ_OPTION_BLOCK_NOT_DEFINED;
    return;
}

_anj_exchange_state_t
_anj_exchange_new_server_request(_anj_exchange_ctx_t *ctx,
                                 uint8_t response_msg_code,
                                 _anj_coap_msg_t *in_out_msg,
                                 _anj_exchange_handlers_t *handlers,
                                 uint8_t *buff,
                                 size_t buff_len) {
    assert(ctx && in_out_msg && handlers && buff && buff_len >= 16);
    assert(ctx->state == ANJ_EXCHANGE_STATE_FINISHED);
    uint8_t result = 0;
    ctx->block_size = _anj_determine_block_buffer_size(buff_len);
    ctx->payload_buff = buff;
    ctx->server_request = true;
    ctx->confirmable = false;
    ctx->handlers = *handlers;
    set_default_handlers(&ctx->handlers);
    ctx->op = in_out_msg->operation;
    ctx->msg_code = response_msg_code;

    exchange_param_init(ctx);

    if (in_out_msg->operation == ANJ_OP_COAP_PING_UDP) {
        in_out_msg->operation = ANJ_OP_COAP_RESET;
        exchange_log(L_DEBUG, "received PING request, sending RESET");
        ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
        return ANJ_EXCHANGE_STATE_MSG_TO_SEND;
    }
    assert(response_msg_code >= ANJ_COAP_CODE_CREATED
           && response_msg_code <= ANJ_COAP_CODE_PROXYING_NOT_SUPPORTED);

    in_out_msg->operation =
            (in_out_msg->operation == ANJ_OP_INF_OBSERVE
             || in_out_msg->operation == ANJ_OP_INF_OBSERVE_COMP)
                    ? ANJ_OP_INF_INITIAL_NOTIFY
                    : ANJ_OP_RESPONSE;
    // response with error code and finish the exchange
    if (response_msg_code >= ANJ_COAP_CODE_BAD_REQUEST) {
        in_out_msg->msg_code = response_msg_code;
        in_out_msg->payload_size = 0;
        in_out_msg->block.block_type = ANJ_OPTION_BLOCK_NOT_DEFINED;
        ctx->block_transfer = false;
        ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
        exchange_log(L_TRACE, "new response created");
        return ANJ_EXCHANGE_STATE_MSG_TO_SEND;
    }
    ctx->block_transfer = in_out_msg->block.block_type == ANJ_OPTION_BLOCK_1
                          && in_out_msg->block.more_flag
                          && in_out_msg->payload_size;
    // in case of ANJ_OPTION_BLOCK_1 client responses with
    // ANJ_COAP_CODE_CONTINUE until last block
    in_out_msg->msg_code =
            ctx->block_transfer ? ANJ_COAP_CODE_CONTINUE : response_msg_code;

    if (in_out_msg->payload_size) {
        result = ctx->handlers.write_payload(ctx->handlers.arg,
                                             in_out_msg->payload,
                                             in_out_msg->payload_size,
                                             !ctx->block_transfer);
    }
    in_out_msg->payload_size = 0;

    // for LwM2M there is possible scenario of block transfer in both directions
    // at the same time, but block2 is always prepared after last block1
    // transfer - so read_payload() is called only when write_payload() handled
    // the last incoming block (ctx->block_transfer == false)
    if (!result && !ctx->block_transfer) {
        // LwM2M client can force block size, but it can't be bigger than the
        // buffer
        if (in_out_msg->block.block_type == ANJ_OPTION_BLOCK_2) {
            ctx->block_size = ANJ_MIN(ctx->block_size, in_out_msg->block.size);
        }
        in_out_msg->payload = buff;
        _anj_exchange_read_result_t read_result = { 0 };
        result = ctx->handlers.read_payload(ctx->handlers.arg, buff,
                                            ctx->block_size, &read_result);
        in_out_msg->payload_size = read_result.payload_len;
        in_out_msg->content_format = read_result.format;
        if (read_result.with_create_path) {
            in_out_msg->attr.create_attr.has_uri = true;
            in_out_msg->attr.create_attr.oid = read_result.created_oid;
            in_out_msg->attr.create_attr.iid = read_result.created_iid;
        }
        if (result == _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED) {
            ctx->block_transfer = true;
            in_out_msg->block = (_anj_block_t) {
                .more_flag = true,
                .number = 0,
                .block_type = ANJ_OPTION_BLOCK_2,
                .size = ctx->block_size
            };
        } else {
            in_out_msg->block.block_type = ANJ_OPTION_BLOCK_NOT_DEFINED;
        }
    }

    if (result && result != _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED) {
        exchange_log(L_ERROR, "response with error code: %" PRIu8, result);
        in_out_msg->msg_code = result;
        in_out_msg->payload_size = 0;
        ctx->block_transfer = false;
        ctx->msg_code = result;
    }

    exchange_log(L_TRACE, "new response created");
    ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
    // store the response in case of retransmission
    ctx->base_msg = *in_out_msg;
    return ANJ_EXCHANGE_STATE_MSG_TO_SEND;
}

_anj_exchange_state_t
_anj_exchange_new_client_request(_anj_exchange_ctx_t *ctx,
                                 _anj_coap_msg_t *in_out_msg,
                                 _anj_exchange_handlers_t *handlers,
                                 uint8_t *buff,
                                 size_t buff_len) {
    assert(ctx && in_out_msg && handlers && buff && buff_len >= 16);
    assert(ctx->state == ANJ_EXCHANGE_STATE_FINISHED);
    uint8_t result = 0;
    ctx->block_size = _anj_determine_block_buffer_size(buff_len);
    ctx->payload_buff = buff;
    ctx->separate_response = false;
    ctx->server_request = false;
    ctx->block_transfer = false;
    ctx->msg_code = 0;
    ctx->handlers = *handlers;
    set_default_handlers(&ctx->handlers);

    _anj_op_t *op = &in_out_msg->operation;
    ctx->confirmable =
            (*op == ANJ_OP_INF_NON_CON_SEND || *op == ANJ_OP_INF_NON_CON_NOTIFY)
                    ? false
                    : true;

    if (*op == ANJ_OP_INF_CON_NOTIFY || *op == ANJ_OP_INF_NON_CON_NOTIFY) {
        const _anj_coap_token_t *token = &in_out_msg->token;
        assert(token->size);
    } else {
        _anj_coap_init_coap_udp_credentials(in_out_msg);
    }

    exchange_param_init(ctx);

    in_out_msg->payload = buff;
    _anj_exchange_read_result_t read_result = { 0 };
    result = ctx->handlers.read_payload(ctx->handlers.arg, buff,
                                        ctx->block_size, &read_result);
    in_out_msg->payload_size = read_result.payload_len;
    in_out_msg->content_format = read_result.format;
    if (result == _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED) {
        ctx->block_transfer = true;
        in_out_msg->block = (_anj_block_t) {
            .more_flag = true,
            .number = 0,
            .block_type = ANJ_OPTION_BLOCK_1,
            .size = ctx->block_size
        };
        if (*op == ANJ_OP_INF_NON_CON_SEND) {
            *op = ANJ_OP_INF_CON_SEND;
            ctx->confirmable = true;
            exchange_log(L_DEBUG, "because of block-transfer changing "
                                  "operation to confirmable");
        }
        if (*op == ANJ_OP_INF_NON_CON_NOTIFY || *op == ANJ_OP_INF_CON_NOTIFY) {
            // for notifications, block transfer results in switching to the
            // READ or READ composite operation
            ctx->confirmable = false;
            ctx->server_request = true;
            ctx->op = ANJ_OP_INF_NON_CON_NOTIFY;
            *op = ANJ_OP_INF_NON_CON_NOTIFY;
            in_out_msg->block.block_type = ANJ_OPTION_BLOCK_2;
            // recalculate timeout for the first message
            exchange_param_init(ctx);
        }
    } else if (result) {
        exchange_log(L_ERROR, "error while preparing request: %" PRIu8, result);
        return finalize_exchange(ctx, NULL, result);
    }

    exchange_log(L_TRACE, "new request created");
    ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
    ctx->base_msg = *in_out_msg;
    return ANJ_EXCHANGE_STATE_MSG_TO_SEND;
}

_anj_exchange_state_t _anj_exchange_process(_anj_exchange_ctx_t *ctx,
                                            _anj_exchange_event_t event,
                                            _anj_coap_msg_t *in_out_msg) {
    assert(ctx && in_out_msg);
    assert(ctx->state == ANJ_EXCHANGE_STATE_WAITING_MSG
           || ctx->state == ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    // new message can't be processed when waiting for ack
    assert(event != ANJ_EXCHANGE_EVENT_NEW_MSG
           || ctx->state != ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    // send ack can't be processed when waiting for message
    assert(event != ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION
           || ctx->state != ANJ_EXCHANGE_STATE_WAITING_MSG);

    if (ctx->state == ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION) {
        handle_send_ack(ctx, event);

        // used in case of separate response, the next request can
        // be sent because an empty response has been sent and no error has
        // occurred in the meantime
        if (ctx->state == ANJ_EXCHANGE_STATE_WAITING_MSG
                && ctx->request_prepared) {
            ctx->request_prepared = false;
            ctx->separate_response = false;
            *in_out_msg = ctx->base_msg;
            ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
            reset_exchange_params(ctx);
            return ANJ_EXCHANGE_STATE_MSG_TO_SEND;
        }
        return ctx->state;
    }

    if (event == ANJ_EXCHANGE_EVENT_NEW_MSG) {
        if (ctx->server_request) {
            handle_server_request(ctx, in_out_msg);
        } else {
            handle_server_response(ctx, in_out_msg);
        }
        if (ctx->state == ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION) {
            ctx->send_ack_timeout_timestamp_ms =
                    anj_time_real_now()
                    + _ANJ_EXCHANGE_COAP_PROCESSING_DELAY_MS;
            return ANJ_EXCHANGE_STATE_MSG_TO_SEND;
        }
        if (ctx->state != ANJ_EXCHANGE_STATE_WAITING_MSG) {
            return ctx->state;
        }
    }

    if (timeout_occurred(ctx->timeout_timestamp_ms)) {
        if (ctx->server_request) {
            exchange_log(L_ERROR, "server request timeout occurred");
            return finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_TIMEOUT);
        } else {
            if (ctx->retry_count < ctx->tx_params.max_retransmit) {
                ctx->retry_count++;
                uint64_t time_real_now = anj_time_real_now();
                ctx->timeout_timestamp_ms =
                        time_real_now
                        + (uint64_t) pow(2.0, (double) ctx->retry_count)
                                  * ctx->timeout_ms;
                ctx->send_ack_timeout_timestamp_ms =
                        time_real_now + _ANJ_EXCHANGE_COAP_PROCESSING_DELAY_MS;
                exchange_log(L_WARNING, "timeout occurred, retrying");
                ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
                *in_out_msg = ctx->base_msg;
                return ANJ_EXCHANGE_STATE_MSG_TO_SEND;
            } else {
                exchange_log(L_ERROR, "client request timeout occurred");
                return finalize_exchange(ctx, NULL,
                                         _ANJ_EXCHANGE_ERROR_TIMEOUT);
            }
        }
    }
    return ANJ_EXCHANGE_STATE_WAITING_MSG;
}

void _anj_exchange_terminate(_anj_exchange_ctx_t *ctx) {
    assert(ctx);
    if (ctx->state == ANJ_EXCHANGE_STATE_FINISHED) {
        return;
    }
    finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_TERMINATED);
    exchange_log(L_DEBUG, "exchange terminated");
}

bool _anj_exchange_ongoing_exchange(_anj_exchange_ctx_t *ctx) {
    assert(ctx);
    return ctx->state != ANJ_EXCHANGE_STATE_FINISHED;
}

_anj_exchange_state_t _anj_exchange_get_state(_anj_exchange_ctx_t *ctx) {
    assert(ctx);
    return ctx->state;
}

int _anj_exchange_set_udp_tx_params(
        _anj_exchange_ctx_t *ctx, const _anj_exchange_udp_tx_params_t *params) {
    assert(ctx && params);
    if (params->ack_random_factor < 1.0 || params->ack_timeout_ms < 1000) {
        exchange_log(L_ERROR, "invalid UDP TX params");
        return -1;
    }
    ctx->tx_params = *params;
    exchange_log(L_DEBUG,
                 "UDP TX params set: ack_timeout_ms=%" PRIu64
                 ", ack_random_factor=%f, max_retransmit=%" PRIu16,
                 ctx->tx_params.ack_timeout_ms,
                 ctx->tx_params.ack_random_factor,
                 ctx->tx_params.max_retransmit);
    return 0;
}

void _anj_exchange_set_server_request_timeout(
        _anj_exchange_ctx_t *ctx, uint64_t server_exchange_timeout) {
    assert(ctx);
    assert(server_exchange_timeout > 0);
    exchange_log(L_DEBUG, "exchange max time set: %" PRIu64,
                 server_exchange_timeout);
    ctx->server_exchange_timeout = server_exchange_timeout;
}

void _anj_exchange_init(_anj_exchange_ctx_t *ctx, unsigned int random_seed) {
    assert(ctx);
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = ANJ_EXCHANGE_STATE_FINISHED;
    ctx->tx_params = _ANJ_EXCHANGE_UDP_TX_PARAMS_DEFAULT;
    ctx->server_exchange_timeout = _ANJ_EXCHANGE_SERVER_REQUEST_TIMEOUT_MS;
    ctx->timeout_rand_seed = random_seed;
    exchange_log(L_DEBUG, "context initialized");
}
