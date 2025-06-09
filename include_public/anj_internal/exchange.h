/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_EXCHANGE_H
#define ANJ_INTERNAL_EXCHANGE_H

#ifndef ANJ_INTERNAL_INCLUDE_EXCHANGE
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_EXCHANGE

#define ANJ_INTERNAL_INCLUDE_UTILS
#include <anj_internal/utils.h>
#undef ANJ_INTERNAL_INCLUDE_UTILS

#define ANJ_INTERNAL_INCLUDE_COAP
#include <anj_internal/coap.h>
#undef ANJ_INTERNAL_INCLUDE_COAP

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @anj_internal_api_do_not_use
 *  All possible states of exchange
 */
typedef enum {
    // There is a message to send
    ANJ_EXCHANGE_STATE_MSG_TO_SEND,
    // Waiting for information about sending result
    ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION,
    // Waiting for incoming message
    ANJ_EXCHANGE_STATE_WAITING_MSG,
    // Exchange is finished, also idle state
    ANJ_EXCHANGE_STATE_FINISHED
} _anj_exchange_state_t;

/**
 * @anj_internal_api_do_not_use
 * CoAP transmission params object.
 *
 * For LwM2M client requests, the timeout is random duration between
 * ack_timeout_ms and ack_timeout_ms * ack_random_factor. For default values it
 * is range from 2 to 3 seconds. Each retransmission doubles the timeout value.
 */
typedef struct {
    /** RFC 7252: ACK_TIMEOUT */
    uint64_t ack_timeout_ms;
    /** RFC 7252: ACK_RANDOM_FACTOR */
    double ack_random_factor;
    /** RFC 7252: MAX_RETRANSMIT */
    uint16_t max_retransmit;
} _anj_exchange_udp_tx_params_t;

/**
 * Output parameters returned from @ref anj_exchange_read_payload_t handler.
 *
 * It complements the raw payload data with information about content format,
 * actual length, and optionally created object/instance identifiers.
 */
typedef struct {
    /** Actual length of the payload written to the buffer. */
    size_t payload_len;
    /** Content format of the payload. */
    uint16_t format;
    /**
     * Whether the operation involved creation of a new object instance path.
     * If true, the `created_oid` and `created_iid` fields are valid.
     * Must not be set if LwM2M server request contained an instance ID.
     */
    bool with_create_path;
    /** Object ID of the created object. */
    anj_oid_t created_oid;
    /** Instance ID of the created object. */
    anj_iid_t created_iid;
} _anj_exchange_read_result_t;

/**
 * @anj_internal_api_do_not_use
 * A handler called by exchange module to provide payload from incoming message.
 * If @ref _anj_exchange_new_server_request was called with @p response_msg_code
 * set to an error code, than the handler will not be called.
 *
 * If @p last_block is set to true, the context used to build the payload should
 * be freed in this function call.
 *
 * @param arg_ptr     Additional user data passed when the function is called.
 * @param payload     Payload buffer.
 * @param payload_len Length of the payload.
 * @param last_block  Flag indicating if this is the last block of payload.
 *
 * @returns The handler should return:
 *  - 0 on success,
 *  - a ANJ_COAP_CODE_* code in case of error.
 */
typedef uint8_t _anj_exchange_write_payload_t(void *arg_ptr,
                                              uint8_t *payload,
                                              size_t payload_len,
                                              bool last_block);

/**
 * A handler called by exchange module to get payload for outgoing message.
 * If there is no payload to send, the handler should return 0 and set @p
 * out_payload_len to 0.
 * If @ref anj_exchange_new_server_request was called with @p response_msg_code
 * set to an error code, than the handler will not be called.
 *
 * Context used to read the payload should be freed before this function returns
 * value different than @ref _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED.
 *
 * @param      arg_ptr          Additional user data passed when the function is
 *                              called.
 * @param[out] buff             Payload buffer.
 * @param      buff_len         Length of the payload buffer.
 * @param[out] out_params       All parameters of the payload that might be set
 *                              in callback.
 *
 * @returns The handler should return:
 *  - 0 on success,
 *  - @ref _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED if @p buff is too small to fit
 *    the whole payload,
 *  - a ANJ_COAP_CODE_* code in case of error.
 */
typedef uint8_t
_anj_exchange_read_payload_t(void *arg_ptr,
                             uint8_t *buff,
                             size_t buff_len,
                             _anj_exchange_read_result_t *out_params);

/**
 * @anj_internal_api_do_not_use
 * A handler called by exchange module to notify about completion of the
 * exchange. In case of timeout @p result is set to @ref
 * _ANJ_EXCHANGE_ERROR_TIMEOUT. For exchange cancellation
 * @p result is set to @ref _ANJ_EXCHANGE_ERROR_TERMINATED. If LwM2M server
 * responded with error code, the error code is passed in @p result, and @p
 * response is NULL.
 *
 * @param arg_ptr   Additional user data passed when the function is called.
 * @param response  Final server response message, provided only if the exchange
 *                  is finished successfully, NULL otherwise.
 * @param result    Result of the exchange, 0 on success, a ANJ_COAP_CODE_*
 *                  code or ANJ_EXCHANGE_ERROR_* in case of error.
 */
typedef void _anj_exchange_completion_t(void *arg_ptr,
                                        const _anj_coap_msg_t *response,
                                        int result);

/**
 * @anj_internal_api_do_not_use
 * Exchange handlers. All handlers must be set.
 */
typedef struct {
    _anj_exchange_write_payload_t *write_payload;
    _anj_exchange_read_payload_t *read_payload;
    _anj_exchange_completion_t *completion;
    void *arg;
} _anj_exchange_handlers_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    _anj_exchange_state_t state;
    _anj_exchange_handlers_t handlers;
    // server response will be provided as a separate message
    bool separate_response;
    // indicate if the request is from the LwM2M Server
    bool server_request;
    // indicate if we expect a response from the LwM2M Server
    bool confirmable;
    bool block_transfer;
    uint8_t *payload_buff;
    uint16_t block_size;
    // used in separate response mode
    bool request_prepared;
    uint32_t block_number;

    uint64_t server_exchange_timeout;
    _anj_exchange_udp_tx_params_t tx_params;
    uint16_t retry_count;
    uint64_t send_ack_timeout_timestamp_ms;
    uint64_t timeout_timestamp_ms;
    uint64_t timeout_ms;
    // based on pseudo-random number generator, used for timeout calculation
    // subsequent requests should have different timeout values
    _anj_rand_seed_t timeout_rand_seed;

    uint8_t msg_code;
    _anj_coap_msg_t base_msg;
    _anj_op_t op;
} _anj_exchange_ctx_t;

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_EXCHANGE_H
