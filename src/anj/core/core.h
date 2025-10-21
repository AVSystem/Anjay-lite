/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_SRC_CORE_CORE_H
#    define ANJ_SRC_CORE_CORE_H

#    include <stdbool.h>
#    include <stdint.h>

#    include <anj/core.h>
#    include <anj/defs.h>

/**
 * Returns codes that can be returned by function called in the @ref
 * anj_core_step_internal function.
 */
typedef enum {
    // Next step should be processed immediately.
    _ANJ_CORE_NEXT_ACTION_CONTINUE,
    // @ref anj_core_step should return to the user code. This flag means that
    // network layer returned one of ANJ_NET_EAGAIN, ANJ_NET_EINPROGRESS or
    // event loop is waiting for an event.
    _ANJ_CORE_NEXT_ACTION_LEAVE
} _anj_core_next_action_t;

/**
 * Internal function similar to @ref anj_core_data_model_changed, but with
 * additional argument @p ssid. Should be used by data model to inform the core
 * about changes that come from LwM2M Server.
 *
 * @param anj         Anjay object to operate on.
 * @param path        Pointer to the path.
 * @param change_type Type of change.
 * @param ssid        Short Server ID of the server that caused the change.
 */
void _anj_core_data_model_changed_with_ssid(anj_t *anj,
                                            const anj_uri_path_t *path,
                                            anj_core_change_type_t change_type,
                                            uint16_t ssid);

/**
 * Returns information if there is active registration session - client is
 * connected to the LwM2M Server (or in Queue Mode).
 */
bool _anj_core_client_registered(anj_t *anj);

bool _anj_core_state_transition_forced(anj_t *anj);
void _anj_core_state_transition_clear(anj_t *anj);

#endif // ANJ_SRC_CORE_CORE_H
