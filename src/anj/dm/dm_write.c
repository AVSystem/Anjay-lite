/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 29

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/log.h>
#include <anj/utils.h>

#include "../core/core.h"
#include "dm_core.h"
#include "dm_io.h"

static int update_res_val(anj_t *anj, const anj_res_value_t *value) {
    _anj_dm_data_model_t *dm = &anj->dm;
    _anj_dm_entity_ptrs_t *entity_ptrs = &dm->entity_ptrs;
    const anj_dm_obj_t *obj = dm->entity_ptrs.obj;
    if (!obj->handlers->res_write) {
        dm_log(L_ERROR, "Write handler not defined");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }
    return obj->handlers->res_write(anj, obj, entity_ptrs->inst->iid,
                                    entity_ptrs->res->rid, entity_ptrs->riid,
                                    value);
}

static int resource_type_check(_anj_dm_data_model_t *dm,
                               const anj_io_out_entry_t *record) {
    _anj_dm_entity_ptrs_t *entity_ptrs = &dm->entity_ptrs;
    if (entity_ptrs->res->type != record->type
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
            && record->type != ANJ_DATA_TYPE_NULL
#endif // ANJ_WITH_COMPOSITE_OPERATIONS)
    ) {
#ifdef ANJ_WITH_EXTERNAL_DATA
        if ((record->type == ANJ_DATA_TYPE_STRING
             && entity_ptrs->res->type == ANJ_DATA_TYPE_EXTERNAL_STRING)
                || (record->type == ANJ_DATA_TYPE_BYTES
                    && entity_ptrs->res->type
                               == ANJ_DATA_TYPE_EXTERNAL_BYTES)) {
            return 0;
        }
#endif // ANJ_WITH_EXTERNAL_DATA
        return ANJ_DM_ERR_BAD_REQUEST;
    }
    return 0;
}

// HACK: booststrap server can write to instance that does not exist in that
// case we need to create it first
static int maybe_create_instance(anj_t *anj) {
    _anj_dm_data_model_t *dm = &anj->dm;
    anj_uri_path_t *path = &dm->op_ctx.write_ctx.path;
    if (!dm->bootstrap_operation) {
        return ANJ_DM_ERR_NOT_FOUND;
    }
    dm->op_ctx.write_ctx.instance_creation_attempted = false;
    int result = _anj_dm_create_object_instance(anj, path->ids[ANJ_ID_IID]);
    if (result) {
        return result;
    }
    return _anj_dm_get_obj_ptrs(dm->entity_ptrs.obj, path, &dm->entity_ptrs);
}

static int begin_write_replace_operation(anj_t *anj) {
    _anj_dm_data_model_t *dm = &anj->dm;
    anj_uri_path_t *path = &dm->op_ctx.write_ctx.path;
    const anj_dm_obj_t *obj;
    int result = _anj_dm_get_obj_ptr_ensure_transaction_begin(
            anj, path->ids[ANJ_ID_OID], &obj);
    if (result) {
        return result;
    }
    result = _anj_dm_get_obj_ptrs(obj, path, &dm->entity_ptrs);
    if (result == ANJ_DM_ERR_NOT_FOUND) {
        dm->entity_ptrs.obj = obj;
        result = maybe_create_instance(anj);
    }
    if (result) {
        return result;
    }

    _anj_dm_entity_ptrs_t *entity_ptrs = &dm->entity_ptrs;
    const anj_dm_res_t *res = entity_ptrs->res;
    if (anj_uri_path_is(path, ANJ_ID_IID)) {
        if (!obj->handlers->inst_reset) {
            dm_log(L_ERROR, "inst_reset handler not defined");
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }
        result = obj->handlers->inst_reset(anj, obj, entity_ptrs->inst->iid);
        if (result) {
            dm_log(L_ERROR, "inst_reset failed");
            return result;
        }
        dm_log(L_DEBUG, "Reset instance IID=%" PRIu16, entity_ptrs->inst->iid);
    } else if (anj_uri_path_is(path, ANJ_ID_RID)
               && _anj_dm_is_multi_instance_resource(res->kind)) {
        // remove all res_insts
        uint16_t inst_count = _anj_dm_count_res_insts(res);
        for (uint16_t idx = 0; idx < inst_count; idx++) {
            entity_ptrs->riid = res->insts[0];
            result = _anj_dm_delete_res_instance(anj);
            if (result) {
                return result;
            }
        }
    }

    return 0;
}

static int handle_res_instances(anj_t *anj, const anj_io_out_entry_t *record) {
    _anj_dm_data_model_t *dm = &anj->dm;
    _anj_dm_entity_ptrs_t *entity_ptrs = &dm->entity_ptrs;
    const anj_dm_obj_t *obj = entity_ptrs->obj;
    const anj_dm_obj_inst_t *inst = entity_ptrs->inst;
    const anj_dm_res_t *res = entity_ptrs->res;
    entity_ptrs->riid = record->path.ids[ANJ_ID_RIID];

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    if (record->type == ANJ_DATA_TYPE_NULL) {
        assert(dm->operation == ANJ_OP_DM_WRITE_COMP);
        if (!_anj_dm_res_inst_exists(res, record->path.ids[ANJ_ID_RIID])) {
            return 0;
        }
        return _anj_dm_delete_res_instance(anj);
    }
#endif // ANJ_WITH_COMPOSITE_OPERATIONS

    uint16_t inst_count = _anj_dm_count_res_insts(res);
    // found res_inst or create new
    for (uint16_t idx = 0; idx < inst_count; idx++) {
        if (res->insts[idx] == record->path.ids[ANJ_ID_RIID]) {
            return 0;
        }
    }
    if (inst_count == res->max_inst_count) {
        dm_log(L_ERROR, "No space for new resource instance");
        return _ANJ_DM_ERR_MEMORY;
    }

    // we need to create new instance
    if (!obj->handlers->res_inst_create) {
        dm_log(L_ERROR, "res_inst_create handler not defined");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }

    int ret = obj->handlers->res_inst_create(anj, obj, inst->iid, res->rid,
                                             entity_ptrs->riid);
    if (ret) {
        dm_log(L_ERROR, "res_inst_create failed");
        return ret;
    }
    dm_log(L_DEBUG, "Created RIID=%" PRIu16, entity_ptrs->riid);

    if (!dm->bootstrap_operation) {
        _anj_core_data_model_changed_with_ssid(
                anj,
                &ANJ_MAKE_RESOURCE_INSTANCE_PATH(obj->oid, inst->iid, res->rid,
                                                 entity_ptrs->riid),
                ANJ_CORE_CHANGE_TYPE_ADDED,
                dm->ssid);
    }
    return 0;
}

static int verify_resource_before_writing(_anj_dm_data_model_t *dm,
                                          const anj_io_out_entry_t *record) {
    if (!_anj_dm_is_writable_resource(dm->entity_ptrs.res->kind,
                                      dm->bootstrap_operation)) {
        dm_log(L_ERROR, "Resource is not writable");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    } else if (resource_type_check(dm, record)) {
        dm_log(L_ERROR, "Invalid record type");
        return ANJ_DM_ERR_BAD_REQUEST;
    } else if (_anj_dm_is_multi_instance_resource(dm->entity_ptrs.res->kind)
               != anj_uri_path_has(&record->path, ANJ_ID_RIID)) {
        dm_log(L_ERROR, "Writing to invalid path");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }
    return 0;
}

int _anj_dm_write_entry(anj_t *anj, const anj_io_out_entry_t *record) {
    assert(anj && record);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress);
    assert(dm->operation == ANJ_OP_DM_CREATE
           || dm->operation == ANJ_OP_DM_WRITE_REPLACE
           || dm->operation == ANJ_OP_DM_WRITE_PARTIAL_UPDATE
           || dm->operation == ANJ_OP_DM_WRITE_COMP);
    assert(dm->operation != ANJ_OP_DM_CREATE
           || dm->op_ctx.write_ctx.instance_creation_attempted);
    int result = 0;

    if (!anj_uri_path_has(&record->path, ANJ_ID_RID)) {
        dm_log(L_ERROR, "Invalid path");
        return ANJ_DM_ERR_BAD_REQUEST;
    }
    if (anj_uri_path_outside_base(&record->path, &dm->op_ctx.write_ctx.path)) {
        dm_log(L_ERROR, "Write record outside of request path");
        return ANJ_DM_ERR_BAD_REQUEST;
    }

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    if (dm->operation == ANJ_OP_DM_WRITE_COMP) {
        assert(!dm->bootstrap_operation);

        if (record->type == ANJ_DATA_TYPE_NULL
                && !anj_uri_path_has(&record->path, ANJ_ID_RIID)) {
            dm_log(L_ERROR, "Invalid path");
            return ANJ_DM_ERR_BAD_REQUEST;
        }

        result = _anj_dm_get_obj_ptr_ensure_transaction_begin(
                anj, record->path.ids[ANJ_ID_OID], &dm->entity_ptrs.obj);
        if (result) {
            return result;
        }
    }
#endif // ANJ_WITH_COMPOSITE_OPERATIONS

    // lack of resource instance is not an error
    result = _anj_dm_get_obj_ptrs(
            dm->entity_ptrs.obj,
            &ANJ_MAKE_RESOURCE_PATH(record->path.ids[ANJ_ID_OID],
                                    record->path.ids[ANJ_ID_IID],
                                    record->path.ids[ANJ_ID_RID]),
            &dm->entity_ptrs);
    if (result) {
        /**
         * Check description below update_res_val(), if NOT_FOUND but base path
         * does target object or object instance and there is no such resource,
         * we should ignore this error.
         */
        if (!anj_uri_path_has(&dm->op_ctx.write_ctx.path, ANJ_ID_RID)) {
            // try to find instance, if exists then resource is missing, so we
            // can reset the error
            result = _anj_dm_get_obj_ptrs(
                    dm->entity_ptrs.obj,
                    &ANJ_MAKE_INSTANCE_PATH(record->path.ids[ANJ_ID_OID],
                                            record->path.ids[ANJ_ID_IID]),
                    &dm->entity_ptrs);
            if (!result) {
                dm_log(L_WARNING,
                       "Ignoring error from writing to unsupported/unknown "
                       "resource");
                return 0;
            }
        }

        return result;
    }

    result = verify_resource_before_writing(dm, record);
    if (result) {
        return result;
    }

    if (_anj_dm_is_multi_instance_resource(dm->entity_ptrs.res->kind)) {
        result = handle_res_instances(anj, record);
        if (result) {
            return result;
        }
    }

    result = update_res_val(anj, &record->value);

    /**
     * According to OMA-TS-LightweightM2M_Core-V1_2_2-20240613-A:
     * - 'In a "Write" operation targeting an Object Instance, any optional
     *    Resources that are not supported by, or unknown to, the LwM2M Client
     *    MUST NOT be interpreted as an error by the LwM2M Client'
     * - 'Any optional Resources included in the "Bootstrap-Write" operation
     *    targeting an Object or an Object Instance that are not supported by,
     *    or unknown to, the LwM2M Client MUST NOT be interpreted as an error by
     *    the LwM2M Client.'
     *
     * Because of that, we ignore NOT_FOUND and NOT_IMPLEMENTED errors here when
     * base path does targets object or object instance.
     */
    if ((result == ANJ_DM_ERR_NOT_FOUND || result == ANJ_DM_ERR_NOT_IMPLEMENTED)
            && !anj_uri_path_has(&dm->op_ctx.write_ctx.path, ANJ_ID_RID)) {
        dm_log(L_WARNING,
               "Ignoring error from writing to unsupported/unknown resource");
        return 0;
    }

    if (result) {
        return result;
    }
    if (!dm->bootstrap_operation) {
        _anj_core_data_model_changed_with_ssid(
                anj,
                &record->path,
                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED,
                dm->ssid);
    }
    return 0;
}

int _anj_dm_begin_write_op(anj_t *anj, const anj_uri_path_t *base_path) {
    assert(base_path && anj_uri_path_has(base_path, ANJ_ID_IID));
    _anj_dm_data_model_t *dm = &anj->dm;
    dm->is_transactional = true;
    dm->op_ctx.write_ctx.path = *base_path;

    if (dm->operation == ANJ_OP_DM_WRITE_REPLACE) {
        return begin_write_replace_operation(anj);
    } else {
        const anj_dm_obj_t *obj;
        int result = _anj_dm_get_obj_ptr_ensure_transaction_begin(
                anj, base_path->ids[ANJ_ID_OID], &obj);
        if (result) {
            return result;
        }
        return _anj_dm_get_obj_ptrs(obj, base_path, &dm->entity_ptrs);
    }
}
