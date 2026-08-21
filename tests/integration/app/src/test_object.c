/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "test_object.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <anj/dm/core.h>
#include <anj/dm/defs.h>

#define TEST_OBJECT_OID 1234
#define VALUE_RID 0
#define LABEL_RID 1
#define MULTI_VALUE_RID 2
#define WRITE_ONLY_RID 3

#define INITIAL_INSTANCE_COUNT 3
#define MAX_INSTANCE_COUNT 6
#define RESOURCE_COUNT 4
#define MULTI_VALUE_MAX_INST_COUNT 2

static anj_dm_res_t resources[MAX_INSTANCE_COUNT][RESOURCE_COUNT];

static const char *const LABELS[] = { "first",  "second", "third",
                                      "fourth", "fifth",  "sixth" };

static const char *const LABEL_NULL = "";

typedef struct {
    anj_iid_t iid;
    double value;
    const char *label;
    double write_only_value;
    anj_riid_t res_insts[MULTI_VALUE_MAX_INST_COUNT];
    int res_instance_values[MULTI_VALUE_MAX_INST_COUNT];
} instance_state_t;

typedef struct {
    anj_dm_obj_t obj;
    anj_dm_obj_inst_t instances[MAX_INSTANCE_COUNT];
    instance_state_t state[MAX_INSTANCE_COUNT];
    anj_dm_obj_inst_t cached_instances[MAX_INSTANCE_COUNT];
    instance_state_t cached_state[MAX_INSTANCE_COUNT];
    anj_dm_handlers_t handlers;
} test_object_t;

static test_object_t TEST_OBJECT;

static void init_instance_resources(anj_dm_res_t *resources,
                                    const anj_riid_t *res_insts) {
    resources[VALUE_RID].rid = VALUE_RID;
    resources[VALUE_RID].type = ANJ_DATA_TYPE_DOUBLE;
    resources[VALUE_RID].kind = ANJ_DM_RES_R;

    resources[LABEL_RID].rid = LABEL_RID;
    resources[LABEL_RID].type = ANJ_DATA_TYPE_STRING;
    resources[LABEL_RID].kind = ANJ_DM_RES_R;

    resources[MULTI_VALUE_RID].rid = MULTI_VALUE_RID;
    resources[MULTI_VALUE_RID].type = ANJ_DATA_TYPE_INT;
    resources[MULTI_VALUE_RID].kind = ANJ_DM_RES_RWM;
    resources[MULTI_VALUE_RID].max_inst_count = MULTI_VALUE_MAX_INST_COUNT;
    resources[MULTI_VALUE_RID].insts = res_insts;

    resources[WRITE_ONLY_RID].rid = WRITE_ONLY_RID;
    resources[WRITE_ONLY_RID].type = ANJ_DATA_TYPE_DOUBLE;
    resources[WRITE_ONLY_RID].kind = ANJ_DM_RES_W;
}

static instance_state_t *find_instance(anj_iid_t iid) {
    for (uint16_t i = 0; i < MAX_INSTANCE_COUNT; ++i) {
        if (TEST_OBJECT.state[i].iid == iid) {
            return &TEST_OBJECT.state[i];
        }
    }
    return NULL;
}

static int *find_resource_instance_value_pointer(instance_state_t *instance,
                                                 anj_riid_t riid) {
    for (uint16_t i = 0; i < MULTI_VALUE_MAX_INST_COUNT; i++) {
        if (instance->res_insts[i] == riid) {
            return &instance->res_instance_values[i];
        }
    }
    return NULL;
}

static int resource_read(anj_t *anj,
                         const anj_dm_obj_t *obj,
                         anj_iid_t iid,
                         anj_rid_t rid,
                         anj_riid_t riid,
                         anj_res_value_t *out_value) {
    (void) anj;
    (void) obj;
    (void) riid;

    instance_state_t *instance = find_instance(iid);
    if (!instance) {
        return ANJ_DM_ERR_NOT_FOUND;
    }

    switch (rid) {
    case VALUE_RID:
        out_value->double_value = instance->value;
        return 0;
    case LABEL_RID:
        out_value->bytes_or_string.data = instance->label;
        return 0;
    case MULTI_VALUE_RID:
        int *res_inst_value =
                find_resource_instance_value_pointer(instance, riid);
        if (res_inst_value == NULL) {
            return ANJ_DM_ERR_NOT_FOUND;
        }

        out_value->int_value = *res_inst_value;
        return 0;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }
}

static int resource_write(anj_t *anj,
                          const anj_dm_obj_t *obj,
                          anj_iid_t iid,
                          anj_rid_t rid,
                          anj_riid_t riid,
                          const anj_res_value_t *value) {
    (void) anj;
    (void) obj;

    instance_state_t *instance = find_instance(iid);
    if (!instance) {
        return ANJ_DM_ERR_NOT_FOUND;
    }

    if (rid == MULTI_VALUE_RID) {
        int *res_inst_value =
                find_resource_instance_value_pointer(instance, riid);
        if (res_inst_value == NULL) {
            return ANJ_DM_ERR_NOT_FOUND;
        }

        *res_inst_value = value->int_value;
    } else if (rid == WRITE_ONLY_RID) {
        instance->write_only_value = value->double_value;
    }
    return 0;
}

static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    (void) obj;

    memcpy(TEST_OBJECT.cached_instances, TEST_OBJECT.instances,
           sizeof(TEST_OBJECT.instances));
    memcpy(TEST_OBJECT.cached_state, TEST_OBJECT.state,
           sizeof(TEST_OBJECT.state));
    return 0;
}

static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    (void) obj;
    return 0;
}

static void transaction_end(anj_t *anj,
                            const anj_dm_obj_t *obj,
                            anj_dm_transaction_result_t result) {
    (void) anj;
    (void) obj;

    if (result != ANJ_DM_TRANSACTION_SUCCESS) {
        memcpy(TEST_OBJECT.instances, TEST_OBJECT.cached_instances,
               sizeof(TEST_OBJECT.instances));
        memcpy(TEST_OBJECT.state, TEST_OBJECT.cached_state,
               sizeof(TEST_OBJECT.state));
    }
}

static void instance_init_resource_values(instance_state_t *instance,
                                          bool add_resource_instance) {
    uint16_t iid = instance->iid;

    for (uint16_t i = 0; i < MULTI_VALUE_MAX_INST_COUNT; i++) {
        instance->res_insts[i] = ANJ_ID_INVALID;
        instance->res_instance_values[i] = 0;
    }
    if (add_resource_instance) {
        // Add one resource instance
        instance->res_insts[0] = 0;
    }

    instance->value = iid;
    instance->write_only_value = 0;
    if (iid < (sizeof(LABELS) / sizeof(LABELS[0]))) {
        instance->label = LABELS[iid];
    } else {
        instance->label = LABEL_NULL;
    }
}

static int instance_create(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    assert(iid != ANJ_ID_INVALID);

    for (uint16_t i = 0; i < MAX_INSTANCE_COUNT; i++) {
        if (TEST_OBJECT.state[i].iid == ANJ_ID_INVALID
                || TEST_OBJECT.state[i].iid >= iid) {
            for (uint16_t j = MAX_INSTANCE_COUNT - 1; j > i; --j) {
                TEST_OBJECT.state[j] = TEST_OBJECT.state[j - 1];
                TEST_OBJECT.instances[j].iid = TEST_OBJECT.instances[j - 1].iid;
            }
            TEST_OBJECT.state[i].iid = iid;
            TEST_OBJECT.instances[i].iid = iid;
            instance_init_resource_values(&TEST_OBJECT.state[i], true);
            break;
        }
    }
    return 0;
}

static int instance_delete(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    (void) obj;

    for (uint16_t i = 0; i < MAX_INSTANCE_COUNT; i++) {
        if (TEST_OBJECT.state[i].iid == iid) {
            for (uint16_t j = i; j < MAX_INSTANCE_COUNT - 1; j++) {
                TEST_OBJECT.state[j] = TEST_OBJECT.state[j + 1];
                TEST_OBJECT.instances[j].iid = TEST_OBJECT.instances[j + 1].iid;
            }
            break;
        }
    }
    TEST_OBJECT.state[MAX_INSTANCE_COUNT - 1].iid = ANJ_ID_INVALID;
    TEST_OBJECT.instances[MAX_INSTANCE_COUNT - 1].iid = ANJ_ID_INVALID;
    return 0;
}

int instance_reset(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    (void) obj;

    instance_state_t *instance = find_instance(iid);
    if (!instance) {
        return ANJ_DM_ERR_NOT_FOUND;
    }

    instance_init_resource_values(instance, false);

    return 0;
}

static int resource_instance_create(anj_t *anj,
                                    const anj_dm_obj_t *obj,
                                    anj_iid_t iid,
                                    anj_rid_t rid,
                                    anj_riid_t riid) {
    (void) anj;
    (void) obj;
    (void) rid;

    instance_state_t *instance = find_instance(iid);
    if (!instance) {
        return ANJ_DM_ERR_NOT_FOUND;
    }

    for (uint16_t i = 0; i < MULTI_VALUE_MAX_INST_COUNT; i++) {
        if (instance->res_insts[i] == ANJ_ID_INVALID
                || instance->res_insts[i] >= riid) {
            for (uint16_t j = MULTI_VALUE_MAX_INST_COUNT - 1; j > i; --j) {
                instance->res_insts[j] = instance->res_insts[j - 1];
                instance->res_instance_values[j] =
                        instance->res_instance_values[j - 1];
            }
            instance->res_insts[i] = riid;
            instance->res_instance_values[i] = 0;
            break;
        }
    }
    return 0;
}

static int resource_instance_delete(anj_t *anj,
                                    const anj_dm_obj_t *obj,
                                    anj_iid_t iid,
                                    anj_rid_t rid,
                                    anj_riid_t riid) {
    instance_state_t *instance = find_instance(iid);
    if (!instance) {
        return ANJ_DM_ERR_NOT_FOUND;
    }

    for (uint16_t i = 0; i < MULTI_VALUE_MAX_INST_COUNT - 1; i++) {
        if (instance->res_insts[i] == riid) {
            for (uint16_t j = i; j < MULTI_VALUE_MAX_INST_COUNT - 1; j++) {
                instance->res_insts[j] = instance->res_insts[j + 1];
                instance->res_instance_values[j] =
                        instance->res_instance_values[j + 1];
            }
            break;
        }
    }
    instance->res_insts[MULTI_VALUE_MAX_INST_COUNT - 1] = ANJ_ID_INVALID;
    return 0;
}

int test_object_install(anj_t *anj) {
    memset(&TEST_OBJECT, 0, sizeof(TEST_OBJECT));

    /* Runtime assignment avoids C++ designated-initializer ordering rules. */
    TEST_OBJECT.handlers.inst_create = instance_create;
    TEST_OBJECT.handlers.inst_delete = instance_delete;
    TEST_OBJECT.handlers.res_read = resource_read;
    TEST_OBJECT.handlers.res_write = resource_write;
    TEST_OBJECT.handlers.transaction_begin = transaction_begin;
    TEST_OBJECT.handlers.transaction_validate = transaction_validate;
    TEST_OBJECT.handlers.transaction_end = transaction_end;
    TEST_OBJECT.handlers.res_inst_create = resource_instance_create;
    TEST_OBJECT.handlers.res_inst_delete = resource_instance_delete;
    TEST_OBJECT.handlers.inst_reset = instance_reset;

    TEST_OBJECT.obj.oid = TEST_OBJECT_OID;
    TEST_OBJECT.obj.version = "1.1";
    TEST_OBJECT.obj.handlers = &TEST_OBJECT.handlers;
    TEST_OBJECT.obj.max_inst_count = MAX_INSTANCE_COUNT;
    TEST_OBJECT.obj.insts = TEST_OBJECT.instances;

    for (uint16_t i = 0; i < MAX_INSTANCE_COUNT; ++i) {
        init_instance_resources(resources[i], TEST_OBJECT.state[i].res_insts);

        TEST_OBJECT.instances[i].resources = resources[i];
        TEST_OBJECT.instances[i].res_count =
                sizeof(resources[i]) / sizeof(resources[i][0]);

        if (i < INITIAL_INSTANCE_COUNT) {
            TEST_OBJECT.instances[i].iid = (anj_iid_t) i;
            TEST_OBJECT.state[i].iid = (anj_iid_t) i;
        } else {
            TEST_OBJECT.state[i].iid = ANJ_ID_INVALID;
            TEST_OBJECT.instances[i].iid = ANJ_ID_INVALID;
        }
        instance_init_resource_values(&TEST_OBJECT.state[i], true);
    }

    return anj_dm_add_obj(anj, &TEST_OBJECT.obj);
}

int test_object_set_value(anj_t *anj, anj_iid_t iid, double value) {
    instance_state_t *instance = find_instance(iid);
    if (!instance) {
        return -1;
    }

    instance->value = value;

    anj_uri_path_t path = { 0 };
    path.ids[0] = TEST_OBJECT_OID;
    path.ids[1] = iid;
    path.ids[2] = VALUE_RID;
    path.uri_len = 3;
    anj_core_data_model_changed(anj, &path, ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    return 0;
}
