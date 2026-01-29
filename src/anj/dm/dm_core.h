/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_DM_DM_CORE_H
#    define SRC_ANJ_DM_DM_CORE_H

#    include <stdbool.h>
#    include <stdint.h>

#    include <anj/defs.h>
#    include <anj/dm/core.h>
#    include <anj/dm/defs.h>
#    include <anj/log.h>

#    define dm_log(...) anj_log(dm, __VA_ARGS__)

#    define _ANJ_DM_OBJ_SERVER_SSID_RID 0
#    define _ANJ_DM_OBJ_SECURITY_SERVER_URI_RID 0
#    define _ANJ_DM_OBJ_SECURITY_BOOTSTRAP_SERVER_RID 1
#    define _ANJ_DM_OBJ_SECURITY_SSID_RID 10
#    define _ANJ_DM_OBJ_SECURITY_OSCORE_RID 17

#    if defined(ANJ_WITH_COMPOSITE_OPERATIONS) || defined(ANJ_WITH_OBSERVE)
int _anj_dm_path_has_readable_resources(_anj_dm_data_model_t *dm,
                                        const anj_uri_path_t *path);
#    endif // defined(ANJ_WITH_COMPOSITE_OPERATIONS) ||
           // defined(ANJ_WITH_OBSERVE)

int _anj_dm_begin_register_op(anj_t *anj);

#    ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
int _anj_dm_begin_bootstrap_discover_op(anj_t *anj,
                                        const anj_uri_path_t *base_path);
#    endif // ANJ_WITH_BOOTSTRAP_DISCOVER

#    ifdef ANJ_WITH_DISCOVER
int _anj_dm_begin_discover_op(anj_t *anj, const anj_uri_path_t *base_path);
#    endif // ANJ_WITH_DISCOVER

int _anj_dm_begin_execute_op(anj_t *anj, const anj_uri_path_t *base_path);

int _anj_dm_begin_read_op(anj_t *anj, const anj_uri_path_t *base_path);

int _anj_dm_get_resource_value(anj_t *anj,
                               const anj_uri_path_t *path,
                               anj_res_value_t *out_value,
                               anj_data_type_t *out_type,
                               bool *out_multi_res);

int _anj_dm_begin_write_op(anj_t *anj, const anj_uri_path_t *base_path);

#    ifdef ANJ_WITH_COMPOSITE_OPERATIONS
int _anj_dm_begin_composite_write_op(anj_t *anj, const anj_uri_path_t *path);
#    endif // ANJ_WITH_COMPOSITE_OPERATIONS

int _anj_dm_begin_create_op(anj_t *anj, const anj_uri_path_t *base_path);

int _anj_dm_process_delete_op(anj_t *anj, const anj_uri_path_t *base_path);

int _anj_dm_delete_res_instance(anj_t *anj);

int _anj_dm_call_transaction_begin(anj_t *anj, const anj_dm_obj_t *obj);

int _anj_dm_get_obj_ptr_ensure_transaction_begin(anj_t *anj,
                                                 anj_oid_t oid,
                                                 const anj_dm_obj_t **out_obj);

int _anj_dm_get_obj_ptrs(const anj_dm_obj_t *obj,
                         const anj_uri_path_t *path,
                         _anj_dm_entity_ptrs_t *out_ptrs);

int _anj_dm_get_entity_ptrs(_anj_dm_data_model_t *dm,
                            const anj_uri_path_t *path,
                            _anj_dm_entity_ptrs_t *out_ptrs);

#    ifndef NDEBUG
int _anj_dm_check_obj_instance(const anj_dm_obj_t *obj,
                               const anj_dm_obj_inst_t *inst);

int _anj_dm_check_obj(const anj_dm_obj_t *obj);
#    endif // NDEBUG

static inline bool _anj_dm_is_multi_instance_resource(anj_dm_res_kind_t kind) {
    return kind == ANJ_DM_RES_RM || kind == ANJ_DM_RES_WM
           || kind == ANJ_DM_RES_RWM;
}

const anj_dm_obj_t *_anj_dm_find_obj(_anj_dm_data_model_t *dm, anj_oid_t oid);

bool _anj_dm_res_inst_exists(const anj_dm_res_t *res, anj_riid_t riid);

bool _anj_dm_is_readable_resource(anj_dm_res_kind_t kind);

bool _anj_dm_is_writable_resource(anj_dm_res_kind_t kind, bool is_bootstrap);

uint16_t _anj_dm_count_res_insts(const anj_dm_res_t *res);

uint16_t _anj_dm_count_obj_insts(const anj_dm_obj_t *obj);

#endif // SRC_ANJ_DM_DM_CORE_H
