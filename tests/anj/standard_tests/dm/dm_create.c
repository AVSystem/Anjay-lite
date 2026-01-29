/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#include "../../../../src/anj/dm/dm_io.h"
#include "../../../../src/anj/io/io.h"

#include <anj_unit_test.h>

static int call_counter_begin;
static int call_counter_end;
static int call_counter_validate;
static bool inst_create_return_eror;
static int call_counter_create;
static anj_dm_transaction_result_t call_result;

static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) obj;
    (void) anj;
    call_counter_begin++;
    return 0;
}

static void transaction_end(anj_t *anj,
                            const anj_dm_obj_t *obj,
                            anj_dm_transaction_result_t result) {
    (void) obj;
    (void) anj;
    call_result = result;
    call_counter_end++;
}

static anj_dm_res_t res_1[] = {
    {
        .rid = 0,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_RW,
    }
};
static anj_dm_res_t res_2[] = {
    {
        .rid = 7,
        .kind = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_DOUBLE
    }
};

static int inst_create(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    call_counter_create++;
    if (inst_create_return_eror) {
        return -1;
    }

    anj_dm_obj_inst_t *inst = (anj_dm_obj_inst_t *) obj->insts;
    uint16_t insert_pos = 0;
    for (uint16_t i = 0; i < obj->max_inst_count; ++i) {
        if (inst[i].iid == ANJ_ID_INVALID || inst[i].iid > iid) {
            insert_pos = i;
            break;
        }
    }
    for (uint16_t i = obj->max_inst_count - 1; i > insert_pos; --i) {
        inst[i] = inst[i - 1];
    }
    inst[insert_pos].iid = iid;
    inst[insert_pos].res_count = 1;
    inst[insert_pos].resources = res_2;
    return 0;
}

static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) obj;
    (void) anj;
    call_counter_validate++;
    return 0;
}

static int g_rid_1_value = 0;
static double g_rid_7_value = 0;
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
    switch (rid) {
    case 1:
        out_value->int_value = g_rid_1_value;
        break;
    case 7:
        out_value->double_value = g_rid_7_value;
        break;
    default:
        break;
    }

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
    switch (rid) {
    case 1:
        g_rid_1_value = value->int_value;
        break;
    case 7:
        g_rid_7_value = value->double_value;
        break;
    default:
        break;
    }
    return 0;
}

#define TEST_INIT(Anj, Obj)                              \
    anj_dm_obj_inst_t obj_insts[5] = {                   \
        {                                                \
            .iid = 1,                                    \
            .res_count = 1,                              \
            .resources = res_1                           \
        },                                               \
        {                                                \
            .iid = 3,                                    \
        },                                               \
        {                                                \
            .iid = ANJ_ID_INVALID,                       \
        },                                               \
        {                                                \
            .iid = ANJ_ID_INVALID,                       \
        },                                               \
        {                                                \
            .iid = ANJ_ID_INVALID,                       \
        }                                                \
    };                                                   \
    anj_dm_handlers_t handlers = {                       \
        .transaction_begin = transaction_begin,          \
        .transaction_end = transaction_end,              \
        .transaction_validate = transaction_validate,    \
        .inst_create = inst_create,                      \
        .res_read = res_read,                            \
        .res_write = res_write,                          \
    };                                                   \
    anj_dm_obj_t Obj = {                                 \
        .oid = 1,                                        \
        .insts = obj_insts,                              \
        .handlers = &handlers,                           \
        .max_inst_count = 5                              \
    };                                                   \
    anj_t Anj = { 0 };                                   \
    _anj_dm_initialize(&Anj);                            \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &Obj)); \
    call_counter_begin = 0;                              \
    call_counter_end = 0;                                \
    call_counter_validate = 0;                           \
    call_counter_create = 0;                             \
    call_result = 4;

ANJ_UNIT_TEST(dm_create, create) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    anj_iid_t iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, iid));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, iid));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, iid));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(obj_insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[2].iid, 2);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[3].iid, 3);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[4].iid, 4);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_create, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_create, create_with_write) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    anj_iid_t iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, iid));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, iid));
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_DOUBLE,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 2, 7),
        .value.double_value = 17.25
    };
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, iid));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(obj_insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[2].iid, 2);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[3].iid, 3);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[4].iid, 4);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_create, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(g_rid_7_value, 17.25);
}

ANJ_UNIT_TEST(dm_create, create_error_write_path) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    anj_iid_t iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, iid));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    iid = 1;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_create_object_instance(&anj, iid),
                          ANJ_DM_ERR_BAD_REQUEST);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_insts[2].iid, 3);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_create, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_create, callback_error) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    inst_create_return_eror = true;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    anj_iid_t iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_create_object_instance(&anj, iid), -1);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_create, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
    inst_create_return_eror = false;
}

ANJ_UNIT_TEST(dm_create, error_no_space) {
    TEST_INIT(anj, obj);

    obj.max_inst_count = 3;
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    anj_iid_t iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, iid));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_create, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);

    path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false,
                                                  &path),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_create, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_create, create_with_write_error) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, false, &path));
    anj_iid_t iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, iid));
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_DOUBLE,
        .path = ANJ_MAKE_RESOURCE_PATH(3, 2, 7),
        .value.double_value = 17.25
    };
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_BAD_REQUEST);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_create, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}
