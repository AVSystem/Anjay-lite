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
#include <stdint.h>
#include <string.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/log.h>
#include <anj/time.h>
#include <anj/utils.h>

#include "../coap/coap.h"
#include "../dm/dm_integration.h"
#include "../exchange.h"
#include "../exchange_cache.h"
#include "bootstrap.h"
#include "core.h"
#include "core_utils.h"
#include "server.h"
#include "server_bootstrap.h"

#ifdef ANJ_WITH_BOOTSTRAP

static int bootstrap_op_read_data_model(anj_t *anj) {
    if (_anj_dm_get_security_obj_instance_iid(anj, _ANJ_SSID_BOOTSTRAP,
                                              &anj->security_instance.iid)) {
        log(L_ERROR, "No Bootstrap Account");
        return -1;
    }

    assert(!_anj_core_utils_validate_security_resource_types(anj));

    anj_res_value_t res_val;
    const anj_uri_path_t path =
            ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY,
                                   anj->security_instance.iid,
                                   SECURITY_OBJ_CLIENT_HOLD_OFF_TIME_RID);
    if (anj_dm_res_read(anj, &path, &res_val) || res_val.int_value < 0
            || res_val.int_value > UINT32_MAX) {
        return -1;
    }
    anj->security_instance.client_hold_off_time =
            anj_time_duration_new(res_val.int_value, ANJ_TIME_UNIT_S);
    if (_anj_core_utils_server_get_resolved_server_uri(anj)
#    ifdef ANJ_WITH_SECURITY
            || (anj->security_instance.type == ANJ_NET_BINDING_DTLS
                && _anj_core_utils_get_security_info(
                           anj, true,
                           &anj->net_socket_cfg.secure_socket_config.security))
#    endif // ANJ_WITH_SECURITY
    ) {
        return -1;
    }

    return 0;
}

int _anj_server_bootstrap_is_needed(anj_t *anj, bool *out_is_needed) {
    assert(anj);
    assert(out_is_needed);
    if (_anj_dm_get_server_obj_instance_data(anj, &anj->server_instance.ssid,
                                             &anj->server_instance.iid)) {
        return -1;
    }
    if (anj->server_instance.ssid != ANJ_ID_INVALID
            || anj->server_instance.iid != ANJ_ID_INVALID) {
        *out_is_needed = false;
        return 0;
    }
    *out_is_needed = true;
    return 0;
}

int _anj_server_bootstrap_start_bootstrap_operation(anj_t *anj) {
    assert(anj);
    if (bootstrap_op_read_data_model(anj)) {
        log(L_ERROR, "Could not get data for bootstrap");
        return -1;
    }
    // if last bootstrap session was aborted, we need to reset the state
    _anj_bootstrap_reset(anj);
    if (anj_time_duration_gt(anj->security_instance.client_hold_off_time,
                             ANJ_TIME_DURATION_ZERO)) {
        anj->server_state.details.bootstrap.bootstrap_timeout =
                anj_time_monotonic_add(
                        anj_time_monotonic_now(),
                        anj->security_instance.client_hold_off_time);
        anj->server_state.details.bootstrap.bootstrap_state =
                _ANJ_SRV_BOOTSTRAP_STATE_WAITING;
        return 0;
    }
    anj->server_state.details.bootstrap.bootstrap_retry_attempt = 0;
    anj->server_state.details.bootstrap.bootstrap_state =
            _ANJ_SRV_BOOTSTRAP_STATE_CONNECTION_IN_PROGRESS;
    return 0;
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

#    ifdef ANJ_WITH_CACHE
    // check if it's a retransmission
    if (_anj_exchange_cache_check(&anj->exchange_cache,
                                  msg.coap_binding_data.udp.message_id)
            != _ANJ_EXCHANGE_CACHE_MISS) {
        return 0;
    }
#    endif // ANJ_WITH_CACHE

    // find the right module to handle the message
    switch (msg.operation) {
    case ANJ_OP_DM_READ:
    case ANJ_OP_DM_DISCOVER:
    case ANJ_OP_DM_WRITE_REPLACE:
    case ANJ_OP_DM_DELETE:
        _anj_dm_process_request(anj, &msg, _ANJ_SSID_BOOTSTRAP, &response_code,
                                &exchange_handlers);
        if (!_ANJ_COAP_CODE_IS_ERROR(response_code)) {
            _anj_bootstrap_timeout_reset(anj);
        }
        break;
    case ANJ_OP_BOOTSTRAP_FINISH:
        _anj_bootstrap_finish_request(anj, &response_code, &exchange_handlers);
        break;
    case ANJ_OP_COAP_PING_UDP:
        break; // PING is handled by the exchange module
    case ANJ_OP_DM_READ_COMP:
    case ANJ_OP_DM_WRITE_PARTIAL_UPDATE:
    case ANJ_OP_DM_WRITE_COMP:
    case ANJ_OP_DM_EXECUTE:
    case ANJ_OP_DM_CREATE:
    case ANJ_OP_DM_WRITE_ATTR:
    case ANJ_OP_INF_OBSERVE:
    case ANJ_OP_INF_OBSERVE_COMP:
    case ANJ_OP_INF_CANCEL_OBSERVE:
    case ANJ_OP_INF_CANCEL_OBSERVE_COMP:
    default: {
        log(L_WARNING, "Invalid operation %d during Bootstrap",
            (int) msg.operation);
        return 0;
    }
    }

    if (_anj_server_prepare_server_request(anj, &msg, response_code,
                                           &exchange_handlers)) {
        return -1;
    }
    return 0;
}

static _anj_core_next_action_t handle_bootstrap_process(anj_t *anj) {
    _anj_exchange_handlers_t exchange_handlers = { 0 };
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    int result = _anj_bootstrap_process(anj, &msg, &exchange_handlers);
    switch (result) {
    case _ANJ_BOOTSTRAP_IN_PROGRESS: {
        // check for new requests
        size_t msg_size;
        result = _anj_server_receive(&anj->connection_ctx, anj->in_buffer,
                                     &msg_size, ANJ_IN_MSG_BUFFER_SIZE);
        if (anj_net_is_inprogress(result)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        if (anj_net_is_ok(result)) {
            // new message received, if decode fails or not recognized -
            // drop
            result = handle_incoming_message(anj, msg_size);
            if (result) {
                anj->server_state.details.bootstrap.bootstrap_state =
                        _ANJ_SRV_BOOTSTRAP_STATE_FINISH_DISCONNECT_AND_RETRY;
            }
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
        if (!anj_net_is_again(result)) {
            log(L_ERROR, "Error while receiving message: %d", result);
            anj->server_state.details.bootstrap.bootstrap_state =
                    _ANJ_SRV_BOOTSTRAP_STATE_FINISH_DISCONNECT_AND_RETRY;
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
    }
    case _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND:
        if (_anj_server_prepare_client_request(anj, &msg, &exchange_handlers)) {
            anj->server_state.details.bootstrap.bootstrap_state =
                    _ANJ_SRV_BOOTSTRAP_STATE_FINISH_DISCONNECT_AND_RETRY;
            log(L_ERROR, "Starting Bootstrap process failed");
        }
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    case _ANJ_BOOTSTRAP_FINISHED: {
        anj->server_state.details.bootstrap.bootstrap_state =
                _ANJ_SRV_BOOTSTRAP_STATE_FINISHED;
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }
    default:
        log(L_ERROR, "Bootstrap process failed");
        anj->server_state.details.bootstrap.bootstrap_state =
                _ANJ_SRV_BOOTSTRAP_STATE_DISCONNECT_AND_RETRY;
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }
}

static void calculate_communication_retry_timeout(anj_t *anj) {
    anj->server_state.details.bootstrap.bootstrap_retry_attempt++;
    anj_time_duration_t delay = anj_time_duration_mul(
            anj->bootstrap_retry_timeout,
            1 << (anj->server_state.details.bootstrap.bootstrap_retry_attempt
                  - 1));
    anj->server_state.details.bootstrap.bootstrap_timeout =
            anj_time_monotonic_add(anj_time_monotonic_now(), delay);

    log(L_INFO,
        "Bootstrap retry no. %" PRIu16 " will start with %s"
        "s delay",
        anj->server_state.details.bootstrap.bootstrap_retry_attempt,
        ANJ_TIME_DURATION_AS_STRING(delay, ANJ_TIME_UNIT_S));
}

_anj_core_next_action_t _anj_server_bootstrap_process_bootstrap_operation(
        anj_t *anj, anj_conn_status_t *out_status) {
    switch (anj->server_state.details.bootstrap.bootstrap_state) {
    case _ANJ_SRV_BOOTSTRAP_STATE_CONNECTION_IN_PROGRESS: {
        int result = _anj_server_connect(&anj->connection_ctx,
                                         anj->security_instance.type,
                                         &anj->net_socket_cfg,
                                         anj->security_instance.server_uri,
                                         anj->security_instance.port);
        if (anj_net_is_inprogress(result)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        if (!anj_net_is_ok(result)) {
            anj->server_state.details.bootstrap.bootstrap_state =
                    _ANJ_SRV_BOOTSTRAP_STATE_RETRY;
            log(L_ERROR, "Setting connection for Bootstrap failed");
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
        anj->server_state.details.bootstrap.bootstrap_state =
                _ANJ_SRV_BOOTSTRAP_STATE_BOOTSTRAP_IN_PROGRESS;
        _anj_exchange_handlers_t exchange_handlers = { 0 };
        _anj_coap_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        if (_anj_bootstrap_process(anj, &msg, &exchange_handlers)
                        != _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND
                || _anj_server_prepare_client_request(anj, &msg,
                                                      &exchange_handlers)) {
            anj->server_state.details.bootstrap.bootstrap_state =
                    _ANJ_SRV_BOOTSTRAP_STATE_FINISH_DISCONNECT_AND_RETRY;
            log(L_ERROR, "Starting Bootstrap process failed");
        }
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }
    case _ANJ_SRV_BOOTSTRAP_STATE_BOOTSTRAP_IN_PROGRESS: {
        int result = _anj_server_handle_request(anj);
        if (anj_net_is_again(result) || anj_net_is_inprogress(result)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        if (result) {
            anj->server_state.details.bootstrap.bootstrap_state =
                    _ANJ_SRV_BOOTSTRAP_STATE_FINISH_DISCONNECT_AND_RETRY;
            log(L_ERROR, "Bootstrap process failed");
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
        return handle_bootstrap_process(anj);
    }
    case _ANJ_SRV_BOOTSTRAP_STATE_FINISHED: {
        int result = _anj_server_close(&anj->connection_ctx, true);
        if (anj_net_is_inprogress(result)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        if (result) {
            log(L_ERROR, "Closing connection failed");
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
        *out_status = ANJ_CONN_STATUS_BOOTSTRAPPED;
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }
    case _ANJ_SRV_BOOTSTRAP_STATE_FINISH_DISCONNECT_AND_RETRY: {
        _anj_bootstrap_connection_lost(anj);
        _anj_exchange_handlers_t exchange_handlers = { 0 };
        _anj_coap_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        int result = _anj_bootstrap_process(anj, &msg, &exchange_handlers);
        assert(result == _ANJ_BOOTSTRAP_ERR_NETWORK);
        (void) result;
        anj->server_state.details.bootstrap.bootstrap_state =
                _ANJ_SRV_BOOTSTRAP_STATE_DISCONNECT_AND_RETRY;
    }
    // fall through
    case _ANJ_SRV_BOOTSTRAP_STATE_DISCONNECT_AND_RETRY: {
        int result = _anj_server_close(&anj->connection_ctx, true);
        if (anj_net_is_inprogress(result)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        if (result) {
            log(L_ERROR, "Closing connection failed");
        }
        anj->server_state.details.bootstrap.bootstrap_state =
                _ANJ_SRV_BOOTSTRAP_STATE_RETRY;
        // terminate any ongoing exchange before retrying
        _anj_exchange_terminate(&anj->exchange_ctx,
                                _ANJ_EXCHANGE_ERROR_TERMINATED);
    }
    // fall through
    case _ANJ_SRV_BOOTSTRAP_STATE_RETRY: {
        log(L_INFO, "Bootstrap entered retry state");

        if (anj->server_state.details.bootstrap.bootstrap_retry_attempt
                >= anj->bootstrap_retry_count) {
            log(L_ERROR, "Bootstrap retry limit reached");
            anj->server_state.details.bootstrap.bootstrap_state =
                    _ANJ_SRV_BOOTSTRAP_STATE_ERROR;
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
        calculate_communication_retry_timeout(anj);
        anj->server_state.details.bootstrap.bootstrap_state =
                _ANJ_SRV_BOOTSTRAP_STATE_WAITING;
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
    }
    case _ANJ_SRV_BOOTSTRAP_STATE_WAITING: {
        if (anj_time_monotonic_leq(
                    anj->server_state.details.bootstrap.bootstrap_timeout,
                    anj_time_monotonic_now())) {
            anj->server_state.details.bootstrap.bootstrap_state =
                    _ANJ_SRV_BOOTSTRAP_STATE_CONNECTION_IN_PROGRESS;
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
    }
    case _ANJ_SRV_BOOTSTRAP_STATE_ERROR: {
        log(L_ERROR, "Bootstrap process failed, no more retries scheduled");
        *out_status = ANJ_CONN_STATUS_FAILURE;
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
    }
    default:
        ANJ_UNREACHABLE();
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
    }
}

#endif // ANJ_WITH_BOOTSTRAP
