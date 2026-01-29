/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 16

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/log.h>
#include <anj/utils.h>

#include "../core/core.h"
#include "../utils.h"
#include "dm_core.h"
#include "dm_io.h"

bool _anj_dm_is_readable_resource(anj_dm_res_kind_t kind) {
    return kind == ANJ_DM_RES_R || kind == ANJ_DM_RES_RM
           || kind == ANJ_DM_RES_RW || kind == ANJ_DM_RES_RWM;
}

bool _anj_dm_is_writable_resource(anj_dm_res_kind_t kind, bool is_bootstrap) {
    return kind == ANJ_DM_RES_W || kind == ANJ_DM_RES_RW
           || kind == ANJ_DM_RES_WM || kind == ANJ_DM_RES_RWM
           || (is_bootstrap && kind != ANJ_DM_RES_E);
}

uint16_t _anj_dm_count_res_insts(const anj_dm_res_t *res) {
    uint16_t count = 0;
    for (uint16_t idx = 0; idx < res->max_inst_count; idx++) {
        if (res->insts[idx] == ANJ_ID_INVALID) {
            break;
        }
        count++;
    }
    return count;
}

uint16_t _anj_dm_count_obj_insts(const anj_dm_obj_t *obj) {
    uint16_t count = 0;
    for (uint16_t idx = 0; idx < obj->max_inst_count; idx++) {
        if (obj->insts[idx].iid == ANJ_ID_INVALID) {
            break;
        }
        count++;
    }
    return count;
}

const anj_dm_obj_t *_anj_dm_find_obj(_anj_dm_data_model_t *dm, anj_oid_t oid) {
    for (uint16_t idx = 0; idx < dm->objs_count; idx++) {
        if (dm->objs[idx]->oid == oid) {
            return dm->objs[idx];
        } else if (dm->objs[idx]->oid > oid) {
            break;
        }
    }
    return NULL;
}

static const anj_dm_obj_inst_t *_anj_dm_find_inst(const anj_dm_obj_t *obj,
                                                  anj_iid_t iid) {
    for (uint16_t idx = 0; idx < obj->max_inst_count; idx++) {
        if (obj->insts[idx].iid == iid) {
            return &obj->insts[idx];
        } else if (obj->insts[idx].iid > iid) {
            break;
        }
    }
    return NULL;
}

static const anj_dm_res_t *_anj_dm_find_res(const anj_dm_obj_inst_t *inst,
                                            anj_rid_t rid) {
    for (uint16_t idx = 0; idx < inst->res_count; idx++) {
        if (inst->resources[idx].rid == rid) {
            return &inst->resources[idx];
        } else if (inst->resources[idx].rid > rid) {
            break;
        }
    }
    return NULL;
}

bool _anj_dm_res_inst_exists(const anj_dm_res_t *res, anj_riid_t riid) {
    for (uint16_t idx = 0; idx < res->max_inst_count; idx++) {
        if (res->insts[idx] == riid) {
            return true;
        } else if (res->insts[idx] > riid) {
            break;
        }
    }
    return false;
}

int _anj_dm_call_transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
    if (obj->handlers->transaction_begin) {
        return obj->handlers->transaction_begin(anj, obj);
    }
    return 0;
}

int _anj_dm_get_obj_ptr_ensure_transaction_begin(anj_t *anj,
                                                 anj_oid_t oid,
                                                 const anj_dm_obj_t **out_obj) {
    _anj_dm_data_model_t *dm = &anj->dm;
    for (uint16_t idx = 0; idx < dm->objs_count; idx++) {
        if (dm->objs[idx]->oid == oid) {
            *out_obj = dm->objs[idx];
            if (!dm->in_transaction[idx]) {
                dm->in_transaction[idx] = true;
                return _anj_dm_call_transaction_begin(anj, *out_obj);
            }
            return 0;
        } else if (dm->objs[idx]->oid > oid) {
            break;
        }
    }
    dm_log(L_ERROR, "Object /%" PRIu16 " not found in data model", oid);
    return ANJ_DM_ERR_NOT_FOUND;
}

int _anj_dm_get_obj_ptrs(const anj_dm_obj_t *obj,
                         const anj_uri_path_t *path,
                         _anj_dm_entity_ptrs_t *out_ptrs) {
    assert(obj && path && out_ptrs);

    const anj_dm_obj_inst_t *inst = NULL;
    const anj_dm_res_t *res = NULL;
    anj_riid_t riid = ANJ_ID_INVALID;

    if (!anj_uri_path_has(path, ANJ_ID_IID)) {
        goto finalize;
    }

    inst = _anj_dm_find_inst(obj, path->ids[ANJ_ID_IID]);
    if (!inst) {
        dm_log(L_WARNING, "Instance not found");
        return ANJ_DM_ERR_NOT_FOUND;
    }
    if (!anj_uri_path_has(path, ANJ_ID_RID)) {
        goto finalize;
    }

    res = _anj_dm_find_res(inst, path->ids[ANJ_ID_RID]);
    if (!res) {
        dm_log(L_ERROR, "Resource not found");
        return ANJ_DM_ERR_NOT_FOUND;
    }
    if (!anj_uri_path_has(path, ANJ_ID_RIID)) {
        goto finalize;
    }
    if (!_anj_dm_is_multi_instance_resource(res->kind)) {
        dm_log(L_ERROR, "Resource is not multi-instance");
        return ANJ_DM_ERR_NOT_FOUND;
    }

    if (!_anj_dm_res_inst_exists(res, path->ids[ANJ_ID_RIID])) {
        dm_log(L_WARNING, "Resource Instance not found");
        return ANJ_DM_ERR_NOT_FOUND;
    }
    riid = path->ids[ANJ_ID_RIID];

finalize:
    out_ptrs->obj = obj;
    out_ptrs->inst = inst;
    out_ptrs->res = res;
    out_ptrs->riid = riid;

    return 0;
}

int _anj_dm_get_entity_ptrs(_anj_dm_data_model_t *dm,
                            const anj_uri_path_t *path,
                            _anj_dm_entity_ptrs_t *out_ptrs) {
    assert(anj_uri_path_has(path, ANJ_ID_OID));
    const anj_dm_obj_t *obj = _anj_dm_find_obj(dm, path->ids[ANJ_ID_OID]);
    if (!obj) {
        dm_log(L_ERROR, "Object not found");
        return ANJ_DM_ERR_NOT_FOUND;
    }
    return _anj_dm_get_obj_ptrs(obj, path, out_ptrs);
}

int _anj_dm_operation_begin(anj_t *anj,
                            _anj_op_t operation,
                            bool is_bootstrap_request,
                            const anj_uri_path_t *path) {
    assert(anj);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(!dm->op_in_progress);

    dm->operation = operation;
    dm->bootstrap_operation = is_bootstrap_request;
    dm->is_transactional = false;
    dm->op_in_progress = true;

    if (!is_bootstrap_request) {
        if (path != NULL && _anj_uri_path_to_security_or_oscore_obj(path)) {
            return ANJ_DM_ERR_UNAUTHORIZED;
        }
    }

    switch (operation) {
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    case ANJ_OP_DM_READ_COMP:
        dm->op_count = 0;
        dm->comp_read_current_object = 0;
        return 0;
    case ANJ_OP_DM_WRITE_COMP:
        dm->is_transactional = true;
        dm->op_ctx.write_ctx.path.uri_len = 0;
        return 0;
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
    case ANJ_OP_REGISTER:
    case ANJ_OP_UPDATE:
        return _anj_dm_begin_register_op(anj);
    case ANJ_OP_DM_DISCOVER:
#ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
        if (dm->bootstrap_operation) {
            return _anj_dm_begin_bootstrap_discover_op(anj, path);
        }
#endif // ANJ_WITH_BOOTSTRAP_DISCOVER
#ifdef ANJ_WITH_DISCOVER
        if (!dm->bootstrap_operation) {
            return _anj_dm_begin_discover_op(anj, path);
        }
#endif // ANJ_WITH_DISCOVER
        {
            dm_log(L_ERROR, "Discover operation is not supported");
            return ANJ_DM_ERR_NOT_IMPLEMENTED;
        }
    case ANJ_OP_DM_EXECUTE:
        return _anj_dm_begin_execute_op(anj, path);
    case ANJ_OP_DM_READ:
        return _anj_dm_begin_read_op(anj, path);
    case ANJ_OP_DM_WRITE_REPLACE:
    case ANJ_OP_DM_WRITE_PARTIAL_UPDATE:
        return _anj_dm_begin_write_op(anj, path);
    case ANJ_OP_DM_CREATE:
        return _anj_dm_begin_create_op(anj, path);
    case ANJ_OP_DM_DELETE:
        return _anj_dm_process_delete_op(anj, path);
    default:
        break;
    }

    dm_log(L_ERROR, "Incorrect operation type");
    return _ANJ_DM_ERR_INPUT_ARG;
}

int _anj_dm_operation_validate(anj_t *anj) {
    assert(anj);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress);
    assert(dm->is_transactional);
    int res = 0;
    for (uint16_t idx = 0; idx < dm->objs_count && !res; idx++) {
        const anj_dm_obj_t *obj = dm->objs[idx];
        if (dm->in_transaction[idx] && obj->handlers->transaction_validate) {
            res = obj->handlers->transaction_validate(anj, obj);
        }
    }
    return res;
}

void _anj_dm_operation_end(anj_t *anj, anj_dm_transaction_result_t result) {
    assert(anj);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress);
    if (dm->is_transactional) {
        for (uint16_t idx = 0; idx < dm->objs_count; idx++) {
            const anj_dm_obj_t *obj = dm->objs[idx];
            if (dm->in_transaction[idx] && obj->handlers->transaction_end) {
                obj->handlers->transaction_end(anj, obj, result);
            }
            dm->in_transaction[idx] = false;
        }
    }
    dm->op_in_progress = false;
}

void _anj_dm_initialize(anj_t *anj) {
    assert(anj);
    memset(&anj->dm, 0, sizeof(anj->dm));
}

#ifndef NDEBUG
static int check_res(const anj_dm_obj_t *obj, const anj_dm_res_t *res) {
    // handlers check
    if ((res->kind == ANJ_DM_RES_E && !obj->handlers->res_execute)
            || (_anj_dm_is_readable_resource(res->kind)
                && !obj->handlers->res_read)
            || (_anj_dm_is_writable_resource(res->kind, false)
                && !obj->handlers->res_write)) {
        goto res_error;
    }
    // resource type check
    if (res->kind != ANJ_DM_RES_E
            && !(res->type == ANJ_DATA_TYPE_BYTES
                 || res->type == ANJ_DATA_TYPE_STRING
                 || res->type == ANJ_DATA_TYPE_INT
                 || res->type == ANJ_DATA_TYPE_DOUBLE
                 || res->type == ANJ_DATA_TYPE_BOOL
                 || res->type == ANJ_DATA_TYPE_OBJLNK
                 || res->type == ANJ_DATA_TYPE_UINT
                 || res->type == ANJ_DATA_TYPE_TIME
#    ifdef ANJ_WITH_EXTERNAL_DATA
                 || res->type == ANJ_DATA_TYPE_EXTERNAL_BYTES
                 || res->type == ANJ_DATA_TYPE_EXTERNAL_STRING
#    endif // ANJ_WITH_EXTERNAL_DATA
                 )) {
        goto res_error;
    }
    if (_anj_dm_is_multi_instance_resource(res->kind) && res->max_inst_count) {
        if (!res->insts) {
            goto res_error;
        }
        anj_rid_t last_riid = 0;
        for (uint16_t idx = 0; idx < res->max_inst_count; idx++) {
            if (idx != 0 && res->insts[idx] <= last_riid) {
                goto res_error;
            }
            last_riid = res->insts[idx];
            if (last_riid == ANJ_ID_INVALID) {
                break;
            }
        }
    }
    return 0;

res_error:
    dm_log(L_ERROR, "Incorrectly defined resource %" PRIu16, res->rid);
    return _ANJ_DM_ERR_INPUT_ARG;
}

int _anj_dm_check_obj(const anj_dm_obj_t *obj) {
    if (obj->max_inst_count == 0) {
        return 0;
    }
    if (!obj->insts || !obj->handlers) {
        goto obj_error;
    }

    anj_iid_t last_iid = 0;
    for (uint16_t idx = 0; idx < obj->max_inst_count; idx++) {
        anj_iid_t iid = obj->insts[idx].iid;
        if ((idx != 0 && iid <= last_iid)
                || _anj_dm_check_obj_instance(obj, &obj->insts[idx])) {
            goto obj_error;
        }
        last_iid = iid;
        if (last_iid == ANJ_ID_INVALID) {
            break;
        }
    }
    return 0;

obj_error:
    dm_log(L_ERROR, "Incorrectly defined object %" PRIu16, obj->oid);
    return _ANJ_DM_ERR_INPUT_ARG;
}

int _anj_dm_check_obj_instance(const anj_dm_obj_t *obj,
                               const anj_dm_obj_inst_t *inst) {
    if (inst->res_count && !inst->resources) {
        goto instance_error;
    }
    if (inst->res_count == 0) {
        return 0;
    }

    anj_rid_t last_rid = 0;
    for (uint16_t res_idx = 0; res_idx < inst->res_count; res_idx++) {
        const anj_dm_res_t *res = &inst->resources[res_idx];
        if (res->rid == ANJ_ID_INVALID || (res_idx != 0 && res->rid <= last_rid)
                || check_res(obj, res)) {
            goto instance_error;
        }
        last_rid = res->rid;
    }
    return 0;

instance_error:
    dm_log(L_ERROR, "Incorrectly defined instance %" PRIu16, inst->iid);
    return _ANJ_DM_ERR_INPUT_ARG;
}

#endif // NDEBUG

int anj_dm_add_obj(anj_t *anj, const anj_dm_obj_t *obj) {
    assert(anj && obj);
    assert(!_anj_validate_obj_version(obj->version));
    assert(!_anj_dm_check_obj(obj));

    _anj_dm_data_model_t *dm = &anj->dm;
    if (dm->op_in_progress) {
        return _ANJ_DM_ERR_LOGIC;
    }
    if (dm->objs_count == ANJ_DM_MAX_OBJECTS_NUMBER) {
        dm_log(L_ERROR, "No space for a new object");
        return _ANJ_DM_ERR_MEMORY;
    }

    uint16_t idx;
    for (idx = 0; idx < dm->objs_count; idx++) {
        if (dm->objs[idx]->oid > obj->oid) {
            break;
        }
        if (dm->objs[idx]->oid == obj->oid) {
            dm_log(L_ERROR, "Object %" PRIu16 " exists", obj->oid);
            return _ANJ_DM_ERR_LOGIC;
        }
    }

    for (uint16_t i = dm->objs_count; i > idx; i--) {
        dm->objs[i] = dm->objs[i - 1];
    }
    dm->objs[idx] = obj;
    dm->objs_count++;

    _anj_core_data_model_changed_with_ssid(anj,
                                           &ANJ_MAKE_OBJECT_PATH(obj->oid),
                                           ANJ_CORE_CHANGE_TYPE_ADDED,
                                           0);
    return 0;
}

int anj_dm_remove_obj(anj_t *anj, anj_oid_t oid) {
    assert(anj);
    _anj_dm_data_model_t *dm = &anj->dm;
    if (dm->op_in_progress) {
        return _ANJ_DM_ERR_LOGIC;
    }

    uint16_t idx;
    bool found = false;
    for (idx = 0; idx < dm->objs_count; idx++) {
        if (dm->objs[idx]->oid == oid) {
            found = true;
            break;
        }
    }
    if (!found) {
        dm_log(L_ERROR, "Object %" PRIu16 " not found", oid);
        return ANJ_DM_ERR_NOT_FOUND;
    }
    dm->objs[idx] = NULL;
    for (uint16_t i = idx; i < dm->objs_count - 1; i++) {
        dm->objs[i] = dm->objs[i + 1];
    }
    dm->objs_count--;
    _anj_core_data_model_changed_with_ssid(anj, &ANJ_MAKE_OBJECT_PATH(oid),
                                           ANJ_CORE_CHANGE_TYPE_DELETED, 0);
    return 0;
}

int anj_dm_write_bytes_chunked(const anj_res_value_t *value,
                               uint8_t *buffer,
                               size_t buffer_len,
                               size_t *out_bytes_len,
                               bool *out_is_last_chunk) {
    size_t offset = value->bytes_or_string.offset;
    size_t chunk_length = value->bytes_or_string.chunk_length;
    size_t full_length = value->bytes_or_string.full_length_hint;

    if (offset + chunk_length > buffer_len) {
        return ANJ_DM_ERR_INTERNAL;
    }
    memcpy(&buffer[offset], value->bytes_or_string.data, chunk_length);
    if ((offset + chunk_length == full_length) && out_bytes_len) {
        *out_bytes_len = full_length;
        if (out_is_last_chunk) {
            *out_is_last_chunk = true;
        }
    } else if (out_is_last_chunk) {
        *out_is_last_chunk = false;
    }
    return 0;
}

int anj_dm_write_string_chunked(const anj_res_value_t *value,
                                char *buffer,
                                size_t buffer_len,
                                bool *out_is_last_chunk) {
    size_t offset = value->bytes_or_string.offset;
    size_t chunk_length = value->bytes_or_string.chunk_length;
    size_t full_length = value->bytes_or_string.full_length_hint;

    if (offset + chunk_length > buffer_len - 1) {
        return ANJ_DM_ERR_INTERNAL;
    }
    memcpy(&buffer[offset], value->bytes_or_string.data, chunk_length);
    if (offset + chunk_length == full_length) {
        buffer[full_length] = '\0';
        if (out_is_last_chunk) {
            *out_is_last_chunk = true;
        }
    } else if (out_is_last_chunk) {
        *out_is_last_chunk = false;
    }
    return 0;
}
