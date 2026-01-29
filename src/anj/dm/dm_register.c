/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 26

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/defs.h>
#include <anj/utils.h>

#include "dm_core.h"
#include "dm_io.h"

int _anj_dm_begin_register_op(anj_t *anj) {
    assert(anj);
    _anj_dm_data_model_t *dm = &anj->dm;
    _anj_dm_reg_ctx_t *reg_ctx = &dm->op_ctx.reg_ctx;
    dm->op_count = 0;
    for (uint16_t idx = 0; idx < dm->objs_count; idx++) {
        const anj_dm_obj_t *obj = dm->objs[idx];
        if (obj->oid != ANJ_OBJ_ID_SECURITY && obj->oid != ANJ_OBJ_ID_OSCORE) {
            uint16_t inst_count = _anj_dm_count_obj_insts(obj);
            dm->op_count = dm->op_count + 1 + inst_count;
        }
    }
    reg_ctx->level = ANJ_ID_OID;
    reg_ctx->obj_idx = 0;
    reg_ctx->inst_idx = 0;
    return 0;
}

int _anj_dm_get_register_record(anj_t *anj,
                                anj_uri_path_t *out_path,
                                const char **out_version) {
    assert(anj && out_path && out_version);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress);
    assert(dm->op_count);
    assert(dm->operation == ANJ_OP_REGISTER || dm->operation == ANJ_OP_UPDATE);

    _anj_dm_reg_ctx_t *reg_ctx = &dm->op_ctx.reg_ctx;
    assert(reg_ctx->obj_idx < dm->objs_count);

    if (reg_ctx->level == ANJ_ID_OID) {
        if (dm->objs[reg_ctx->obj_idx]->oid == ANJ_OBJ_ID_SECURITY
                || dm->objs[reg_ctx->obj_idx]->oid == ANJ_OBJ_ID_OSCORE) {
            reg_ctx->obj_idx++;
        }

        const anj_dm_obj_t *obj = dm->objs[reg_ctx->obj_idx];
        *out_path = ANJ_MAKE_OBJECT_PATH(obj->oid);
        *out_version = obj->version;
        uint16_t inst_count = _anj_dm_count_obj_insts(obj);
        if (!inst_count) {
            reg_ctx->obj_idx++;
        } else {
            reg_ctx->level = ANJ_ID_IID;
            reg_ctx->inst_idx = 0;
        }
    } else {
        const anj_dm_obj_t *obj = dm->objs[reg_ctx->obj_idx];
        uint16_t inst_count = _anj_dm_count_obj_insts(obj);
        assert(reg_ctx->inst_idx < inst_count);

        *out_path = ANJ_MAKE_INSTANCE_PATH(obj->oid,
                                           obj->insts[reg_ctx->inst_idx].iid);
        *out_version = NULL;
        reg_ctx->inst_idx++;
        if (reg_ctx->inst_idx == inst_count) {
            reg_ctx->level = ANJ_ID_OID;
            reg_ctx->obj_idx++;
        }
    }

    dm->op_count--;
    return dm->op_count > 0 ? 0 : _ANJ_DM_LAST_RECORD;
}
