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
#include <stdbool.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#include "../core/core.h"
#include "dm_core.h"

static int delete_instance(anj_t *anj) {
    _anj_dm_data_model_t *dm = &anj->dm;
    const anj_dm_obj_t *obj = dm->entity_ptrs.obj;
    if (!obj->handlers->inst_delete) {
        dm_log(L_ERROR, "inst_delete handler not defined");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }
    anj_iid_t deleted_iid = dm->entity_ptrs.inst->iid;
    int ret = obj->handlers->inst_delete(anj, obj, deleted_iid);
    if (ret) {
        dm_log(L_ERROR, "inst_delete failed");
        return ret;
    }
    dm_log(L_DEBUG, "Instance %" PRIu16 " deleted", deleted_iid);

    if (!dm->bootstrap_operation) {
        _anj_core_data_model_changed_with_ssid(
                anj,
                &ANJ_MAKE_INSTANCE_PATH(obj->oid, deleted_iid),
                ANJ_CORE_CHANGE_TYPE_DELETED,
                dm->ssid);
    }
    return 0;
}

#ifdef ANJ_WITH_OSCORE
static bool is_oscore_bootstrap_instance(anj_t *anj) {
    _anj_dm_data_model_t *dm = &anj->dm;
    anj_dm_obj_t *security_object = _anj_dm_find_obj(dm, ANJ_OBJ_ID_SECURITY);
    if (!security_object) {
        return false;
    }
    anj_res_value_t value;
    for (uint16_t idx = 0; idx < security_object->max_inst_count; idx++) {
        if (security_object->insts[idx].iid == ANJ_ID_INVALID) {
            break;
        }
        // first find Bootstrap Server Instance,
        // then read related OSCORE Instance
        if (!anj_dm_res_read(anj,
                             &ANJ_MAKE_RESOURCE_PATH(
                                     ANJ_OBJ_ID_SECURITY,
                                     security_object->insts[idx].iid,
                                     _ANJ_DM_OBJ_SECURITY_BOOTSTRAP_SERVER_RID),
                             &value)
                && value.bool_value
                && !anj_dm_res_read(anj,
                                    &ANJ_MAKE_RESOURCE_PATH(
                                            ANJ_OBJ_ID_SECURITY,
                                            security_object->insts[idx].iid,
                                            _ANJ_DM_OBJ_SECURITY_OSCORE_RID),
                                    &value)
                && value.objlnk.iid == dm->entity_ptrs.inst->iid) {
            return true;
        }
    }
    return false;
}
#endif // ANJ_WITH_OSCORE

static bool is_bootstrap_instance(anj_t *anj) {
    _anj_dm_data_model_t *dm = &anj->dm;
    if (dm->entity_ptrs.obj->oid == ANJ_OBJ_ID_SECURITY) {
        anj_res_value_t value;
        int result = anj_dm_res_read(
                anj,
                &ANJ_MAKE_RESOURCE_PATH(
                        ANJ_OBJ_ID_SECURITY, dm->entity_ptrs.inst->iid,
                        _ANJ_DM_OBJ_SECURITY_BOOTSTRAP_SERVER_RID),
                &value);
        if (result) {
            return false;
        }
        return value.bool_value;
    }
#ifdef ANJ_WITH_OSCORE
    if (dm->entity_ptrs.obj->oid == ANJ_OBJ_ID_OSCORE) {
        return is_oscore_bootstrap_instance(anj);
    }
#endif // ANJ_WITH_OSCORE
    return false;
}

#ifdef ANJ_WITH_BOOTSTRAP
static int process_bootstrap_delete_op(anj_t *anj,
                                       const anj_uri_path_t *base_path) {
    assert(!anj_uri_path_has(base_path, ANJ_ID_RID));

    bool all_objects = !anj_uri_path_has(base_path, ANJ_ID_OID);
    bool all_instances = !anj_uri_path_has(base_path, ANJ_ID_IID);

    if (!all_objects && base_path->ids[ANJ_ID_OID] == ANJ_OBJ_ID_DEVICE) {
        dm_log(L_ERROR, "Device Object Instance cannot be deleted");
        return ANJ_DM_ERR_BAD_REQUEST;
    }

    int result = 0;
    _anj_dm_data_model_t *dm = &anj->dm;
    for (uint16_t idx = 0; idx < dm->objs_count; idx++) {
        // ignore Device Object
        if (dm->objs[idx]->oid == ANJ_OBJ_ID_DEVICE) {
            continue;
        }
        if (all_objects || base_path->ids[ANJ_ID_OID] == dm->objs[idx]->oid) {
            dm->in_transaction[idx] = true;
            result = _anj_dm_call_transaction_begin(anj, dm->objs[idx]);
            if (result) {
                return result;
            }
            dm->entity_ptrs.obj = dm->objs[idx];
            uint16_t inst_idx = 0;
            uint16_t inst_count = _anj_dm_count_obj_insts(dm->objs[idx]);
            for (uint16_t i = 0; i < inst_count; i++) {
                dm->entity_ptrs.inst = &dm->objs[idx]->insts[inst_idx];
                // ignore instance if it not the targeted one or if it is
                // bootstrap instance, inrcrement inst_idx to skip bootstrap
                // instance or find valid one
                bool bootstrap_instance = is_bootstrap_instance(anj);
                if ((!all_instances
                     && base_path->ids[ANJ_ID_IID] != dm->entity_ptrs.inst->iid)
                        || bootstrap_instance) {
                    if (!all_objects && !all_instances && bootstrap_instance) {
                        dm_log(L_ERROR,
                               "Bootstrap-Server Instance can't be deleted");
                        return ANJ_DM_ERR_BAD_REQUEST;
                    }
                    inst_idx++;
                    continue;
                }
                result = delete_instance(anj);
                // end in case of error or if targeted instance was deleted
                if (result || (!all_objects && !all_instances)) {
                    return result;
                }
            }
        }
    }
    return result;
}
#endif // ANJ_WITH_BOOTSTRAP

int _anj_dm_process_delete_op(anj_t *anj, const anj_uri_path_t *base_path) {
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->bootstrap_operation || anj_uri_path_is(base_path, ANJ_ID_IID)
           || anj_uri_path_is(base_path, ANJ_ID_RIID));

    dm->is_transactional = true;

    if (dm->bootstrap_operation) {
#ifdef ANJ_WITH_BOOTSTRAP
        dm->result = process_bootstrap_delete_op(anj, base_path);
#else  // ANJ_WITH_BOOTSTRAP
        ANJ_UNREACHABLE("Bootstrap operation not supported");
#endif // ANJ_WITH_BOOTSTRAP
        return dm->result;
    }
    const anj_dm_obj_t *obj;
    dm->result = _anj_dm_get_obj_ptr_ensure_transaction_begin(
            anj, base_path->ids[ANJ_ID_OID], &obj);
    if (dm->result) {
        return dm->result;
    }
    dm->result = _anj_dm_get_obj_ptrs(obj, base_path, &dm->entity_ptrs);
    if (dm->result) {
        return dm->result;
    }

    dm->result = anj_uri_path_is(base_path, ANJ_ID_IID)
                         ? delete_instance(anj)
                         : _anj_dm_delete_res_instance(anj);

    return dm->result;
}

int _anj_dm_delete_res_instance(anj_t *anj) {
    _anj_dm_data_model_t *dm = &anj->dm;
    const anj_dm_obj_t *obj = dm->entity_ptrs.obj;
    const anj_dm_obj_inst_t *inst = dm->entity_ptrs.inst;
    const anj_dm_res_t *res = dm->entity_ptrs.res;
    anj_iid_t deleted_riid = dm->entity_ptrs.riid;

    if (!obj->handlers->res_inst_delete) {
        dm_log(L_ERROR, "res_inst_delete handler not defined");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }
    int ret = obj->handlers->res_inst_delete(anj, obj, inst->iid, res->rid,
                                             deleted_riid);
    if (ret) {
        dm_log(L_ERROR, "res_inst_delete failed");
        return ret;
    }
    dm_log(L_DEBUG, "Deleted RIID=%" PRIu16, deleted_riid);

    if (!dm->bootstrap_operation) {
        _anj_core_data_model_changed_with_ssid(
                anj,
                &ANJ_MAKE_RESOURCE_INSTANCE_PATH(obj->oid, inst->iid, res->rid,
                                                 deleted_riid),
                ANJ_CORE_CHANGE_TYPE_DELETED,
                dm->ssid);
    }
    return 0;
}
