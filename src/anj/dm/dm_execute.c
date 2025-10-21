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
#include <stddef.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/log.h>
#include <anj/utils.h>

#include "dm_core.h"
#include "dm_io.h"

int _anj_dm_begin_execute_op(anj_t *anj, const anj_uri_path_t *base_path) {
    assert(anj && base_path && anj_uri_path_is(base_path, ANJ_ID_RID));
    _anj_dm_data_model_t *dm = &anj->dm;
    int result = _anj_dm_get_entity_ptrs(dm, base_path, &dm->entity_ptrs);
    if (result) {
        return result;
    }
    if (dm->entity_ptrs.res->kind != ANJ_DM_RES_E) {
        dm_log(L_ERROR, "Resource is not executable");
        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }
    return 0;
}

int _anj_dm_execute(anj_t *anj,
                    const char *execute_arg,
                    size_t execute_arg_len) {
    assert(anj);
    _anj_dm_data_model_t *dm = &anj->dm;
    assert(dm->op_in_progress);
    int result = dm->entity_ptrs.obj->handlers->res_execute(
            anj, dm->entity_ptrs.obj, dm->entity_ptrs.inst->iid,
            dm->entity_ptrs.res->rid, execute_arg, execute_arg_len);
    if (result) {
        dm_log(L_ERROR, "res_execute handler failed.");
        return result;
    }
    return 0;
}
