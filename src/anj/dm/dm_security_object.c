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
#include <anj/dm/security_object.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#include "dm_core.h"

#ifdef ANJ_WITH_DEFAULT_SECURITY_OBJ

#    define ANJ_DM_SECURITY_RESOURCES_COUNT 8

/*
 * Security Object Resources IDs
 */
enum anj_dm_security_resources {
    ANJ_DM_SECURITY_RID_SERVER_URI = 0,
    ANJ_DM_SECURITY_RID_BOOTSTRAP_SERVER = 1,
    ANJ_DM_SECURITY_RID_SECURITY_MODE = 2,
    ANJ_DM_SECURITY_RID_PUBLIC_KEY_OR_IDENTITY = 3,
    ANJ_DM_SECURITY_RID_SERVER_PUBLIC_KEY = 4,
    ANJ_DM_SECURITY_RID_SECRET_KEY = 5,
    ANJ_DM_SECURITY_RID_SSID = 10,
    ANJ_DM_SECURITY_RID_CLIENT_HOLD_OFF_TIME = 11,
};

enum security_resources_idx {
    SERVER_URI_IDX = 0,
    BOOTSTRAP_SERVER_IDX,
    SECURITY_MODE_IDX,
    PUBLIC_KEY_OR_IDENTITY_IDX,
    SERVER_PUBLIC_KEY_IDX,
    SECRET_KEY_IDX,
    SSID_IDX,
    CLIENT_HOLD_OFF_TIME_IDX,
    _SECURITY_OBJ_RESOURCES_COUNT
};

ANJ_STATIC_ASSERT(ANJ_DM_SECURITY_RESOURCES_COUNT
                          == _SECURITY_OBJ_RESOURCES_COUNT,
                  security_resources_count_mismatch);

static const anj_dm_res_t RES[ANJ_DM_SECURITY_RESOURCES_COUNT] = {
    [SERVER_URI_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_SERVER_URI,
        .type = ANJ_DATA_TYPE_STRING,
        .operation = ANJ_DM_RES_RW
    },
    [BOOTSTRAP_SERVER_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_BOOTSTRAP_SERVER,
        .type = ANJ_DATA_TYPE_BOOL,
        .operation = ANJ_DM_RES_RW
    },
    [SECURITY_MODE_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_SECURITY_MODE,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    },
    [PUBLIC_KEY_OR_IDENTITY_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_PUBLIC_KEY_OR_IDENTITY,
        .type = ANJ_DATA_TYPE_BYTES,
        .operation = ANJ_DM_RES_RW
    },
    [SERVER_PUBLIC_KEY_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_SERVER_PUBLIC_KEY,
        .type = ANJ_DATA_TYPE_BYTES,
        .operation = ANJ_DM_RES_RW
    },
    [SECRET_KEY_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_SECRET_KEY,
        .type = ANJ_DATA_TYPE_BYTES,
        .operation = ANJ_DM_RES_RW
    },
    [SSID_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_SSID,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    },
    [CLIENT_HOLD_OFF_TIME_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_CLIENT_HOLD_OFF_TIME,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    }
};

static void initialize_instance(anj_dm_security_instance_t *inst,
                                anj_iid_t iid) {
    assert(inst);
    memset(inst, 0, sizeof(anj_dm_security_instance_t));
    inst->iid = iid;
}

static anj_iid_t find_free_iid(anj_dm_security_obj_t *security_obj_ctx) {
    for (anj_iid_t candidate = 0; candidate < UINT16_MAX; candidate++) {
        bool used = false;
        for (uint16_t i = 0; i < ANJ_DM_SECURITY_OBJ_INSTANCES; i++) {
            if (security_obj_ctx->security_instances[i].iid == candidate) {
                used = true;
                break;
            }
        }
        if (!used) {
            return candidate;
        }
    }
    return ANJ_ID_INVALID;
}

static const char *URI_SCHEME[] = { "coap", "coaps", "coap+tcp", "coaps+tcp" };

static bool valid_uri_scheme(const char *uri) {
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(URI_SCHEME); i++) {
        if (!strncmp(uri, URI_SCHEME[i], strlen(URI_SCHEME[i]))
                && uri[strlen(URI_SCHEME[i])] == ':') {
            return true;
        }
    }
    return false;
}

static bool valid_security_mode(int64_t mode) {
    /* LwM2M specification defines modes in range 0..4 */
    return mode >= ANJ_DM_SECURITY_PSK && mode <= ANJ_DM_SECURITY_EST;
}

static int validate_instance(anj_dm_security_instance_t *inst) {
    if (!inst) {
        return -1;
    }
    if (!valid_uri_scheme(inst->server_uri)) {
        return -1;
    }
    if (!valid_security_mode(inst->security_mode)) {
        return -1;
    }
    if (inst->ssid == ANJ_ID_INVALID
            || (inst->ssid == _ANJ_SSID_BOOTSTRAP && !inst->bootstrap_server)) {
        return -1;
    }
    return 0;
}

static anj_dm_security_instance_t *find_sec_inst(const anj_dm_obj_t *obj,
                                                 anj_iid_t iid) {
    anj_dm_security_obj_t *ctx =
            ANJ_CONTAINER_OF(obj, anj_dm_security_obj_t, obj);
    for (uint16_t idx = 0; idx < ANJ_DM_SECURITY_OBJ_INSTANCES; idx++) {
        if (ctx->security_instances[idx].iid == iid) {
            return &ctx->security_instances[idx];
        }
    }
    return NULL;
}

static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) anj;
    (void) riid;

    anj_dm_security_instance_t *sec_inst = find_sec_inst(obj, iid);

    switch (rid) {
    case ANJ_DM_SECURITY_RID_SERVER_URI:
        return anj_dm_write_string_chunked(value, sec_inst->server_uri,
                                           sizeof(sec_inst->server_uri), NULL);
    case ANJ_DM_SECURITY_RID_BOOTSTRAP_SERVER:
        sec_inst->bootstrap_server = value->bool_value;
        break;
    case ANJ_DM_SECURITY_RID_SECURITY_MODE:
        if (!valid_security_mode(value->int_value)) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        sec_inst->security_mode = value->int_value;
        break;
    case ANJ_DM_SECURITY_RID_PUBLIC_KEY_OR_IDENTITY:
        return anj_dm_write_bytes_chunked(
                value, (uint8_t *) sec_inst->public_key_or_identity,
                sizeof(sec_inst->public_key_or_identity),
                &sec_inst->public_key_or_identity_size, NULL);
    case ANJ_DM_SECURITY_RID_SERVER_PUBLIC_KEY:
        return anj_dm_write_bytes_chunked(
                value, (uint8_t *) sec_inst->server_public_key,
                sizeof(sec_inst->server_public_key),
                &sec_inst->server_public_key_size, NULL);
    case ANJ_DM_SECURITY_RID_SECRET_KEY:
        return anj_dm_write_bytes_chunked(value,
                                          (uint8_t *) sec_inst->secret_key,
                                          sizeof(sec_inst->secret_key),
                                          &sec_inst->secret_key_size, NULL);
    case ANJ_DM_SECURITY_RID_SSID:
        if (value->int_value <= 0 || value->int_value >= UINT16_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        sec_inst->ssid = (uint16_t) value->int_value;
        break;
    case ANJ_DM_SECURITY_RID_CLIENT_HOLD_OFF_TIME:
        if (value->int_value < 0 || value->int_value > UINT32_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        sec_inst->client_hold_off_time = (uint32_t) value->int_value;
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

    anj_dm_security_instance_t *sec_inst = find_sec_inst(obj, iid);

    switch (rid) {
    case ANJ_DM_SECURITY_RID_SERVER_URI:
        out_value->bytes_or_string.data = sec_inst->server_uri;
        break;
    case ANJ_DM_SECURITY_RID_BOOTSTRAP_SERVER:
        out_value->bool_value = sec_inst->bootstrap_server;
        break;
    case ANJ_DM_SECURITY_RID_SECURITY_MODE:
        out_value->int_value = sec_inst->security_mode;
        break;
    case ANJ_DM_SECURITY_RID_PUBLIC_KEY_OR_IDENTITY:
        out_value->bytes_or_string.data = sec_inst->public_key_or_identity;
        out_value->bytes_or_string.chunk_length =
                sec_inst->public_key_or_identity_size;
        break;
    case ANJ_DM_SECURITY_RID_SERVER_PUBLIC_KEY:
        out_value->bytes_or_string.data = sec_inst->server_public_key;
        out_value->bytes_or_string.chunk_length =
                sec_inst->server_public_key_size;
        break;
    case ANJ_DM_SECURITY_RID_SECRET_KEY:
        out_value->bytes_or_string.data = sec_inst->secret_key;
        out_value->bytes_or_string.chunk_length = sec_inst->secret_key_size;
        break;
    case ANJ_DM_SECURITY_RID_SSID:
        out_value->int_value = sec_inst->ssid;
        break;
    case ANJ_DM_SECURITY_RID_CLIENT_HOLD_OFF_TIME:
        out_value->int_value = sec_inst->client_hold_off_time;
        break;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }
    return 0;
}

static void insert_new_instance(anj_dm_security_obj_t *ctx,
                                anj_iid_t current_iid,
                                anj_iid_t new_iid) {
    for (uint16_t idx = 0; idx < ANJ_DM_SECURITY_OBJ_INSTANCES; idx++) {
        if (ctx->inst[idx].iid == current_iid) {
            ctx->inst[idx].iid = new_iid;
#    ifdef ANJ_WITH_BOOTSTRAP
            // sort the instances by iid, for active bootstrap there is only two
            // instances
            if (ctx->inst[0].iid > ctx->inst[1].iid) {
                anj_dm_obj_inst_t tmp = ctx->inst[0];
                ctx->inst[0] = ctx->inst[1];
                ctx->inst[1] = tmp;
            }
#    endif // ANJ_WITH_BOOTSTRAP
            return;
        }
    }
    // should never happen
    ANJ_UNREACHABLE("Instance not found");
}

#    ifdef ANJ_WITH_BOOTSTRAP
static int inst_create(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    anj_dm_security_obj_t *ctx =
            ANJ_CONTAINER_OF(obj, anj_dm_security_obj_t, obj);
    anj_dm_security_instance_t *sec_inst = find_sec_inst(obj, ANJ_ID_INVALID);
    // set only iid for the instance, remaining fields are already set in
    // anj_dm_security_obj_init()
    insert_new_instance(ctx, ANJ_ID_INVALID, iid);
    initialize_instance(sec_inst, iid);
    // in case of failure, iid will be set to ANJ_ID_INVALID
    ctx->new_instance_iid = iid;
    return 0;
}

static int inst_delete(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    anj_dm_security_obj_t *ctx =
            ANJ_CONTAINER_OF(obj, anj_dm_security_obj_t, obj);
    anj_dm_security_instance_t *sec_inst = find_sec_inst(obj, iid);
    sec_inst->iid = ANJ_ID_INVALID;
    insert_new_instance(ctx, iid, ANJ_ID_INVALID);
    return 0;
}
#    endif // ANJ_WITH_BOOTSTRAP

static int inst_reset(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    anj_dm_security_instance_t *sec_inst = find_sec_inst(obj, iid);
    initialize_instance(sec_inst, iid);
    return 0;
}

static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    anj_dm_security_obj_t *ctx =
            ANJ_CONTAINER_OF(obj, anj_dm_security_obj_t, obj);
    memcpy(ctx->cache_security_instances,
           ctx->security_instances,
           sizeof(ctx->security_instances));
    memcpy(ctx->cache_inst, ctx->inst, sizeof(ctx->inst));
    return 0;
}

static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    anj_dm_security_obj_t *ctx =
            ANJ_CONTAINER_OF(obj, anj_dm_security_obj_t, obj);
    for (uint16_t idx = 0; idx < ANJ_DM_SECURITY_OBJ_INSTANCES; idx++) {
        anj_dm_security_instance_t *sec_inst = &ctx->security_instances[idx];
        if (sec_inst->iid != ANJ_ID_INVALID) {
            if (validate_instance(sec_inst)) {
                return ANJ_DM_ERR_BAD_REQUEST;
            }
        }
    }
    return 0;
}

static void transaction_end(anj_t *anj, const anj_dm_obj_t *obj, int result) {
    (void) anj;
    anj_dm_security_obj_t *ctx =
            ANJ_CONTAINER_OF(obj, anj_dm_security_obj_t, obj);
    // for Delete operation, there is no possibility of failure
    if (result) {
        memcpy(ctx->security_instances,
               ctx->cache_security_instances,
               sizeof(ctx->security_instances));
        memcpy(ctx->inst, ctx->cache_inst, sizeof(ctx->inst));
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
};

void anj_dm_security_obj_init(anj_dm_security_obj_t *security_obj_ctx) {
    assert(security_obj_ctx);
    memset(security_obj_ctx, 0, sizeof(*security_obj_ctx));

    security_obj_ctx->obj = (anj_dm_obj_t) {
        .oid = ANJ_OBJ_ID_SECURITY,
        .version = "1.0",
        .max_inst_count = ANJ_DM_SECURITY_OBJ_INSTANCES,
        .insts = security_obj_ctx->inst,
        .handlers = &HANDLERS
    };

    for (uint16_t idx = 0; idx < ANJ_DM_SECURITY_OBJ_INSTANCES; idx++) {
        security_obj_ctx->inst[idx].resources = RES;
        security_obj_ctx->inst[idx].res_count = _SECURITY_OBJ_RESOURCES_COUNT;
        security_obj_ctx->inst[idx].iid = ANJ_ID_INVALID;
        security_obj_ctx->security_instances[idx].iid = ANJ_ID_INVALID;
    }
}

int anj_dm_security_obj_add_instance(
        anj_dm_security_obj_t *security_obj_ctx,
        anj_dm_security_instance_init_t *instance) {
    assert(security_obj_ctx && instance);
    assert(!security_obj_ctx->installed);
    assert(!instance->iid || *instance->iid != ANJ_ID_INVALID);
    assert(instance->server_uri);

    anj_dm_security_instance_t *sec_inst =
            find_sec_inst(&security_obj_ctx->obj, ANJ_ID_INVALID);
    if (!sec_inst) {
        dm_log(L_ERROR, "Maximum number of instances reached");
        return -1;
    }

    for (uint16_t idx = 0; idx < ANJ_DM_SECURITY_OBJ_INSTANCES; idx++) {
        if (instance->ssid
                && instance->ssid
                               == security_obj_ctx->security_instances[idx]
                                          .ssid) {
            dm_log(L_ERROR, "Given ssid already exists");
            return -1;
        }
        if (instance->iid
                && *instance->iid == security_obj_ctx->inst[idx].iid) {
            dm_log(L_ERROR, "Given iid already exists");
            return -1;
        }
    }

    initialize_instance(sec_inst, ANJ_ID_INVALID);
    if (strlen(instance->server_uri) > sizeof(sec_inst->server_uri) - 1) {
        dm_log(L_ERROR, "Server URI too long");
        return -1;
    }
    if (instance->public_key_or_identity
            && instance->public_key_or_identity_size
                           > sizeof(sec_inst->public_key_or_identity)) {
        dm_log(L_ERROR, "Public key or identity too long");
        return -1;
    }
    if (instance->server_public_key
            && instance->server_public_key_size
                           > sizeof(sec_inst->server_public_key)) {
        dm_log(L_ERROR, "Server public key too long");
        return -1;
    }
    if (instance->secret_key
            && instance->secret_key_size > sizeof(sec_inst->secret_key)) {
        dm_log(L_ERROR, "Secret key too long");
        return -1;
    }

    strcpy(sec_inst->server_uri, instance->server_uri);
    sec_inst->bootstrap_server = instance->bootstrap_server;
    sec_inst->ssid =
            sec_inst->bootstrap_server ? _ANJ_SSID_BOOTSTRAP : instance->ssid;
    sec_inst->security_mode = instance->security_mode;
    if (instance->public_key_or_identity) {
        memcpy(sec_inst->public_key_or_identity,
               instance->public_key_or_identity,
               instance->public_key_or_identity_size);
        sec_inst->public_key_or_identity_size =
                instance->public_key_or_identity_size;
    }
    if (instance->server_public_key) {
        memcpy(sec_inst->server_public_key, instance->server_public_key,
               instance->server_public_key_size);
        sec_inst->server_public_key_size = instance->server_public_key_size;
    }
    if (instance->secret_key) {
        memcpy(sec_inst->secret_key, instance->secret_key,
               instance->secret_key_size);
        sec_inst->secret_key_size = instance->secret_key_size;
    }
    sec_inst->client_hold_off_time = instance->client_hold_off_time;
    if (validate_instance(sec_inst)) {
        sec_inst->ssid = ANJ_ID_INVALID;
        return -1;
    }

    anj_iid_t iid =
            instance->iid ? *instance->iid : find_free_iid(security_obj_ctx);
    sec_inst->iid = iid;

    insert_new_instance(security_obj_ctx, ANJ_ID_INVALID, iid);
    return 0;
}

int anj_dm_security_obj_install(anj_t *anj,
                                anj_dm_security_obj_t *security_obj_ctx) {
    assert(anj && security_obj_ctx);
    assert(!security_obj_ctx->installed);
    int res = anj_dm_add_obj(anj, &security_obj_ctx->obj);
    if (res) {
        return res;
    }
    dm_log(L_INFO, "Security object installed");
    security_obj_ctx->installed = true;
    return 0;
}

#endif // ANJ_WITH_DEFAULT_SECURITY_OBJ
