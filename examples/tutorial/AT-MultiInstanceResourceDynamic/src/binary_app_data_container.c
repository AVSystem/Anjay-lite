/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/utils.h>

#include "binary_app_data_container.h"

#define RID_DATA 0

#define DATA_RES_INST_MAX_SIZE 64

typedef struct {
    uint8_t data[DATA_RES_INST_MAX_SIZE];
    size_t data_size;
} binary_app_data_container_inst_t;

#define DATA_RES_MAX_INST_COUNT 3

static anj_riid_t res_insts[DATA_RES_MAX_INST_COUNT];
static anj_riid_t res_insts_cached[DATA_RES_MAX_INST_COUNT];
static binary_app_data_container_inst_t bin_data_insts[DATA_RES_MAX_INST_COUNT];
static binary_app_data_container_inst_t
        bin_data_insts_cached[DATA_RES_MAX_INST_COUNT];

static const anj_dm_res_t RES_DATA = {
    .rid = RID_DATA,
    .type = ANJ_DATA_TYPE_BYTES,
    .kind = ANJ_DM_RES_RWM,
    .insts = res_insts,
    .max_inst_count = DATA_RES_MAX_INST_COUNT,
};

static const anj_dm_obj_inst_t INST = {
    .iid = 0,
    .res_count = 1,
    .resources = &RES_DATA
};

static binary_app_data_container_inst_t *get_inst_ctx(anj_riid_t riid) {
    for (uint16_t i = 0; i < DATA_RES_MAX_INST_COUNT; i++) {
        if (res_insts[i] == riid) {
            return &bin_data_insts[i];
        }
    }
    return NULL;
}

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) obj;
    (void) iid;

    binary_app_data_container_inst_t *inst_ctx = get_inst_ctx(riid);
    assert(inst_ctx);

    switch (rid) {
    case RID_DATA:
        out_value->bytes_or_string.data = inst_ctx->data;
        out_value->bytes_or_string.chunk_length = inst_ctx->data_size;
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
    (void) obj;
    (void) iid;

    binary_app_data_container_inst_t *inst_ctx = get_inst_ctx(riid);
    assert(inst_ctx);

    switch (rid) {
    case RID_DATA:
        return anj_dm_write_bytes_chunked(value, inst_ctx->data,
                                          DATA_RES_INST_MAX_SIZE,
                                          &inst_ctx->data_size, NULL);
        break;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }
    return 0;
}

static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    (void) obj;
    memcpy(res_insts_cached, res_insts, sizeof(res_insts_cached));
    memcpy(bin_data_insts_cached, bin_data_insts, sizeof(bin_data_insts));
    return 0;
}

static void transaction_end(anj_t *anj,
                            const anj_dm_obj_t *obj,
                            anj_dm_transaction_result_t result) {
    (void) anj;
    (void) obj;
    if (result == ANJ_DM_TRANSACTION_SUCCESS) {
        return;
    }
    memcpy(res_insts, res_insts_cached, sizeof(res_insts));
    memcpy(bin_data_insts, bin_data_insts_cached, sizeof(bin_data_insts));
}

static int inst_reset(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    (void) obj;
    (void) iid;

    // Clear all resource instances
    for (uint8_t i = 0; i < DATA_RES_MAX_INST_COUNT; i++) {
        res_insts[i] = ANJ_ID_INVALID;
    }

    return 0;
}

static int res_inst_create(anj_t *anj,
                           const anj_dm_obj_t *obj,
                           anj_iid_t iid,
                           anj_rid_t rid,
                           anj_riid_t riid) {
    (void) anj;
    (void) obj;
    (void) iid;
    (void) rid;
    // there is space for new instance
    assert(res_insts[DATA_RES_MAX_INST_COUNT - 1] == ANJ_ID_INVALID);

    for (uint16_t i = 0; i < DATA_RES_MAX_INST_COUNT; i++) {
        if (res_insts[i] == ANJ_ID_INVALID || res_insts[i] >= riid) {
            for (uint16_t j = DATA_RES_MAX_INST_COUNT - 1; j > i; --j) {
                res_insts[j] = res_insts[j - 1];
                bin_data_insts[j] = bin_data_insts[j - 1];
            }
            res_insts[i] = riid;
            bin_data_insts[i].data_size = 0;
            break;
        }
    }
    return 0;
}

static int res_inst_delete(anj_t *anj,
                           const anj_dm_obj_t *obj,
                           anj_iid_t iid,
                           anj_rid_t rid,
                           anj_riid_t riid) {
    (void) anj;
    (void) obj;
    (void) iid;
    (void) rid;

    for (uint16_t i = 0; i < DATA_RES_MAX_INST_COUNT - 1; i++) {
        if (res_insts[i] == riid) {
            for (uint16_t j = i; j < DATA_RES_MAX_INST_COUNT - 1; j++) {
                res_insts[j] = res_insts[j + 1];
                bin_data_insts[j] = bin_data_insts[j + 1];
            }
            break;
        }
    }
    res_insts[DATA_RES_MAX_INST_COUNT - 1] = ANJ_ID_INVALID;
    return 0;
}

static const anj_dm_handlers_t HANDLERS = {
    .res_read = res_read,
    .res_write = res_write,
    .inst_reset = inst_reset,
    .res_inst_create = res_inst_create,
    .res_inst_delete = res_inst_delete,
    .transaction_begin = transaction_begin,
    .transaction_end = transaction_end,
};

static const anj_dm_obj_t OBJ = {
    .oid = 19,
    .insts = &INST,
    .handlers = &HANDLERS,
    .max_inst_count = 1
};

const anj_dm_obj_t *init_binary_app_data_container(void) {
    for (uint16_t i = 1; i < DATA_RES_MAX_INST_COUNT; i++) {
        bin_data_insts[i].data_size = 0;
        res_insts[i] = ANJ_ID_INVALID;
    }
    bin_data_insts[0].data[0] = (uint8_t) 'X';
    bin_data_insts[0].data_size = 1;
    res_insts[0] = 0;

    return &OBJ;
}
