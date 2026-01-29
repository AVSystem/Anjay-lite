/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 19

#include <assert.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/dm/device_object.h>
#include <anj/log.h>
#include <anj/utils.h>

#include "dm_core.h"

#ifdef ANJ_WITH_DEFAULT_DEVICE_OBJ

#    define ANJ_DM_DEVICE_RESOURCES_COUNT 8

enum {
    RID_MANUFACTURER = 0,
    RID_MODEL_NUMBER = 1,
    RID_SERIAL_NUMBER = 2,
    RID_FIRMWARE_VERSION = 3,
    RID_REBOOT = 4,
    RID_ERROR_CODE = 11,
    RID_BINDING_MODES = 16,
    RID_SOFTWARE_VERSION = 19
};

enum {
    RID_MANUFACTURER_IDX = 0,
    RID_MODEL_NUMBER_IDX,
    RID_SERIAL_NUMBER_IDX,
    RID_FIRMWARE_VERSION_IDX,
    RID_REBOOT_IDX,
    RID_ERROR_CODE_IDX,
    RID_BINDING_MODES_IDX,
    RID_SOFTWARE_VERSION_IDX,
    _RID_LAST
};

ANJ_STATIC_ASSERT(_RID_LAST == ANJ_DM_DEVICE_RESOURCES_COUNT,
                  device_resources_count_mismatch);

/*
 * HACK: error handling is not supported so in order to comply with the
 * definition of the object only one instance of Error Code resource is
 * defined with the value set to 0 which means "no errors".
 */
static const anj_riid_t RES_INST[] = { 0 };

static const anj_dm_res_t RES[ANJ_DM_DEVICE_RESOURCES_COUNT] = {
    [RID_MANUFACTURER_IDX] = {
        .rid = RID_MANUFACTURER,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_R
    },
    [RID_MODEL_NUMBER_IDX] = {
        .rid = RID_MODEL_NUMBER,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_R
    },
    [RID_SERIAL_NUMBER_IDX] = {
        .rid = RID_SERIAL_NUMBER,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_R
    },
    [RID_FIRMWARE_VERSION_IDX] = {
        .rid = RID_FIRMWARE_VERSION,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_R
    },
    [RID_REBOOT_IDX] = {
        .rid = RID_REBOOT,
        .kind = ANJ_DM_RES_E
    },
    [RID_ERROR_CODE_IDX] = {
        .rid = RID_ERROR_CODE,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_RM,
        .max_inst_count = ANJ_ARRAY_SIZE(RES_INST),
        .insts = RES_INST
    },
    [RID_BINDING_MODES_IDX] = {
        .rid = RID_BINDING_MODES,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_R
    },
    [RID_SOFTWARE_VERSION_IDX] = {
        .rid = RID_SOFTWARE_VERSION,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_R
    }
};

static int res_execute(anj_t *anj,
                       const anj_dm_obj_t *obj,
                       anj_iid_t iid,
                       anj_rid_t rid,
                       const char *execute_arg,
                       size_t execute_arg_len) {

    (void) iid;
    (void) execute_arg;
    (void) execute_arg_len;

    anj_dm_device_obj_t *device_obj =
            ANJ_CONTAINER_OF(obj, anj_dm_device_obj_t, obj);

    switch (rid) {
    case RID_REBOOT: {
        if (!device_obj->reboot_cb) {
            dm_log(L_ERROR, "Reboot callback not set");
            return ANJ_DM_ERR_NOT_FOUND;
        }
        device_obj->reboot_cb(device_obj->reboot_cb_arg, anj);
        return 0;
    }
    default:
        break;
    }

    return ANJ_DM_ERR_NOT_FOUND;
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

    anj_dm_device_obj_t *ctx = ANJ_CONTAINER_OF(obj, anj_dm_device_obj_t, obj);

    switch (rid) {
    case RID_MANUFACTURER:
        out_value->bytes_or_string.data = ctx->manufacturer;
        break;
    case RID_MODEL_NUMBER:
        out_value->bytes_or_string.data = ctx->model_number;
        break;
    case RID_SERIAL_NUMBER:
        out_value->bytes_or_string.data = ctx->serial_number;
        break;
    case RID_FIRMWARE_VERSION:
        out_value->bytes_or_string.data = ctx->firmware_version;
        break;
    case RID_BINDING_MODES:
        out_value->bytes_or_string.data = ctx->binding_modes;
        break;
    case RID_SOFTWARE_VERSION:
        out_value->bytes_or_string.data = ctx->software_version;
        break;
    case RID_ERROR_CODE: {
        // resource is not supported, there is only one instance with default
        // value
        out_value->int_value = 0;
        return 0;
    }
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }
    return 0;
}

static const anj_dm_handlers_t HANDLERS = {
    .res_read = res_read,
    .res_execute = res_execute
};

int anj_dm_device_obj_install(anj_t *anj,
                              anj_dm_device_obj_t *device_obj,
                              const anj_dm_device_object_init_t *obj_init) {
    assert(anj && device_obj && obj_init);

    memset(device_obj, 0, sizeof(*device_obj));
    device_obj->obj = (anj_dm_obj_t) {
        .oid = ANJ_OBJ_ID_DEVICE,
        .version = "1.0",
        .max_inst_count = 1,
        .insts = &device_obj->inst,
        .handlers = &HANDLERS
    };
    device_obj->manufacturer = obj_init->manufacturer;
    device_obj->model_number = obj_init->model_number;
    device_obj->serial_number = obj_init->serial_number;
    device_obj->firmware_version = obj_init->firmware_version;
    device_obj->software_version = obj_init->software_version;
    device_obj->binding_modes = ANJ_SUPPORTED_BINDING_MODES;

    device_obj->inst.resources = RES;
    device_obj->inst.res_count = ANJ_DM_DEVICE_RESOURCES_COUNT;
    device_obj->inst.iid = 0;

    if (obj_init->reboot_cb) {
        device_obj->reboot_cb = obj_init->reboot_cb;
        device_obj->reboot_cb_arg = obj_init->reboot_cb_arg;
    }
    int res = anj_dm_add_obj(anj, &device_obj->obj);
    if (!res) {
        dm_log(L_INFO, "Device object installed");
    }
    return res;
}

#endif // ANJ_WITH_DEFAULT_DEVICE_OBJ
