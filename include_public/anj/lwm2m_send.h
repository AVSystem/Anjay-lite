/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Public API for composing and queuing LwM2M Send messages.
 *
 * The Send operation allows a client to proactively transmit Resource values to
 * the LwM2M Server (without a preceding request). Requests are queued and sent
 * when a registration session is active and no higher-priority CoAP exchange
 * is in progress.
 */

#ifndef ANJ_LWM2M_SEND_H
#    define ANJ_LWM2M_SEND_H

#    include <anj/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_LWM2M_SEND

/**
 * Special ID that matches all queued/active Send requests.
 *
 * @see anj_send_abort
 */
#        define ANJ_SEND_ID_ALL UINT16_MAX

/** @defgroup anj_send_errors LwM2M Send error codes
 * Error and result values reported by the Send API.
 * @{
 */

/** Delivery succeeded (acknowledged by the Server). */
#        define ANJ_SEND_SUCCESS 0

/**
 * Timed out.
 *
 * This error indicates that the Send request could not be completed within
 * the expected time window. It may occur in two situations:
 *
 * - The Server did not acknowledge or respond within the retransmission
 *   limits defined by the CoAP transmission parameters.
 * - The Client could not hand off the request to the transport layer within
 *   an implementation-defined processing delay (e.g., due to a blocked or
 *   unresponsive network stack). In this case, the message may never have been
 *   sent to the Server.
 *
 * In both cases, the Send operation is considered failed.
 */
#        define ANJ_SEND_ERR_TIMEOUT -1

/**
 * Delivery aborted.
 *
 * Possible causes include:
 * - Explicit cancellation via @ref anj_send_abort,
 * - Network error,
 * - Value of Mute Send Resource is set to true,
 * - Registration session ended.
 */
#        define ANJ_SEND_ERR_ABORT -2

/**
 * Delivery failed due to a CoAP-level error.
 *
 * Returned when the exchange completes with a non-success CoAP status
 * (e.g., server responds 4.xx/5.xx, sends RST), or when the client's
 * processing produces a CoAP error code (e.g., failure while preparing or
 * handling the payload that maps to a CoAP error). Not used for timeouts
 * or explicit aborts; those are reported as @ref ANJ_SEND_ERR_TIMEOUT and
 * @ref ANJ_SEND_ERR_ABORT respectively.
 */
#        define ANJ_SEND_ERR_REJECTED -3

/**
 * @ref anj_send_finished_handler_t : Network error occurred while sending the
 * message.
 */
#        define ANJ_SEND_ERR_NETWORK -4

/**
 * @ref anj_send_finished_handler_t : Internal error occurred while sending the
 * message.
 */
#        define ANJ_SEND_ERR_INTERNAL -5

/** @ref anj_send_abort : no request with the given ID was found. */
#        define ANJ_SEND_ERR_NO_REQUEST_FOUND -6

/**
 * @ref anj_send_new_request : queue full (see @ref ANJ_LWM2M_SEND_QUEUE_SIZE).
 */
#        define ANJ_SEND_ERR_NO_SPACE -7

/**
 * @ref anj_send_new_request : request cannot be sent in the current state:
 * - client is not in @ref ANJ_CONN_STATUS_REGISTERED or
 *   @ref ANJ_CONN_STATUS_QUEUE_MODE, or
 * - Mute Send is set to true.
 */
#        define ANJ_SEND_ERR_NOT_ALLOWED -8

/** @ref anj_send_new_request : provided data is invalid. */
#        define ANJ_SEND_ERR_DATA_NOT_VALID -9

/**@}*/ /* end of anj_send_errors */

/**
 * Content format of the Send payload.
 *
 * Requires the corresponding build-time support to be enabled.
 * - **SenML CBOR** supports timestamps (useful for time series).
 * - **LwM2M CBOR** is typically more compact; **paths must be unique** within
 *   one payload (duplicate paths are invalid even if timestamps differ).
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
 * Completion handler for a Send request.
 *
 * @param anjay   Anjay object.
 * @param send_id ID of the Send request.
 * @param result  One of @ref anj_send_errors.
 * @param data    User pointer from @ref anj_send_request_t::data.
 */
typedef void anj_send_finished_handler_t(anj_t *anjay,
                                         uint16_t send_id,
                                         int result,
                                         void *data);

/**
 * LwM2M Send message to be queued.
 */
typedef struct {
    /** Array of records (entries) to include in the payload. */
    const anj_io_out_entry_t *records;

    /** Number of elements in @ref records. */
    size_t records_cnt;

    /**
     * Handler invoked on success, or after the final delivery attempt.
     */
    anj_send_finished_handler_t *finished_handler;

    /** Opaque user pointer passed to @ref finished_handler. */
    void *data;

    /** Content format used to encode the payload. */
    anj_send_content_format_t content_format;
} anj_send_request_t;

/**
 * Queue a new LwM2M Send request.
 *
 * If this function returns 0, the request is queued and the finish handler
 * will be called with the final result. The request is processed when:
 * - a registration session is active, and
 * - no higher-priority CoAP exchange is in progress.
 *
 * When multiple requests are queued, they are processed FIFO; only one Send
 * request is processed at a time.
 *
 * @note **Timestamps:** Only SenML CBOR supports timestamps. In non-SenML
 *       formats, @ref anj_io_out_entry_t::timestamp is ignored.
 *
 * @note **Duplicate paths when using LwM2M CBOR:** LwM2M CBOR uses a CBOR map
 *       keyed by paths; keys must be unique. A request with duplicate paths is
 *       invalid, even if timestamps differ.
 *
 * @note The @p send_request object is **not copied**. All referenced memory
 *       (including the @ref anj_send_request_t::records array) must remain
 *       valid and unchanged until the operation completes and the finish
 *       handler returns.
 *
 * @param      anj          Anjay object.
 * @param      send_request Description of the message to send (must remain
 *                          valid until completion).
 * @param[out] out_send_id  If non-NULL, receives the assigned Send request ID
 *                          (valid until the finish handler is invoked).
 *
 * @return 0 on success, otherwise one of @ref anj_send_errors :
 *         - @ref ANJ_SEND_ERR_NO_SPACE
 *         - @ref ANJ_SEND_ERR_NOT_ALLOWED
 *         - @ref ANJ_SEND_ERR_DATA_NOT_VALID
 */
int anj_send_new_request(anj_t *anj,
                         const anj_send_request_t *send_request,
                         uint16_t *out_send_id);

/**
 * Abort a queued or in-flight Send request.
 *
 * - If the request is queued, it is removed from the queue.
 * - If the request is in progress, the exchange is cancelled.
 *
 * In both cases, @ref ANJ_SEND_ERR_ABORT is reported to the completion handler.
 *
 * @param anj     Anjay object.
 * @param send_id ID of the Send request to abort. Use @ref ANJ_SEND_ID_ALL to
 *                abort all pending requests.
 *
 * @return 0 on success; @ref ANJ_SEND_ERR_NO_REQUEST_FOUND if no matching
 *         request exists; @ref ANJ_SEND_ERR_ABORT if abort is already in
 *         progress.
 */
int anj_send_abort(anj_t *anj, uint16_t send_id);

/** @cond */
#        define ANJ_INTERNAL_INCLUDE_SEND
#        include <anj_internal/lwm2m_send.h>
#        undef ANJ_INTERNAL_INCLUDE_SEND
/** @endcond */

#    endif // ANJ_WITH_LWM2M_SEND

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_LWM2M_SEND_H
