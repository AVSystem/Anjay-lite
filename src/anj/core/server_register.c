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
#include <math.h>
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
#ifdef ANJ_WITH_LWM2M_SEND
#    include <anj/lwm2m_send.h>
#endif // ANJ_WITH_LWM2M_SEND

#include "../dm/dm_integration.h"
#include "../dm/dm_io.h"
#include "../exchange.h"
#include "core_utils.h"
#include "register.h"
#include "server.h"
#include "server_register.h"

#ifndef NDEBUG
typedef struct {
    uint16_t rid;
    anj_data_type_t type;
} _resource_type_check_t;

static int validate_server_resource_types(anj_t *anj) {
    // if resource is not present and it is mandatory, it will be handled later
    // in the code
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER,
                                                 anj->server_instance.iid, 0);
    anj_data_type_t type;
    // clang-format off
    _resource_type_check_t server_obj_check[] = {
        {.rid = SERVER_OBJ_LIFETIME_RID,                          .type = ANJ_DATA_TYPE_INT},
        {.rid = SERVER_OBJ_DEFAULT_PMIN_RID,                      .type = ANJ_DATA_TYPE_INT},
        {.rid = SERVER_OBJ_DEFAULT_PMAX_RID,                      .type = ANJ_DATA_TYPE_INT},
        {.rid = SERVER_OBJ_DISABLE_TIMEOUT,                       .type = ANJ_DATA_TYPE_INT},
        {.rid = SERVER_OBJ_NOTIFICATION_STORING_RID,              .type = ANJ_DATA_TYPE_BOOL},
        {.rid = SERVER_OBJ_BOOTSTRAP_ON_REGISTRATION_FAILURE_RID, .type = ANJ_DATA_TYPE_BOOL},
        {.rid = SERVER_OBJ_COMMUNICATION_RETRY_COUNT_RID,         .type = ANJ_DATA_TYPE_UINT},
        {.rid = SERVER_OBJ_COMMUNICATION_RETRY_TIMER_RID,         .type = ANJ_DATA_TYPE_UINT},
        {.rid = SERVER_OBJ_COMMUNICATION_SEQUENCE_DELAY_TIMER_RID,.type = ANJ_DATA_TYPE_UINT},
        {.rid = SERVER_OBJ_COMMUNICATION_SEQUENCE_RETRY_COUNT_RID,.type = ANJ_DATA_TYPE_UINT},
#    ifdef ANJ_WITH_LWM2M12
        {.rid = SERVER_OBJ_DEFAULT_NOTIFICATION_MODE_RID,         .type = ANJ_DATA_TYPE_INT},
#    endif // ANJ_WITH_LWM2M12
    };
    // clang-format on
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(server_obj_check); i++) {
        path.ids[ANJ_ID_RID] = server_obj_check[i].rid;
        if (!_anj_dm_get_resource_type(anj, &path, &type)
                && type != server_obj_check[i].type) {
            log(L_ERROR, "Invalid resource type, for %" PRIu16 " RID",
                server_obj_check[i].rid);
            return -1;
        }
    }
    return 0;
}
#endif // NDEBUG

/**
 * IMPORTANT: Data validation is omitted on purpose, it is done at the level of
 * object definition and there is no point in repeating it for constrained
 * devices.
 */
static int register_op_read_data_model(anj_t *anj) {
    if (_anj_dm_get_server_obj_instance_data(anj, &anj->server_instance.ssid,
                                             &anj->server_instance.iid)
            || anj->server_instance.ssid == ANJ_ID_INVALID
            || anj->server_instance.iid == ANJ_ID_INVALID
            || _anj_dm_get_security_obj_instance_iid(
                       anj, anj->server_instance.ssid,
                       &anj->security_instance.iid)) {
        return -1;
    }

    assert(!validate_server_resource_types(anj));
    assert(!_anj_validate_security_resource_types(anj));

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER,
                                                 anj->server_instance.iid,
                                                 SERVER_OBJ_LIFETIME_RID);
    anj_res_value_t res_val;
    if (anj_dm_res_read(anj, &path, &res_val) || res_val.int_value < 0
            || res_val.int_value > UINT32_MAX) {
        return -1;
    }
    anj->server_instance.lifetime = (uint32_t) res_val.int_value;

    // Communication Retry ... resources are optional so in case of error we
    // just use default values
    anj->server_instance.retry_res = ANJ_COMMUNICATION_RETRY_RES_DEFAULT;
    path.ids[ANJ_ID_RID] = SERVER_OBJ_COMMUNICATION_RETRY_COUNT_RID;
    if (!anj_dm_res_read(anj, &path, &res_val)
            && res_val.uint_value <= UINT16_MAX) {
        anj->server_instance.retry_res.retry_count =
                (uint16_t) res_val.uint_value;
    }
    path.ids[ANJ_ID_RID] = SERVER_OBJ_COMMUNICATION_RETRY_TIMER_RID;
    if (!anj_dm_res_read(anj, &path, &res_val)
            && res_val.uint_value <= UINT32_MAX) {
        anj->server_instance.retry_res.retry_timer =
                (uint32_t) res_val.uint_value;
    }
    path.ids[ANJ_ID_RID] = SERVER_OBJ_COMMUNICATION_SEQUENCE_DELAY_TIMER_RID;
    if (!anj_dm_res_read(anj, &path, &res_val)
            && res_val.uint_value <= UINT32_MAX) {
        anj->server_instance.retry_res.seq_delay_timer =
                (uint32_t) res_val.uint_value;
    }
    path.ids[ANJ_ID_RID] = SERVER_OBJ_COMMUNICATION_SEQUENCE_RETRY_COUNT_RID;
    if (!anj_dm_res_read(anj, &path, &res_val)
            && res_val.uint_value <= UINT16_MAX) {
        anj->server_instance.retry_res.seq_retry_count =
                (uint16_t) res_val.uint_value;
    }

    // the same applies to Bootstrap on Registration Failure resource
    path.ids[ANJ_ID_RID] = SERVER_OBJ_BOOTSTRAP_ON_REGISTRATION_FAILURE_RID;
    if (!anj_dm_res_read(anj, &path, &res_val)) {
        anj->server_instance.bootstrap_on_registration_failure =
                res_val.bool_value;
    } else {
        anj->server_instance.bootstrap_on_registration_failure = true;
    }

    if (_anj_server_get_resolved_server_uri(anj)) {
        return -1;
    }

    return 0;
}

static int register_op_post_connect_operations(anj_t *anj) {
    _anj_exchange_handlers_t exchange_handlers = { 0 };
    _anj_attr_register_t register_attr = {
        .has_endpoint = true,
        .has_lifetime = true,
        .has_lwm2m_ver = true,
        .has_binding = true,
        .has_Q = anj->queue_mode_enabled,
        .endpoint = anj->endpoint_name,
        .lifetime = anj->server_instance.lifetime,
        .lwm2m_ver = _ANJ_LWM2M_VERSION_STR,
        // "OMA-TS-LightweightM2M_Core-V1_2_2-20240613-A":
        // "This value SHOULD be the same as the value in the “Supported
        // Binding and Modes” resource in the Device Object (/3/0/16)"
        .binding = ANJ_SUPPORTED_BINDING_MODES
    };
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    _anj_register_register(anj, &register_attr, &msg, &exchange_handlers);
    return _anj_server_prepare_client_request(anj, &msg, &exchange_handlers);
}

int _anj_server_register_start_register_operation(anj_t *anj) {
    if (register_op_read_data_model(anj)) {
        log(L_ERROR, "Could not get data for registration");
        return -1;
    }

    anj->server_state.details.registration.registration_state =
            _ANJ_SRV_REG_STATE_CONNECTION_IN_PROGRESS;
    anj->server_state.details.registration.retry_count = 0;
    anj->server_state.details.registration.retry_timeout = 0;
    anj->server_state.details.registration.retry_seq_count = 0;

#ifdef ANJ_WITH_LWM2M_SEND
    anj_send_abort(anj, ANJ_SEND_ID_ALL);
#endif // ANJ_WITH_LWM2M_SEND
#ifdef ANJ_WITH_OBSERVE
    _anj_observe_remove_all_observations(anj, ANJ_OBSERVE_ANY_SERVER);
#endif // ANJ_WITH_OBSERVE

    return 0;
}

static void calculate_communication_retry_timeout(anj_t *anj) {
    anj->server_state.details.registration.retry_count++;
    if (anj->server_state.details.registration.retry_count
            < anj->server_instance.retry_res.retry_count) {
        /**
         * Communication Retry Timer resource (ID: 18):
         * The delay, in seconds, between successive communication attempts in a
         * communication sequence. This value is multiplied by two to the power
         * of the communication retry attempt minus one (2**(retry attempt-1))
         * to create an exponential back-off.
         */
        uint64_t delay =
                anj->server_instance.retry_res.retry_timer
                * (uint64_t) pow(
                          2,
                          anj->server_state.details.registration.retry_count
                                  - 1);
        anj->server_state.details.registration.retry_timeout =
                anj_time_real_now()
                + delay * 1000; // *1000 because retry_timer is in seconds
        log(L_INFO,
            "Registration retry no. %" PRIu16 " will start with %" PRIu64
            "s delay",
            anj->server_state.details.registration.retry_count,
            delay);
        // disconnect and reconnect
        anj->server_state.details.registration.registration_state =
                _ANJ_SRV_REG_STATE_DISCONNECT_IN_PROGRESS;
        return;
    }

    anj->server_state.details.registration.retry_seq_count++;
    if (anj->server_state.details.registration.retry_seq_count
            >= anj->server_instance.retry_res.seq_retry_count) {
        // registration failed, fall back to bootstrap or error state
        anj->server_state.details.registration.registration_state =
                _ANJ_SRV_REG_STATE_CLEANUP_WITH_FAILURE_IN_PROGRESS;
        return;
    }

    anj->server_state.details.registration.retry_timeout =
            anj_time_real_now()
            + anj->server_instance.retry_res.seq_delay_timer * 1000;
    anj->server_state.details.registration.retry_count = 0;
    log(L_INFO,
        "Registration retry sequence no. %" PRIu16 " will start with %" PRIu32
        "s delay",
        anj->server_state.details.registration.retry_seq_count,
        anj->server_instance.retry_res.seq_delay_timer);
    // disconnect with network context cleanup and reconnect
    anj->server_state.details.registration.registration_state =
            _ANJ_SRV_REG_STATE_CLEANUP_IN_PROGRESS;
    // refresh data model - if operation failed, use old values
    register_op_read_data_model(anj);
}

_anj_core_next_action_t
_anj_server_register_process_register_operation(anj_t *anj,
                                                anj_conn_status_t *out_status) {
    switch (anj->server_state.details.registration.registration_state) {
    case _ANJ_SRV_REG_STATE_CONNECTION_IN_PROGRESS: {
        int result = _anj_server_connect(&anj->connection_ctx,
                                         anj->security_instance.type,
                                         &anj->net_socket_cfg,
                                         anj->security_instance.server_uri,
                                         anj->security_instance.port,
                                         false);
        if (anj_net_is_again(result)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        if (anj_net_is_ok(result)) {
            anj->server_state.details.registration.registration_state =
                    _ANJ_SRV_REG_STATE_REGISTER_IN_PROGRESS;
            result = register_op_post_connect_operations(anj);
        }

        if (result) {
            anj->server_state.details.registration.registration_state =
                    _ANJ_SRV_REG_STATE_ERROR_HANDLING_IN_PROGRESS;
            log(L_ERROR, "Registration error: %d", result);
        }
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }

    case _ANJ_SRV_REG_STATE_REGISTER_IN_PROGRESS: {

        int result = _anj_server_handle_request(anj);
        if (anj_net_is_again(result)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        // error occurred, or exchange is finished properly but registration
        // failed (e.g. server returned error response)
        if (result
                || _anj_register_operation_status(anj)
                               != _ANJ_REGISTER_OPERATION_FINISHED) {
            anj->server_state.details.registration.registration_state =
                    _ANJ_SRV_REG_STATE_ERROR_HANDLING_IN_PROGRESS;
            log(L_ERROR, "Registration error: %d", result);
        } else {
            *out_status = ANJ_CONN_STATUS_REGISTERED;
        }
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }

    case _ANJ_SRV_REG_STATE_ERROR_HANDLING_IN_PROGRESS: {
        _anj_exchange_terminate(&anj->exchange_ctx);
        // new value of details.registration.registration_state is set in this
        // function
        calculate_communication_retry_timeout(anj);
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }

    case _ANJ_SRV_REG_STATE_DISCONNECT_IN_PROGRESS:
    case _ANJ_SRV_REG_STATE_CLEANUP_IN_PROGRESS:
    case _ANJ_SRV_REG_STATE_CLEANUP_WITH_FAILURE_IN_PROGRESS: {
        bool with_cleanup =
                anj->server_state.details.registration.registration_state
                != _ANJ_SRV_REG_STATE_DISCONNECT_IN_PROGRESS;
        int result = _anj_server_close(&anj->connection_ctx, with_cleanup);
        if (anj_net_is_again(result)) {
            return _ANJ_CORE_NEXT_ACTION_LEAVE;
        }
        anj->server_state.details.registration.registration_state =
                (anj->server_state.details.registration.registration_state
                 == _ANJ_SRV_REG_STATE_CLEANUP_WITH_FAILURE_IN_PROGRESS)
                        ? _ANJ_SRV_REG_STATE_REGISTRATION_FAILURE_IN_PROGRESS
                        : _ANJ_SRV_REG_STATE_RESTART_IN_PROGRESS;
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }

    case _ANJ_SRV_REG_STATE_RESTART_IN_PROGRESS: {
        if (anj->server_state.details.registration.retry_timeout
                < anj_time_real_now()) {
            anj->server_state.details.registration.registration_state =
                    _ANJ_SRV_REG_STATE_CONNECTION_IN_PROGRESS;
            return _ANJ_CORE_NEXT_ACTION_CONTINUE;
        }
        return _ANJ_CORE_NEXT_ACTION_LEAVE;
    }

    case _ANJ_SRV_REG_STATE_REGISTRATION_FAILURE_IN_PROGRESS: {
        if (anj->server_instance.bootstrap_on_registration_failure) {
            log(L_ERROR, "Registration failed, fall back to bootstrap");
            *out_status = ANJ_CONN_STATUS_BOOTSTRAPPING;
        } else {
            log(L_ERROR, "Registration failed, client disabled");
            *out_status = ANJ_CONN_STATUS_FAILURE;
        }
        return _ANJ_CORE_NEXT_ACTION_CONTINUE;
    }
    default:
        ANJ_UNREACHABLE();
    }
    ANJ_UNREACHABLE("Invalid state");
    return _ANJ_CORE_NEXT_ACTION_LEAVE;
}
