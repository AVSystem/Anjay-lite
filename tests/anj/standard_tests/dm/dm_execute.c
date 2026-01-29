/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
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

#include "../../../../src/anj/dm/dm_io.h"

#include <anj_unit_test.h>

static size_t call_execute_arg_len;
static const char *call_execute_arg;
static int call_counter_execute;
static anj_iid_t called_iid;
static anj_rid_t called_rid;

static int res_execute(anj_t *anj,
                       const anj_dm_obj_t *obj,
                       anj_iid_t iid,
                       anj_rid_t rid,
                       const char *execute_arg,
                       size_t execute_arg_len) {
    (void) anj;
    (void) obj;
    call_execute_arg_len = execute_arg_len;
    call_execute_arg = execute_arg;
    call_counter_execute++;
    called_iid = iid;
    called_rid = rid;
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

static anj_dm_handlers_t handlers = {
    .res_execute = res_execute,
    .res_write = res_write,
};

static anj_dm_res_t res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_E
    },
    {
        .rid = 1,
        .kind = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT
    }
};

static anj_dm_obj_inst_t obj_insts[] = {
    {
        .iid = 1,
        .res_count = 2,
        .resources = res
    }
};

static anj_dm_obj_t obj = {
    .oid = 1,
    .insts = obj_insts,
    .handlers = &handlers,
    .max_inst_count = 1
};

ANJ_UNIT_TEST(_anj_dm_execute, base) {
    anj_t anj = { 0 };
    _anj_dm_initialize(&anj);

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj));

    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(1, 1, 0)));

    ANJ_UNIT_ASSERT_EQUAL(call_counter_execute, 0);

    const char *test_arg = "ddd";
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_execute(&anj, test_arg, sizeof(test_arg)));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_EQUAL(call_counter_execute, 1);
    ANJ_UNIT_ASSERT_TRUE(called_iid == 1);
    ANJ_UNIT_ASSERT_TRUE(called_rid == 0);
    ANJ_UNIT_ASSERT_TRUE(call_execute_arg == test_arg);
    ANJ_UNIT_ASSERT_EQUAL(call_execute_arg_len, sizeof(test_arg));

    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_EQUAL(call_counter_execute, 1);
}

ANJ_UNIT_TEST(_anj_dm_execute, error_calls) {
    anj_t anj = { 0 };
    _anj_dm_initialize(&anj);

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj));

    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_operation_begin(&anj,
                                    ANJ_OP_DM_EXECUTE,
                                    false,
                                    &ANJ_MAKE_RESOURCE_PATH(1, 1, 1)),
            ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_operation_begin(&anj,
                                    ANJ_OP_DM_EXECUTE,
                                    false,
                                    &ANJ_MAKE_RESOURCE_PATH(1, 2, 1)),
            ANJ_DM_ERR_NOT_FOUND);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_operation_begin(&anj,
                                    ANJ_OP_DM_EXECUTE,
                                    false,
                                    &ANJ_MAKE_RESOURCE_PATH(2, 2, 1)),
            ANJ_DM_ERR_NOT_FOUND);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(1, 1, 0)));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
}
