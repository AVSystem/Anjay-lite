/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_SRC_CORE_SERVER_REGISTER_H
#    define ANJ_SRC_CORE_SERVER_REGISTER_H

#    include <anj/core.h>
#    include <anj/defs.h>

#    include "core.h"

#    define _ANJ_SRV_REG_STATE_CONNECTION_IN_PROGRESS 1
#    define _ANJ_SRV_REG_STATE_REGISTER_IN_PROGRESS 2
#    define _ANJ_SRV_REG_STATE_ERROR_HANDLING_IN_PROGRESS 3
// disconnect and reconnect
#    define _ANJ_SRV_REG_STATE_DISCONNECT_IN_PROGRESS 4
// disconnect with network context cleanup and reconnect
#    define _ANJ_SRV_REG_STATE_CLEANUP_IN_PROGRESS 5
// disconnect with network context cleanup and go to registration failure state
#    define _ANJ_SRV_REG_STATE_CLEANUP_WITH_FAILURE_IN_PROGRESS 6
#    define _ANJ_SRV_REG_STATE_RESTART_IN_PROGRESS 7
#    define _ANJ_SRV_REG_STATE_REGISTRATION_FAILURE_IN_PROGRESS 8

/**
 * Starts the process of registering the client to the LwM2M server. All errors
 * returned by this function are result of invalid configuration or internal
 * problems with data model/objects implementation - if the returned value is
 * different from 0, @ref ANJ_CONN_STATUS_INVALID should be set.
 *
 * @param anj  Anjay object to operate on.
 *
 * @returns 0 on success, negative value in case of error.
 */
int _anj_server_register_start_register_operation(anj_t *anj);

/**
 * Processes the ongoing registration operation. Should be called in a loop for
 * @ref ANJ_CONN_STATUS_REGISTERING state.
 *
 * @param      anj        Anjay object to operate on.
 * @param[out] out_status Current status of the server connection, will be
 *                        updated on finish.
 *
 * @returns Next action to be taken by the core.
 */
_anj_core_next_action_t
_anj_server_register_process_register_operation(anj_t *anj,
                                                anj_conn_status_t *out_status);

#endif // ANJ_SRC_CORE_SERVER_REGISTER_H
