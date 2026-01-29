/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
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

#include <anj/time.h>

#define ANJ_INTERNAL_INCLUDE_UTILS
#include <anj_internal/utils.h> // IWYU pragma: export
#undef ANJ_INTERNAL_INCLUDE_UTILS

#define ANJ_INTERNAL_INCLUDE_COAP
#include <anj_internal/coap.h> // IWYU pragma: export
#undef ANJ_INTERNAL_INCLUDE_COAP

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Used for block transfers. The buffer is too small to fit the whole payload.
 */
#define _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED 1

/**
 * Completion code in @ref _anj_exchange_completion_t when the exchange
 * finished successfully.
 */
#define _ANJ_EXCHANGE_RESULT_SUCCESS 0

/**
 * Completion code when the exchange ended due to a timeout.
 */
#define _ANJ_EXCHANGE_ERROR_TIMEOUT -1

/**
 * Completion code when the exchange ended due to an error response
 * from the server (e.g. ACK with error code or RST).
 */
#define _ANJ_EXCHANGE_ERROR_SERVER_RESPONSE -2

/**
 * Completion code when the exchange ended due to an error reported
 * while processing the request.
 *
 * This may happen in two cases:
 * - a failure occurs while starting to handle a request,
 * - a user callback (e.g., @ref _anj_exchange_read_payload_t or
 *   @ref _anj_exchange_write_payload_t) returned an error.
 */
#define _ANJ_EXCHANGE_ERROR_REQUEST -3

/**
 * Completion code when the exchange ended due to a network error.
 * Should be set via @ref _anj_exchange_terminate in such cases.
 */
#define _ANJ_EXCHANGE_ERROR_NETWORK -4

/**
 * Completion code when the exchange was terminated due to a protocol processing
 * error (e.g. failure while decoding an incoming message, etag mismatch).
 * Should be set via @ref _anj_exchange_terminate in such cases.
 */
#define _ANJ_EXCHANGE_ERROR_PROTOCOL -5

/**
 * Generic completion code when the exchange was terminated due to user request
 * (e.g. connection reset). Should be set via @ref _anj_exchange_terminate in
 * such cases.
 */
#define _ANJ_EXCHANGE_ERROR_TERMINATED -6

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
 * exchange. @p result provides information if the exchange was successful or
 * if it failed.
 *
 * @note: This callback for Separate Response mode is called before last Empty
 *        ACK is sent.
 *
 * @param arg_ptr   Additional user data passed when the function is called.
 * @param response  Final server response message, provided only for client
 *                  initiated exchanges (confirmable) and if answer was
 *                  received. NULL otherwise.
 * @param result    Result of the exchange, @ref _ANJ_EXCHANGE_RESULT_SUCCESS
 *                  on success, a _ANJ_EXCHANGE_ERROR_* value in case of error.
 */
typedef void _anj_exchange_completion_t(void *arg_ptr,
                                        const _anj_coap_msg_t *response,
                                        int result);

/**
 * @anj_internal_api_do_not_use
 * Exchange handlers. If handler is not set, the exchange module will use
 * default implementation.
 */
typedef struct {
    _anj_exchange_write_payload_t *write_payload;
    _anj_exchange_read_payload_t *read_payload;
    _anj_exchange_completion_t *completion;
    void *arg;
} _anj_exchange_handlers_t;

#ifdef ANJ_WITH_CACHE
/**
 * @anj_internal_api_do_not_use
 * Single cache entry for messages older than the latest one
 */
typedef struct {
    anj_time_monotonic_t expiration_time;
    uint16_t mid;
} _anj_exchange_cache_msg_non_recent_t;

/**
 * @anj_internal_api_do_not_use
 * Single cache entry for the latest message
 */
typedef struct {
    anj_time_monotonic_t expiration_time;
    _anj_coap_msg_t response;
    uint8_t payload[ANJ_OUT_PAYLOAD_BUFFER_SIZE];
} _anj_exchange_cache_msg_recent_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    // total cached responses - ANJ_CACHE_ENTRIES_NUMBER - one most recent with
    // payload and ANJ_CACHE_ENTRIES_NUMBER - 1 responses with only MID saved
    _anj_exchange_cache_msg_recent_t cache_recent;
#    if ANJ_CACHE_ENTRIES_NUMBER > 1
    _anj_exchange_cache_msg_non_recent_t
            cache_non_recent[ANJ_CACHE_ENTRIES_NUMBER - 1];
#    endif // ANJ_CACHE_ENTRIES_NUMBER > 1
    bool handling_retransmission;
} _anj_exchange_cache_t;
#endif // ANJ_WITH_CACHE

/** @anj_internal_api_do_not_use */
typedef struct {
    _anj_exchange_state_t state;
    _anj_exchange_handlers_t handlers;
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

    anj_time_duration_t server_exchange_timeout;
    anj_exchange_udp_tx_params_t tx_params;
    uint16_t retry_count;
    anj_time_monotonic_t send_confirmation_timeout_timestamp;
    anj_time_monotonic_t timeout_timestamp;
    anj_time_duration_t timeout;

    uint16_t msg_id;

    uint8_t msg_code;
    _anj_coap_msg_t base_msg;

#ifdef ANJ_WITH_CACHE
    // must be set by _anj_exchange_setup_cache after context initialization
    _anj_exchange_cache_t *cache;
#endif // ANJ_WITH_CACHE

    _anj_op_t op;
} _anj_exchange_ctx_t;

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_EXCHANGE_H
