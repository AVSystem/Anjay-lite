/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_REGISTER_H
#    define ANJ_REGISTER_H

#    include <stdbool.h>

#    include <anj/core.h>
#    include <anj/defs.h>
#    include <anj/time.h>

/**
 * A group of codes that can be returned by @ref _anj_register_operation_status.
 */

/** Registration interface operation still in progress. */
#    define _ANJ_REGISTER_OPERATION_IN_PROGRESS 0
/** Registration interface operation has been successfully finished. */
#    define _ANJ_REGISTER_OPERATION_FINISHED 1
/** Indicates that the registration interface operation has failed or context
 * has never been used. */
#    define _ANJ_REGISTER_OPERATION_ERROR -1

/**
 * Initializes the register module context. Should be called once before any
 * other register module function.
 *
 * This module handles Registration Interface related operations, but it is not
 * responsible for server management logic. The state of the operation is not
 * controlled internally, so the user is responsible for making sure what he is
 * doing is legal.
 *
 * @param anj  Anjay object to operate on.
 */
void _anj_register_ctx_init(anj_t *anj);

/**
 * Prepare Register request with the given attributes. This function
 * should be called to start the registration process. The operation is
 * considered finished when @ref _anj_register_operation_status returns a value
 * different than @ref _ANJ_REGISTER_OPERATION_IN_PROGRESS. This function is
 * compliant with the anj_exchange API.
 *
 * Location paths forwarded in response to the Register message are stored in
 * the context. If number of location paths exceeds @ref
 * ANJ_COAP_MAX_LOCATION_PATHS_NUMBER, or the length of any location
 * path exceeds @ref ANJ_COAP_MAX_LOCATION_PATH_SIZE, the operation will fail.
 *
 * @param      anj          Anjay object to operate on.
 * @param      attr         Attributes of the Register message. Attributes are
 *                          not copied, so they must be valid during the whole
 *                          lifetime of the exchange.
 * @param[out] out_msg      Register message to be sent.
 * @param[out] out_handlers Handlers compliant with anj_exchange API
 *                          specifications.
 */
void _anj_register_register(anj_t *anj,
                            const _anj_attr_register_t *attr,
                            _anj_coap_msg_t *out_msg,
                            _anj_exchange_handlers_t *out_handlers);

/**
 * Prepare Update request. This function should be called to start the
 * registration update process. The operation is considered finished when @ref
 * _anj_register_operation_status returns a value different than @ref
 * _ANJ_REGISTER_OPERATION_IN_PROGRESS. This function is compliant with the
 * anj_exchange API.
 *
 * @param      anj          Anjay object to operate on.
 * @param      lifetime     Lifetime value to be updated. If NULL, lifetime is
 *                          not included in the message.
 * @param      with_payload Indicates if the payload should be included in the
 *                          message. Should be set to true if since the last
 *                          registration update data model changed - objects or
 *                          instances list was modified.
 * @param[out] out_msg      Update message to be sent.
 * @param[out] out_handlers Handlers compliant with anj_exchange API
 *                          specifications.
 */
void _anj_register_update(anj_t *anj,
                          const anj_time_duration_t *lifetime,
                          bool with_payload,
                          _anj_coap_msg_t *out_msg,
                          _anj_exchange_handlers_t *out_handlers);

/**
 * Prepare De-register request. This function should be called to start the
 * deregistration process. The operation is considered finished when @ref
 * _anj_register_operation_status returns a value different than @ref
 * _ANJ_REGISTER_OPERATION_IN_PROGRESS or this function returns an error.
 * This function is compliant with the anj_exchange API. Even if the operation
 * fails, De-register should be considered complete.
 *
 * @param      anj          Anjay object to operate on.
 * @param[out] out_msg      De-register message to be sent.
 * @param[out] out_handlers Handlers compliant with anj_exchange API
 *                          specifications.
 */
void _anj_register_deregister(anj_t *anj,
                              _anj_coap_msg_t *out_msg,
                              _anj_exchange_handlers_t *out_handlers);

/**
 * Should be called to get the status of ongoing operation.
 *
 * @param anj  Anjay object to operate on.
 *
 * @returns One of the following codes:
 * - @ref _ANJ_REGISTER_OPERATION_IN_PROGRESS, if the operation is still in
 *   progress,
 * - @ref _ANJ_REGISTER_OPERATION_FINISHED, if the operation has been
 *   successfully finished,
 * - @ref _ANJ_REGISTER_OPERATION_ERROR, if the operation has failed, or this
 *   function is call right after @ref _anj_register_ctx_init call.
 */
int _anj_register_operation_status(anj_t *anj);

#endif // ANJ_REGISTER_H
