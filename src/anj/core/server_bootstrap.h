/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_SRC_CORE_SERVER_BOOTSTRAP_H
#    define ANJ_SRC_CORE_SERVER_BOOTSTRAP_H

#    include <stdbool.h>

#    include <anj/core.h>
#    include <anj/defs.h>

#    include "core.h"

#    define _ANJ_SRV_BOOTSTRAP_STATE_CONNECTION_IN_PROGRESS 1
#    define _ANJ_SRV_BOOTSTRAP_STATE_BOOTSTRAP_IN_PROGRESS 2
#    define _ANJ_SRV_BOOTSTRAP_STATE_FINISHED 3
// The following three states are intended to cascade:
// FINISH_DISCONNECT_AND_RETRY performs bootstrap finish before
// DISCONNECT_AND_RETRY, which in turn disconnects and leads to RETRY to try one
// more time.
#    define _ANJ_SRV_BOOTSTRAP_STATE_FINISH_DISCONNECT_AND_RETRY 4
#    define _ANJ_SRV_BOOTSTRAP_STATE_DISCONNECT_AND_RETRY 5
#    define _ANJ_SRV_BOOTSTRAP_STATE_RETRY 6
#    define _ANJ_SRV_BOOTSTRAP_STATE_WAITING 7
#    define _ANJ_SRV_BOOTSTRAP_STATE_ERROR 8

/**
 * Checks if the Bootstrap operation is needed. Bootstrap is needed if there is
 * no LwM2M Server instance in the data model.
 *
 * @param anj                Anjay object to operate on.
 * @param[out] out_is_needed Output parameter for the result.
 *
 * @returns 0 on success, negative value in case of error.
 */
int _anj_server_bootstrap_is_needed(anj_t *anj, bool *out_is_needed);

/**
 * Starts the process of Bootstrap operation. All errors returned by this
 * are result of invalid configuration or internal problems with data
 * model/objects implementation - if the returned value is different from 0,
 * @ref ANJ_CONN_STATUS_INVALID should be set.
 *
 * @param anj  Anjay object to operate on.
 *
 * @returns 0 on success, negative value in case of error.
 */
int _anj_server_bootstrap_start_bootstrap_operation(anj_t *anj);

/**
 * Processes the ongoing Bootstrap operation.
 *
 * This function handles the progression of a previously started Bootstrap
 * operation. It must be called in a loop after a successful call to @ref
 * _anj_server_bootstrap_start_bootstrap_operation.
 *
 * The function updates the status of the operation through the @p out_status
 * output parameter. By change of this param, it will indicate either success
 * (@ref ANJ_CONN_STATUS_BOOTSTRAPPED) or failure (@ref
 * ANJ_CONN_STATUS_FAILURE).
 *
 * The return value informs the core module of the next action to take.
 *
 * @param       anj         Pointer to the ANJ context structure.
 * @param[out]  out_status  Pointer to a variable that will receive the
 *                          connection status.
 *
 * @returns _ANJ_CORE_NEXT_ACTION_CONTINUE, when the next action can be
 *          processed immediately, or _ANJ_CORE_NEXT_ACTION_LEAVE, when the
 *          function should return to the user code.
 */
_anj_core_next_action_t _anj_server_bootstrap_process_bootstrap_operation(
        anj_t *anj, anj_conn_status_t *out_status);

#endif // ANJ_SRC_CORE_SERVER_BOOTSTRAP_H
