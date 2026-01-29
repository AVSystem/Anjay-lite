/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#include "../../../../src/anj/dm/dm_io.h"

#include <anj_unit_test.h>

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) obj;
    (void) iid;
    (void) riid;
    (void) rid;
    (void) out_value;
    return 0;
}
static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) anj;
    (void) obj;
    (void) iid;
    (void) riid;
    (void) rid;
    (void) value;
    return 0;
}

static anj_dm_obj_t obj_0 = {
    .oid = 0
};

static anj_dm_res_t inst_1_res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 1,
        .kind = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT
    }
};

static anj_riid_t res_insts[] = { 1, 2 };
static anj_dm_res_t inst_2_res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 1,
        .kind = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 2,
        .kind = ANJ_DM_RES_RWM,
        .type = ANJ_DATA_TYPE_INT,
        .max_inst_count = 2,
        .insts = res_insts
    },
    {
        .rid = 3,
        .kind = ANJ_DM_RES_WM,
        .type = ANJ_DATA_TYPE_INT,
        .max_inst_count = 0
    },
    {
        .rid = 4,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    }
};
#define OBJ_1_INST_MAX_COUNT 3
static anj_dm_obj_inst_t obj_1_insts[OBJ_1_INST_MAX_COUNT] = {
    {
        .iid = 1,
        .res_count = 2,
        .resources = inst_1_res
    },
    {
        .iid = 2,
        .res_count = 5,
        .resources = inst_2_res
    },
    {
        .iid = ANJ_ID_INVALID
    }
};
static anj_dm_handlers_t handlers = {
    .res_read = res_read,
    .res_write = res_write,
};
static anj_dm_obj_t obj_1 = {
    .oid = 1,
    .version = "1.1",
    .insts = obj_1_insts,
    .max_inst_count = OBJ_1_INST_MAX_COUNT,
    .handlers = &handlers
};

static anj_dm_obj_inst_t obj_3_insts[1] = {
    {
        .iid = 0
    }
};
static anj_dm_obj_t obj_3 = {
    .oid = 3,
    .insts = obj_3_insts,
    .max_inst_count = 1,
    .handlers = &handlers
};
static anj_dm_obj_t obj_5 = {
    .oid = 5
};
static anj_dm_obj_t obj_55 = {
    .oid = 55,
    .version = "1.2"
};

typedef struct {
    anj_uri_path_t path;
    const uint16_t dim;
    const char *version;
} discover_record_t;

#define DISCOVER_TEST(Path, Idx_start, Idx_end)                               \
    anj_t anj = { 0 };                                                        \
    _anj_dm_initialize(&anj);                                                 \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_0));                    \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1));                    \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));                    \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_5));                    \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_55));                   \
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));                  \
    ANJ_UNIT_ASSERT_SUCCESS(                                                  \
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DISCOVER, false, &Path)); \
    for (size_t idx = Idx_start; idx <= Idx_end; idx++) {                     \
        ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));               \
        anj_uri_path_t out_path;                                              \
        const char *out_version;                                              \
        const uint16_t *out_dim;                                              \
        int res = _anj_dm_get_discover_record(                                \
                &anj, &out_path, &out_version, &out_dim);                     \
        ANJ_UNIT_ASSERT_TRUE(                                                 \
                anj_uri_path_equal(&out_path, &disc_records[idx].path));      \
        if (disc_records[idx].version) {                                      \
            ANJ_UNIT_ASSERT_FALSE(                                            \
                    strcmp(out_version, disc_records[idx].version));          \
        } else {                                                              \
            ANJ_UNIT_ASSERT_NULL(out_version);                                \
        }                                                                     \
        if (disc_records[idx].dim != ANJ_ID_INVALID) {                        \
            ANJ_UNIT_ASSERT_TRUE(*out_dim == disc_records[idx].dim);          \
        } else {                                                              \
            ANJ_UNIT_ASSERT_NULL(out_dim);                                    \
        }                                                                     \
        if (idx == Idx_end) {                                                 \
            ANJ_UNIT_ASSERT_EQUAL(res, _ANJ_DM_LAST_RECORD);                  \
        } else {                                                              \
            ANJ_UNIT_ASSERT_EQUAL(res, 0);                                    \
        }                                                                     \
    }                                                                         \
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);                  \
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

/**
 * Object 1:
 * 1: version = "1.1"
 *    1
 *       0
 *       1
 *    2
 *       0
 *       1
 *       2: dim = 2
 *          1
 *          2
 *       3: dim = 0
 *       4
 */
// clang-format off
static discover_record_t disc_records [12] = {
    {.path = ANJ_MAKE_OBJECT_PATH(1), .version = "1.1",  .dim = ANJ_ID_INVALID},
    {.path = ANJ_MAKE_INSTANCE_PATH(1,1) , .dim = ANJ_ID_INVALID},
    {.path = ANJ_MAKE_RESOURCE_PATH(1,1,0) , .dim = ANJ_ID_INVALID},
    {.path = ANJ_MAKE_RESOURCE_PATH(1,1,1) , .dim = ANJ_ID_INVALID},
    {.path = ANJ_MAKE_INSTANCE_PATH(1,2) , .dim = ANJ_ID_INVALID},
    {.path = ANJ_MAKE_RESOURCE_PATH(1,2,0) , .dim = ANJ_ID_INVALID},
    {.path = ANJ_MAKE_RESOURCE_PATH(1,2,1) , .dim = ANJ_ID_INVALID},
    {.path = ANJ_MAKE_RESOURCE_PATH(1,2,2), .dim = 2},
    {.path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1,2,2,1) , .dim = ANJ_ID_INVALID},
    {.path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1,2,2,2) , .dim = ANJ_ID_INVALID},
    {.path = ANJ_MAKE_RESOURCE_PATH(1,2,3), .dim = 0},
    {.path = ANJ_MAKE_RESOURCE_PATH(1,2,4) , .dim = ANJ_ID_INVALID}
};
// clang-format on
ANJ_UNIT_TEST(dm_discover, discover_operation_object) {
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    DISCOVER_TEST(path, 0, 11);
}
ANJ_UNIT_TEST(dm_discover, discover_operation_inst_1) {
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    DISCOVER_TEST(path, 1, 3);
}
ANJ_UNIT_TEST(dm_discover, discover_operation_inst_2) {
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 2);
    DISCOVER_TEST(path, 4, 11);
}
ANJ_UNIT_TEST(dm_discover, discover_operation_inst_1_res_0) {
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    DISCOVER_TEST(path, 2, 2);
}
ANJ_UNIT_TEST(dm_discover, discover_operation_inst_1_res_1) {
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1);
    DISCOVER_TEST(path, 3, 3);
}
ANJ_UNIT_TEST(dm_discover, discover_operation_inst_2_res_0) {
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 2, 0);
    DISCOVER_TEST(path, 5, 5);
}
ANJ_UNIT_TEST(dm_discover, discover_operation_inst_2_res_1) {
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 2, 1);
    DISCOVER_TEST(path, 6, 6);
}
ANJ_UNIT_TEST(dm_discover, discover_operation_inst_2_res_2) {
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 2, 2);
    DISCOVER_TEST(path, 7, 9);
}
ANJ_UNIT_TEST(dm_discover, discover_operation_inst_2_res_3) {
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 2, 3);
    DISCOVER_TEST(path, 10, 10);
}
ANJ_UNIT_TEST(dm_discover, discover_operation_inst_2_res_4) {
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 2, 4);
    DISCOVER_TEST(path, 11, 11);
}
