/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <anj/compat/rng.h>
#include <anj/compat/time.h>
#include <anj/defs.h>
#include <anj/log.h>
#include <anj/time.h>
#include <anj/utils.h>

#include "coap/coap.h"
#include "exchange.h"
#include "exchange_cache.h"
#include "utils.h"

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

static int exchange_param_init(_anj_exchange_ctx_t *ctx) {
    ctx->retry_count = 0;
    ctx->block_number = 0;
    // RFC 7252 "The initial timeout is set to a random number between
    // ACK_TIMEOUT and (ACK_TIMEOUT * ACK_RANDOM_FACTOR)"
    anj_time_duration_t ack_timeout = ctx->tx_params.ack_timeout;

    if (ctx->server_request) {
        ctx->timeout = ctx->server_exchange_timeout;
    } else {
        // calculate timeout for the first message, comply with RFC 7252 4.2
        uint32_t random;
        if (anj_rng_generate((uint8_t *) &random, sizeof(random))) {
            exchange_log(L_ERROR, "Could not generate random number");
            return -1;
        }
        double random_factor = ((double) random / (double) UINT32_MAX)
                               * (ctx->tx_params.ack_random_factor - 1.0);
        ctx->timeout = anj_time_duration_fmul(ack_timeout, random_factor + 1.0);
    }
    ctx->timeout_timestamp =
            anj_time_monotonic_add(anj_time_monotonic_now(), ctx->timeout);
    ctx->send_confirmation_timeout_timestamp =
            anj_time_monotonic_add(anj_time_monotonic_now(),
                                   _ANJ_EXCHANGE_COAP_PROCESSING_DELAY);
    return 0;
}

static void reset_exchange_params(_anj_exchange_ctx_t *ctx) {
    ctx->timeout_timestamp =
            anj_time_monotonic_add(anj_time_monotonic_now(), ctx->timeout);
    ctx->retry_count = 0;
}

static bool timeout_occurred(anj_time_monotonic_t timeout_timestamp) {
    return anj_time_monotonic_geq(anj_time_monotonic_now(), timeout_timestamp);
}

static void handle_send_confirmation(_anj_exchange_ctx_t *ctx,
                                     _anj_exchange_event_t event) {
    // no retransmission if the message is not sent in the allowed time
    if (timeout_occurred(ctx->send_confirmation_timeout_timestamp)) {
        exchange_log(L_ERROR, "sending timeout occurred");
        finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_TIMEOUT);
    } else if (event == ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION) {
        if (!ctx->confirmable && !ctx->block_transfer) {
            // The msg_code is determined while handling a server request.
            // It is either set based on the callback's return value,
            // or directly inside _anj_exchange_new_server_request().
            finalize_exchange(ctx, NULL,
                              ctx->msg_code >= ANJ_COAP_CODE_BAD_REQUEST
                                      ? _ANJ_EXCHANGE_ERROR_REQUEST
                                      : _ANJ_EXCHANGE_RESULT_SUCCESS);
            exchange_log(L_TRACE, "exchange finished");
        } else {
            ctx->state = ANJ_EXCHANGE_STATE_WAITING_MSG;
            exchange_log(L_TRACE, "message sent, waiting for response");
        }
    }
}

// This function can be used only to handle server responses.
static bool is_separate_response_mode(const _anj_coap_msg_t *msg) {
    return msg->coap_binding_data.udp.type == ANJ_COAP_UDP_TYPE_CONFIRMABLE;
}

/**
 * Creates a new CoAP token. The token is a pseudo-random 8-byte value.
 * During @ref _anj_coap_encode_udp call token is not created again.
 */
static int token_create(_anj_coap_token_t *token) {
    token->size = _ANJ_COAP_MAX_TOKEN_LENGTH;
    assert(_ANJ_COAP_MAX_TOKEN_LENGTH == 8);
    uint64_t random;
    if (anj_rng_generate((uint8_t *) &random, sizeof(random))) {
        exchange_log(L_ERROR, "Could not generate random number");
        return -1;
    }
    memcpy(token->bytes, &random, sizeof(random));
    return 0;
}

/**
 * Creates a new CoAP Message ID.
 * During @ref _anj_coap_encode_udp call message ID is not created again.
 */
static void init_msd_id(_anj_exchange_ctx_t *ctx, _anj_coap_msg_t *msg) {
    assert(msg);
    msg->coap_binding_data.udp.message_id = ++ctx->msg_id;
}

static void handle_server_response(_anj_exchange_ctx_t *ctx,
                                   _anj_coap_msg_t *in_out_msg) {
    if (in_out_msg->operation == ANJ_OP_COAP_EMPTY_MSG) {
        if (ctx->base_msg.operation == ANJ_OP_INF_CON_NOTIFY) {
            finalize_exchange(ctx, in_out_msg, _ANJ_EXCHANGE_RESULT_SUCCESS);
            return;
        } else {
            exchange_log(
                    L_DEBUG,
                    "empty message received, waiting for separate response");
            reset_exchange_params(ctx);
            return;
        }
    }
    if (in_out_msg->operation == ANJ_OP_COAP_RESET) {
        exchange_log(L_WARNING, "received CoAP RESET message");
        finalize_exchange(ctx, in_out_msg, _ANJ_EXCHANGE_ERROR_SERVER_RESPONSE);
        return;
    }

    if (!_anj_tokens_equal(&in_out_msg->token, &ctx->base_msg.token)) {
        if (in_out_msg->msg_code > ANJ_COAP_CODE_IPATCH) {
            exchange_log(L_INFO, "token mismatch, ignoring the message");
            return;
        }
        // response only for requests
        exchange_log(L_INFO, "token mismatch, respond with %s",
                     COAP_CODE_FORMAT(ANJ_COAP_CODE_SERVICE_UNAVAILABLE));
        goto send_service_unavailable;
    }

    if (in_out_msg->msg_code >= ANJ_COAP_CODE_BAD_REQUEST) {
        exchange_log(L_ERROR, "received error response: %" PRIu8,
                     in_out_msg->msg_code);
        finalize_exchange(ctx, in_out_msg, _ANJ_EXCHANGE_ERROR_SERVER_RESPONSE);
        return;
    }

    if (ctx->block_number != in_out_msg->block.number) {
        exchange_log(L_WARNING, "block number mismatch, ignoring");
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
    // BootstrapPack-Request and Downloader GET are the only LwM2M Client
    // request that contains payload in response
    if (in_out_msg->payload_size) {
        uint8_t result = ctx->handlers.write_payload(ctx->handlers.arg,
                                                     in_out_msg->payload,
                                                     in_out_msg->payload_size,
                                                     !ctx->block_transfer);
        ctx->base_msg.block = (_anj_block_t) {
            .more_flag = false,
            .number = ++ctx->block_number,
            .block_type = ANJ_OPTION_BLOCK_2,
            .size = in_out_msg->block.size // get block size from the message
        };
        if (result) {
            exchange_log(L_ERROR,
                         "error while writing payload: %" PRIu8
                         ", cancel exchange",
                         result);
            finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_REQUEST);
            return;
        }
        // there is no client request that contain payload in both request and
        // response
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
            finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_REQUEST);
            return;
        }
    }

    if (ctx->block_transfer || ctx->base_msg.payload_size != 0) {
        if (!ctx->request_prepared && is_separate_response_mode(in_out_msg)) {
            ctx->request_prepared = true;
            goto send_empty_ack;
        }
        ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
        reset_exchange_params(ctx);
        init_msd_id(ctx, &ctx->base_msg);
        if (token_create(&ctx->base_msg.token)) {
            finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_REQUEST);
        }
        *in_out_msg = ctx->base_msg;
        return;
    } else {
        if (is_separate_response_mode(in_out_msg)) {
            // last block of the response, from external module point of view
            // exchange can be finished here, call completion callback and set
            // default callback to prevent the second call
            ctx->handlers.completion(ctx->handlers.arg, in_out_msg, 0);
            ctx->handlers.completion = default_exchange_completion_handler;
            ctx->confirmable = false;
            goto send_empty_ack;
        }
        exchange_log(L_TRACE, "exchange finished");
        finalize_exchange(ctx, in_out_msg, _ANJ_EXCHANGE_RESULT_SUCCESS);
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
#ifdef ANJ_WITH_CACHE
    // store the response in case of retransmission
    _anj_exchange_cache_add(ctx->cache, &ctx->tx_params, in_out_msg);
#endif // ANJ_WITH_CACHE
    return;
}

static void handle_server_request(_anj_exchange_ctx_t *ctx,
                                  _anj_coap_msg_t *in_out_msg) {
    uint8_t response_code = ANJ_COAP_CODE_EMPTY;
    size_t payload_size = 0;
    // For block transfer, token in next request don't have to be the same
    // that's why we don't check token equality, but operation type and block
    // number. In case of observe/observe-composite/notify/cancel
    // observation/cancel observation-composite operations and block transfer,
    // the server responds with a READ or READ composite operation.
    if (in_out_msg->operation != ctx->op
            && !((ctx->op == ANJ_OP_INF_NON_CON_NOTIFY
                  || ctx->op == ANJ_OP_INF_OBSERVE
                  || ctx->op == ANJ_OP_INF_OBSERVE_COMP
                  || ctx->op == ANJ_OP_INF_CANCEL_OBSERVE
                  || ctx->op == ANJ_OP_INF_CANCEL_OBSERVE_COMP)
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
        exchange_log(L_INFO, "different request, respond with %s",
                     COAP_CODE_FORMAT(ANJ_COAP_CODE_SERVICE_UNAVAILABLE));
        goto send_service_unavailable;
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
#ifdef ANJ_WITH_CACHE
    // store the response in case of retransmission
    _anj_exchange_cache_add(ctx->cache, &ctx->tx_params, in_out_msg);
#endif // ANJ_WITH_CACHE
    return;

send_service_unavailable:
    ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
    in_out_msg->operation = ANJ_OP_RESPONSE;
    in_out_msg->payload_size = 0;
    in_out_msg->msg_code = ANJ_COAP_CODE_SERVICE_UNAVAILABLE;
    in_out_msg->block.block_type = ANJ_OPTION_BLOCK_NOT_DEFINED;
#ifdef ANJ_WITH_CACHE
    // store the response in case of retransmission
    _anj_exchange_cache_add(ctx->cache, &ctx->tx_params, in_out_msg);
#endif // ANJ_WITH_CACHE
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

    if (exchange_param_init(ctx)) {
        result = ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
        goto respone_with_error;
    }

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
        result = response_msg_code;
        goto respone_with_error;
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

respone_with_error:
    if (result && result != _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED) {
        exchange_log(L_ERROR, "response with error code: %" PRIu8, result);
        in_out_msg->msg_code = result;
        in_out_msg->payload_size = 0;
        in_out_msg->block.block_type = ANJ_OPTION_BLOCK_NOT_DEFINED;
        ctx->block_transfer = false;
        ctx->msg_code = result;
    }
    exchange_log(L_TRACE, "new response created");
    ctx->state = ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION;
#ifdef ANJ_WITH_CACHE
    // store the response in case of retransmission
    _anj_exchange_cache_add(ctx->cache, &ctx->tx_params, in_out_msg);
#endif // ANJ_WITH_CACHE
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

    if (*op != ANJ_OP_INF_CON_NOTIFY && *op != ANJ_OP_INF_NON_CON_NOTIFY) {
        if (token_create(&in_out_msg->token)) {
            return finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_REQUEST);
        }
    }
    assert(&in_out_msg->token.size);
    init_msd_id(ctx, in_out_msg);

    if (exchange_param_init(ctx)) {
        return finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_REQUEST);
    }

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
            if (exchange_param_init(ctx)) {
                return finalize_exchange(ctx, NULL,
                                         _ANJ_EXCHANGE_ERROR_REQUEST);
            }
        }
    } else if (result) {
        exchange_log(L_ERROR, "error while preparing request: %" PRIu8, result);
        return finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_REQUEST);
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
    // new message can't be processed when waiting for send confirmation
    assert(event != ANJ_EXCHANGE_EVENT_NEW_MSG
           || ctx->state != ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION);
    // send confirmation can't be processed when waiting for message
    assert(event != ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION
           || ctx->state != ANJ_EXCHANGE_STATE_WAITING_MSG);
#ifdef ANJ_WITH_CACHE
    assert(ctx->cache == NULL || !ctx->cache->handling_retransmission);
#endif // ANJ_WITH_CACHE

    if (ctx->state == ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION) {
        handle_send_confirmation(ctx, event);

        // used in case of separate response, the next request can
        // be sent because an empty response has been sent and no error has
        // occurred in the meantime
        if (ctx->state == ANJ_EXCHANGE_STATE_WAITING_MSG
                && ctx->request_prepared) {
            ctx->request_prepared = false;
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
            ctx->send_confirmation_timeout_timestamp =
                    anj_time_monotonic_add(anj_time_monotonic_now(),
                                           _ANJ_EXCHANGE_COAP_PROCESSING_DELAY);
            return ANJ_EXCHANGE_STATE_MSG_TO_SEND;
        }
        if (ctx->state != ANJ_EXCHANGE_STATE_WAITING_MSG) {
            return ctx->state;
        }
    }

    if (timeout_occurred(ctx->timeout_timestamp)) {
        if (ctx->server_request) {
            exchange_log(L_ERROR, "server request timeout occurred");
            return finalize_exchange(ctx, NULL, _ANJ_EXCHANGE_ERROR_TIMEOUT);
        } else {
            if (ctx->retry_count < ctx->tx_params.max_retransmit) {
                ctx->retry_count++;
                anj_time_monotonic_t time_real_now = anj_time_monotonic_now();
                ctx->timeout_timestamp = anj_time_monotonic_add(
                        time_real_now,
                        anj_time_duration_mul(ctx->timeout,
                                              1 << ctx->retry_count));
                ctx->send_confirmation_timeout_timestamp =
                        anj_time_monotonic_add(
                                time_real_now,
                                _ANJ_EXCHANGE_COAP_PROCESSING_DELAY);
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

void _anj_exchange_terminate(_anj_exchange_ctx_t *ctx, int reason) {
    assert(ctx);
    assert(reason == _ANJ_EXCHANGE_ERROR_NETWORK
           || reason == _ANJ_EXCHANGE_ERROR_PROTOCOL
           || reason == _ANJ_EXCHANGE_ERROR_TERMINATED);
    if (ctx->state == ANJ_EXCHANGE_STATE_FINISHED) {
        return;
    }
    finalize_exchange(ctx, NULL, reason);
    if (reason == _ANJ_EXCHANGE_ERROR_TERMINATED) {
        exchange_log(L_INFO, "exchange terminated manually");
    } else if (reason == _ANJ_EXCHANGE_ERROR_NETWORK) {
        exchange_log(L_INFO, "exchange terminated due to network error");
    } else {
        exchange_log(L_INFO,
                     "exchange terminated due to protocol processing error");
    }
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
        _anj_exchange_ctx_t *ctx, const anj_exchange_udp_tx_params_t *params) {
    assert(ctx && params);
    if (params->ack_random_factor < 1.0
            || anj_time_duration_lt(params->ack_timeout,
                                    anj_time_duration_new(1000,
                                                          ANJ_TIME_UNIT_MS))) {
        exchange_log(L_ERROR, "invalid UDP TX params");
        return -1;
    }
    ctx->tx_params = *params;
    exchange_log(L_DEBUG,
                 "UDP TX params set: ack_timeout=%sms"
                 ", ack_random_factor=%f, max_retransmit=%" PRIu16,
                 ANJ_TIME_DURATION_AS_STRING(ctx->tx_params.ack_timeout,
                                             ANJ_TIME_UNIT_MS),
                 ctx->tx_params.ack_random_factor,
                 ctx->tx_params.max_retransmit);
    return 0;
}

void _anj_exchange_set_server_request_timeout(
        _anj_exchange_ctx_t *ctx, anj_time_duration_t server_exchange_timeout) {
    assert(ctx);
    assert(anj_time_duration_gt(server_exchange_timeout,
                                ANJ_TIME_DURATION_ZERO));
    exchange_log(L_DEBUG, "exchange max time set: %sms",
                 ANJ_TIME_DURATION_AS_STRING(server_exchange_timeout,
                                             ANJ_TIME_UNIT_MS));
    ctx->server_exchange_timeout = server_exchange_timeout;
}

int _anj_exchange_init(_anj_exchange_ctx_t *ctx) {
    assert(ctx);
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = ANJ_EXCHANGE_STATE_FINISHED;
    ctx->tx_params = ANJ_EXCHANGE_UDP_TX_PARAMS_DEFAULT;
    ctx->server_exchange_timeout = ANJ_EXCHANGE_SERVER_REQUEST_TIMEOUT;
    if (anj_rng_generate((uint8_t *) &ctx->msg_id, sizeof(ctx->msg_id))) {
        exchange_log(L_ERROR, "Could not generate random number");
        return -1;
    }
    exchange_log(L_DEBUG, "context initialized");
    return 0;
}

#ifdef ANJ_WITH_CACHE
void _anj_exchange_setup_cache(_anj_exchange_ctx_t *ctx,
                               _anj_exchange_cache_t *cache) {
    assert(ctx && cache);
    ctx->cache = cache;
#    ifdef ANJ_WITH_CACHE
    // there is no uint16_t value for "invalid MID" so we invalidate expiration
    // time
    ctx->cache->cache_recent.expiration_time = ANJ_TIME_REAL_INVALID;
    ctx->cache->handling_retransmission = false;
#        if ANJ_CACHE_ENTRIES_NUMBER > 1
    for (uint8_t i = 0; i < ANJ_ARRAY_SIZE(ctx->cache->cache_non_recent); i++) {
        ctx->cache->cache_non_recent[i].expiration_time = ANJ_TIME_REAL_INVALID;
    }
#        endif // ANJ_CACHE_ENTRIES_NUMBER > 1
#    endif     // ANJ_WITH_CACHE
}
#endif // ANJ_WITH_CACHE
