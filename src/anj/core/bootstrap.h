/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_SRC_CORE_BOOTSTRAP_H
#    define ANJ_SRC_CORE_BOOTSTRAP_H

#    include <stdint.h>

#    include <anj/core.h>
#    include <anj/defs.h>
#    include <anj/time.h>

#    ifdef ANJ_WITH_BOOTSTRAP

/**
 * Initializes the bootstrap module context. Should be called once before any
 * other bootstrap module function.
 *
 * @param anj      Anjay object to operate on.
 * @param endpoint Endpoint Client Name, string is not copied, so it must be
 *                 valid during the whole lifetime of the context.
 * @param lifetime  Bootstrap timeout. If the timeout is exceeded,
 *                 Bootstrap process will be finished with @ref
 *                 _ANJ_BOOTSTRAP_ERR_BOOTSTRAP_TIMEOUT error code.
 */
void _anj_bootstrap_ctx_init(anj_t *anj,
                             const char *endpoint,
                             anj_time_duration_t lifetime);

/**
 * Processes Client Initiated Bootstrap. Before calling this function, the
 * connection to Bootstrap LwM2M Server must be established. As long as this
 * function does not return @ref _ANJ_BOOTSTRAP_FINISHED, or any error code from
 * the ANJ_BOOTSTRAP_ERR_* group, it should be called periodically.
 *
 * If @ref _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND is returned, the @p out_msg
 * should be sent using the exchange module. @p out_msg will contain the
 * Bootstrap-Request or Bootstrap-Pack-Request message.
 *
 * If enabled, the Bootstrap-Pack-Request will be sent first and then, if it
 * fails, the Bootstrap-Request. The Bootstrap-Write/Read/Discover/Delete
 * operations must be handled by the data model API. Bootstrap-Finish request
 * must be served by calling @ref _anj_bootstrap_finish_request.
 *
 * If Bootstrap-Finish is not received before timeout, the process will fail.
 *
 * @param      anj          Anjay object to operate on.
 * @param[out] out_msg      Request message to be sent if @ref
 *                          _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND is returned.
 * @param[out] out_handlers Exchange handlers, set only if @ref
 *                          _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND is returned.
 *
 * @returns One of the following values:
 * - @ref _ANJ_BOOTSTRAP_IN_PROGRESS, if Bootstrap is in progress,
 * - @ref _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND, if new message should be sent,
 * - @ref _ANJ_BOOTSTRAP_FINISHED, if Bootstrap has been successfully finished,
 * - ANJ_BOOTSTRAP_ERR_* code in case of error, in this case Bootstrap is also
 *   finished.
 */
int _anj_bootstrap_process(anj_t *anj,
                           _anj_coap_msg_t *out_msg,
                           _anj_exchange_handlers_t *out_handlers);

/**
 * Handles Bootstrap-Finish message. This function should be called when
 * Bootstrap-Finish message is received.
 *
 * @param      anj               Anjay object to operate on.
 * @param[out] out_response_code Response code to be sent in response to the
 *                               Bootstrap-Finish message.
 * @param[out] out_handlers      Exchange handlers.
 */
void _anj_bootstrap_finish_request(anj_t *anj,
                                   uint8_t *out_response_code,
                                   _anj_exchange_handlers_t *out_handlers);

/**
 * Should be called when the connection to Bootstrap Server is lost and there is
 * no ongoing exchange. In next call to @ref _anj_bootstrap_process, the
 * Bootstrap process will be finished and @ref _ANJ_BOOTSTRAP_ERR_NETWORK will
 * be returned.
 *
 * @param anj Anjay object to operate on.
 */
void _anj_bootstrap_connection_lost(anj_t *anj);

/**
 * Restarts the timer tracking the time since the last Bootstrap operation.
 *
 * Should be called after receiving each of the Bootstrap operations that affect
 * the data model: Bootstrap-Read, Bootstrap-Write, Bootstrap-Delete, and
 * Bootstrap-Discover.
 *
 * This function must be called only after starting the Bootstrap process with
 * @ref _anj_bootstrap_process, and before calling @ref
 * _anj_bootstrap_finish_request.
 *
 * @param anj Anjay object to operate on.
 */
void _anj_bootstrap_timeout_reset(anj_t *anj);

/**
 * Resets the Bootstrap module context. Should be called when the Bootstrap
 * process is aborted externally. Next @ref _anj_bootstrap_process call will
 * start new session.
 *
 * @param anj Anjay object to operate on.
 */
void _anj_bootstrap_reset(anj_t *anj);

#    endif // ANJ_WITH_BOOTSTRAP

#endif // ANJ_SRC_CORE_BOOTSTRAP_H
