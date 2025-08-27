/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#ifdef ANJ_WITH_LWM2M_SEND
#    include <anj/lwm2m_send.h>
#endif // ANJ_WITH_LWM2M_SEND

#ifdef ANJ_WITH_BOOTSTRAP
#    include "bootstrap.h"
#endif // ANJ_WITH_BOOTSTRAP

#ifdef ANJ_WITH_OBSERVE
#    include "../observe/observe.h"
#endif // ANJ_WITH_OBSERVE

#include "../dm/dm_io.h"
#include "../exchange.h"
#include "core.h"
#include "core_utils.h"
#include "reg_session.h"
#include "register.h"
#include "server.h"
#include "server_register.h"

#ifdef ANJ_WITH_BOOTSTRAP
#    include "server_bootstrap.h"
#    define _ANJ_CORE_BOOTSTRAP_DEFAULT_TIMEOUT 247
#endif // ANJ_WITH_BOOTSTRAP

bool _anj_core_state_transition_forced(anj_t *anj) {
    return anj->server_state.bootstrap_request_triggered
           || anj->server_state.restart_triggered
           || anj->server_state.disable_triggered;
}

void _anj_core_state_transition_clear(anj_t *anj) {
    anj->server_state.bootstrap_request_triggered = false;
    anj->server_state.restart_triggered = false;
    anj->server_state.disable_triggered = false;
}

int anj_core_init(anj_t *anj, const anj_configuration_t *config) {
    assert(anj && config);

    memset(anj, 0, sizeof(*anj));

    if (!config->endpoint_name) {
        log(L_ERROR, "Endpoint name not provided");
        return -1;
    }

    if (config->net_socket_cfg) {
        anj->net_socket_cfg = *config->net_socket_cfg;
    }
    anj->endpoint_name = config->endpoint_name;
    anj->queue_mode_enabled = config->queue_mode_enabled;

    _anj_dm_initialize(anj);

    _anj_exchange_init(&anj->exchange_ctx, (unsigned int) anj_time_real_now());
#ifdef ANJ_WITH_CACHE
    _anj_exchange_setup_cache(&anj->exchange_ctx, &anj->exchange_cache);
#endif // ANJ_WITH_CACHE
    if (config->udp_tx_params) {
        _anj_exchange_set_udp_tx_params(&anj->exchange_ctx,
                                        config->udp_tx_params);
    }
    if (config->exchange_request_timeout_ms != 0) {
        _anj_exchange_set_server_request_timeout(
                &anj->exchange_ctx, config->exchange_request_timeout_ms);
    }

    if (config->queue_mode_enabled) {
        anj->queue_mode_timeout_ms =
                (config->queue_mode_timeout_ms == 0)
                        ? _anj_server_calculate_max_transmit_wait(
                                  &anj->exchange_ctx.tx_params)
                        : config->queue_mode_timeout_ms;
    }

    _anj_register_ctx_init(anj);
#ifdef ANJ_WITH_BOOTSTRAP
    uint32_t bootstrap_timeout = config->bootstrap_timeout
                                         ? config->bootstrap_timeout
                                         : _ANJ_CORE_BOOTSTRAP_DEFAULT_TIMEOUT;
    _anj_bootstrap_ctx_init(anj, anj->endpoint_name, bootstrap_timeout);
    anj->bootstrap_retry_count = config->bootstrap_retry_count;
    anj->bootstrap_retry_timeout = config->bootstrap_retry_timeout;
#endif // ANJ_WITH_BOOTSTRAP
#ifdef ANJ_WITH_OBSERVE
    _anj_observe_init(anj);
#endif // ANJ_WITH_OBSERVE

    if (config->connection_status_cb) {
        anj->conn_status_cb = config->connection_status_cb;
        anj->conn_status_cb_arg = config->connection_status_cb_arg;
    }

    anj->server_state.conn_status = ANJ_CONN_STATUS_INITIAL;
    log(L_INFO, "Anjay Lite initialized");
    return 0;
}

void anj_core_server_obj_disable_executed(anj_t *anj, uint32_t timeout) {
    assert(anj);
    if (anj->server_state.conn_status != ANJ_CONN_STATUS_REGISTERED) {
        log(L_ERROR, "Invalid state for the operation");
        return;
    }
    log(L_INFO, "Disable resource executed");
    anj->server_state.disable_triggered = true;
    anj->server_state.enable_time =
            anj_time_real_now() + (uint64_t) (timeout * 1000);
}

void anj_core_server_obj_registration_update_trigger_executed(anj_t *anj) {
    assert(anj);
    if (anj->server_state.conn_status != ANJ_CONN_STATUS_REGISTERED) {
        log(L_ERROR, "Invalid state for the operation");
        return;
    }
    log(L_INFO, "Registration Update Trigger resource executed");
    anj->server_state.registration_update_triggered = true;
}

void anj_core_server_obj_bootstrap_request_trigger_executed(anj_t *anj) {
    assert(anj);
    if (anj->server_state.conn_status != ANJ_CONN_STATUS_REGISTERED) {
        log(L_ERROR, "Invalid state for the operation");
        return;
    }
    log(L_INFO, "Bootstrap Request Trigger resource executed");
    anj->server_state.bootstrap_request_triggered = true;
}

bool _anj_core_client_registered(anj_t *anj) {
    return (anj->server_state.conn_status == ANJ_CONN_STATUS_REGISTERED
            || anj->server_state.conn_status
                       == ANJ_CONN_STATUS_ENTERING_QUEUE_MODE
            || anj->server_state.conn_status == ANJ_CONN_STATUS_QUEUE_MODE);
}

void anj_core_request_update(anj_t *anj) {
    assert(anj);
    if (!_anj_core_client_registered(anj)) {
        log(L_ERROR, "Invalid state for the operation");
        return;
    }
    anj->server_state.registration_update_triggered = true;
}

void _anj_core_data_model_changed_with_ssid(anj_t *anj,
                                            const anj_uri_path_t *path,
                                            anj_core_change_type_t change_type,
                                            uint16_t ssid) {
    // we don't to check the return value of this function
#ifdef ANJ_WITH_OBSERVE
    anj_observe_data_model_changed(
            anj, path, (anj_observe_change_type_t) change_type, ssid);
#endif // ANJ_WITH_OBSERVE
    if (!_anj_core_client_registered(anj)) {
        return;
    }
    // check if Server object resources were changed
    if (change_type == ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED
            && path->ids[ANJ_ID_OID] == ANJ_OBJ_ID_SERVER) {
        int64_t last_lifetime = anj->server_instance.lifetime;
        _anj_reg_session_refresh_registration_related_resources(anj);
        if (last_lifetime != anj->server_instance.lifetime) {
            anj->server_state.details.registered.update_with_lifetime = true;
        }
    }
    // check if user added or removed object or object instance
    if (ssid == 0 && change_type != ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED
            && !anj_uri_path_has(path, ANJ_ID_RID)) {
        anj->server_state.details.registered.update_with_payload = true;
    }
}

void anj_core_data_model_changed(anj_t *anj,
                                 const anj_uri_path_t *path,
                                 anj_core_change_type_t change_type) {
    assert(anj && path);
    _anj_core_data_model_changed_with_ssid(anj, path, change_type, 0);
}

bool anj_core_ongoing_operation(anj_t *anj) {
    assert(anj);
    return _anj_exchange_ongoing_exchange(&anj->exchange_ctx);
}

static _anj_core_next_action_t anj_core_step_internal(anj_t *anj) {
    switch (anj->server_state.conn_status) {
    case ANJ_CONN_STATUS_INITIAL: {
#ifdef ANJ_WITH_BOOTSTRAP
        bool bootstrap_needed;
        if (_anj_server_bootstrap_is_needed(anj, &bootstrap_needed)) {
            anj->server_state.conn_status = ANJ_CONN_STATUS_INVALID;
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        if (bootstrap_needed) {
            anj->server_state.conn_status = ANJ_CONN_STATUS_BOOTSTRAPPING;
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
#endif // ANJ_WITH_BOOTSTRAP
        anj->server_state.conn_status = ANJ_CONN_STATUS_REGISTERING;
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }
#ifdef ANJ_WITH_BOOTSTRAP
    case ANJ_CONN_STATUS_BOOTSTRAPPING:
        return _anj_server_bootstrap_process_bootstrap_operation(
                anj, &anj->server_state.conn_status);
    case ANJ_CONN_STATUS_BOOTSTRAPPED:
        anj->server_state.conn_status = ANJ_CONN_STATUS_REGISTERING;
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
#else
    case ANJ_CONN_STATUS_BOOTSTRAPPING:
    case ANJ_CONN_STATUS_BOOTSTRAPPED:
        anj->server_state.conn_status = ANJ_CONN_STATUS_INITIAL;
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
#endif // ANJ_WITH_BOOTSTRAP
    case ANJ_CONN_STATUS_REGISTERING:
        return _anj_server_register_process_register_operation(
                anj, &anj->server_state.conn_status);
    case ANJ_CONN_STATUS_REGISTERED:
    case ANJ_CONN_STATUS_ENTERING_QUEUE_MODE:
    case ANJ_CONN_STATUS_QUEUE_MODE:
        return _anj_reg_session_process_registered(
                anj, &anj->server_state.conn_status);
    case ANJ_CONN_STATUS_INVALID:
        anj->server_state.conn_status = ANJ_CONN_STATUS_FAILURE;
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
    case ANJ_CONN_STATUS_FAILURE:
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
    case ANJ_CONN_STATUS_SUSPENDED:
        return _anj_reg_session_process_suspended(
                anj, &anj->server_state.conn_status);
    default:
        break;
    }
    ANJ_UNREACHABLE("Invalid server state");
    return _ANJ_CORE_NEXT_ACTION_LEAVE;
}

static void init_new_conn_status(anj_t *anj,
                                 anj_conn_status_t last_conn_status) {
    switch (anj->server_state.conn_status) {
#ifdef ANJ_WITH_BOOTSTRAP
    case ANJ_CONN_STATUS_BOOTSTRAPPING:
        if (_anj_server_bootstrap_start_bootstrap_operation(anj)) {
            anj->server_state.conn_status = ANJ_CONN_STATUS_INVALID;
        }
        return;
#endif // ANJ_WITH_BOOTSTRAP
    case ANJ_CONN_STATUS_REGISTERING:
        if (_anj_server_register_start_register_operation(anj)) {
            anj->server_state.conn_status = ANJ_CONN_STATUS_INVALID;
        }
        return;
    case ANJ_CONN_STATUS_REGISTERED:
        if (last_conn_status == ANJ_CONN_STATUS_REGISTERING) {
            _anj_reg_session_init(anj);
        }
        return;
    case ANJ_CONN_STATUS_SUSPENDED:
        log(L_INFO, "Client suspended");
    default:
        break;
    }
}

void anj_core_step(anj_t *anj) {
    assert(anj);

    _anj_core_next_action_t next_action = _ANJ_CORE_NEXT_ACTION_CONTINUE;
    while (next_action == _ANJ_CORE_NEXT_ACTION_CONTINUE) {
        anj_conn_status_t last_conn_status = anj->server_state.conn_status;

        // Handle all state transitions explicitly triggered by the user,
        // but only if the client is not currently Registered.
        // In the Registered state, a De-Register message must be sent first.
        // This is handled internally by _anj_reg_session_process_registered().
        if (!_anj_core_client_registered(anj)
                && _anj_core_state_transition_forced(anj)) {
            // Any ongoing exchange has already been cancelled in the respective
            // trigger functions, so the only remaining task here is to close
            // the connection before changing the state. We always perform
            // connection cleanup here.
            int res = _anj_server_close(&anj->connection_ctx, true);
            if (anj_net_is_again(res)) {
                return;
            }
            // Regardless of the result of _anj_server_close, we proceed with
            // the state change. Priority of state transitions (highest to
            // lowest): Restart, Bootstrap Request, Disable
            if (anj->server_state.restart_triggered) {
                anj->server_state.conn_status = ANJ_CONN_STATUS_INITIAL;
            } else if (anj->server_state.bootstrap_request_triggered) {
                anj->server_state.conn_status = ANJ_CONN_STATUS_BOOTSTRAPPING;
            } else if (anj->server_state.disable_triggered) {
                anj->server_state.conn_status = ANJ_CONN_STATUS_SUSPENDED;
            }
            _anj_core_state_transition_clear(anj);
        } else {
            // Continue with standard step logic
            next_action = anj_core_step_internal(anj);
        }

        if (anj->server_state.conn_status != last_conn_status) {
            if (anj->conn_status_cb) {
                anj->conn_status_cb(anj->conn_status_cb_arg, anj,
                                    anj->server_state.conn_status);
            }
            init_new_conn_status(anj, last_conn_status);
            log(L_TRACE, "Connection status changed from %d to %d",
                (int) last_conn_status, (int) anj->server_state.conn_status);
        }
    }
}

uint64_t anj_core_next_step_time(anj_t *anj) {
    assert(anj);
    uint64_t current_time = anj_time_real_now();
    if (anj->server_state.conn_status == ANJ_CONN_STATUS_SUSPENDED) {
        uint64_t enable_time =
                ANJ_MAX(anj->server_state.enable_time_user_triggered,
                        anj->server_state.enable_time);
        if (enable_time > current_time) {
            return enable_time - current_time;
        }
    } else if (anj->server_state.conn_status == ANJ_CONN_STATUS_QUEUE_MODE) {
        uint64_t time_to_next_update =
                anj->server_state.details.registered.next_update_time;
        if (time_to_next_update > current_time) {
            time_to_next_update = time_to_next_update - current_time;
        } else {
            time_to_next_update = 0;
        }
#ifdef ANJ_WITH_OBSERVE
        uint64_t time_to_next_notification = 0;
        if (!anj_observe_time_to_next_notification(
                    anj, &anj->server_instance.observe_state,
                    &time_to_next_notification)) {
            return ANJ_MIN(time_to_next_update, time_to_next_notification);
        }
#endif // ANJ_WITH_OBSERVE
        return time_to_next_update;
    }
    return 0;
}

void anj_core_disable_server(anj_t *anj, uint64_t timeout_ms) {
    assert(anj);
    log(L_INFO, "Disable called");
    if (timeout_ms == ANJ_TIME_UNDEFINED) {
        anj->server_state.enable_time_user_triggered = ANJ_TIME_UNDEFINED;
    } else {
        anj->server_state.enable_time_user_triggered =
                anj_time_real_now() + timeout_ms;
    }

    if (anj->server_state.conn_status == ANJ_CONN_STATUS_SUSPENDED
            || anj->server_state.disable_triggered == true) {
        log(L_DEBUG, "Already in progress");
        return;
    }
    _anj_exchange_terminate(&anj->exchange_ctx);
    anj->server_state.disable_triggered = true;
}

void anj_core_request_bootstrap(anj_t *anj) {
    assert(anj);
    log(L_INFO, "Bootstrap request triggered");
    if (anj->server_state.conn_status == ANJ_CONN_STATUS_BOOTSTRAPPING
            || anj->server_state.conn_status == ANJ_CONN_STATUS_BOOTSTRAPPED
            || anj->server_state.bootstrap_request_triggered == true) {
        log(L_DEBUG, "Already in progress");
        return;
    }
    _anj_exchange_terminate(&anj->exchange_ctx);
    anj->server_state.bootstrap_request_triggered = true;
}

void anj_core_restart(anj_t *anj) {
    assert(anj);
    log(L_INFO, "Restart triggered");
    if (anj->server_state.restart_triggered == true) {
        log(L_DEBUG, "Already in progress");
        return;
    }
    _anj_exchange_terminate(&anj->exchange_ctx);
    anj->server_state.restart_triggered = true;
}

int anj_core_shutdown(anj_t *anj) {
    // Functions called until _anj_server_close() have no side effects when
    // called again, so we do not track if the shutdown process was already
    // initiated.
    assert(anj);
    _anj_exchange_terminate(&anj->exchange_ctx);
#ifdef ANJ_WITH_LWM2M_SEND
    // abort all queued send request to call finish callbacks
    anj_send_abort(anj, ANJ_SEND_ID_ALL);
#endif // ANJ_WITH_LWM2M_SEND

    int res = _anj_server_close(&anj->connection_ctx, true);
    if (anj_net_is_again(res)) {
        return res;
    }
    // clear anjay, not necessarily needed, but let's prevent accidental misuse
    memset(anj, 0, sizeof(*anj));
    anj->server_state.conn_status = ANJ_CONN_STATUS_INVALID;

    log(L_INFO, "Anjay Lite instance shutdown with result %d", res);
    return res;
}
