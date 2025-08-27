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

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/dm/server_object.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#include "dm_core.h"

#ifdef ANJ_WITH_DEFAULT_SERVER_OBJ

#    define _ANJ_DM_DEFAULT_SERVER_OBJ_INSTANCE_IID 0

#    define ANJ_DM_SERVER_RESOURCES_COUNT 17

/*
 * Server Object Resources IDs
 */
enum anj_dm_server_resources {
    ANJ_DM_SERVER_RID_SSID = 0,
    ANJ_DM_SERVER_RID_LIFETIME = 1,
    ANJ_DM_SERVER_RID_DEFAULT_MIN_PERIOD = 2,
    ANJ_DM_SERVER_RID_DEFAULT_MAX_PERIOD = 3,
    ANJ_DM_SERVER_RID_DISABLE = 4,
    ANJ_DM_SERVER_RID_DISABLE_TIMEOUT = 5,
    ANJ_DM_SERVER_RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE = 6,
    ANJ_DM_SERVER_RID_BINDING = 7,
    ANJ_DM_SERVER_RID_REGISTRATION_UPDATE_TRIGGER = 8,
    ANJ_DM_SERVER_RID_BOOTSTRAP_REQUEST_TRIGGER = 9,
    ANJ_DM_SERVER_RID_BOOTSTRAP_ON_REGISTRATION_FAILURE = 16,
    ANJ_DM_SERVER_RID_COMMUNICATION_RETRY_COUNT = 17,
    ANJ_DM_SERVER_RID_COMMUNICATION_RETRY_TIMER = 18,
    ANJ_DM_SERVER_RID_COMMUNICATION_SEQUENCE_DELAY_TIMER = 19,
    ANJ_DM_SERVER_RID_COMMUNICATION_SEQUENCE_RETRY_COUNT = 20,
    ANJ_DM_SERVER_RID_MUTE_SEND = 23,
    ANJ_DM_SERVER_RID_DEFAULT_NOTIFICATION_MODE = 26
};

enum server_resources_idx {
    SSID_IDX,
    LIFETIME_IDX,
    DEFAULT_MIN_PERIOD_IDX,
    DEFAULT_MAX_PERIOD_IDX,
    DISABLE_IDX,
    DISABLE_TIMEOUT_IDX,
    NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE_IDX,
    BINDING_IDX,
    REGISTRATION_UPDATE_TRIGGER_IDX,
    BOOTSTRAP_REQUEST_TRIGGER_IDX,
    BOOTSTRAP_ON_REGISTRATION_FAILURE_IDX,
    COMMUNICATION_RETRY_COUNT_IDX,
    COMMUNICATION_RETRY_TIMER_IDX,
    COMMUNICATION_SEQUENCE_DELAY_TIMER_IDX,
    COMMUNICATION_SEQUENCE_RETRY_COUNT_IDX,
    MUTE_SEND_IDX,
    DEFAULT_NOTIFICATION_MODE_IDX,
    _SERVER_OBJ_RESOURCES_COUNT
};

ANJ_STATIC_ASSERT(ANJ_DM_SERVER_RESOURCES_COUNT == _SERVER_OBJ_RESOURCES_COUNT,
                  server_resources_count_mismatch);

static const anj_dm_res_t RES[ANJ_DM_SERVER_RESOURCES_COUNT] = {
    [SSID_IDX] = {
        .rid = ANJ_DM_SERVER_RID_SSID,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_R
    },
    [LIFETIME_IDX] = {
        .rid = ANJ_DM_SERVER_RID_LIFETIME,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    },
    [DEFAULT_MIN_PERIOD_IDX] = {
        .rid = ANJ_DM_SERVER_RID_DEFAULT_MIN_PERIOD,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    },
    [DEFAULT_MAX_PERIOD_IDX] = {
        .rid = ANJ_DM_SERVER_RID_DEFAULT_MAX_PERIOD,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    },
    [DISABLE_IDX] = {
        .rid = ANJ_DM_SERVER_RID_DISABLE,
        .operation = ANJ_DM_RES_E
    },
    [DISABLE_TIMEOUT_IDX] = {
        .rid = ANJ_DM_SERVER_RID_DISABLE_TIMEOUT,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    },
    [NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE_IDX] = {
        .rid = ANJ_DM_SERVER_RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE,
        .type = ANJ_DATA_TYPE_BOOL,
        .operation = ANJ_DM_RES_RW
    },
    [BINDING_IDX] = {
        .rid = ANJ_DM_SERVER_RID_BINDING,
        .type = ANJ_DATA_TYPE_STRING,
        .operation = ANJ_DM_RES_RW
    },
    [REGISTRATION_UPDATE_TRIGGER_IDX] = {
        .rid = ANJ_DM_SERVER_RID_REGISTRATION_UPDATE_TRIGGER,
        .operation = ANJ_DM_RES_E
    },
    [BOOTSTRAP_REQUEST_TRIGGER_IDX] = {
        .rid = ANJ_DM_SERVER_RID_BOOTSTRAP_REQUEST_TRIGGER,
        .operation = ANJ_DM_RES_E
    },
    [BOOTSTRAP_ON_REGISTRATION_FAILURE_IDX] = {
        .rid = ANJ_DM_SERVER_RID_BOOTSTRAP_ON_REGISTRATION_FAILURE,
        .type = ANJ_DATA_TYPE_BOOL,
        .operation = ANJ_DM_RES_R
    },
    [COMMUNICATION_RETRY_COUNT_IDX] = {
        .rid = ANJ_DM_SERVER_RID_COMMUNICATION_RETRY_COUNT,
        .type = ANJ_DATA_TYPE_UINT,
        .operation = ANJ_DM_RES_RW
    },
    [COMMUNICATION_RETRY_TIMER_IDX] = {
        .rid = ANJ_DM_SERVER_RID_COMMUNICATION_RETRY_TIMER,
        .type = ANJ_DATA_TYPE_UINT,
        .operation = ANJ_DM_RES_RW
    },
    [COMMUNICATION_SEQUENCE_DELAY_TIMER_IDX] = {
        .rid = ANJ_DM_SERVER_RID_COMMUNICATION_SEQUENCE_DELAY_TIMER,
        .type = ANJ_DATA_TYPE_UINT,
        .operation = ANJ_DM_RES_RW
    },
    [COMMUNICATION_SEQUENCE_RETRY_COUNT_IDX] = {
        .rid = ANJ_DM_SERVER_RID_COMMUNICATION_SEQUENCE_RETRY_COUNT,
        .type = ANJ_DATA_TYPE_UINT,
        .operation = ANJ_DM_RES_RW
    },
    [MUTE_SEND_IDX] = {
        .rid = ANJ_DM_SERVER_RID_MUTE_SEND,
        .type = ANJ_DATA_TYPE_BOOL,
        .operation = ANJ_DM_RES_RW
    },
    [DEFAULT_NOTIFICATION_MODE_IDX] = {
        .rid = ANJ_DM_SERVER_RID_DEFAULT_NOTIFICATION_MODE,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    }
};

static void initialize_instance(anj_server_instance_t *inst) {
    assert(inst);
    memset(inst, 0, sizeof(*inst));
    inst->bootstrap_on_registration_failure = true;
    inst->comm_retry_res = ANJ_COMMUNICATION_RETRY_RES_DEFAULT;
    inst->disable_timeout = ANJ_DISABLE_TIMEOUT_DEFAULT_VALUE;
}

static int is_valid_binding_mode(const char *binding_mode) {
    if (!*binding_mode) {
        return -1;
    }
    for (const char *p = binding_mode; *p; ++p) {
        if (!strchr(ANJ_DM_SERVER_OBJ_BINDINGS, *p)) {
            return -1;
        }
        if (strchr(p + 1, *p)) {
            // duplications
            return -1;
        }
    }
    return 0;
}

static int validate_instance(anj_server_instance_t *inst) {
    if (!inst) {
        return -1;
    }
    if (inst->ssid == ANJ_ID_INVALID || inst->ssid == _ANJ_SSID_BOOTSTRAP
            || (inst->default_max_period != 0
                && inst->default_max_period < inst->default_min_period)
            || is_valid_binding_mode(inst->binding)
            || inst->comm_retry_res.retry_count == 0
            || inst->comm_retry_res.seq_retry_count == 0
            || inst->default_notification_mode > 1) {
        return -1;
    }
    return 0;
}

static int res_execute(anj_t *anj,
                       const anj_dm_obj_t *obj,
                       anj_iid_t iid,
                       anj_rid_t rid,
                       const char *execute_arg,
                       size_t execute_arg_len) {
    (void) iid;
    (void) execute_arg;
    (void) execute_arg_len;

    switch (rid) {
    case ANJ_DM_SERVER_RID_DISABLE: {
        anj_dm_server_obj_t *ctx =
                ANJ_CONTAINER_OF(obj, anj_dm_server_obj_t, obj);
        anj_server_instance_t *inst = &ctx->server_instance;
        anj_core_server_obj_disable_executed(anj, inst->disable_timeout);
        break;
    }
    case ANJ_DM_SERVER_RID_REGISTRATION_UPDATE_TRIGGER:
        anj_core_server_obj_registration_update_trigger_executed(anj);
        break;
    case ANJ_DM_SERVER_RID_BOOTSTRAP_REQUEST_TRIGGER:
        anj_core_server_obj_bootstrap_request_trigger_executed(anj);
        break;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }

    return 0;
}

static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) anj;
    (void) iid;
    (void) riid;

    anj_dm_server_obj_t *ctx = ANJ_CONTAINER_OF(obj, anj_dm_server_obj_t, obj);
    anj_server_instance_t *serv_inst = &ctx->server_instance;

    switch (rid) {
    case ANJ_DM_SERVER_RID_SSID:
        if (value->int_value <= 0 || value->int_value >= UINT16_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->ssid = (uint16_t) value->int_value;
        break;
    case ANJ_DM_SERVER_RID_LIFETIME:
        if (value->int_value < 0 || value->int_value > UINT32_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->lifetime = (uint32_t) value->int_value;
        break;
    case ANJ_DM_SERVER_RID_DISABLE_TIMEOUT:
        if (value->int_value < 0 || value->int_value > UINT32_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->disable_timeout = (uint32_t) value->int_value;
        break;
    case ANJ_DM_SERVER_RID_DEFAULT_MIN_PERIOD:
        if (value->int_value < 0 || value->int_value > UINT32_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->default_min_period = (uint32_t) value->int_value;
        break;
    case ANJ_DM_SERVER_RID_DEFAULT_MAX_PERIOD:
        if (value->int_value < 0 || value->int_value > UINT32_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->default_max_period = (uint32_t) value->int_value;
        break;
    case ANJ_DM_SERVER_RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE:
        serv_inst->notification_storing = value->bool_value;
        break;
    case ANJ_DM_SERVER_RID_BINDING:
        return anj_dm_write_string_chunked(value, serv_inst->binding,
                                           ANJ_ARRAY_SIZE(serv_inst->binding),
                                           NULL);
    case ANJ_DM_SERVER_RID_BOOTSTRAP_ON_REGISTRATION_FAILURE:
        serv_inst->bootstrap_on_registration_failure = value->bool_value;
        break;
    case ANJ_DM_SERVER_RID_COMMUNICATION_RETRY_COUNT:
        if (value->uint_value > UINT16_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->comm_retry_res.retry_count = (uint16_t) value->uint_value;
        break;
    case ANJ_DM_SERVER_RID_COMMUNICATION_RETRY_TIMER:
        if (value->uint_value > UINT32_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->comm_retry_res.retry_timer = (uint32_t) value->uint_value;
        break;
    case ANJ_DM_SERVER_RID_COMMUNICATION_SEQUENCE_DELAY_TIMER:
        if (value->uint_value > UINT32_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->comm_retry_res.seq_delay_timer =
                (uint32_t) value->uint_value;
        break;
    case ANJ_DM_SERVER_RID_COMMUNICATION_SEQUENCE_RETRY_COUNT:
        if (value->uint_value > UINT16_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->comm_retry_res.seq_retry_count =
                (uint16_t) value->uint_value;
        break;
    case ANJ_DM_SERVER_RID_MUTE_SEND:
        serv_inst->mute_send = value->bool_value;
        break;
    case ANJ_DM_SERVER_RID_DEFAULT_NOTIFICATION_MODE:
        if (value->int_value > UINT8_MAX || value->int_value < 0) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        serv_inst->default_notification_mode = (uint8_t) value->int_value;
        break;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }
    return 0;
}

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) riid;
    (void) iid;

    anj_dm_server_obj_t *ctx = ANJ_CONTAINER_OF(obj, anj_dm_server_obj_t, obj);
    anj_server_instance_t *serv_inst = &ctx->server_instance;

    switch (rid) {
    case ANJ_DM_SERVER_RID_SSID:
        out_value->int_value = serv_inst->ssid;
        break;
    case ANJ_DM_SERVER_RID_LIFETIME:
        out_value->int_value = serv_inst->lifetime;
        break;
    case ANJ_DM_SERVER_RID_DISABLE_TIMEOUT:
        out_value->int_value = serv_inst->disable_timeout;
        break;
    case ANJ_DM_SERVER_RID_DEFAULT_MIN_PERIOD:
        out_value->int_value = serv_inst->default_min_period;
        break;
    case ANJ_DM_SERVER_RID_DEFAULT_MAX_PERIOD:
        out_value->int_value = serv_inst->default_max_period;
        break;
    case ANJ_DM_SERVER_RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE:
        out_value->bool_value = serv_inst->notification_storing;
        break;
    case ANJ_DM_SERVER_RID_BINDING:
        out_value->bytes_or_string = (anj_bytes_or_string_value_t) {
            .data = serv_inst->binding,
        };
        break;
    case ANJ_DM_SERVER_RID_BOOTSTRAP_ON_REGISTRATION_FAILURE:
        out_value->bool_value = serv_inst->bootstrap_on_registration_failure;
        break;
    case ANJ_DM_SERVER_RID_COMMUNICATION_RETRY_COUNT:
        out_value->uint_value = serv_inst->comm_retry_res.retry_count;
        break;
    case ANJ_DM_SERVER_RID_COMMUNICATION_RETRY_TIMER:
        out_value->uint_value = serv_inst->comm_retry_res.retry_timer;
        break;
    case ANJ_DM_SERVER_RID_COMMUNICATION_SEQUENCE_DELAY_TIMER:
        out_value->uint_value = serv_inst->comm_retry_res.seq_delay_timer;
        break;
    case ANJ_DM_SERVER_RID_COMMUNICATION_SEQUENCE_RETRY_COUNT:
        out_value->uint_value = serv_inst->comm_retry_res.seq_retry_count;
        break;
    case ANJ_DM_SERVER_RID_MUTE_SEND:
        out_value->bool_value = serv_inst->mute_send;
        break;
    case ANJ_DM_SERVER_RID_DEFAULT_NOTIFICATION_MODE:
        out_value->int_value = serv_inst->default_notification_mode;
        break;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }

    return 0;
}

#    ifdef ANJ_WITH_BOOTSTRAP
static int inst_create(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    anj_dm_server_obj_t *ctx = ANJ_CONTAINER_OF(obj, anj_dm_server_obj_t, obj);
    initialize_instance(&ctx->server_instance);
    ctx->inst.iid = iid;
    return 0;
}

static int inst_delete(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    (void) iid;
    anj_dm_server_obj_t *ctx = ANJ_CONTAINER_OF(obj, anj_dm_server_obj_t, obj);
    initialize_instance(&ctx->server_instance);
    ctx->inst.iid = ANJ_ID_INVALID;
    return 0;
}
#    endif // ANJ_WITH_BOOTSTRAP

static int inst_reset(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    (void) iid;
    anj_dm_server_obj_t *ctx = ANJ_CONTAINER_OF(obj, anj_dm_server_obj_t, obj);
    initialize_instance(&ctx->server_instance);
    return 0;
}

static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    anj_dm_server_obj_t *ctx = ANJ_CONTAINER_OF(obj, anj_dm_server_obj_t, obj);
    memcpy(&ctx->cache_server_instance,
           &ctx->server_instance,
           sizeof(ctx->server_instance));
    memcpy(&ctx->cache_inst, &ctx->inst, sizeof(ctx->inst));
    return 0;
}

static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    anj_dm_server_obj_t *ctx = ANJ_CONTAINER_OF(obj, anj_dm_server_obj_t, obj);
    if (ctx->inst.iid != ANJ_ID_INVALID
            && validate_instance(&ctx->server_instance)) {
        return ANJ_DM_ERR_BAD_REQUEST;
    }
    return 0;
}

static void transaction_end(anj_t *anj, const anj_dm_obj_t *obj, int result) {
    (void) anj;
    anj_dm_server_obj_t *ctx = ANJ_CONTAINER_OF(obj, anj_dm_server_obj_t, obj);
    if (result) {
        // restore cached data
        memcpy(&ctx->server_instance, &ctx->cache_server_instance,
               sizeof(ctx->server_instance));
        memcpy(&ctx->inst, &ctx->cache_inst, sizeof(ctx->inst));
    }
}

static const anj_dm_handlers_t HANDLERS = {
#    ifdef ANJ_WITH_BOOTSTRAP
    .inst_create = inst_create,
    .inst_delete = inst_delete,
#    endif // ANJ_WITH_BOOTSTRAP
    .inst_reset = inst_reset,
    .transaction_begin = transaction_begin,
    .transaction_validate = transaction_validate,
    .transaction_end = transaction_end,
    .res_read = res_read,
    .res_write = res_write,
    .res_execute = res_execute
};

void anj_dm_server_obj_init(anj_dm_server_obj_t *server_obj_ctx) {
    assert(server_obj_ctx);
    memset(server_obj_ctx, 0, sizeof(*server_obj_ctx));

    server_obj_ctx->obj = (anj_dm_obj_t) {
        .oid = ANJ_OBJ_ID_SERVER,
        .version = "1.2",
        .max_inst_count = 1,
        .insts = &server_obj_ctx->inst,
        .handlers = &HANDLERS
    };

    server_obj_ctx->inst.resources = RES;
    server_obj_ctx->inst.res_count = _SERVER_OBJ_RESOURCES_COUNT;
    server_obj_ctx->inst.iid = ANJ_ID_INVALID;
}

int anj_dm_server_obj_add_instance(
        anj_dm_server_obj_t *server_obj_ctx,
        const anj_dm_server_instance_init_t *instance) {
    assert(server_obj_ctx && instance);
    assert(!server_obj_ctx->installed);
    assert(!instance->iid || *instance->iid != ANJ_ID_INVALID);
    assert(instance->binding);

    if (server_obj_ctx->inst.iid != ANJ_ID_INVALID) {
        dm_log(L_ERROR, "Only one instance of Server Object is allowed");
        return -1;
    }

    anj_server_instance_t *serv_inst = &server_obj_ctx->server_instance;
    if (strlen(instance->binding) > sizeof(serv_inst->binding) - 1) {
        dm_log(L_ERROR, "Binding string too long");
        return -1;
    }
    initialize_instance(serv_inst);
    strcpy(serv_inst->binding, instance->binding);
    serv_inst->ssid = instance->ssid;
    if (instance->bootstrap_on_registration_failure) {
        serv_inst->bootstrap_on_registration_failure =
                *instance->bootstrap_on_registration_failure;
    }
    if (instance->comm_retry_res) {
        serv_inst->comm_retry_res = *instance->comm_retry_res;
    }
    if (instance->disable_timeout) {
        serv_inst->disable_timeout = instance->disable_timeout;
    }
    serv_inst->default_max_period = instance->default_max_period;
    serv_inst->default_min_period = instance->default_min_period;
    serv_inst->lifetime = instance->lifetime;
    serv_inst->mute_send = instance->mute_send;
    serv_inst->notification_storing = instance->notification_storing;
    serv_inst->default_notification_mode = instance->default_notification_mode;
    if (validate_instance(serv_inst)) {
        serv_inst->ssid = ANJ_ID_INVALID;
        return -1;
    }
    server_obj_ctx->inst.iid =
            instance->iid ? *instance->iid
                          : _ANJ_DM_DEFAULT_SERVER_OBJ_INSTANCE_IID;
    return 0;
}

int anj_dm_server_obj_install(anj_t *anj, anj_dm_server_obj_t *server_obj_ctx) {
    assert(anj && server_obj_ctx);
    assert(!server_obj_ctx->installed);
    int res = anj_dm_add_obj(anj, &server_obj_ctx->obj);
    if (!res) {
        server_obj_ctx->installed = true;
        dm_log(L_INFO, "Server object installed");
    }
    return res;
}

#endif // ANJ_WITH_DEFAULT_SERVER_OBJ
