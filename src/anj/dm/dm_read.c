/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 25

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/log.h>
#include <anj/utils.h>

#include "../utils.h"
#include "dm_core.h"
#include "dm_io.h"

static size_t get_readable_res_count_from_resource(const anj_dm_res_t *res) {
    if (!_anj_dm_is_readable_resource(res->kind)) {
        return 0;
    }
    if (!_anj_dm_is_multi_instance_resource(res->kind)) {
        return 1;
    }
    return _anj_dm_count_res_insts(res);
}

static size_t
get_readable_res_count_from_instance(const anj_dm_obj_inst_t *inst) {
    size_t count = 0;

    for (uint16_t idx = 0; idx < inst->res_count; idx++) {
        count += get_readable_res_count_from_resource(&inst->resources[idx]);
    }
    return count;
}

static size_t get_readable_res_count_from_object(const anj_dm_obj_t *obj) {
    size_t count = 0;

    for (uint16_t idx = 0; idx < obj->max_inst_count; idx++) {
        if (obj->insts[idx].iid == ANJ_ID_INVALID) {
            break;
        }
        count += get_readable_res_count_from_instance(&obj->insts[idx]);
    }
    return count;
}

static int
get_readable_res_count_and_set_start_level(_anj_dm_data_model_t *dm) {
    _anj_dm_read_ctx_t *read_ctx = &dm->op_ctx.read_ctx;
    _anj_dm_entity_ptrs_t *entity_ptrs = &dm->entity_ptrs;
    if (entity_ptrs->riid != ANJ_ID_INVALID) {
        read_ctx->base_level = ANJ_ID_RIID;
        read_ctx->total_op_count =
                _anj_dm_is_readable_resource(entity_ptrs->res->kind) ? 1 : 0;
        if (read_ctx->total_op_count == 0) {
            dm_log(L_ERROR, "Resource is not readable");
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }
    } else if (entity_ptrs->res) {
        if (!_anj_dm_is_readable_resource(entity_ptrs->res->kind)) {
            dm_log(L_ERROR, "Resource is not readable");
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }
        read_ctx->base_level = ANJ_ID_RID;
        read_ctx->total_op_count =
                get_readable_res_count_from_resource(entity_ptrs->res);
    } else if (entity_ptrs->inst) {
        read_ctx->base_level = ANJ_ID_IID;
        read_ctx->total_op_count =
                get_readable_res_count_from_instance(entity_ptrs->inst);
    } else {
        read_ctx->base_level = ANJ_ID_OID;
        read_ctx->total_op_count =
                get_readable_res_count_from_object(entity_ptrs->obj);
    }

    dm->op_count = read_ctx->total_op_count;
    return 0;
}

#if defined(ANJ_WITH_COMPOSITE_OPERATIONS) || defined(ANJ_WITH_OBSERVE)
static bool resource_can_be_read(const anj_dm_res_t *res) {
    if (!_anj_dm_is_readable_resource(res->kind)) {
        return false;
    }
    if (_anj_dm_is_multi_instance_resource(res->kind)
            && (res->max_inst_count == 0 || res->insts[0] == ANJ_ID_INVALID)) {
        return false;
    }
    return true;
}

static bool instance_can_be_read(const anj_dm_obj_inst_t *inst) {
    for (uint16_t idx = 0; idx < inst->res_count; idx++) {
        if (resource_can_be_read(&inst->resources[idx])) {
            return true;
        }
    }
    return false;
}

static bool object_can_be_read(const anj_dm_obj_t *obj) {
    for (uint16_t idx = 0; idx < obj->max_inst_count; idx++) {
        if (obj->insts[idx].iid != ANJ_ID_INVALID
                && instance_can_be_read(&obj->insts[idx])) {
            return true;
        }
    }
    return false;
}

int _anj_dm_path_has_readable_resources(_anj_dm_data_model_t *dm,
                                        const anj_uri_path_t *path) {

    if (path != NULL && _anj_uri_path_to_security_or_oscore_obj(path)) {
        return ANJ_DM_ERR_NOT_FOUND;
    }
#    ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    if (!anj_uri_path_has(path, ANJ_ID_OID)) {
        for (uint16_t idx = 0; idx < dm->objs_count; idx++) {
            if (object_can_be_read(dm->objs[idx])) {
                return 0;
            }
        }
    } else
#    endif // ANJ_WITH_COMPOSITE_OPERATIONS
    {
        _anj_dm_entity_ptrs_t entity_ptrs;
        if (_anj_dm_get_entity_ptrs(dm, path, &entity_ptrs)) {
            return ANJ_DM_ERR_NOT_FOUND;
        }

        if (anj_uri_path_is(path, ANJ_ID_RIID)
                || anj_uri_path_is(path, ANJ_ID_RID)) {
            return resource_can_be_read(entity_ptrs.res)
                           ? 0
                           : ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        } else if (anj_uri_path_is(path, ANJ_ID_IID)) {
            return instance_can_be_read(entity_ptrs.inst)
                           ? 0
                           : ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        } else if (anj_uri_path_is(path, ANJ_ID_OID)) {
            return object_can_be_read(entity_ptrs.obj)
                           ? 0
                           : ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }
    }
    return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
}
#endif // defined(ANJ_WITH_COMPOSITE_OPERATIONS) || defined(ANJ_WITH_OBSERVE)

static int get_read_value(anj_t *anj,
                          anj_res_value_t *out_value,
                          _anj_dm_entity_ptrs_t *ptrs) {
    memset(out_value, 0, sizeof(*out_value));
    int ret = ptrs->obj->handlers->res_read(anj, ptrs->obj, ptrs->inst->iid,
                                            ptrs->res->rid, ptrs->riid,
                                            out_value);
    if (!ret && ptrs->res->type == ANJ_DATA_TYPE_STRING) {
        out_value->bytes_or_string.chunk_length =
                out_value->bytes_or_string.data
                        ? strlen((const char *) out_value->bytes_or_string.data)
                        : 0;
    }
    return ret;
}

static void increment_idx_starting_from_res(_anj_dm_read_ctx_t *read_ctx,
                                            uint16_t res_count) {
    read_ctx->res_idx++;
    if (read_ctx->res_idx == res_count) {
        read_ctx->res_idx = 0;
        read_ctx->inst_idx++;
    }
}

static void get_readable_resource(_anj_dm_data_model_t *dm) {
    _anj_dm_read_ctx_t *read_ctx = &dm->op_ctx.read_ctx;
    _anj_dm_entity_ptrs_t *entity_ptrs = &dm->entity_ptrs;
    const anj_dm_obj_t *obj = entity_ptrs->obj;
    const anj_dm_res_t *res;
    bool found = false;
    while (!found) {
        if (read_ctx->base_level == ANJ_ID_OID) {
            assert(read_ctx->inst_idx < obj->max_inst_count);
            entity_ptrs->inst = &obj->insts[read_ctx->inst_idx];
        }
        assert(read_ctx->res_idx < entity_ptrs->inst->res_count);
        res = &entity_ptrs->inst->resources[read_ctx->res_idx];
        if (_anj_dm_is_readable_resource(res->kind)) {
            if (_anj_dm_is_multi_instance_resource(res->kind)
                    && res->max_inst_count != 0
                    && res->insts[0] != ANJ_ID_INVALID) {
                uint16_t inst_count = _anj_dm_count_res_insts(res);
                assert(read_ctx->res_inst_idx < inst_count);
                entity_ptrs->riid = res->insts[read_ctx->res_inst_idx];
                // increment resource instance index
                read_ctx->res_inst_idx++;
                if (read_ctx->res_inst_idx == inst_count) {
                    read_ctx->res_inst_idx = 0;
                    increment_idx_starting_from_res(
                            read_ctx, entity_ptrs->inst->res_count);
                }
                found = true;
            } else {
                if (!_anj_dm_is_multi_instance_resource(res->kind)) {
                    found = true;
                    entity_ptrs->riid = ANJ_ID_INVALID;
                }
                increment_idx_starting_from_res(read_ctx,
                                                entity_ptrs->inst->res_count);
            }
        } else {
            increment_idx_starting_from_res(read_ctx,
                                            entity_ptrs->inst->res_count);
        }
    }
    entity_ptrs->res = res;
}

int _anj_dm_get_read_entry(anj_t *anj, anj_io_out_entry_t *out_record) {
    assert(anj && out_record);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress);
    assert(dm->op_count);
    assert(dm->operation == ANJ_OP_DM_READ
           || dm->operation == ANJ_OP_DM_READ_COMP);

    _anj_dm_read_ctx_t *read_ctx = &dm->op_ctx.read_ctx;
    _anj_dm_entity_ptrs_t *entity_ptrs = &dm->entity_ptrs;

    if (read_ctx->base_level == ANJ_ID_OID
            || read_ctx->base_level == ANJ_ID_IID) {
        get_readable_resource(dm);
    }
    // there is nothing to do on ANJ_ID_RIID level
    if (read_ctx->base_level == ANJ_ID_RID) {
        if (_anj_dm_is_multi_instance_resource(entity_ptrs->res->kind)) {
            assert(read_ctx->res_inst_idx < entity_ptrs->res->max_inst_count);
            entity_ptrs->riid =
                    entity_ptrs->res->insts[read_ctx->res_inst_idx++];
        }
        // there is nothing to do on ANJ_ID_RID level for single-instance case
    }
    const anj_dm_obj_t *obj = entity_ptrs->obj;
    const anj_dm_obj_inst_t *obj_inst = entity_ptrs->inst;
    const anj_dm_res_t *res = entity_ptrs->res;
    anj_riid_t riid = entity_ptrs->riid;
    out_record->type = res->type;
    out_record->path =
            (riid != ANJ_ID_INVALID)
                    ? ANJ_MAKE_RESOURCE_INSTANCE_PATH(obj->oid, obj_inst->iid,
                                                      res->rid, riid)
                    : ANJ_MAKE_RESOURCE_PATH(obj->oid, obj_inst->iid, res->rid);
    int result = get_read_value(anj, &out_record->value, entity_ptrs);
    if (result) {
        return result;
    }

    dm->op_count--;

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    // comp_read_current_object variable is used for root path
    if (dm->operation == ANJ_OP_DM_READ_COMP && dm->op_count == 0
            && dm->comp_read_current_object != 0
            && dm->comp_read_current_object < dm->objs_count) {
        result = _anj_dm_composite_next_path(anj, &ANJ_MAKE_ROOT_PATH());
        if (result && result != _ANJ_DM_NO_RECORD) {
            return result;
        }
    }
#endif // ANJ_WITH_COMPOSITE_OPERATIONS

    return dm->op_count > 0 ? 0 : _ANJ_DM_LAST_RECORD;
}

void _anj_dm_get_readable_res_count(anj_t *anj, size_t *out_res_count) {
    assert(anj && out_res_count);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress);
    assert(dm->operation == ANJ_OP_DM_READ);

    *out_res_count = dm->op_ctx.read_ctx.total_op_count;
}

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
int _anj_dm_count_readable_res_if_allowed(anj_t *anj,
                                          const anj_uri_path_t *path,
                                          size_t *out_res_count) {
    assert(anj && path && out_res_count);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress);
    assert(dm->operation == ANJ_OP_DM_READ_COMP);

    size_t count = 0;

    if (anj_uri_path_has(path, ANJ_ID_OID)) {
        _anj_dm_entity_ptrs_t ptrs;
        int result = _anj_dm_get_entity_ptrs(dm, path, &ptrs);
        if (result) {
            return result;
        }

        if (ptrs.riid != ANJ_ID_INVALID) {
            count = _anj_dm_is_readable_resource(ptrs.res->kind) ? 1 : 0;
        } else if (ptrs.res) {
            count = get_readable_res_count_from_resource(ptrs.res);
        } else if (ptrs.inst) {
            count = get_readable_res_count_from_instance(ptrs.inst);
        } else {
            count = get_readable_res_count_from_object(ptrs.obj);
        }
    } else {
        for (uint16_t idx = 0; idx < dm->objs_count; idx++) {
            if (dm->objs[idx]->oid == ANJ_OBJ_ID_SECURITY
                    || dm->objs[idx]->oid == ANJ_OBJ_ID_OSCORE) {
                continue;
            }
            count += get_readable_res_count_from_object(dm->objs[idx]);
        }
    }
    *out_res_count = count;
    return 0;
}

int _anj_dm_composite_next_path(anj_t *anj, const anj_uri_path_t *path) {
    assert(anj && path);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress);
    assert(dm->operation == ANJ_OP_DM_READ_COMP);

    int ret = 0;
    _anj_dm_read_ctx_t *read_ctx = &dm->op_ctx.read_ctx;
    bool root_path = !anj_uri_path_has(path, ANJ_ID_OID);

    assert(dm->op_count == 0);

    anj_uri_path_t object_path;
    do {
        ret = 0;
        if (root_path) {
            while (dm->comp_read_current_object < dm->objs_count
                   && (dm->objs[dm->comp_read_current_object]->oid
                               == ANJ_OBJ_ID_SECURITY
                       || dm->objs[dm->comp_read_current_object]->oid
                                  == ANJ_OBJ_ID_OSCORE)) {
                dm->comp_read_current_object++;
            }

            if (dm->comp_read_current_object >= dm->objs_count) {
                return _ANJ_DM_NO_RECORD;
            }

            object_path = ANJ_MAKE_OBJECT_PATH(
                    dm->objs[dm->comp_read_current_object++]->oid);
            path = &object_path;
        }

        int result = _anj_dm_get_entity_ptrs(dm, path, &dm->entity_ptrs);
        if (result) {
            return result;
        }

        result = get_readable_res_count_and_set_start_level(dm);
        if (result) {
            return result;
        }

        if (dm->op_count == 0) {
            ret = _ANJ_DM_NO_RECORD;
        }
    } while (ret == _ANJ_DM_NO_RECORD && root_path
             && dm->comp_read_current_object < dm->objs_count);

    if (ret) {
        return ret;
    }

    read_ctx->inst_idx = 0;
    read_ctx->res_idx = 0;
    read_ctx->res_inst_idx = 0;

    return 0;
}
#endif // ANJ_WITH_COMPOSITE_OPERATIONS

int _anj_dm_get_resource_value(anj_t *anj,
                               const anj_uri_path_t *path,
                               anj_res_value_t *out_value,
                               anj_data_type_t *out_type,
                               bool *out_multi_res) {
    assert(anj && path);
    _anj_dm_data_model_t *dm = &anj->dm;
    if (!anj_uri_path_has(path, ANJ_ID_RID)) {
        dm_log(L_ERROR, "Incorect path");
        return ANJ_DM_ERR_BAD_REQUEST;
    }
    _anj_dm_entity_ptrs_t ptrs;
    int ret = _anj_dm_get_entity_ptrs(dm, path, &ptrs);
    if (ret) {
        return ret;
    }
    if (out_type) {
        *out_type = ptrs.res->type;
    }
    if (!out_value) {
        return 0;
    }
    if (!_anj_dm_is_readable_resource(ptrs.res->kind)) {
        dm_log(L_ERROR, "Resource is not readable");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }

    bool is_multi_instance = _anj_dm_is_multi_instance_resource(ptrs.res->kind);
    if (out_multi_res) {
        *out_multi_res = is_multi_instance ? true : false;
    }
    if (is_multi_instance && !anj_uri_path_has(path, ANJ_ID_RIID)) {
        dm_log(L_ERROR, "Resource is multi-instance, provide path with RIID");
        return ANJ_DM_ERR_BAD_REQUEST;
    }

    return get_read_value(anj, out_value, &ptrs);
}

int _anj_dm_get_resource_type(anj_t *anj,
                              const anj_uri_path_t *path,
                              anj_data_type_t *out_type) {
    assert(anj && path && out_type);
    // HACK: to get resource type we only need to get resource pointer
    // resource instance might not exist at this point yet
    anj_uri_path_t path_to_find = *path;
    if (anj_uri_path_has(path, ANJ_ID_RIID)) {
        path_to_find.uri_len--;
    }
    return _anj_dm_get_resource_value(anj, &path_to_find, NULL, out_type, NULL);
}

int _anj_dm_begin_read_op(anj_t *anj, const anj_uri_path_t *base_path) {
    assert(anj && base_path && anj_uri_path_has(base_path, ANJ_ID_OID));

    _anj_dm_data_model_t *dm = &anj->dm;
    if (dm->bootstrap_operation) {
        if (base_path->ids[ANJ_ID_OID] != ANJ_OBJ_ID_SERVER
                && base_path->ids[ANJ_ID_OID] != ANJ_OBJ_ID_ACCESS_CONTROL) {
            dm_log(L_ERROR, "Bootstrap server can't access this object");
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }
        if (anj_uri_path_has(base_path, ANJ_ID_RID)) {
            dm_log(L_ERROR, "Bootstrap read can't target resource");
            return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
        }
    }

    int result = _anj_dm_get_entity_ptrs(dm, base_path, &dm->entity_ptrs);
    if (result) {
        return result;
    }
    result = get_readable_res_count_and_set_start_level(dm);
    if (result) {
        return result;
    }

    _anj_dm_read_ctx_t *read_ctx = &dm->op_ctx.read_ctx;
    read_ctx->inst_idx = 0;
    read_ctx->res_idx = 0;
    read_ctx->res_inst_idx = 0;
    return 0;
}

int anj_dm_res_read(anj_t *anj,
                    const anj_uri_path_t *path,
                    anj_res_value_t *out_value) {
    assert(anj && path && out_value && anj_uri_path_has(path, ANJ_ID_RID));
    return _anj_dm_get_resource_value(anj, path, out_value, NULL, NULL);
}
