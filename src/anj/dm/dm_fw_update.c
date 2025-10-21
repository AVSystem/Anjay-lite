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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/dm/fw_update.h>
#include <anj/utils.h>

#ifdef ANJ_WITH_DEFAULT_FOTA_OBJ

/* FOTA method support */
#    ifndef ANJ_FOTA_WITH_PUSH_METHOD
// if push method if not supported, it's safe to assume that pull is -
// guaranteed by a condition check in anj_config.h 0 -> pull only
#        define METHODS_SUPPORTED 0
#    else // ANJ_FOTA_WITH_PUSH_METHOD
#        ifdef ANJ_FOTA_WITH_PULL_METHOD
// this means push is supported as well
// 2 -> pull & push
#            define METHODS_SUPPORTED 2
#        else // ANJ_FOTA_WITH_PULL_METHOD
// only push enabled
// 1 -> push
#            define METHODS_SUPPORTED 1
#        endif // ANJ_FOTA_WITH_PULL_METHOD
#    endif     // ANJ_FOTA_WITH_PUSH_METHOD

#    ifdef ANJ_FOTA_WITH_PULL_METHOD
#        define ANJ_DM_FW_UPDATE_RESOURCES_COUNT 9
#    else // ANJ_FOTA_WITH_PULL_METHOD
#        define ANJ_DM_FW_UPDATE_RESOURCES_COUNT 8
#    endif // ANJ_FOTA_WITH_PULL_METHOD

/*
 * FW Update Object Resources IDs
 */
enum anj_dm_fw_update_resources {
    ANJ_DM_FW_UPDATE_RID_PACKAGE = 0,
    ANJ_DM_FW_UPDATE_RID_PACKAGE_URI = 1,
    ANJ_DM_FW_UPDATE_RID_UPDATE = 2,
    ANJ_DM_FW_UPDATE_RID_STATE = 3,
    ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT = 5,
    ANJ_DM_FW_UPDATE_RID_PKG_NAME = 6,
    ANJ_DM_FW_UPDATE_RID_PKG_VERSION = 7,
#    ifdef ANJ_FOTA_WITH_PULL_METHOD
    ANJ_DM_FW_UPDATE_RID_UPDATE_PROTOCOL_SUPPORT = 8,
#    endif // ANJ_FOTA_WITH_PULL_METHOD
    ANJ_DM_FW_UPDATE_RID_UPDATE_DELIVERY_METHOD = 9
};

enum {
    PACKAGE_RES_IDX,
    PACKAGE_URI_RES_IDX,
    UPDATE_RES_IDX,
    STATE_RES_IDX,
    UPDATE_RESULT_RES_IDX,
    PKG_NAME_RES_IDX,
    PKG_VER_RES_IDX,
#    ifdef ANJ_FOTA_WITH_PULL_METHOD
    SUPPORTED_PROTOCOLS_RES_IDX,
#    endif // ANJ_FOTA_WITH_PULL_METHOD
    DELIVERY_METHOD_RES_IDX,
    _RESOURCES_COUNT
};

ANJ_STATIC_ASSERT(_RESOURCES_COUNT == ANJ_DM_FW_UPDATE_RESOURCES_COUNT,
                  resources_count_consistent);

enum _anj_dm_fw_update_protocols {
    ANJ_DM_FW_UPDATE_PROTOCOL_COAP = 0,
    ANJ_DM_FW_UPDATE_PROTOCOL_COAPS = 1,
    ANJ_DM_FW_UPDATE_PROTOCOL_HTTP = 2,
    ANJ_DM_FW_UPDATE_PROTOCOL_HTTPS = 3,
    ANJ_DM_FW_UPDATE_PROTOCOL_COAP_TCP = 4,
    ANJ_DM_FW_UPDATE_PROTOCOL_COAPS_TCP = 5
};

#    ifdef ANJ_FOTA_WITH_PULL_METHOD
static const anj_riid_t SUPPORTED_PROTOCOLS_INST[] = {
#        ifdef ANJ_FOTA_WITH_COAP
    ANJ_DM_FW_UPDATE_PROTOCOL_COAP,
#        endif // ANJ_FOTA_WITH_COAP
#        ifdef ANJ_FOTA_WITH_COAPS
    ANJ_DM_FW_UPDATE_PROTOCOL_COAPS,
#        endif // ANJ_FOTA_WITH_COAPS
#        ifdef ANJ_FOTA_WITH_HTTP
    ANJ_DM_FW_UPDATE_PROTOCOL_HTTP,
#        endif // ANJ_FOTA_WITH_HTTP
#        ifdef ANJ_FOTA_WITH_HTTPS
    ANJ_DM_FW_UPDATE_PROTOCOL_HTTPS,
#        endif // ANJ_FOTA_WITH_HTTPS
#        ifdef ANJ_FOTA_WITH_COAP_TCP
    ANJ_DM_FW_UPDATE_PROTOCOL_COAP_TCP,
#        endif // ANJ_FOTA_WITH_COAP_TCP
#        ifdef ANJ_FOTA_WITH_COAPS_TCP
    ANJ_DM_FW_UPDATE_PROTOCOL_COAPS_TCP,
#        endif // ANJ_FOTA_WITH_COAPS_TCP
};
#    endif // ANJ_FOTA_WITH_PULL_METHOD

static const anj_dm_res_t RES[ANJ_DM_FW_UPDATE_RESOURCES_COUNT] = {
    [PACKAGE_RES_IDX] = {
        .rid = ANJ_DM_FW_UPDATE_RID_PACKAGE,
        .type = ANJ_DATA_TYPE_BYTES,
        .kind = ANJ_DM_RES_W
    },
    [PACKAGE_URI_RES_IDX] = {
        .rid = ANJ_DM_FW_UPDATE_RID_PACKAGE_URI,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_RW
    },
    [UPDATE_RES_IDX] = {
        .rid = ANJ_DM_FW_UPDATE_RID_UPDATE,
        .kind = ANJ_DM_RES_E
    },
    [STATE_RES_IDX] = {
        .rid = ANJ_DM_FW_UPDATE_RID_STATE,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_R
    },
    [UPDATE_RESULT_RES_IDX] = {
        .rid = ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_R
    },
    [PKG_NAME_RES_IDX] = {
        .rid = ANJ_DM_FW_UPDATE_RID_PKG_NAME,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_R
    },
    [PKG_VER_RES_IDX] = {
        .rid = ANJ_DM_FW_UPDATE_RID_PKG_VERSION,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_R
    },
#    ifdef ANJ_FOTA_WITH_PULL_METHOD
    [SUPPORTED_PROTOCOLS_RES_IDX] = {
        .rid = ANJ_DM_FW_UPDATE_RID_UPDATE_PROTOCOL_SUPPORT,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_RM,
        .max_inst_count = ANJ_ARRAY_SIZE(SUPPORTED_PROTOCOLS_INST),
        .insts = SUPPORTED_PROTOCOLS_INST
    },
#    endif // ANJ_FOTA_WITH_PULL_METHOD
    [DELIVERY_METHOD_RES_IDX] = {
        .rid = ANJ_DM_FW_UPDATE_RID_UPDATE_DELIVERY_METHOD,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_R
    }
};

#    ifdef ANJ_FOTA_WITH_PUSH_METHOD
static inline bool writing_last_data_chunk(const anj_res_value_t *value) {
    return value->bytes_or_string.chunk_length + value->bytes_or_string.offset
           == value->bytes_or_string.full_length_hint;
}

static inline bool is_reset_request_package(const anj_res_value_t *value) {
    if (value->bytes_or_string.full_length_hint == 1
            && value->bytes_or_string.offset == 0
            && ((const char *) value->bytes_or_string.data)[0] == '\0') {
        return true;
    }
    return false;
}
#    endif // ANJ_FOTA_WITH_PUSH_METHOD

#    ifdef ANJ_FOTA_WITH_PULL_METHOD
static inline bool is_reset_request_uri(const anj_res_value_t *value) {
    if (value->bytes_or_string.full_length_hint == 0
            && value->bytes_or_string.offset == 0
            && value->bytes_or_string.chunk_length == 0) {
        return true;
    }
    return false;
}
#    endif // ANJ_FOTA_WITH_PULL_METHOD

static void fw_data_model_changed(anj_t *anj,
                                  anj_dm_fw_update_entity_ctx_t *entity_ctx,
                                  enum anj_dm_fw_update_resources fw_rid) {
    anj_core_data_model_changed(anj,
                                &ANJ_MAKE_RESOURCE_PATH(entity_ctx->obj.oid,
                                                        entity_ctx->inst.iid,
                                                        (uint16_t) fw_rid),
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
}

static void reset(anj_t *anj, anj_dm_fw_update_entity_ctx_t *entity_ctx) {
    entity_ctx->repr.user_handlers->reset_handler(entity_ctx->repr.user_ptr);
    entity_ctx->repr.state = ANJ_DM_FW_UPDATE_STATE_IDLE;
    fw_data_model_changed(anj, entity_ctx, ANJ_DM_FW_UPDATE_RID_STATE);

#    ifdef ANJ_FOTA_WITH_PUSH_METHOD
    entity_ctx->repr.write_start_called = false;
#    endif // ANJ_FOTA_WITH_PUSH_METHOD

#    ifdef ANJ_FOTA_WITH_PULL_METHOD
    entity_ctx->repr.uri[0] = '\0';
    fw_data_model_changed(anj, entity_ctx, ANJ_DM_FW_UPDATE_RID_PACKAGE_URI);
#    endif // ANJ_FOTA_WITH_PULL_METHOD
}

static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) iid;
    (void) riid;

    anj_dm_fw_update_entity_ctx_t *entity =
            ANJ_CONTAINER_OF(obj, anj_dm_fw_update_entity_ctx_t, obj);

    switch (rid) {
    case ANJ_DM_FW_UPDATE_RID_PACKAGE: {
#    if !defined(ANJ_FOTA_WITH_PUSH_METHOD)
        return ANJ_DM_ERR_BAD_REQUEST;
#    else  // !defined (ANJ_FOTA_WITH_PUSH_METHOD)
        // any write in updating state is illegal
        if (entity->repr.state == ANJ_DM_FW_UPDATE_STATE_UPDATING) {
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }

        // handle state machine reset with an empty write
        if (is_reset_request_package(value)) {
            entity->repr.result = ANJ_DM_FW_UPDATE_RESULT_INITIAL;
            reset(anj, entity);
            fw_data_model_changed(
                    anj, entity, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
            return 0;
        }

        // non-empty writes can be performed only in IDLE state since for the
        // time of writing FW package in chunks the state does not change to
        // DOWNLOADING and goes directly from IDLE to DOWNLOADED on last chunk
        if (entity->repr.state != ANJ_DM_FW_UPDATE_STATE_IDLE) {
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }

        anj_dm_fw_update_result_t result;

        // handle first chunk if needed
        if (!entity->repr.write_start_called) {
            result = entity->repr.user_handlers->package_write_start_handler(
                    entity->repr.user_ptr);
            if (result != ANJ_DM_FW_UPDATE_RESULT_SUCCESS) {
                entity->repr.result = (int8_t) result;
                fw_data_model_changed(
                        anj, entity, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
                return ANJ_DM_ERR_INTERNAL;
            }
            entity->repr.write_start_called = true;
        }

        // write actual data
        result = entity->repr.user_handlers->package_write_handler(
                entity->repr.user_ptr,
                value->bytes_or_string.data,
                value->bytes_or_string.chunk_length);
        if (result != ANJ_DM_FW_UPDATE_RESULT_SUCCESS) {
            entity->repr.result = (int8_t) result;
            reset(anj, entity);
            fw_data_model_changed(
                    anj, entity, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
            return ANJ_DM_ERR_INTERNAL;
        }

        // check if that's the last chunk (block)
        if (writing_last_data_chunk(value)) {
            result = entity->repr.user_handlers->package_write_finish_handler(
                    entity->repr.user_ptr);
            if (result != ANJ_DM_FW_UPDATE_RESULT_SUCCESS) {
                entity->repr.result = (int8_t) result;
                reset(anj, entity);
                fw_data_model_changed(
                        anj, entity, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
                return ANJ_DM_ERR_INTERNAL;
            } else {
                entity->repr.result = ANJ_DM_FW_UPDATE_RESULT_INITIAL;
                entity->repr.state = ANJ_DM_FW_UPDATE_STATE_DOWNLOADED;
                fw_data_model_changed(
                        anj, entity, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
                fw_data_model_changed(anj, entity, ANJ_DM_FW_UPDATE_RID_STATE);
            }
        }

        return 0;
#    endif // !defined (ANJ_FOTA_WITH_PUSH_METHOD)
    }
    case ANJ_DM_FW_UPDATE_RID_PACKAGE_URI: {
#    if !defined(ANJ_FOTA_WITH_PULL_METHOD)
        return ANJ_DM_ERR_BAD_REQUEST;
#    else  // !defined (ANJ_FOTA_WITH_PULL_METHOD)
        if (entity->repr.state == ANJ_DM_FW_UPDATE_STATE_UPDATING) {
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }

        // handle state machine reset with an empty write
        if (is_reset_request_uri(value)) {
            entity->repr.result = ANJ_DM_FW_UPDATE_RESULT_INITIAL;
            reset(anj, entity);
            fw_data_model_changed(
                    anj, entity, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
            return 0;
        }

        // non-empty write can be handled only in IDLE state
        if (entity->repr.state != ANJ_DM_FW_UPDATE_STATE_IDLE) {
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }

        bool last_chunk = false;
        int res = anj_dm_write_string_chunked(
                value, entity->repr.uri, sizeof(entity->repr.uri), &last_chunk);
        if (res) {
            return res;
        }
        if (!last_chunk) {
            return 0;
        }

        anj_dm_fw_update_result_t result =
                entity->repr.user_handlers->uri_write_handler(
                        entity->repr.user_ptr, entity->repr.uri);
        if (result != ANJ_DM_FW_UPDATE_RESULT_INITIAL) {
            entity->repr.result = (int8_t) result;
            fw_data_model_changed(
                    anj, entity, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
            return ANJ_DM_ERR_BAD_REQUEST;
        }

        entity->repr.state = ANJ_DM_FW_UPDATE_STATE_DOWNLOADING;
        fw_data_model_changed(anj, entity, ANJ_DM_FW_UPDATE_RID_STATE);
        return 0;
#    endif // !defined (ANJ_FOTA_WITH_PULL_METHOD)
    }
    default:
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }
}

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) iid;
    (void) riid;

    anj_dm_fw_update_entity_ctx_t *entity =
            ANJ_CONTAINER_OF(obj, anj_dm_fw_update_entity_ctx_t, obj);

    switch (rid) {
    case ANJ_DM_FW_UPDATE_RID_UPDATE_DELIVERY_METHOD:
        out_value->int_value = (int64_t) METHODS_SUPPORTED;
        break;
    case ANJ_DM_FW_UPDATE_RID_STATE:
        out_value->int_value = (int64_t) entity->repr.state;
        break;
    case ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT:
        out_value->int_value = (int64_t) entity->repr.result;
        break;
    case ANJ_DM_FW_UPDATE_RID_PACKAGE_URI: {
#    ifdef ANJ_FOTA_WITH_PULL_METHOD
        out_value->bytes_or_string.data = entity->repr.uri;
#    else  // ANJ_FOTA_WITH_PULL_METHOD
        out_value->bytes_or_string.data = NULL;
#    endif // ANJ_FOTA_WITH_PULL_METHOD
        break;
    }
    case ANJ_DM_FW_UPDATE_RID_PKG_NAME: {
        if (entity->repr.state == ANJ_DM_FW_UPDATE_STATE_DOWNLOADED
                && entity->repr.user_handlers->get_name) {
            out_value->bytes_or_string.data =
                    entity->repr.user_handlers->get_name(entity->repr.user_ptr);
        } else {
            out_value->bytes_or_string.data = NULL;
        }
        break;
    }
    case ANJ_DM_FW_UPDATE_RID_PKG_VERSION: {
        if (entity->repr.state == ANJ_DM_FW_UPDATE_STATE_DOWNLOADED
                && entity->repr.user_handlers->get_version) {
            out_value->bytes_or_string.data =
                    entity->repr.user_handlers->get_version(
                            entity->repr.user_ptr);
        } else {
            out_value->bytes_or_string.data = NULL;
        }
        break;
    }
#    ifdef ANJ_FOTA_WITH_PULL_METHOD
    case ANJ_DM_FW_UPDATE_RID_UPDATE_PROTOCOL_SUPPORT: {
        out_value->int_value = (int64_t) riid;
        break;
    }
#    endif // ANJ_FOTA_WITH_PULL_METHOD
    default:
        return ANJ_DM_ERR_NOT_FOUND;
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

    anj_dm_fw_update_entity_ctx_t *entity =
            ANJ_CONTAINER_OF(obj, anj_dm_fw_update_entity_ctx_t, obj);

    switch (rid) {
    case ANJ_DM_FW_UPDATE_RID_UPDATE:
        if (entity->repr.state != ANJ_DM_FW_UPDATE_STATE_DOWNLOADED) {
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }
        int result = entity->repr.user_handlers->update_start_handler(
                entity->repr.user_ptr);
        if (result) {
            entity->repr.result = ANJ_DM_FW_UPDATE_RESULT_FAILED;
            entity->repr.state = ANJ_DM_FW_UPDATE_STATE_IDLE;
            fw_data_model_changed(
                    anj, entity, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
            fw_data_model_changed(anj, entity, ANJ_DM_FW_UPDATE_RID_STATE);
            return ANJ_DM_ERR_INTERNAL;
        }
        entity->repr.state = ANJ_DM_FW_UPDATE_STATE_UPDATING;
        fw_data_model_changed(anj, entity, ANJ_DM_FW_UPDATE_RID_STATE);
        break;
    default:
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }
    return 0;
}

static const anj_dm_handlers_t HANDLERS = {
    .res_read = res_read,
    .res_write = res_write,
    .res_execute = res_execute
};

int anj_dm_fw_update_object_install(anj_t *anj,
                                    anj_dm_fw_update_entity_ctx_t *entity_ctx,
                                    anj_dm_fw_update_handlers_t *handlers,
                                    void *user_ptr) {
    if (!anj || !entity_ctx || !handlers || !handlers->update_start_handler
            || !handlers->reset_handler) {
        return -1;
    }
    memset(entity_ctx, 0, sizeof(*entity_ctx));

#    ifdef ANJ_FOTA_WITH_PUSH_METHOD
    if (!handlers->package_write_start_handler
            || !handlers->package_write_handler
            || !handlers->package_write_finish_handler) {
        return -1;
    }
    entity_ctx->repr.write_start_called = false;
#    endif // ANJ_FOTA_WITH_PUSH_METHOD

#    ifdef ANJ_FOTA_WITH_PULL_METHOD
    if (!handlers->uri_write_handler) {
        return -1;
    }
#    endif // ANJ_FOTA_WITH_PULL_METHOD

    entity_ctx->repr.user_ptr = user_ptr;
    entity_ctx->repr.user_handlers = handlers;

    entity_ctx->obj = (anj_dm_obj_t) {
        .oid = ANJ_OBJ_ID_FIRMWARE_UPDATE,
        .version = "1.0",
        .max_inst_count = 1,
        .insts = &entity_ctx->inst,
        .handlers = &HANDLERS
    };

    entity_ctx->inst.iid = 0;
    entity_ctx->inst.resources = RES;
    entity_ctx->inst.res_count = ANJ_DM_FW_UPDATE_RESOURCES_COUNT;

    return anj_dm_add_obj(anj, &entity_ctx->obj);
}

void anj_dm_fw_update_object_set_update_result(
        anj_t *anj,
        anj_dm_fw_update_entity_ctx_t *entity_ctx,
        anj_dm_fw_update_result_t result) {
    assert(entity_ctx);
    entity_ctx->repr.result = (int8_t) result;
    entity_ctx->repr.state = ANJ_DM_FW_UPDATE_STATE_IDLE;
#    ifdef ANJ_FOTA_WITH_PUSH_METHOD
    entity_ctx->repr.write_start_called = false;
#    endif // ANJ_FOTA_WITH_PUSH_METHOD
    fw_data_model_changed(anj, entity_ctx, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
    fw_data_model_changed(anj, entity_ctx, ANJ_DM_FW_UPDATE_RID_STATE);
}

int anj_dm_fw_update_object_set_download_result(
        anj_t *anj,
        anj_dm_fw_update_entity_ctx_t *entity_ctx,
        anj_dm_fw_update_result_t result) {
    assert(entity_ctx);
    if (entity_ctx->repr.state != ANJ_DM_FW_UPDATE_STATE_DOWNLOADING) {
        return -1;
    }
    if (result != ANJ_DM_FW_UPDATE_RESULT_SUCCESS) {
        entity_ctx->repr.result = (int8_t) result;
        reset(anj, entity_ctx);
        fw_data_model_changed(
                anj, entity_ctx, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
        return 0;
    }
    entity_ctx->repr.result = ANJ_DM_FW_UPDATE_RESULT_INITIAL;
    entity_ctx->repr.state = ANJ_DM_FW_UPDATE_STATE_DOWNLOADED;
    fw_data_model_changed(anj, entity_ctx, ANJ_DM_FW_UPDATE_RID_UPDATE_RESULT);
    fw_data_model_changed(anj, entity_ctx, ANJ_DM_FW_UPDATE_RID_STATE);

    return 0;
}

#endif // ANJ_WITH_DEFAULT_FOTA_OBJ
