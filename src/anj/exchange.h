/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_EXCHANGE_H
#    define SRC_ANJ_EXCHANGE_H

#    include <anj/time.h>
#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/defs.h>
#    include <anj/log.h>

#    define ANJ_INTERNAL_INCLUDE_EXCHANGE
#    include <anj_internal/exchange.h> // IWYU pragma: export
#    undef ANJ_INTERNAL_INCLUDE_EXCHANGE
// IWYU pragma: no_include "anj_internal/exchange.h"

#    define exchange_log(...) anj_log(exchange, __VA_ARGS__)

/**
 * From RFC7252: "PROCESSING_DELAY is the time a node takes to turn around a
 * Confirmable message into an acknowledgement. We assume the node will attempt
 * to send an ACK before having the sender timeout, so as a conservative
 * assumption we set it equal to ACK_TIMEOUT"
 *
 * If context is in @ref ANJ_EXCHANGE_STATE_WAITING_SEND_ACK state for
 * longer than @ref _ANJ_EXCHANGE_COAP_PROCESSING_DELAY_MS value, the exchange
 * is canceled.
 *
 * The purpose of this delay is to break the exchange if send function blocks
 * for too long (possible error in the implementation of network layer).
 */
#    define _ANJ_EXCHANGE_COAP_PROCESSING_DELAY \
        anj_time_duration_new(2, ANJ_TIME_UNIT_S)

/**
 * See https://tools.ietf.org/html/rfc7252#section-4.8.2
 * Unit: seconds
 */
#    define _ANJ_EXCHANGE_COAP_MAX_LATENCY \
        anj_time_duration_new(100, ANJ_TIME_UNIT_S)

/**
 * Type of event related to the exchange module.
 * Used in @ref _anj_exchange_process function.
 */
typedef enum {
    /* New msg from the LwM2M Server. */
    ANJ_EXCHANGE_EVENT_NEW_MSG,
    /* Message was sent successfully. */
    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
    /* No event, just check the exchange state. */
    ANJ_EXCHANGE_EVENT_NONE
} _anj_exchange_event_t;

/**
 * Initializes the exchange module. Should be called after processing the first
 * message of the LwM2M Server request. @p response_msg_code will be used in the
 * response, if any handler returns an error it will be used instead. After this
 * function is called, the user should call @ref _anj_exchange_process in a loop
 * until the exchange is finished. Exchange module handles the whole process of
 * exchange, including block transfer, retransmissions and timeouts.
 *
 * This function should be used to handle all LwM2M server requests.
 *
 * IMPORTANT: The @p buff can't be touched during the exchange until @ref
 * _anj_exchange_ongoing_exchange returns false. It can be used multiple times.
 * Pointer to the @p buff is stored internally in the context.
 *
 * @param        ctx               Exchange context.
 * @param        response_msg_code Message code of the response, must be one of
 *                                 the ANJ_COAP_CODE_* codes.
 * @param[inout] in_out_msg        Request to process, response will be stored
 *                                 in it.
 * @param        handlers          Exchange handlers, all callbacks are
 *                                 optional. Pointer content is copied.
 * @param[inout] buff              Payload buffer used for the response.
 * @param        buff_len          Length of the payload buffer.
 *
 * @returns Initial state of the exchange.
 */
_anj_exchange_state_t
_anj_exchange_new_server_request(_anj_exchange_ctx_t *ctx,
                                 uint8_t response_msg_code,
                                 _anj_coap_msg_t *in_out_msg,
                                 _anj_exchange_handlers_t *handlers,
                                 uint8_t *buff,
                                 size_t buff_len);

/**
 * Initializes the exchange module. Should be used for the LwM2M Client
 * requests. @p in_out_msg contains the request to process. @ref
 * _anj_exchange_read_payload_t if present in @p handlers will be called to get
 * the payload for the message. Block option will be added to the message if
 * needed. After this function is called, the user should call @ref
 * _anj_exchange_process in a loop until the exchange is finished. Exchange
 * module handles the whole process of exchange, including block transfer,
 * retransmissions and timeouts. If request is not confirmable, but the payload
 * is too big to fit in one message, the block transfer will be used and message
 * type will change to confirmable.
 *
 * This function should be used to handle exchanges of the following operations:
 *  - ANJ_OP_BOOTSTRAP_REQ,
 *  - ANJ_OP_BOOTSTRAP_PACK_REQ,
 *  - ANJ_OP_REGISTER,
 *  - ANJ_OP_UPDATE,
 *  - ANJ_OP_DEREGISTER,
 *  - ANJ_OP_INF_CON_SEND,
 *  - ANJ_OP_INF_NON_CON_SEND,
 *  - ANJ_OP_INF_CON_NOTIFY,
 *  - ANJ_OP_INF_NON_CON_NOTIFY.
 *
 * For Notify messages, the @p in_out_msg must contain the token and
 * observation number.
 *
 * IMPORTANT: The @p buff can't be touched during the exchange until @ref
 * _anj_exchange_ongoing_exchange returns false. It can be used multiple times.
 * Pointer to the @p buff is stored internally in the context.
 *
 * @param        ctx        Exchange context.
 * @param[inout] in_out_msg Request to process.
 * @param        handlers   Exchange handlers, all callbacks are optional.
 *                          Pointer content is copied.
 * @param[out]   buff       Payload buffer.
 * @param        buff_len   Length of the payload buffer.
 *
 * @returns Initial state of the exchange.
 */
_anj_exchange_state_t
_anj_exchange_new_client_request(_anj_exchange_ctx_t *ctx,
                                 _anj_coap_msg_t *in_out_msg,
                                 _anj_exchange_handlers_t *handlers,
                                 uint8_t *buff,
                                 size_t buff_len);

/**
 * Processes the exchange. The function should be called after @ref
 * _anj_exchange_new_server_request or @ref _anj_exchange_new_client_request in
 * a loop until the exchange is finished (@ref ANJ_EXCHANGE_STATE_FINISHED is
 * returned).
 *
 * If any function of this API returns @ref ANJ_EXCHANGE_STATE_MSG_TO_SEND, the
 * user should send the message and than call @ref _anj_exchange_process with
 * the @p event set to @ref ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, or cancel the
 * exchange if sending fails. If exchange is in @ref
 * ANJ_EXCHANGE_STATE_WAITING_MSG state, every incoming message should be passed
 * to the function with @p event set to @ref ANJ_EXCHANGE_EVENT_NEW_MSG. If
 * there is no message to process, or there is no information about sending
 * result, this function should be called in intervals with @p event set to @ref
 * ANJ_EXCHANGE_EVENT_NONE. During that call exchange module can decide to
 * resend the message, or cancel the exchange in case of timeout. The exchange
 * can be canceled by calling @ref _anj_exchange_terminate.
 *
 * This module handles:
 *  - retransmissions and timeouts RFC 7252,
 *  - block-wise transfers RFC 7959,
 *  - separate responses for LwM2M Client requests,
 *  - block-wise Early negotiation for LwM2M Server requests, except for the
 *    Composite operations with block-wise transfer in both directions,
 *  - NSTART = 1, which means that for additional LwM2M Server request will
 *    respond with the @ref ANJ_COAP_CODE_SERVICE_UNAVAILABLE code,
 *  - block-wise transfer in both directions, but only with the same block size,
 * Late negotiation mechanism for block-wise transfer is not supported. Changing
 * the size of a block during an ongoing exchange will cause an error and abort
 * the process.
 *
 * @param        ctx          Exchange context.
 * @param[in]    event        Type of event; @ref _anj_exchange_event_t.
 * @param[inout] in_out_msg   Message to process, or space for new message.
 *
 * @returns Current state of the exchange or @ref ANJ_EXCHANGE_STATE_MSG_TO_SEND
 *          if the user should send the message.
 */
_anj_exchange_state_t _anj_exchange_process(_anj_exchange_ctx_t *ctx,
                                            _anj_exchange_event_t event,
                                            _anj_coap_msg_t *in_out_msg);

/**
 * Terminates the exchange and stop the ongoing operation. In result
 * @ref _anj_exchange_completion_t will be called with @p reason as the
 * argument. If the exchange is already finished, this function does nothing.
 *
 * @param ctx     Exchange context.
 * @param reason  Reason of termination. Possible values:
 *               - @ref _ANJ_EXCHANGE_ERROR_NETWORK,
 *               - @ref _ANJ_EXCHANGE_ERROR_PROTOCOL,
 *               - @ref _ANJ_EXCHANGE_ERROR_TERMINATED.
 */
void _anj_exchange_terminate(_anj_exchange_ctx_t *ctx, int reason);

/**
 * Check if there is an ongoing exchange. Only one exchange can be processed at
 * a time.
 *
 * IMPORTANT: Exchange API can process any number of requests at the same time,
 * the use of this function is related to the use and blocking of related
 * modules, such as data model or observation context.
 *
 * @param ctx  Exchange context.
 *
 * @returns True if the exchange is ongoing and next exchange can't be started.
 */
bool _anj_exchange_ongoing_exchange(_anj_exchange_ctx_t *ctx);

/**
 * Gets the current state of the exchange. This function can't return @ref
 * ANJ_EXCHANGE_STATE_MSG_TO_SEND, if other function of this API returns this
 * state, the state immediately changes. @ref ANJ_EXCHANGE_STATE_FINISHED also
 * indicates an idle state.
 *
 * @param ctx  Exchange context.
 *
 * @returns Current state of the exchange.
 */
_anj_exchange_state_t _anj_exchange_get_state(_anj_exchange_ctx_t *ctx);

/**
 * Sets the CoAP transmission parameters for given context. If never called, the
 * default values will be used (RFC 7252).
 *
 * @param ctx     Exchange context,
 * @param params  CoAP transmission parameters.
 *
 * @returns 0 on success, negative value if transmission parameters are invalid.
 */
int _anj_exchange_set_udp_tx_params(_anj_exchange_ctx_t *ctx,
                                    const anj_exchange_udp_tx_params_t *params);

/**
 * Sets the maximum time of the CoAP exchange - this is the time to wait for the
 * next block of the LwM2M Server request. The default value is @ref
 * ANJ_EXCHANGE_SERVER_REQUEST_TIMEOUT. If the time is exceeded, the
 * exchange will be canceled.
 *
 * @param ctx                     Exchange context,
 * @param server_exchange_timeout Maximum time of the CoAP exchange
 */
void _anj_exchange_set_server_request_timeout(
        _anj_exchange_ctx_t *ctx, anj_time_duration_t server_exchange_timeout);

/**
 * Initializes the exchange module context. Should be called once for each
 * context. Specific exchange context is related with one server connection.
 *
 * @param ctx Exchange context
 *
 * @returns 0 on success, negative value in case of error.
 */
int _anj_exchange_init(_anj_exchange_ctx_t *ctx);

#    ifdef ANJ_WITH_CACHE
void _anj_exchange_setup_cache(_anj_exchange_ctx_t *ctx,
                               _anj_exchange_cache_t *cache);
#    endif // ANJ_WITH_CACHE

#endif // SRC_ANJ_EXCHANGE_H
