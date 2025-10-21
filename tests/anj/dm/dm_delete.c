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

#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#include "../../../src/anj/dm/dm_io.h"

#include <anj_unit_test.h>

static int call_counter_begin;
static int call_counter_end;
static int call_counter_validate;
static int call_counter_delete;
static int call_counter_res_delete;
static bool inst_delete_return_eror;
static bool inst_transaction_end_return_eror;
static bool res_inst_operation_return_eror;
static anj_dm_transaction_result_t call_result;

static int res_inst_delete(anj_t *anj,
                           const anj_dm_obj_t *obj,
                           anj_iid_t iid,
                           anj_rid_t rid,
                           anj_riid_t riid) {
    (void) anj;
    call_counter_res_delete++;
    if (res_inst_operation_return_eror) {
        return -1;
    }
    ANJ_UNIT_ASSERT_EQUAL(iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(rid, 4);
    anj_riid_t *insts = (anj_riid_t *) obj->insts[1].resources[1].insts;
    if (insts[0] == riid) {
        insts[0] = insts[1];
        insts[1] = insts[2];
        insts[2] = ANJ_ID_INVALID;
    } else if (insts[1] == riid) {
        insts[1] = insts[2];
        insts[2] = ANJ_ID_INVALID;
    } else if (insts[2] == riid) {
        insts[2] = ANJ_ID_INVALID;
    }
    return 0;
}

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

static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) obj;
    (void) anj;
    call_counter_validate++;
    if (inst_transaction_end_return_eror) {
        return -22;
    }
    return 0;
}
static int inst_delete(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    call_counter_delete++;
    if (inst_delete_return_eror) {
        return -1;
    }
    anj_dm_obj_inst_t *insts = (anj_dm_obj_inst_t *) obj->insts;
    if (insts[0].iid == iid) {
        insts[0] = insts[1];
        insts[1] = insts[2];
        insts[2].iid = ANJ_ID_INVALID;
    } else if (insts[1].iid == iid) {
        insts[1] = insts[2];
        insts[2].iid = ANJ_ID_INVALID;
    } else if (insts[2].iid == iid) {
        insts[2].iid = ANJ_ID_INVALID;
    }
    return 0;
}

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

#define TEST_INIT(Anj, Obj)                              \
    anj_riid_t res_insts[3] = { 0, 1, 2 };               \
    anj_dm_res_t res_0[] = {                             \
        {                                                \
            .rid = 0,                                    \
            .kind = ANJ_DM_RES_R,                        \
            .type = ANJ_DATA_TYPE_INT                    \
        },                                               \
        {                                                \
            .rid = 4,                                    \
            .kind = ANJ_DM_RES_RM,                       \
            .type = ANJ_DATA_TYPE_INT,                   \
            .insts = res_insts,                          \
            .max_inst_count = 3,                         \
        }                                                \
    };                                                   \
    anj_dm_obj_inst_t obj_insts[3] = {                   \
        {                                                \
            .iid = 0,                                    \
        },                                               \
        {                                                \
            .iid = 1,                                    \
            .res_count = 2,                              \
            .resources = res_0                           \
        },                                               \
        {                                                \
            .iid = 2,                                    \
        }                                                \
    };                                                   \
    anj_dm_handlers_t handlers = {                       \
        .transaction_begin = transaction_begin,          \
        .transaction_end = transaction_end,              \
        .transaction_validate = transaction_validate,    \
        .inst_delete = inst_delete,                      \
        .res_inst_delete = res_inst_delete,              \
        .res_read = res_read,                            \
    };                                                   \
    anj_dm_obj_t Obj = {                                 \
        .oid = 1,                                        \
        .insts = obj_insts,                              \
        .handlers = &handlers,                           \
        .max_inst_count = 3                              \
    };                                                   \
    anj_t Anj = { 0 };                                   \
    _anj_dm_initialize(&Anj);                            \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &Obj)); \
    call_counter_begin = 0;                              \
    call_counter_end = 0;                                \
    call_counter_validate = 0;                           \
    call_counter_delete = 0;                             \
    call_counter_res_delete = 0;                         \
    call_result = 4;

ANJ_UNIT_TEST(dm_delete, delete_last) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 2);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 1);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_delete, delete_first) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 0);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_delete, delete_middle) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_delete, delete_all) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);

    path = ANJ_MAKE_INSTANCE_PATH(1, 2);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);

    path = ANJ_MAKE_INSTANCE_PATH(1, 0);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, ANJ_ID_INVALID);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_delete, delete_error_no_exist) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 4);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
}

ANJ_UNIT_TEST(dm_delete, delete_error_removed) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 1);
}

ANJ_UNIT_TEST(dm_delete, delete_error_no_callback) {
    TEST_INIT(anj, obj);
    handlers.inst_delete = NULL;
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false,
                                                  &path),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
}

ANJ_UNIT_TEST(dm_delete, delete_error_callback_error_1) {
    TEST_INIT(anj, obj);
    inst_delete_return_eror = true;
    call_result = true;

    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 0);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path), -1);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
    inst_delete_return_eror = false;
}

ANJ_UNIT_TEST(dm_delete, delete_error_callback_error_2) {
    TEST_INIT(anj, obj);
    inst_transaction_end_return_eror = true;

    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 0);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_validate(&anj), -22);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
    inst_transaction_end_return_eror = false;
}

#ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(dm_delete, delete_res_last) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 2);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], 0);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[1], 1);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[2], ANJ_ID_INVALID);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_delete, delete_res_first) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], 1);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[2], ANJ_ID_INVALID);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_delete, delete_res_middle) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj.insts[2].iid, 2);

    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], 0);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[2], ANJ_ID_INVALID);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_delete, delete_res_all) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], 0);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[2], ANJ_ID_INVALID);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);

    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], 2);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[1], ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[2], ANJ_ID_INVALID);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);

    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 2);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[1], ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[2], ANJ_ID_INVALID);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_delete, delete_res_error_path) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 1, 1);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 0);
}

ANJ_UNIT_TEST(dm_delete, delete_res_error_no_instances) {
    TEST_INIT(anj, obj);
    res_insts[0] = ANJ_ID_INVALID;
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_delete, delete_res_error_callback) {
    TEST_INIT(anj, obj);
    res_inst_operation_return_eror = true;
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false, &path), -1);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
    res_inst_operation_return_eror = false;
}

ANJ_UNIT_TEST(dm_delete, delete_res_error_not_authorized) {
    TEST_INIT(anj, obj);
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(ANJ_OBJ_ID_SECURITY, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false,
                                                  &path),
                          ANJ_DM_ERR_UNAUTHORIZED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    path = ANJ_MAKE_INSTANCE_PATH(ANJ_OBJ_ID_OSCORE, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, false,
                                                  &path),
                          ANJ_DM_ERR_UNAUTHORIZED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_delete, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 0);
}
#endif // ANJ_WITH_LWM2M12
