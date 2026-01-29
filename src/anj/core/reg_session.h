/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_SRC_CORE_SERVER_MANAGEMENT_H
#    define ANJ_SRC_CORE_SERVER_MANAGEMENT_H

#    include <anj/core.h>
#    include <anj/defs.h>

#    include "core.h"

#    define _ANJ_SRV_MAN_STATE_IDLE_IN_PROGRESS 1
#    define _ANJ_SRV_MAN_STATE_QUEUE_MODE_IN_PROGRESS 2
#    define _ANJ_SRV_MAN_STATE_EXCHANGE_IN_PROGRESS 3
#    define _ANJ_SRV_MAN_STATE_DISCONNECT_IN_PROGRESS 4
#    define _ANJ_SRV_MAN_STATE_ENTERING_QUEUE_MODE_IN_PROGRESS 5
#    define _ANJ_SRV_MAN_STATE_EXITING_QUEUE_MODE_IN_PROGRESS 6

/**
 * Should be called after successful registration. Initializes the server
 * management logic.
 *
 * @param  anj    Anjay object to operate on.
 */
void _anj_reg_session_init(anj_t *anj);

/**
 * Processes the ongoing registration operation. Should be called in a loop for
 * @ref ANJ_CONN_STATUS_REGISTERED, @ref ANJ_CONN_STATUS_ENTERING_QUEUE_MODE or
 * @ref ANJ_CONN_STATUS_QUEUE_MODE states.
 *
 * @param      anj        Anjay object to operate on.
 * @param[out] out_status Current status of the server connection, will be
 *                        updated if necessary.
 *
 * @returns Next action to be taken by the core.
 */
_anj_core_next_action_t
_anj_reg_session_process_registered(anj_t *anj, anj_conn_status_t *out_status);

/**
 * Processes the suspended state, waiting for the disable timeout. Should be
 * called in a loop for
 * @ref ANJ_CONN_STATUS_SUSPENDED state.
 *
 * @param      anj        Anjay object to operate on.
 * @param[out] out_status Current status of the server connection, will be
 *                        updated on finish.
 *
 * @returns Next action to be taken by the core.
 */
_anj_core_next_action_t
_anj_reg_session_process_suspended(anj_t *anj, anj_conn_status_t *out_status);
/**
 * Read the registration related resources from server object.
 *
 * @param        anj    Anjay object to operate on.
 */
void _anj_reg_session_refresh_registration_related_resources(anj_t *anj);

#endif // ANJ_SRC_CORE_SERVER_MANAGEMENT_H
