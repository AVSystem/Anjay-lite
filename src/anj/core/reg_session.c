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
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#ifdef ANJ_WITH_OBSERVE
#    include "../observe/observe.h"
#endif // ANJ_WITH_OBSERVE

#include "../coap/coap.h"
#include "../dm/dm_integration.h"
#include "../exchange.h"
#include "../exchange_cache.h"
#include "core.h"
#include "core_utils.h"
#include "reg_session.h"
#include "register.h"
#include "server.h"

#ifdef ANJ_WITH_LWM2M_SEND
#    include "lwm2m_send.h"
#endif // ANJ_WITH_LWM2M_SEND

#define _ANJ_REG_SESSION_NEW_EXCHANGE 1

static uint64_t calculate_next_update(anj_t *anj) {
    if (anj->server_instance.lifetime == 0) {
        // "...If the value is set to 0, the lifetime is infinite."
        return ANJ_TIME_UNDEFINED;
    }
    uint64_t max_transmit_wait_s = _anj_server_calculate_max_transmit_wait(
                                           &anj->exchange_ctx.tx_params)
                                   / 1000;
    uint64_t timeout_s = (uint64_t) ANJ_MAX(
            anj->server_instance.lifetime - (int64_t) max_transmit_wait_s,
            anj->server_instance.lifetime / 2);
    return anj_time_real_now() + timeout_s * 1000;
}

static void refresh_queue_mode_timeout(anj_t *anj) {
    anj->server_state.details.registered.queue_start_time =
            anj->queue_mode_enabled
                    ? (anj_time_real_now() + anj->queue_mode_timeout_ms)
                    : ANJ_TIME_UNDEFINED;
}

void _anj_reg_session_init(anj_t *anj) {
    anj->server_state.details.registered.internal_state =
            _ANJ_SRV_MAN_STATE_IDLE_IN_PROGRESS;
    anj->server_state.registration_update_triggered = false;
    _anj_reg_session_refresh_registration_related_resources(anj);
    anj->server_state.details.registered.next_update_time =
            calculate_next_update(anj);
    anj->server_state.details.registered.update_with_lifetime = false;
    anj->server_state.details.registered.update_with_payload = false;
    _anj_core_state_transition_clear(anj);
    anj->server_state.enable_time = 0;
    anj->server_state.enable_time_user_triggered = 0;
    refresh_queue_mode_timeout(anj);
}

#ifdef ANJ_WITH_OBSERVE
static void update_observe_parameters(anj_t *anj) {
    anj->server_instance.observe_state = (_anj_observe_server_state_t) {
        .is_server_online = true,
        .ssid = anj->server_instance.ssid,
        .default_min_period = 0,
        .default_max_period = 0,
        .notify_store = false,
#    ifdef ANJ_WITH_LWM2M12
        .default_con = false,
#    endif // ANJ_WITH_LWM2M12
    };

    anj_res_value_t res_val;
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER,
                                                 anj->server_instance.iid,
                                                 SERVER_OBJ_DEFAULT_PMIN_RID);
    int res = anj_dm_res_read(anj, &path, &res_val);
    if (!res && res_val.int_value >= 0 && res_val.int_value <= UINT32_MAX) {
        anj->server_instance.observe_state.default_min_period =
                (uint32_t) res_val.int_value;
    } else if (res != ANJ_DM_ERR_NOT_FOUND) {
        log(L_ERROR, "Could not read default pmin resource");
    }
    path.ids[ANJ_ID_RID] = SERVER_OBJ_DEFAULT_PMAX_RID;
    res = anj_dm_res_read(anj, &path, &res_val);
    if (!res && res_val.int_value >= 0 && res_val.int_value <= UINT32_MAX) {
        anj->server_instance.observe_state.default_max_period =
                (uint32_t) res_val.int_value;
    } else if (res != ANJ_DM_ERR_NOT_FOUND) {
        log(L_ERROR, "Could not read default pmax resource");
    }
    path.ids[ANJ_ID_RID] = SERVER_OBJ_NOTIFICATION_STORING_RID;
    if (!anj_dm_res_read(anj, &path, &res_val)) {
        anj->server_instance.observe_state.notify_store = res_val.bool_value;
    } else {
        log(L_ERROR, "Could not read default notification storing resource");
    }
#    ifdef ANJ_WITH_LWM2M12
    path.ids[ANJ_ID_RID] = SERVER_OBJ_DEFAULT_NOTIFICATION_MODE_RID;
    res = anj_dm_res_read(anj, &path, &res_val);
    if (!res) {
        // 0 = NonConfirmable, 1 = Confirmable.
        anj->server_instance.observe_state.default_con =
                (res_val.int_value == 1);
    } else if (res != ANJ_DM_ERR_NOT_FOUND) {
        log(L_ERROR,
            "Could not read default defualt notification mode resource");
    }
#    endif // ANJ_WITH_LWM2M12
}
#endif // ANJ_WITH_OBSERVE

static void get_lifetime(anj_t *anj) {
    anj_res_value_t res_val;
    if (anj_dm_res_read(anj,
                        &ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER,
                                                anj->server_instance.iid,
                                                SERVER_OBJ_LIFETIME_RID),
                        &res_val)
            || res_val.int_value < 0 || res_val.int_value > UINT32_MAX) {
        log(L_ERROR, "Could not read lifetime resource");
    } else {
        // in case of error, the value is not changed
        anj->server_instance.lifetime = (uint32_t) res_val.int_value;
    }
}

#ifdef ANJ_WITH_LWM2M_SEND
static void get_mute_send(anj_t *anj) {
    anj_res_value_t res_val;
    if (anj_dm_res_read(anj,
                        &ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER,
                                                anj->server_instance.iid,
                                                SERVER_OBJ_MUTE_SEND_RID),
                        &res_val)) {
        log(L_ERROR, "Could not read mute send resource");
        // "If true or the Resource is not present, the LwM2M Client Send
        // command capability is de-activated"
        anj->server_instance.mute_send = true;
    } else {
        anj->server_instance.mute_send = res_val.bool_value;
    }
}
#endif // ANJ_WITH_LWM2M_SEND

void _anj_reg_session_refresh_registration_related_resources(anj_t *anj) {
    assert(_anj_core_client_registered(anj));
    get_lifetime(anj);
#ifdef ANJ_WITH_LWM2M_SEND
    get_mute_send(anj);
#endif // ANJ_WITH_LWM2M_SEND
#ifdef ANJ_WITH_OBSERVE
    update_observe_parameters(anj);
#endif // ANJ_WITH_OBSERVE
}

static int handle_incoming_message(anj_t *anj, size_t msg_size) {
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    int res = _anj_coap_decode_udp(anj->in_buffer, msg_size, &msg);
    if (res) {
        ANJ_CORE_LOG_COAP_ERROR(res);
        // ignore invalid messages
        return 0;
    }

    _anj_exchange_handlers_t exchange_handlers = { 0 };
    uint8_t response_code = 0;

#ifdef ANJ_WITH_CACHE
    // check if it's a retransmission
    int cache_try =
            _anj_exchange_cache_check(&anj->exchange_cache,
                                      msg.coap_binding_data.udp.message_id);
    if (cache_try == _ANJ_EXCHANGE_CACHE_HIT_RECENT) {
        return _ANJ_REG_SESSION_NEW_EXCHANGE;
    } else if (cache_try == _ANJ_EXCHANGE_CACHE_HIT_NON_RECENT) {
        // don't respond
        return 0;
    }
#endif // ANJ_WITH_CACHE

    // find the right module to handle the message
    switch (msg.operation) {
    case ANJ_OP_DM_READ:
    case ANJ_OP_DM_DISCOVER:
    case ANJ_OP_DM_WRITE_REPLACE:
    case ANJ_OP_DM_WRITE_PARTIAL_UPDATE:
    case ANJ_OP_DM_EXECUTE:
    case ANJ_OP_DM_CREATE:
    case ANJ_OP_DM_DELETE:
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    case ANJ_OP_DM_READ_COMP:
    case ANJ_OP_DM_WRITE_COMP:
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
        _anj_dm_process_request(anj, &msg, anj->server_instance.ssid,
                                &response_code, &exchange_handlers);
        break;
#ifndef ANJ_WITH_COMPOSITE_OPERATIONS
    case ANJ_OP_DM_READ_COMP:
    case ANJ_OP_DM_WRITE_COMP:
        log(L_WARNING, "Composite operations not supported");
        response_code = ANJ_COAP_CODE_NOT_IMPLEMENTED;
        break;
#endif // NOT ANJ_WITH_COMPOSITE_OPERATIONS
    case ANJ_OP_DM_WRITE_ATTR:
    case ANJ_OP_INF_OBSERVE:
    case ANJ_OP_INF_OBSERVE_COMP:
    case ANJ_OP_INF_CANCEL_OBSERVE:
    case ANJ_OP_INF_CANCEL_OBSERVE_COMP:
#ifdef ANJ_WITH_OBSERVE
    {
        _anj_observe_new_request(anj,
                                 &exchange_handlers,
                                 &anj->server_instance.observe_state,
                                 &msg,
                                 &response_code);
        break;
    }
#else  // ANJ_WITH_OBSERVE
    {
        log(L_WARNING, "Observe operation not supported");
        response_code = ANJ_COAP_CODE_NOT_IMPLEMENTED;
        break;
    }
#endif // ANJ_WITH_OBSERVE
    case ANJ_OP_COAP_PING_UDP:
        break; // PING is handled by the exchange module
    default: {
        log(L_WARNING, "Invalid %d", (int) msg.operation);
        return 0;
    }
    }

    if (_anj_server_prepare_server_request(anj, &msg, response_code,
                                           &exchange_handlers)) {
        return -1;
    }
    return _ANJ_REG_SESSION_NEW_EXCHANGE;
}

static int handle_registration_update(anj_t *anj) {
    // "When any of the parameters listed in Table: 6.2.2.-1 Update Parameters
    // changes, the LwM2M Client MUST send an "Update" operation to the LwM2M
    // Server"
    if (anj_time_real_now()
                    < anj->server_state.details.registered.next_update_time
            && !anj->server_state.details.registered.update_with_lifetime
            && !anj->server_state.details.registered.update_with_payload
            && !anj->server_state.registration_update_triggered) {
        return 0;
    }
    _anj_reg_session_refresh_registration_related_resources(anj);
    anj->server_state.details.registered.next_update_time =
            calculate_next_update(anj);
    anj->server_state.registration_update_triggered = false;

    uint32_t update_msg_lifetime = anj->server_instance.lifetime;
    uint32_t *lifetime =
            anj->server_state.details.registered.update_with_lifetime
                    ? &update_msg_lifetime
                    : NULL;
    anj->server_state.details.registered.update_with_lifetime = false;
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers = { 0 };
    bool with_payload =
            anj->server_state.details.registered.update_with_payload;
    anj->server_state.details.registered.update_with_payload = false;

    _anj_register_update(anj, lifetime, with_payload, &msg, &exchange_handlers);
    if (_anj_server_prepare_client_request(anj, &msg, &exchange_handlers)) {
        return -1;
    }
    return _ANJ_REG_SESSION_NEW_EXCHANGE;
}

#ifdef ANJ_WITH_LWM2M_SEND
static int handle_send(anj_t *anj) {
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers = { 0 };
    _anj_lwm2m_send_process(anj, &exchange_handlers, &msg);
    if (msg.operation != ANJ_OP_INF_CON_SEND) {
        return 0;
    }
    log(L_DEBUG, "Sending LwM2M Send");
    if (_anj_server_prepare_client_request(anj, &msg, &exchange_handlers)) {
        return -1;
    }
    return _ANJ_REG_SESSION_NEW_EXCHANGE;
}
#endif // ANJ_WITH_LWM2M_SEND

#ifdef ANJ_WITH_OBSERVE
static int handle_observe(anj_t *anj) {
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t exchange_handlers = { 0 };
    _anj_observe_process(anj, &exchange_handlers,
                         &anj->server_instance.observe_state, &msg);
    if (msg.operation != ANJ_OP_INF_CON_NOTIFY
            && msg.operation != ANJ_OP_INF_NON_CON_NOTIFY) {
        return 0;
    }
    log(L_DEBUG, "Sending notification");
    if (_anj_server_prepare_client_request(anj, &msg, &exchange_handlers)) {
        return -1;
    }
    return _ANJ_REG_SESSION_NEW_EXCHANGE;
}
#endif // ANJ_WITH_OBSERVE

static int try_send_deregistrer(anj_t *anj) {
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_exchange_handlers_t out_handlers = { 0 };
    _anj_register_deregister(anj, &msg, &out_handlers);
    if (_anj_server_prepare_client_request(anj, &msg, &out_handlers)) {
        return -1;
    }
    return _ANJ_REG_SESSION_NEW_EXCHANGE;
}

static uint8_t get_new_state_for_new_exchange(uint8_t current_state,
                                              int result) {
    if (result != _ANJ_REG_SESSION_NEW_EXCHANGE) {
        return _ANJ_SRV_MAN_STATE_DISCONNECT_IN_PROGRESS;
    }
    switch (current_state) {
    case _ANJ_SRV_MAN_STATE_IDLE_IN_PROGRESS:
        return _ANJ_SRV_MAN_STATE_EXCHANGE_IN_PROGRESS;
    case _ANJ_SRV_MAN_STATE_QUEUE_MODE_IN_PROGRESS:
        return _ANJ_SRV_MAN_STATE_EXITING_QUEUE_MODE_IN_PROGRESS;
    default:
        ANJ_UNREACHABLE("Invalid state");
        return _ANJ_SRV_MAN_STATE_DISCONNECT_IN_PROGRESS;
    }
}

_anj_core_next_action_t
_anj_reg_session_process_registered(anj_t *anj, anj_conn_status_t *out_status) {
    switch (anj->server_state.details.registered.internal_state) {
    case _ANJ_SRV_MAN_STATE_IDLE_IN_PROGRESS: {
        int res;
        if (_anj_core_state_transition_forced(anj)) {
            res = try_send_deregistrer(anj);
            anj->server_state.details.registered.internal_state =
                    get_new_state_for_new_exchange(
                            anj->server_state.details.registered.internal_state,
                            res);
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }

        // check for new requests
        size_t msg_size;
        res = _anj_server_receive(&anj->connection_ctx, anj->in_buffer,
                                  &msg_size, ANJ_IN_MSG_BUFFER_SIZE);
        if (anj_net_is_ok(res)) {
            // new message received, if decode fails or not recognized - drop
            res = handle_incoming_message(anj, msg_size);
            if (res) {
                anj->server_state.details.registered
                        .internal_state = get_new_state_for_new_exchange(
                        anj->server_state.details.registered.internal_state,
                        res);
                return _ANJ_CORE_NEXT_ACTION_CONTINUE;
            }
        } else if (!anj_net_is_again(res)) {
            log(L_ERROR, "Error while receiving message: %d", res);
            anj->server_state.details.registered.internal_state =
                    _ANJ_SRV_MAN_STATE_DISCONNECT_IN_PROGRESS;
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
    }
    // fall through
    case _ANJ_SRV_MAN_STATE_QUEUE_MODE_IN_PROGRESS: {
        // allow to force state transition if we are in queue mode
        if (_anj_core_state_transition_forced(anj)
                && anj->server_state.details.registered.internal_state
                               == _ANJ_SRV_MAN_STATE_QUEUE_MODE_IN_PROGRESS) {
            anj->server_state.details.registered.internal_state =
                    _ANJ_SRV_MAN_STATE_EXITING_QUEUE_MODE_IN_PROGRESS;
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }

        // state is not changed so there is no ongoing exchange, check if
        // registration update is needed
        int res = handle_registration_update(anj);
        if (res) {
            anj->server_state.details.registered.internal_state =
                    get_new_state_for_new_exchange(
                            anj->server_state.details.registered.internal_state,
                            res);
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }

#ifdef ANJ_WITH_LWM2M_SEND
        // still no ongoing exchange, check for Send requests
        res = handle_send(anj);
        if (res) {
            anj->server_state.details.registered.internal_state =
                    get_new_state_for_new_exchange(
                            anj->server_state.details.registered.internal_state,
                            res);
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
#endif // ANJ_WITH_LWM2M_SEND

#ifdef ANJ_WITH_OBSERVE
        // still no ongoing exchange, check for observe notifications
        res = handle_observe(anj);
        if (res) {
            anj->server_state.details.registered.internal_state =
                    get_new_state_for_new_exchange(
                            anj->server_state.details.registered.internal_state,
                            res);
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
#endif // ANJ_WITH_OBSERVE

        // check if we should enter queue mode, if we are not already in it
        if (anj->queue_mode_enabled
                && anj->server_state.details.registered.internal_state
                               != _ANJ_SRV_MAN_STATE_QUEUE_MODE_IN_PROGRESS
                && anj_time_real_now() > anj->server_state.details.registered
                                                 .queue_start_time) {
            anj->server_state.details.registered.internal_state =
                    _ANJ_SRV_MAN_STATE_ENTERING_QUEUE_MODE_IN_PROGRESS;
            *out_status = ANJ_CONN_STATUS_ENTERING_QUEUE_MODE;
            log(L_INFO, "Entering queue mode");
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }

        // nothing to do, wait for next iteration
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
    }

    case _ANJ_SRV_MAN_STATE_EXITING_QUEUE_MODE_IN_PROGRESS: {
        int res = _anj_server_connect(&anj->connection_ctx,
                                      anj->security_instance.type,
                                      &anj->net_socket_cfg,
                                      anj->security_instance.server_uri,
                                      anj->security_instance.port,
                                      true);
        if (anj_net_is_again(res)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        if (anj_net_is_ok(res)) {
            // there are 2 scenarios of exiting queue mode:
            //  - new client initiatied exchange
            //  - forced state transition
            anj->server_state.details.registered.internal_state =
                    _anj_core_state_transition_forced(anj)
                            ? _ANJ_SRV_MAN_STATE_IDLE_IN_PROGRESS
                            : _ANJ_SRV_MAN_STATE_EXCHANGE_IN_PROGRESS;
            *out_status = ANJ_CONN_STATUS_REGISTERED;
        } else {
            anj->server_state.details.registered.internal_state =
                    _ANJ_SRV_MAN_STATE_DISCONNECT_IN_PROGRESS;
            log(L_ERROR, "Connection error: %d", res);
        }
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }

    case _ANJ_SRV_MAN_STATE_EXCHANGE_IN_PROGRESS: {
        int res = _anj_server_handle_request(anj);
        if (anj_net_is_again(res)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        // _anj_register_operation_status() value is important only in case of
        // Update/Deregister operation, in other cases
        // _anj_register_operation_status() always returns
        // _ANJ_REGISTER_OPERATION_FINISHED
        if (res
                || _anj_register_operation_status(anj)
                               != _ANJ_REGISTER_OPERATION_FINISHED
                || _anj_core_state_transition_forced(anj)) {
            anj->server_state.details.registered.internal_state =
                    _ANJ_SRV_MAN_STATE_DISCONNECT_IN_PROGRESS;
        } else {
            anj->server_state.details.registered.internal_state =
                    _ANJ_SRV_MAN_STATE_IDLE_IN_PROGRESS;
            // exchange finished successfully update queue mode timeout
            refresh_queue_mode_timeout(anj);
        }
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }

    case _ANJ_SRV_MAN_STATE_ENTERING_QUEUE_MODE_IN_PROGRESS:
    case _ANJ_SRV_MAN_STATE_DISCONNECT_IN_PROGRESS: {
        _anj_exchange_terminate(&anj->exchange_ctx);
        // bootstrap or restart request is the only case when we want to clean
        // up the connection
        bool with_cleanup = anj->server_state.bootstrap_request_triggered
                            || anj->server_state.restart_triggered;
        int res = _anj_server_close(&anj->connection_ctx, with_cleanup);
        if (anj_net_is_again(res)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        // priority of the state transition is:
        // 1. restart 2. bootstrap 3. disable
        if (_anj_core_state_transition_forced(anj)) {
            if (anj->server_state.restart_triggered) {
                *out_status = ANJ_CONN_STATUS_INITIAL;
            } else if (anj->server_state.bootstrap_request_triggered) {
                *out_status = ANJ_CONN_STATUS_BOOTSTRAPPING;
            } else if (anj->server_state.disable_triggered) {
                *out_status = ANJ_CONN_STATUS_SUSPENDED;
            }
            _anj_core_state_transition_clear(anj);
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }

        // queue is allowed only if the connection is closed properly
        if (anj->server_state.details.registered.internal_state
                        == _ANJ_SRV_MAN_STATE_ENTERING_QUEUE_MODE_IN_PROGRESS
                && !res) {
            anj->server_state.details.registered.internal_state =
                    _ANJ_SRV_MAN_STATE_QUEUE_MODE_IN_PROGRESS;
            *out_status = ANJ_CONN_STATUS_QUEUE_MODE;
            log(L_DEBUG, "Queue mode started");
        } else {
            *out_status = ANJ_CONN_STATUS_REGISTERING;
        }
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }
    }
    ANJ_UNREACHABLE("Invalid state");
    return _ANJ_CORE_NEXT_ACTION_LEAVE;
}

_anj_core_next_action_t
_anj_reg_session_process_suspended(anj_t *anj, anj_conn_status_t *out_status) {
    assert(anj->server_state.conn_status == ANJ_CONN_STATUS_SUSPENDED);
    uint64_t enable_time = ANJ_MAX(anj->server_state.enable_time_user_triggered,
                                   anj->server_state.enable_time);
    if (enable_time <= anj_time_real_now()) {
        anj->server_state.enable_time = 0;
        anj->server_state.enable_time_user_triggered = 0;
        *out_status = ANJ_CONN_STATUS_INITIAL;
        log(L_INFO, "Server leaving suspended state");
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }
    // stay in ANJ_CONN_STATUS_SUSPENDED
    return _ANJ_CORE_NEXT_ACTION_LEAVE;
}
