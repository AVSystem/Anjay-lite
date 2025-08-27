/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_LWM2M_SEND_H
#    define ANJ_LWM2M_SEND_H

#    include <anj/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_LWM2M_SEND

#        define ANJ_SEND_ID_ALL UINT16_MAX

/**
 * Result passed to #anj_send_finished_handler_t: Server confirmed successful
 * message delivery.
 */
#        define ANJ_SEND_SUCCESS 0

/**
 * Result passed to #anj_send_finished_handler_t: No response from Server was
 * received in expected time.
 */
#        define ANJ_SEND_ERR_TIMEOUT -1

/**
 * Result passed to #anj_send_finished_handler_t: Sending the message was
 * aborted. There are several reasons why this might happen:
 * - the message was cancelled by the user by calling @ref anj_send_abort
 * - because of a network error or other unexpected condition (e,g. Mute Send
 *   resource changed),
 * - registration session ended.
 */
#        define ANJ_SEND_ERR_ABORT -2

/**
 * Result passed to #anj_send_finished_handler_t: Server rejected the message
 * and response with 4.xx code.
 */
#        define ANJ_SEND_ERR_REJECTED -3

/**
 * Can be returned by @ref anj_send_abort if no request with given ID was found.
 */
#        define ANJ_SEND_ERR_NO_REQUEST_FOUND -4

/**
 * Can be returned by @ref anj_send_new_request if there is no space for new
 * request - @ref ANJ_LWM2M_SEND_QUEUE_SIZE has been reached.
 */
#        define ANJ_SEND_ERR_NO_SPACE -5

/**
 * Can be returned by @ref anj_send_new_request if request can't be sent in
 * current state of the library:
 *      - library is not in @ref ANJ_CONN_STATUS_REGISTERED or
 *        @ref ANJ_CONN_STATUS_QUEUE_MODE state,
 *      - Mute Send resource is set to true.
 */
#        define ANJ_SEND_ERR_NOT_ALLOWED -6

/**
 * Can be returned by @ref anj_send_new_request because provided data is not
 * valid.
 */
#        define ANJ_SEND_ERR_DATA_NOT_VALID -7

/**
 * Content format of the message payload to be sent.
 */
typedef enum {
#        ifdef ANJ_WITH_SENML_CBOR
    ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
#        endif // ANJ_WITH_SENML_CBOR
#        ifdef ANJ_WITH_LWM2M_CBOR
    ANJ_SEND_CONTENT_FORMAT_LWM2M_CBOR,
#        endif // ANJ_WITH_LWM2M_CBOR
} anj_send_content_format_t;

/**
 * A handler called if acknowledgement for LwM2M Send operation is received from
 * the Server or message delivery fails.
 *
 * @param anjay   Anjay object for which the Send operation was attempted.
 * @param send_id ID of the Send operation that was attempted.
 * @param result  Result of the Send message delivery attempt. May be one of:
 *                   - @ref ANJ_SEND_SUCCESS,
 *                   - @ref ANJ_SEND_ERR_TIMEOUT,
 *                   - @ref ANJ_SEND_ERR_ABORT,
 *                   - @ref ANJ_SEND_ERR_REJECTED.
 * @param data    Data defined by user passed into the handler.
 */
typedef void anj_send_finished_handler_t(anj_t *anjay,
                                         uint16_t send_id,
                                         int result,
                                         void *data);

/**
 * Structure representing a single LwM2M Send message to be sent.
 */
typedef struct {
    /**
     * Array of records to be sent.
     */
    const anj_io_out_entry_t *records;
    /**
     * Number of records in the @ref records array.
     */
    size_t records_cnt;
    /**
     * Handler called after the final attempt to deliver the message,
     * regardless of success or failure.
     */
    anj_send_finished_handler_t *finished_handler;
    /**
     * Data to be passed to the @ref finished_handler.
     */
    void *data;
    /**
     * Content format of the payload.
     */
    anj_send_content_format_t content_format;
} anj_send_request_t;

/**
 * Registers a new LwM2M Send request to be sent.
 *
 * If this function returns 0, the finish handler will be called with the result
 * of the operation. A Send request is allowed only if a registration session is
 * active. The request will be processed when possible — if there is no ongoing
 * exchange, only Update messages have higher priority.
 *
 * If multiple Send requests are queued, the oldest one is sent first.
 * Only one Send request is processed at a time.
 *
 * @note Only SenML CBOR provides support for timestamps.
 *
 * @note If @p send_request contains multiple records with the same
 *       path listed more than once, the request will fail when encoded as
 *       LwM2M CBOR — even if the records have different timestamps.
 *
 *       This is because LwM2M CBOR maps resource paths to keys in a CBOR map,
 *       which requires keys (i.e., paths) to be unique. Having duplicate
 *       paths in the encoded structure is invalid and not supported.
 *
 * @note The @p send_request structure is not copied internally.
 *       The pointer must remain valid and unchanged until the send operation
 *       completes and the associated finish handler has been called.
 *
 * @param      anj          Anjay object to operate on.
 * @param      send_request Structure representing the message to be sent.
 *                          Must remain valid until the operation is finished.
 * @param[out] out_send_id  Pointer to a variable where the ID of the new Send
 *                          operation will be stored. May be NULL if the ID is
 *                          not needed. The ID remains valid until the
 *                          associated @ref anj_send_finished_handler_t is
 *                          invoked.
 *
 * @returns 0 on success, a negative value in case of an error:
 *          - @ref ANJ_SEND_ERR_NO_SPACE if there is no space for new request,
 *          - @ref ANJ_SEND_ERR_NOT_ALLOWED if the request can't be sent in
 *            current state of the library,
 *          - @ref ANJ_SEND_ERR_DATA_NOT_VALID if provided data is not valid.
 */
int anj_send_new_request(anj_t *anj,
                         const anj_send_request_t *send_request,
                         uint16_t *out_send_id);

/**
 * Aborts a LwM2M Send request with given ID. If the request is still in the
 * queue to be sent, it will be removed. If the request is already being sent,
 * the exchange will be cancelled first. For both cases @ref ANJ_SEND_ERR_ABORT
 * will be passed to the @ref anj_send_finished_handler_t as a result.
 *
 * @param anj     Anjay object to operate on.
 * @param send_id ID of the Send operation to be aborted.
 *                If @ref ANJ_SEND_ID_ALL is passed, all pending requests will
 *                be aborted.
 *
 * @returns 0 on success, @ref ANJ_SEND_ERR_NO_REQUEST_FOUND if no request with
 *          given ID was found, @ref ANJ_SEND_ERR_ABORT if aborting is already
 *          in progress.
 */
int anj_send_abort(anj_t *anj, uint16_t send_id);

#        define ANJ_INTERNAL_INCLUDE_SEND
#        include <anj_internal/lwm2m_send.h>
#        undef ANJ_INTERNAL_INCLUDE_SEND

#    endif // ANJ_WITH_LWM2M_SEND

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_LWM2M_SEND_H
