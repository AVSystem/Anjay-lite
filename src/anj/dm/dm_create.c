/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 17

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/log.h>
#include <anj/utils.h>

#include "../core/core.h"
#include "dm_core.h"
#include "dm_io.h"

static anj_iid_t find_free_iid(const anj_dm_obj_t *obj) {
    for (uint16_t idx = 0; idx < UINT16_MAX; idx++) {
        if (idx >= obj->max_inst_count || idx != obj->insts[idx].iid) {
            return idx;
        }
    }
    ANJ_UNREACHABLE("object has more than 65534 instances");
    return 0;
}

int _anj_dm_begin_create_op(anj_t *anj, const anj_uri_path_t *base_path) {
    assert(base_path && anj_uri_path_is(base_path, ANJ_ID_OID));
    _anj_dm_data_model_t *dm = &anj->dm;
    dm->is_transactional = true;
    dm->op_ctx.write_ctx.path = *base_path;
    dm->op_ctx.write_ctx.instance_creation_attempted = false;

    const anj_dm_obj_t *obj;
    int result = _anj_dm_get_obj_ptr_ensure_transaction_begin(
            anj, base_path->ids[ANJ_ID_OID], &obj);
    if (result) {
        return result;
    }
    result = _anj_dm_get_obj_ptrs(obj, base_path, &dm->entity_ptrs);
    if (result) {
        return result;
    }
    uint16_t inst_count = _anj_dm_count_obj_insts(obj);
    if (inst_count >= obj->max_inst_count) {
        dm_log(L_ERROR, "Maximum number of instances reached");
        result = ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }
    return result;
}

int _anj_dm_create_object_instance(anj_t *anj, anj_iid_t iid) {
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress
           && (dm->operation == ANJ_OP_DM_CREATE
               || (dm->operation == ANJ_OP_DM_WRITE_REPLACE
                   && dm->bootstrap_operation))
           && !dm->op_ctx.write_ctx.instance_creation_attempted);
    const anj_dm_obj_t *obj = dm->entity_ptrs.obj;

    if (_anj_dm_count_obj_insts(obj) >= obj->max_inst_count) {
        dm_log(L_ERROR, "Maximum number of instances reached");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }
    if (iid == ANJ_ID_INVALID) {
        iid = find_free_iid(obj);
        dm->iid_provided = false;
        dm_log(L_DEBUG,
               "Creating instance with auto-generated IID: %" PRIu16,
               iid);
    } else {
        dm->iid_provided = true;
        for (uint16_t idx = 0; idx < obj->max_inst_count; idx++) {
            if (iid == obj->insts[idx].iid) {
                dm_log(L_ERROR, "Instance already exists");
                return ANJ_DM_ERR_BAD_REQUEST;
            }
        }
    }

    if (!obj->handlers->inst_create) {
        dm_log(L_ERROR, "inst_create handler not defined");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }

    int result = obj->handlers->inst_create(anj, obj, iid);
    if (result) {
        dm_log(L_ERROR, "inst_create failed");
        return result;
    }

    // find new instance
    for (uint16_t idx = 0; idx < obj->max_inst_count; idx++) {
        if (obj->insts[idx].iid == iid) {
            dm->entity_ptrs.inst = &obj->insts[idx];
            break;
        }
    }
    assert(dm->entity_ptrs.inst);
    assert(!_anj_dm_check_obj_instance(obj, dm->entity_ptrs.inst));

    dm_log(L_DEBUG, "Created /%" PRIu16 "/%" PRIu16, obj->oid, iid);

    dm->op_ctx.write_ctx.path.ids[ANJ_ID_IID] = iid;
    dm->op_ctx.write_ctx.instance_creation_attempted = true;
    if (!dm->bootstrap_operation) {
        _anj_core_data_model_changed_with_ssid(
                anj,
                &ANJ_MAKE_INSTANCE_PATH(obj->oid, dm->entity_ptrs.inst->iid),
                ANJ_CORE_CHANGE_TYPE_ADDED,
                dm->ssid);
    }
    return 0;
}
