/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stdio.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#include "../../../src/anj/dm/dm_io.h"

#include <anj_unit_test.h>

static anj_dm_obj_t obj_0 = {
    .oid = 0
};

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
    (void) value;
    return 0;
}

static anj_dm_res_t inst_1_res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
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
        .max_inst_count = 0,
    },
    {
        .rid = 4,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    }
};

static anj_dm_obj_inst_t obj_1_insts[3] = {
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
    .max_inst_count = 3,
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

ANJ_UNIT_TEST(dm_register, register_operation) {
    anj_t anj = { 0 };
    _anj_dm_initialize(&anj);

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_0));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_5));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_55));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_REGISTER, false, NULL));

    anj_uri_path_t path;
    const char *version;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_register_record(&anj, &path, &version));
    ANJ_UNIT_ASSERT_TRUE(anj_uri_path_equal(&path, &ANJ_MAKE_OBJECT_PATH(1)));
    ANJ_UNIT_ASSERT_EQUAL_STRING(version, obj_1.version);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_register_record(&anj, &path, &version));
    ANJ_UNIT_ASSERT_TRUE(
            anj_uri_path_equal(&path, &ANJ_MAKE_INSTANCE_PATH(1, 1)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_register_record(&anj, &path, &version));
    ANJ_UNIT_ASSERT_TRUE(
            anj_uri_path_equal(&path, &ANJ_MAKE_INSTANCE_PATH(1, 2)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_register_record(&anj, &path, &version));
    ANJ_UNIT_ASSERT_TRUE(anj_uri_path_equal(&path, &ANJ_MAKE_OBJECT_PATH(3)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_register_record(&anj, &path, &version));
    ANJ_UNIT_ASSERT_TRUE(
            anj_uri_path_equal(&path, &ANJ_MAKE_INSTANCE_PATH(3, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_register_record(&anj, &path, &version));
    ANJ_UNIT_ASSERT_TRUE(anj_uri_path_equal(&path, &ANJ_MAKE_OBJECT_PATH(5)));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_register_record(&anj, &path, &version),
                          _ANJ_DM_LAST_RECORD);
    ANJ_UNIT_ASSERT_TRUE(anj_uri_path_equal(&path, &ANJ_MAKE_OBJECT_PATH(55)));
    ANJ_UNIT_ASSERT_EQUAL_STRING(version, obj_55.version);

    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
}
