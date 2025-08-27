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
#include <stdint.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#include "dm_core.h"
#include "dm_io.h"

#ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
static void get_security_obj_ssid_value(anj_t *anj,
                                        const anj_dm_obj_t *obj,
                                        const anj_dm_obj_inst_t *inst,
                                        const uint16_t **ssid) {
    anj_res_value_t value;
    anj_data_type_t type;
    *ssid = NULL;
    _anj_dm_data_model_t *dm = &anj->dm;
    _anj_dm_disc_ctx_t *disc_ctx = &dm->op_ctx.disc_ctx;
    if (!_anj_dm_get_resource_value(
                anj,
                &ANJ_MAKE_RESOURCE_PATH(
                        obj->oid,
                        inst->iid,
                        _ANJ_DM_OBJ_SECURITY_BOOTSTRAP_SERVER_RID),
                &value, &type, NULL)
            && type == ANJ_DATA_TYPE_BOOL && !value.bool_value
            && !_anj_dm_get_resource_value(
                       anj,
                       &ANJ_MAKE_RESOURCE_PATH(obj->oid, inst->iid,
                                               _ANJ_DM_OBJ_SECURITY_SSID_RID),
                       &value, &type, NULL)
            && type == ANJ_DATA_TYPE_INT) {
        disc_ctx->ssid = (uint16_t) value.uint_value;
        *ssid = &disc_ctx->ssid;
    }
}
#    ifdef ANJ_WITH_OSCORE
static void get_security_instance_ssid_for_oscore_obj(anj_t *anj,
                                                      anj_iid_t iid,
                                                      const uint16_t **ssid) {
    _anj_dm_data_model_t *dm = &anj->dm;
    anj_dm_obj_t *security_object = _anj_dm_find_obj(dm, ANJ_OBJ_ID_SECURITY);
    if (!security_object) {
        *ssid = NULL;
        return;
    }
    // Instance have _ANJ_DM_OBJ_SECURITY_OSCORE_RID equal to given oid and
    // iid, it's not a bootstrap server instance and SSID value is present.
    anj_res_value_t value;
    anj_data_type_t type;
    for (uint16_t idx = 0; idx < security_object->max_inst_count; idx++) {
        if (security_object->insts[idx].iid == ANJ_ID_INVALID) {
            break;
        }
        if (!_anj_dm_get_resource_value(
                    anj,
                    &ANJ_MAKE_RESOURCE_PATH(security_object->oid,
                                            security_object->insts[idx].iid,
                                            _ANJ_DM_OBJ_SECURITY_OSCORE_RID),
                    &value, &type, NULL)
                && type == ANJ_DATA_TYPE_OBJLNK && value.objlnk.iid == iid) {
            assert(value.objlnk.oid == ANJ_OBJ_ID_OSCORE);
            get_security_obj_ssid_value(anj, security_object,
                                        &security_object->insts[idx], ssid);
        }
    }
}
#    endif // ANJ_WITH_OSCORE

static void get_ssid_and_uri(anj_t *anj,
                             const anj_dm_obj_t *obj,
                             const anj_dm_obj_inst_t *inst,
                             const uint16_t **ssid,
                             const char **uri) {
    if (obj->oid != ANJ_OBJ_ID_SECURITY && obj->oid != ANJ_OBJ_ID_SERVER
            && obj->oid != ANJ_OBJ_ID_OSCORE) {
        return;
    }
    *ssid = NULL;
    *uri = NULL;
    _anj_dm_data_model_t *dm = &anj->dm;
    _anj_dm_disc_ctx_t *disc_ctx = &dm->op_ctx.disc_ctx;
    anj_res_value_t value;
    anj_data_type_t type;
    // SSID and URI are added if instance is not related to Bootstrap-Server.
    // Resource /1 of Security Object is checked to determine SSID and URI
    // presence. If there are no resources needed in the operation, we will not
    // add the URI and SSID to the message, without any error returns.
    if (obj->oid == ANJ_OBJ_ID_SECURITY) {
        get_security_obj_ssid_value(anj, obj, inst, ssid);
        if (!*ssid) {
            return;
        }
        anj_uri_path_t server_uri_path =
                ANJ_MAKE_RESOURCE_PATH(obj->oid, inst->iid,
                                       _ANJ_DM_OBJ_SECURITY_SERVER_URI_RID);
        // ANJ_DATA_TYPE_EXTERNAL_STRING is not allowed here
        if (!_anj_dm_get_resource_value(anj, &server_uri_path, &value, &type,
                                        NULL)
                && type == ANJ_DATA_TYPE_STRING) {
            *uri = (const char *) value.bytes_or_string.data;
        }
    } else if (obj->oid == ANJ_OBJ_ID_SERVER) {
        if (!_anj_dm_get_resource_value(
                    anj,
                    &ANJ_MAKE_RESOURCE_PATH(obj->oid, inst->iid,
                                            _ANJ_DM_OBJ_SERVER_SSID_RID),
                    &value, &type, NULL)
                && type == ANJ_DATA_TYPE_INT) {
            disc_ctx->ssid = (uint16_t) value.int_value;
            *ssid = &disc_ctx->ssid;
        }
    }
#    ifdef ANJ_WITH_OSCORE
    else if (obj->oid == ANJ_OBJ_ID_OSCORE) {
        // find Security Object Instance related with this OSCORE Instance and
        // read SSID
        get_security_instance_ssid_for_oscore_obj(anj, inst->iid, ssid);
    }
#    endif // ANJ_WITH_OSCORE
}

int _anj_dm_begin_bootstrap_discover_op(anj_t *anj,
                                        const anj_uri_path_t *base_path) {
    assert(anj);
    _anj_dm_data_model_t *dm = &anj->dm;
    if (base_path && anj_uri_path_has(base_path, ANJ_ID_IID)) {
        dm_log(L_ERROR, "Bootstrap discover can't target object instance");
        dm->result = _ANJ_DM_ERR_INPUT_ARG;
        return dm->result;
    }
    _anj_dm_disc_ctx_t *disc_ctx = &dm->op_ctx.disc_ctx;

    dm->op_count = 0;
    disc_ctx->obj_idx = 0;
    disc_ctx->inst_idx = 0;
    disc_ctx->level = ANJ_ID_OID;
    bool all_objects = !base_path || !anj_uri_path_has(base_path, ANJ_ID_OID);
    for (uint16_t idx = 0; idx < dm->objs_count; idx++) {
        if (all_objects || dm->objs[idx]->oid == base_path->ids[ANJ_ID_OID]) {
            if (!all_objects) {
                disc_ctx->obj_idx = idx;
            }
            dm->op_count =
                    dm->op_count + 1 + _anj_dm_count_obj_insts(dm->objs[idx]);
        }
    }
    return 0;
}

int _anj_dm_get_bootstrap_discover_record(anj_t *anj,
                                          anj_uri_path_t *out_path,
                                          const char **out_version,
                                          const uint16_t **out_ssid,
                                          const char **out_uri) {
    assert(anj && out_path && out_version && out_ssid && out_uri);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress && !dm->result);
    assert(dm->op_count);
    assert(dm->operation == ANJ_OP_DM_DISCOVER && dm->bootstrap_operation);

    _anj_dm_disc_ctx_t *disc_ctx = &dm->op_ctx.disc_ctx;
    assert(disc_ctx->obj_idx < dm->objs_count);

    *out_version = NULL;
    *out_ssid = NULL;
    *out_uri = NULL;

    const anj_dm_obj_t *obj = dm->objs[disc_ctx->obj_idx];

    if (disc_ctx->level == ANJ_ID_OID) {
        *out_path = ANJ_MAKE_OBJECT_PATH(obj->oid);
        *out_version = obj->version;

        if (obj->max_inst_count != 0 && obj->insts[0].iid != ANJ_ID_INVALID) {
            disc_ctx->level = ANJ_ID_IID;
        } else {
            disc_ctx->obj_idx++;
        }
    } else {
        assert(disc_ctx->inst_idx < obj->max_inst_count);
        const anj_dm_obj_inst_t *inst = &obj->insts[disc_ctx->inst_idx];
        *out_path = ANJ_MAKE_INSTANCE_PATH(obj->oid, inst->iid);
        get_ssid_and_uri(anj, obj, inst, out_ssid, out_uri);

        disc_ctx->inst_idx++;
        if (disc_ctx->inst_idx == _anj_dm_count_obj_insts(obj)) {
            disc_ctx->inst_idx = 0;
            disc_ctx->obj_idx++;
            disc_ctx->level = ANJ_ID_OID;
        }
    }

    dm->op_count--;
    return dm->op_count > 0 ? 0 : _ANJ_DM_LAST_RECORD;
}
#endif // ANJ_WITH_BOOTSTRAP_DISCOVER

#ifdef ANJ_WITH_DISCOVER
int _anj_dm_begin_discover_op(anj_t *anj, const anj_uri_path_t *base_path) {
    assert(anj);
    assert(base_path && anj_uri_path_has(base_path, ANJ_ID_OID)
           && !anj_uri_path_has(base_path, ANJ_ID_RIID));
    _anj_dm_data_model_t *dm = &anj->dm;
    _anj_dm_disc_ctx_t *disc_ctx = &dm->op_ctx.disc_ctx;
    dm->op_count = 0;
    bool all_instances = !anj_uri_path_has(base_path, ANJ_ID_IID);
    bool all_resources =
            all_instances || !anj_uri_path_has(base_path, ANJ_ID_RID);
    disc_ctx->inst_idx = 0;
    disc_ctx->res_idx = 0;
    disc_ctx->res_inst_idx = 0;
    if (all_instances) {
        disc_ctx->level = ANJ_ID_OID;
        dm->op_count++;
    } else if (all_resources) {
        disc_ctx->level = ANJ_ID_IID;
    } else {
        disc_ctx->level = ANJ_ID_RID;
    }

    dm->entity_ptrs.obj = _anj_dm_find_obj(dm, base_path->ids[ANJ_ID_OID]);
    if (!dm->entity_ptrs.obj) {
        dm->result = ANJ_DM_ERR_NOT_FOUND;
        dm_log(L_ERROR, "Object not found");
        return dm->result;
    }

    const anj_dm_obj_t *obj = dm->entity_ptrs.obj;

    for (uint16_t idx = 0; idx < obj->max_inst_count; idx++) {
        const anj_dm_obj_inst_t *inst = &obj->insts[idx];
        if (inst->iid == ANJ_ID_INVALID) {
            break;
        }
        if (!all_instances && base_path->ids[ANJ_ID_IID] == inst->iid) {
            disc_ctx->inst_idx = idx;
        }
        if (all_instances || base_path->ids[ANJ_ID_IID] == inst->iid) {
            if (all_resources) {
                dm->op_count++;
            }
            for (uint16_t res_idx = 0; res_idx < inst->res_count; res_idx++) {
                const anj_dm_res_t *res = &inst->resources[res_idx];
                if (!all_resources && base_path->ids[ANJ_ID_RID] == res->rid) {
                    disc_ctx->res_idx = res_idx;
                }
                if (all_resources || base_path->ids[ANJ_ID_RID] == res->rid) {
                    dm->op_count++;
                    if (_anj_dm_is_multi_instance_resource(res->operation)) {
                        dm->op_count += _anj_dm_count_res_insts(res);
                    }
                }
            }
        }
    }
    dm->op_ctx.disc_ctx.total_op_count = dm->op_count;
    return 0;
}

static void get_inst_record(_anj_dm_data_model_t *dm,
                            anj_uri_path_t *out_path) {
    _anj_dm_disc_ctx_t *disc_ctx = &dm->op_ctx.disc_ctx;
    const anj_dm_obj_t *obj = dm->entity_ptrs.obj;
    assert(disc_ctx->inst_idx < obj->max_inst_count);
    const anj_dm_obj_inst_t *inst = &obj->insts[disc_ctx->inst_idx];
    *out_path = ANJ_MAKE_INSTANCE_PATH(obj->oid, inst->iid);
    if (inst->res_count) {
        disc_ctx->level = ANJ_ID_RID;
    } else {
        disc_ctx->inst_idx++;
    }
}

static void increment_idx_starting_from_res(_anj_dm_disc_ctx_t *disc_ctx,
                                            uint16_t res_count) {
    disc_ctx->res_idx++;
    if (disc_ctx->res_idx == res_count) {
        disc_ctx->res_idx = 0;
        disc_ctx->inst_idx++;
        disc_ctx->level = ANJ_ID_IID;
    }
}

static void get_res_record(_anj_dm_data_model_t *dm,
                           anj_uri_path_t *out_path,
                           const uint16_t **out_dim) {
    _anj_dm_disc_ctx_t *disc_ctx = &dm->op_ctx.disc_ctx;
    const anj_dm_obj_t *obj = dm->entity_ptrs.obj;
    const anj_dm_obj_inst_t *inst = &obj->insts[disc_ctx->inst_idx];
    assert(disc_ctx->res_idx < inst->res_count);
    const anj_dm_res_t *res = &inst->resources[disc_ctx->res_idx];
    *out_path = ANJ_MAKE_RESOURCE_PATH(obj->oid, inst->iid, res->rid);
    bool is_multi_instance = _anj_dm_is_multi_instance_resource(res->operation);
    uint16_t inst_count = 0;
    if (is_multi_instance) {
        inst_count = _anj_dm_count_res_insts(res);
        *out_dim = &dm->op_ctx.disc_ctx.dim;
        dm->op_ctx.disc_ctx.dim = inst_count;
        if (inst_count) {
            disc_ctx->level = ANJ_ID_RIID;
        }
    }
    if (!is_multi_instance || !inst_count) {
        increment_idx_starting_from_res(disc_ctx, inst->res_count);
    }
}

static void get_res_inst_record(_anj_dm_data_model_t *dm,
                                anj_uri_path_t *out_path) {
    _anj_dm_disc_ctx_t *disc_ctx = &dm->op_ctx.disc_ctx;
    const anj_dm_obj_t *obj = dm->entity_ptrs.obj;
    const anj_dm_obj_inst_t *inst = &obj->insts[disc_ctx->inst_idx];
    const anj_dm_res_t *res = &inst->resources[disc_ctx->res_idx];
    uint16_t insts_count = _anj_dm_count_res_insts(res);
    assert(disc_ctx->res_inst_idx < insts_count);
    anj_riid_t riid = res->insts[disc_ctx->res_inst_idx];
    *out_path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(obj->oid, inst->iid, res->rid,
                                                riid);

    disc_ctx->res_inst_idx++;
    if (disc_ctx->res_inst_idx == insts_count) {
        disc_ctx->res_inst_idx = 0;
        disc_ctx->level = ANJ_ID_RID;
        increment_idx_starting_from_res(disc_ctx, inst->res_count);
    }
}

int _anj_dm_get_discover_record(anj_t *anj,
                                anj_uri_path_t *out_path,
                                const char **out_version,
                                const uint16_t **out_dim) {
    assert(anj && out_path && out_version && out_dim);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress && !dm->result);
    assert(dm->op_count);
    assert(dm->operation == ANJ_OP_DM_DISCOVER && !dm->bootstrap_operation);

    _anj_dm_disc_ctx_t *disc_ctx = &dm->op_ctx.disc_ctx;

    *out_version = NULL;
    *out_dim = NULL;

    if (disc_ctx->level == ANJ_ID_OID) {
        *out_path = ANJ_MAKE_OBJECT_PATH(dm->entity_ptrs.obj->oid);
        *out_version = dm->entity_ptrs.obj->version;
        disc_ctx->level = ANJ_ID_IID;
    } else if (disc_ctx->level == ANJ_ID_IID) {
        get_inst_record(dm, out_path);
    } else if (disc_ctx->level == ANJ_ID_RID) {
        get_res_record(dm, out_path, out_dim);
    } else if (disc_ctx->level == ANJ_ID_RIID) {
        get_res_inst_record(dm, out_path);
    }

    dm->op_count--;
    return dm->op_count > 0 ? 0 : _ANJ_DM_LAST_RECORD;
}
#endif // ANJ_WITH_DISCOVER
