/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 27

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/core.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/dm/security_object.h>
#include <anj/log.h>
#include <anj/utils.h>

#include "dm_core.h"

#ifdef ANJ_WITH_SECURITY
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
#        include <anj/compat/crypto/storage.h>
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
#    include <anj/crypto.h>
#endif // ANJ_WITH_SECURITY

#ifdef ANJ_WITH_PERSISTENCE
#    include <anj/persistence.h>
#endif // ANJ_WITH_PERSISTENCE

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
        .kind = ANJ_DM_RES_RW
    },
    [BOOTSTRAP_SERVER_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_BOOTSTRAP_SERVER,
        .type = ANJ_DATA_TYPE_BOOL,
        .kind = ANJ_DM_RES_RW
    },
    [SECURITY_MODE_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_SECURITY_MODE,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_RW
    },
    [PUBLIC_KEY_OR_IDENTITY_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_PUBLIC_KEY_OR_IDENTITY,
        .type = ANJ_DATA_TYPE_BYTES,
        .kind = ANJ_DM_RES_RW
    },
    [SERVER_PUBLIC_KEY_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_SERVER_PUBLIC_KEY,
        .type = ANJ_DATA_TYPE_BYTES,
        .kind = ANJ_DM_RES_RW
    },
    [SECRET_KEY_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_SECRET_KEY,
        .type = ANJ_DATA_TYPE_BYTES,
        .kind = ANJ_DM_RES_RW
    },
    [SSID_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_SSID,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_RW
    },
    [CLIENT_HOLD_OFF_TIME_IDX] = {
        .rid = ANJ_DM_SECURITY_RID_CLIENT_HOLD_OFF_TIME,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_RW
    },
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

static const char *URI_SCHEME[] = { "coap", "coaps" };

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
    if (!valid_security_mode((int64_t) inst->security_mode)) {
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

#    ifdef ANJ_WITH_SECURITY
static int write_security_info(anj_t *anj,
                               const anj_res_value_t *value,
                               anj_crypto_security_info_t *sec_info,
                               uint8_t *buffer,
                               size_t buffer_len,
                               anj_crypto_security_tag_t tag) {
#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    (void) buffer;
    (void) buffer_len;
    int res = 0;
    if (sec_info->source == ANJ_CRYPTO_DATA_SOURCE_EMPTY) {
        // set basic security info before storing data
        sec_info->source = ANJ_CRYPTO_DATA_SOURCE_EXTERNAL;
        sec_info->tag = tag;
        res = anj_crypto_storage_create_new_record(anj->crypto_ctx, sec_info);
        if (res) {
            dm_log(L_ERROR, "Failed to create new security record: %d", res);
            return res;
        }
    }
    bool last_chunk =
            value->bytes_or_string.offset + value->bytes_or_string.chunk_length
            == value->bytes_or_string.full_length_hint;
    res = anj_crypto_storage_store_data(anj->crypto_ctx,
                                        sec_info,
                                        value->bytes_or_string.data,
                                        value->bytes_or_string.chunk_length,
                                        last_chunk);
    if (res) {
        dm_log(L_ERROR, "Failed to store security data: %d", res);
        return res;
    }
    return 0;
#        else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    (void) anj;
    (void) tag;
    sec_info->source = ANJ_CRYPTO_DATA_SOURCE_BUFFER;
    sec_info->info.buffer.data = buffer;
    return anj_dm_write_bytes_chunked(value, buffer, buffer_len,
                                      &sec_info->info.buffer.data_size, NULL);
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
}
#    endif // ANJ_WITH_SECURITY

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
        sec_inst->security_mode = (anj_dm_security_mode_t) value->int_value;
        break;
#    ifdef ANJ_WITH_SECURITY
    case ANJ_DM_SECURITY_RID_PUBLIC_KEY_OR_IDENTITY:
        return write_security_info(anj, value,
                                   &sec_inst->public_key_or_identity,
                                   sec_inst->public_key_or_identity_buff,
                                   ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE,
#        ifdef ANJ_WITH_CERTIFICATES
                                   ANJ_CRYPTO_SECURITY_TAG_CERTIFICATE_CHAIN
#        else  // ANJ_WITH_CERTIFICATES
                                   ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY
#        endif // ANJ_WITH_CERTIFICATES
        );
    case ANJ_DM_SECURITY_RID_SERVER_PUBLIC_KEY:
        return write_security_info(anj, value, &sec_inst->server_public_key,
                                   sec_inst->server_public_key_buff,
                                   ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE,
                                   ANJ_CRYPTO_SECURITY_TAG_CERTIFICATE_CHAIN);
    case ANJ_DM_SECURITY_RID_SECRET_KEY:
        return write_security_info(anj, value, &sec_inst->secret_key,
                                   sec_inst->secret_key_buff,
                                   ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE,
#        ifdef ANJ_WITH_CERTIFICATES
                                   ANJ_CRYPTO_SECURITY_TAG_PRIVATE_KEY
#        else  // ANJ_WITH_CERTIFICATES
                                   ANJ_CRYPTO_SECURITY_TAG_PSK_KEY
#        endif // ANJ_WITH_CERTIFICATES
        );
#    else  // ANJ_WITH_SECURITY
    case ANJ_DM_SECURITY_RID_PUBLIC_KEY_OR_IDENTITY:
    case ANJ_DM_SECURITY_RID_SERVER_PUBLIC_KEY:
    case ANJ_DM_SECURITY_RID_SECRET_KEY:
        dm_log(L_ERROR, "Security resources are not available in this build");
        return ANJ_DM_ERR_INTERNAL;
#    endif // ANJ_WITH_SECURITY
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
    case ANJ_DM_SECURITY_RID_SERVER_PUBLIC_KEY:
    case ANJ_DM_SECURITY_RID_SECRET_KEY:
        dm_log(L_ERROR,
               "Reading security resources through res_read() is not allowed");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
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

#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
static void maybe_delete_external_data(anj_t *anj,
                                       anj_crypto_security_info_t *info) {
    if (info->source == ANJ_CRYPTO_DATA_SOURCE_EXTERNAL) {
        // don't check return value, as it is not important
        anj_crypto_storage_delete_record(anj->crypto_ctx, info);
        info->source = ANJ_CRYPTO_DATA_SOURCE_EMPTY;
    }
}
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

static int inst_delete(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    anj_dm_security_obj_t *ctx =
            ANJ_CONTAINER_OF(obj, anj_dm_security_obj_t, obj);
    anj_dm_security_instance_t *sec_inst = find_sec_inst(obj, iid);
#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    assert(sec_inst);
    maybe_delete_external_data(anj, &sec_inst->public_key_or_identity);
    maybe_delete_external_data(anj, &sec_inst->server_public_key);
    maybe_delete_external_data(anj, &sec_inst->secret_key);
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
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
    ctx->new_instance_iid = ANJ_ID_INVALID;
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

static void transaction_end(anj_t *anj,
                            const anj_dm_obj_t *obj,
                            anj_dm_transaction_result_t result) {
    (void) anj;
    anj_dm_security_obj_t *ctx =
            ANJ_CONTAINER_OF(obj, anj_dm_security_obj_t, obj);
    // for Delete operation, there is no possibility of failure
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    // delete external data for failed transaction
    if (result == ANJ_DM_TRANSACTION_FAILURE) {
        for (uint16_t idx = 0; idx < ANJ_DM_SECURITY_OBJ_INSTANCES; idx++) {
            anj_dm_security_instance_t *sec_inst =
                    &ctx->security_instances[idx];
            // new_instance_iid is set only for inst_create, so we can
            // safely check if it is not ANJ_ID_INVALID
            if (sec_inst->iid == ANJ_ID_INVALID
                    || sec_inst->iid != ctx->new_instance_iid) {
                continue;
            }
            maybe_delete_external_data(anj, &sec_inst->public_key_or_identity);
            maybe_delete_external_data(anj, &sec_inst->server_public_key);
            maybe_delete_external_data(anj, &sec_inst->secret_key);
        }
    }
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    if (result == ANJ_DM_TRANSACTION_FAILURE) {
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

#    ifdef ANJ_WITH_SECURITY
static int copy_security_info(anj_crypto_security_info_t *dest,
                              const anj_crypto_security_info_t *src,
                              uint8_t *buffer,
                              size_t buffer_size) {
    *dest = *src;
    if (src->source != ANJ_CRYPTO_DATA_SOURCE_BUFFER) {
        return 0; // nothing to copy
    }
    if (src->info.buffer.data_size > buffer_size) {
        dm_log(L_ERROR, "Internal buffer too small for security info");
        return -1;
    }
    memcpy(buffer, src->info.buffer.data, src->info.buffer.data_size);
    // set the destination buffer to point to the copied data
    dest->info.buffer.data = buffer;
    return 0;
}
#    endif // ANJ_WITH_SECURITY

int anj_dm_security_obj_add_instance(
        anj_dm_security_obj_t *security_obj_ctx,
        const anj_dm_security_instance_init_t *instance) {
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

#    ifdef ANJ_WITH_SECURITY
    if (copy_security_info(&sec_inst->public_key_or_identity,
                           &instance->public_key_or_identity,
                           sec_inst->public_key_or_identity_buff,
                           ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE)) {
        dm_log(L_ERROR, "Increase ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE");
        return -1;
    }
    if (copy_security_info(&sec_inst->server_public_key,
                           &instance->server_public_key,
                           sec_inst->server_public_key_buff,
                           ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE)) {
        dm_log(L_ERROR, "Increase ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE");
        return -1;
    }
    if (copy_security_info(&sec_inst->secret_key, &instance->secret_key,
                           sec_inst->secret_key_buff,
                           ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE)) {
        dm_log(L_ERROR, "Increase ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE");
        return -1;
    }
#    endif // ANJ_WITH_SECURITY

    strcpy(sec_inst->server_uri, instance->server_uri);
    sec_inst->bootstrap_server = instance->bootstrap_server;
    sec_inst->ssid =
            sec_inst->bootstrap_server ? _ANJ_SSID_BOOTSTRAP : instance->ssid;
    sec_inst->security_mode = instance->security_mode;
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

#    ifdef ANJ_WITH_SECURITY

int anj_dm_security_obj_get_psk(const anj_t *anj,
                                bool bootstrap_credentials,
                                anj_crypto_security_info_t *out_psk_identity,
                                anj_crypto_security_info_t *out_psk_key) {
    assert(anj && out_psk_identity && out_psk_key);
    const anj_dm_obj_t *obj = anj->dm.objs[0];
    // security object is always the first one
    assert(obj && obj->oid == ANJ_OBJ_ID_SECURITY);
    anj_dm_security_obj_t *ctx =
            ANJ_CONTAINER_OF(obj, anj_dm_security_obj_t, obj);

    for (int i = 0; i < ANJ_DM_SECURITY_OBJ_INSTANCES; i++) {
        anj_dm_security_instance_t *sec_inst = &ctx->security_instances[i];
        if (sec_inst->iid == ANJ_ID_INVALID) {
            continue;
        }
        if (sec_inst->bootstrap_server != bootstrap_credentials) {
            continue;
        }
        *out_psk_identity = sec_inst->public_key_or_identity;
        *out_psk_key = sec_inst->secret_key;
        // because security object don't use tags, we set them here
        out_psk_identity->tag = ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY;
        out_psk_key->tag = ANJ_CRYPTO_SECURITY_TAG_PSK_KEY;
        return 0;
    }
    dm_log(L_ERROR, "No PSK found for the server");
    return ANJ_DM_ERR_NOT_FOUND;
}
#    endif // ANJ_WITH_SECURITY

#    ifdef ANJ_WITH_PERSISTENCE

// If the build configuration changes in the meantime, drop persistence data.
#        ifdef ANJ_WITH_SECURITY
#            define HAS_SECURITY_SUPPORT 0x01u
#        else
#            define HAS_SECURITY_SUPPORT 0x00u
#        endif

#        ifdef ANJ_WITH_BOOTSTRAP
#            define HAS_BOOTSTRAP_SUPPORT 0x01u
#        else
#            define HAS_BOOTSTRAP_SUPPORT 0x00u
#        endif

static const uint8_t g_persistence_header[] = { 'S',
                                                'E',
                                                'C',
                                                0x01, // version
                                                HAS_SECURITY_SUPPORT,
                                                HAS_BOOTSTRAP_SUPPORT };

#        ifdef ANJ_WITH_SECURITY
// store: source - u8, size - u32, data - u8[]
// source: 0 - empty, 1 - internal, 2 - external
static int security_persistence(anj_t *anj,
                                const anj_persistence_context_t *ctx,
                                anj_crypto_security_info_t *info,
                                uint8_t *buffer,
                                size_t buffer_size) {
    (void) anj;
    if (anj_persistence_direction(ctx) == ANJ_PERSISTENCE_RESTORE) {
        uint8_t source;
        uint32_t record_size;
        if (anj_persistence_u8(ctx, &source)
                || anj_persistence_u32(ctx, &record_size)) {
            return -1;
        }
        switch (source) {
        case 0:
            info->source = ANJ_CRYPTO_DATA_SOURCE_EMPTY;
            return 0;
        case 1:
            if (buffer_size < record_size) {
                dm_log(L_ERROR, "Buffer too small for security info");
                return -1;
            }
            if (anj_persistence_bytes(ctx, (char *) buffer, record_size)) {
                return -1;
            }
            info->source = ANJ_CRYPTO_DATA_SOURCE_BUFFER;
            info->info.buffer.data = (uint8_t *) buffer;
            info->info.buffer.data_size = record_size;
            return 0;
#            ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        case 2:
            if (record_size > ANJ_CRYPTO_STORAGE_PERSISTENCE_INFO_MAX_SIZE) {
                dm_log(L_ERROR, "Too large external security info");
                return -1;
            }
            uint8_t buff[ANJ_CRYPTO_STORAGE_PERSISTENCE_INFO_MAX_SIZE];
            if (anj_persistence_bytes(ctx, buff, record_size)) {
                return -1;
            }
            info->source = ANJ_CRYPTO_DATA_SOURCE_EXTERNAL;
            return anj_crypto_storage_resolve_persistence_info(
                    anj->crypto_ctx, buff, record_size, &info->info.external);
#            endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        default:
            dm_log(L_ERROR, "Invalid security info source");
            return -1;
        }
    }
    uint8_t source;
    uint32_t record_size;
#            ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    uint8_t external_buff[ANJ_CRYPTO_STORAGE_PERSISTENCE_INFO_MAX_SIZE];
#            endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

    switch (info->source) {
    case ANJ_CRYPTO_DATA_SOURCE_EMPTY:
        source = 0;
        record_size = 0;
        break;
    case ANJ_CRYPTO_DATA_SOURCE_BUFFER:
        source = 1;
        record_size = (uint32_t) info->info.buffer.data_size;
        break;
#            ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    case ANJ_CRYPTO_DATA_SOURCE_EXTERNAL:
        source = 2;
        size_t external_record_size;
        if (anj_crypto_storage_get_persistence_info(anj->crypto_ctx,
                                                    &info->info.external,
                                                    external_buff,
                                                    &external_record_size)) {
            return -1;
        }
        assert(external_record_size
               <= ANJ_CRYPTO_STORAGE_PERSISTENCE_INFO_MAX_SIZE);
        record_size = (uint32_t) external_record_size;
        break;
#            endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    default:
        return -1; // we never should be here
    }

    if (anj_persistence_u8(ctx, &source)
            || anj_persistence_u32(ctx, &record_size)) {
        return -1;
    }
    if (record_size == 0) {
        return 0;
    }
#            ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    if (anj_persistence_bytes(ctx, source == 1 ? buffer : external_buff,
                              record_size)) {
#            else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    if (anj_persistence_bytes(ctx, buffer, record_size)) {
#            endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        return -1;
    }
    return 0;
}
#        endif // ANJ_WITH_SECURITY

static int instance_persistence(anj_t *anj,
                                anj_dm_security_obj_t *security_obj_ctx,
                                const anj_persistence_context_t *ctx,
                                uint8_t idx) {
    (void) anj;
    anj_dm_security_instance_t *sec_inst =
            &security_obj_ctx->security_instances[idx];
    uint8_t security_mode = (uint8_t) sec_inst->security_mode;
    if (anj_persistence_u16(ctx, &security_obj_ctx->inst[idx].iid)
            || anj_persistence_string(ctx, sec_inst->server_uri,
                                      ANJ_SERVER_URI_MAX_SIZE)
            || anj_persistence_bool(ctx, &sec_inst->bootstrap_server)
            || anj_persistence_u8(ctx, &security_mode)
            || anj_persistence_u16(ctx, &sec_inst->ssid)
            || anj_persistence_u32(ctx, &sec_inst->client_hold_off_time)
#        ifdef ANJ_WITH_SECURITY
            || security_persistence(
                       anj, ctx, &sec_inst->public_key_or_identity,
                       sec_inst->public_key_or_identity_buff,
                       sizeof(sec_inst->public_key_or_identity_buff))
            || security_persistence(anj, ctx, &sec_inst->server_public_key,
                                    sec_inst->server_public_key_buff,
                                    sizeof(sec_inst->server_public_key_buff))
            || security_persistence(anj, ctx, &sec_inst->secret_key,
                                    sec_inst->secret_key_buff,
                                    sizeof(sec_inst->secret_key_buff))
#        endif // ANJ_WITH_SECURITY
    ) {
        return -1;
    }

    sec_inst->security_mode = (anj_dm_security_mode_t) security_mode;
    sec_inst->iid = security_obj_ctx->inst[idx].iid;
    return 0;
}

int anj_dm_security_obj_store(anj_t *anj,
                              anj_dm_security_obj_t *security_obj_ctx,
                              const anj_persistence_context_t *ctx) {
    assert(security_obj_ctx && ctx);
    assert(anj_persistence_direction(ctx) == ANJ_PERSISTENCE_STORE);
    assert(security_obj_ctx->installed);

    uint8_t instance_count =
            (security_obj_ctx->inst[0].iid == ANJ_ID_INVALID) ? 0 : 1;
#        ifdef ANJ_WITH_BOOTSTRAP
    instance_count +=
            (uint8_t) ((security_obj_ctx->inst[1].iid == ANJ_ID_INVALID) ? 0
                                                                         : 1);
#        endif // ANJ_WITH_BOOTSTRAP
    if (instance_count == 0) {
        dm_log(L_ERROR, "No Security Object instances to store");
        return -1;
    }

    if (anj_persistence_magic(ctx, g_persistence_header,
                              sizeof(g_persistence_header))) {
        return -1;
    }
    if (anj_persistence_u8(ctx, &instance_count)) {
        return -1;
    }

    for (uint8_t i = 0; i < ANJ_DM_SECURITY_OBJ_INSTANCES; i++) {
        if (security_obj_ctx->inst[i].iid == ANJ_ID_INVALID) {
            continue;
        }
        if (instance_persistence(anj, security_obj_ctx, ctx, i)) {
            return -1;
        }
    }
    return 0;
}

int anj_dm_security_obj_restore(anj_t *anj,
                                anj_dm_security_obj_t *security_obj_ctx,
                                const anj_persistence_context_t *ctx) {
    assert(security_obj_ctx && ctx);
    assert(anj_persistence_direction(ctx) == ANJ_PERSISTENCE_RESTORE);
    assert(!security_obj_ctx->installed);

    if (anj_persistence_magic(ctx, g_persistence_header,
                              sizeof(g_persistence_header))) {
        return -1;
    }
    uint8_t instance_count = 0;
    if (anj_persistence_u8(ctx, &instance_count)) {
        return -1;
    }

    for (uint8_t i = 0; i < instance_count; i++) {
        if (instance_persistence(anj, security_obj_ctx, ctx, i)
                || validate_instance(
                           &security_obj_ctx->security_instances[i])) {
            goto err;
        }
    }
    return 0;

err:
    // reset instances in case of error
    security_obj_ctx->inst[0].iid = ANJ_ID_INVALID;
    security_obj_ctx->security_instances[0].iid = ANJ_ID_INVALID;
#        ifdef ANJ_WITH_BOOTSTRAP
    security_obj_ctx->inst[1].iid = ANJ_ID_INVALID;
    security_obj_ctx->security_instances[1].iid = ANJ_ID_INVALID;
#        endif // ANJ_WITH_BOOTSTRAP
    return -1;
}
#    endif // ANJ_WITH_PERSISTENCE

#endif // ANJ_WITH_DEFAULT_SECURITY_OBJ
