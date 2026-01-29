/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_DM_DEFS_H
#define ANJ_INTERNAL_DM_DEFS_H

#include <anj/dm/defs.h>

#ifndef ANJ_INTERNAL_INCLUDE_DM_DEFS
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_DM_DEFS

#define ANJ_INTERNAL_INCLUDE_COAP
#include <anj_internal/coap.h> // IWYU pragma: export
#undef ANJ_INTERNAL_INCLUDE_COAP

#ifdef __cplusplus
extern "C" {
#endif

/** @anj_internal_api_do_not_use */
typedef struct {
    uint16_t obj_idx;
    uint16_t inst_idx;
    anj_id_type_t level;
} _anj_dm_reg_ctx_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    uint16_t ssid;
    uint16_t obj_idx;
    uint16_t inst_idx;
    uint16_t res_idx;
    uint16_t res_inst_idx;
    anj_id_type_t level;
    size_t total_op_count;
    uint16_t dim;
} _anj_dm_disc_ctx_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    anj_uri_path_t path;
    bool instance_creation_attempted;
} _anj_dm_write_ctx_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    uint16_t inst_idx;
    uint16_t res_idx;
    uint16_t res_inst_idx;
    size_t total_op_count;
    anj_id_type_t base_level;
} _anj_dm_read_ctx_t;
/**
 * @anj_internal_api_do_not_use
 * Set of pointers related with operation.
 */
typedef struct {
    const anj_dm_obj_t *obj;
    const anj_dm_obj_inst_t *inst;
    const anj_dm_res_t *res;
    anj_riid_t riid;
} _anj_dm_entity_ptrs_t;

/**
 * @anj_internal_api_do_not_use
 * Data model context, do not modify this structure directly, its fields are
 * changed during dm API calls. Initialize it by calling @ref
 * _anj_dm_initialize. Objects can be added using @ref anj_dm_add_obj and
 * removed with @ref anj_dm_remove_obj.
 */
typedef struct {
    const anj_dm_obj_t *objs[ANJ_DM_MAX_OBJECTS_NUMBER];
    /** Indicates an ongoing transactional operation. */
    bool in_transaction[ANJ_DM_MAX_OBJECTS_NUMBER];
    uint16_t objs_count;
    union {
        _anj_dm_reg_ctx_t reg_ctx;
        _anj_dm_disc_ctx_t disc_ctx;
        _anj_dm_write_ctx_t write_ctx;
        _anj_dm_read_ctx_t read_ctx;
    } op_ctx;
    _anj_dm_entity_ptrs_t entity_ptrs;
    bool bootstrap_operation;
    bool is_transactional;
    size_t op_count;
    bool op_in_progress;
    _anj_op_t operation;

    // dm_integration variables
    bool data_to_copy;
    anj_io_out_entry_t out_record;
    uint16_t ssid;
    // used for Create operation to indicate if iid was provided by the Server
    bool iid_provided;
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    anj_uri_path_t comp_read_paths[ANJ_DM_MAX_COMP_READ_ENTRIES];
    size_t comp_read_path_count;
    size_t comp_read_already_processed;
    uint16_t comp_read_format;
    // used only when processing root path
    size_t comp_read_current_object;
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
} _anj_dm_data_model_t;

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_DM_DEFS_H
