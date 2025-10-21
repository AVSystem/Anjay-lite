/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdio.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>

#include "../../../src/anj/dm/dm_core.h"
#include "../../../src/anj/dm/dm_io.h"

#include <anj_unit_test.h>

ANJ_UNIT_TEST(dm, add_remove_object) {
    anj_t anj = { 0 };

    _anj_dm_initialize(&anj);

    anj_dm_obj_t obj_1 = {
        .oid = 1
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1));
    anj_dm_obj_t obj_2 = {
        .oid = 3,
        .version = "2.2"
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_2));
    anj_dm_obj_t obj_3 = {
        .oid = 2
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));
    anj_dm_obj_t obj_e1 = {
        .oid = 2
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_dm_add_obj(&anj, &obj_e1), _ANJ_DM_ERR_LOGIC);
    anj_dm_obj_t obj_4 = {
        .oid = 0
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_4));
    anj_dm_obj_t obj_5 = {
        .oid = 4
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_5));
    anj_dm_obj_t obj_74 = {
        .oid = 74
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_74));
    anj_dm_obj_t obj_75 = {
        .oid = 75
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_75));
    anj_dm_obj_t obj_76 = {
        .oid = 76
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_76));
    anj_dm_obj_t obj_77 = {
        .oid = 77
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_77));
    anj_dm_obj_t obj_78 = {
        .oid = 78
    };
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_78));
    anj_dm_obj_t obj_e3 = {
        .oid = 7
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_dm_add_obj(&anj, &obj_e3), _ANJ_DM_ERR_MEMORY);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 10);

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_remove_obj(&anj, 4));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 9);
    ANJ_UNIT_ASSERT_EQUAL(anj_dm_remove_obj(&anj, 4), ANJ_DM_ERR_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 9);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_remove_obj(&anj, 1));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 8);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_remove_obj(&anj, 2));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 7);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_remove_obj(&anj, 3));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 6);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 7);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_remove_obj(&anj, 2));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 6);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_remove_obj(&anj, 0));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 5);
    ANJ_UNIT_ASSERT_EQUAL(anj_dm_remove_obj(&anj, 4), ANJ_DM_ERR_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 5);
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
    (void) rid;
    (void) riid;
    (void) value;
    return 0;
}
static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) iid;
    (void) rid;
    (void) riid;
    (void) out_value;
    return 0;
}

static int res_execute(anj_t *anj,
                       const anj_dm_obj_t *obj,
                       anj_iid_t iid,
                       anj_rid_t rid,
                       const char *execute_arg,
                       size_t execute_arg_len) {
    (void) anj;
    (void) obj;
    (void) iid;
    (void) rid;
    (void) execute_arg;
    (void) execute_arg_len;
    return 0;
}

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
        .max_inst_count = 0,
    },
    {
        .rid = 4,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 5,
        .kind = ANJ_DM_RES_E
    }
};

static anj_dm_obj_inst_t obj_1_insts[2] = {
    {
        .iid = 1,
        .res_count = 2,
        .resources = inst_1_res
    },
    {
        .iid = 2,
        .res_count = 6,
        .resources = inst_2_res
    }
};

static anj_dm_handlers_t handlers = {
    .res_write = res_write,
    .res_read = res_read,
    .res_execute = res_execute
};

static anj_dm_obj_t obj = {
    .oid = 1,
    .version = "1.1",
    .insts = obj_1_insts,
    .max_inst_count = 2,
    .handlers = &handlers
};

ANJ_UNIT_TEST(dm, add_obj_check) {
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_check_obj(&obj));
}

ANJ_UNIT_TEST(dm, add_obj_check_error_instances) {
    obj.insts = NULL;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_check_obj(&obj), _ANJ_DM_ERR_INPUT_ARG);
    obj.insts = obj_1_insts;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_check_obj(&obj));
}

ANJ_UNIT_TEST(dm, add_obj_check_error_iid) {
    obj_1_insts[0].iid = 5;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_check_obj(&obj), _ANJ_DM_ERR_INPUT_ARG);
    obj_1_insts[0].iid = 2;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_check_obj(&obj), _ANJ_DM_ERR_INPUT_ARG);
    obj_1_insts[0].iid = 1;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_check_obj(&obj));
}

ANJ_UNIT_TEST(dm, add_obj_check_error_rid) {
    inst_2_res[0].rid = 5;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_check_obj(&obj), _ANJ_DM_ERR_INPUT_ARG);
    inst_2_res[0].rid = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_check_obj(&obj));
}

ANJ_UNIT_TEST(dm, add_obj_check_error_type) {
    inst_2_res[0].type = 7777;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_check_obj(&obj), _ANJ_DM_ERR_INPUT_ARG);
    inst_2_res[0].type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_check_obj(&obj));
}

ANJ_UNIT_TEST(dm, add_obj_check_error_riid) {
    res_insts[0] = 2;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_check_obj(&obj), _ANJ_DM_ERR_INPUT_ARG);
    res_insts[0] = 1;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_check_obj(&obj));
}

ANJ_UNIT_TEST(dm, add_obj_check_error_execute_handler) {
    handlers.res_execute = NULL;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_check_obj(&obj), _ANJ_DM_ERR_INPUT_ARG);
    handlers.res_execute = res_execute;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_check_obj(&obj));
}

ANJ_UNIT_TEST(dm, add_obj_check_error_execute_handler_2) {
    handlers.res_write = NULL;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_check_obj(&obj), _ANJ_DM_ERR_INPUT_ARG);
    handlers.res_write = res_write;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_check_obj(&obj));
}

ANJ_UNIT_TEST(dm, add_obj_check_error_max_allowed_res_insts_number) {
    inst_2_res[2].insts = NULL;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_check_obj(&obj), _ANJ_DM_ERR_INPUT_ARG);
    inst_2_res[2].insts = res_insts;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_check_obj(&obj));
}
