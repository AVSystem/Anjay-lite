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
#include <string.h>

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
static int call_counter_res_write;
static int call_counter_res_create;
static int call_counter_res_delete;
static int call_counter_reset;
static anj_iid_t call_iid;
static anj_rid_t call_rid;
static anj_riid_t call_riid;
static anj_riid_t call_riid_inst_delete;
static int res_write_operation_error;
static bool res_create_operation_return_error;
static bool validate_return_error;
static const anj_res_value_t *call_value;
static anj_dm_transaction_result_t call_result;

static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) anj;
    (void) obj;
    call_iid = iid;
    call_rid = rid;
    call_riid = riid;
    call_value = value;
    call_counter_res_write++;
    if (res_write_operation_error) {
        return res_write_operation_error;
    }
    return 0;
}

static int res_inst_create(anj_t *anj,
                           const anj_dm_obj_t *obj,
                           anj_iid_t iid,
                           anj_rid_t rid,
                           anj_riid_t riid) {
    (void) anj;
    (void) iid;

    call_counter_res_create++;
    if (res_create_operation_return_error) {
        return -1;
    }
    anj_dm_res_t *res = (anj_dm_res_t *) &obj->insts[1].resources[rid];
    anj_riid_t *inst = (anj_riid_t *) res->insts;
    uint16_t insert_pos = 0;
    for (uint16_t i = 0; i < res->max_inst_count; ++i) {
        if (inst[i] == ANJ_ID_INVALID || inst[i] > riid) {
            insert_pos = i;
            break;
        }
    }
    for (uint16_t i = res->max_inst_count - 1; i > insert_pos; --i) {
        inst[i] = inst[i - 1];
    }
    inst[insert_pos] = riid;
    return 0;
}
static int res_inst_delete(anj_t *anj,
                           const anj_dm_obj_t *obj,
                           anj_iid_t iid,
                           anj_rid_t rid,
                           anj_riid_t riid) {
    (void) anj;
    (void) obj;
    (void) iid;
    (void) rid;
    (void) riid;
    call_riid_inst_delete = riid;
    call_counter_res_delete++;
    anj_riid_t *insts = (anj_riid_t *) obj->insts[1].resources[rid].insts;
    if (insts[0] == riid) {
        insts[0] = insts[1];
        insts[1] = insts[2];
        insts[2] = insts[3];
        insts[3] = ANJ_ID_INVALID;
    } else if (insts[1] == riid) {
        insts[1] = insts[2];
        insts[2] = insts[3];
        insts[3] = ANJ_ID_INVALID;
    } else if (insts[2] == riid) {
        insts[2] = insts[3];
        insts[3] = ANJ_ID_INVALID;
    } else if (insts[3] == riid) {
        insts[3] = ANJ_ID_INVALID;
    }
    return 0;
}

static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    (void) obj;
    call_counter_begin++;
    return 0;
}

static void transaction_end(anj_t *anj,
                            const anj_dm_obj_t *obj,
                            anj_dm_transaction_result_t result) {
    (void) anj;
    (void) obj;
    call_counter_end++;
    call_result = result;
}

static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    (void) obj;
    call_counter_validate++;
    if (validate_return_error) {
        return -12;
    }
    return 0;
}

static int inst_reset(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
    (void) obj;
    (void) iid;
    call_counter_reset++;
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
    (void) rid;
    (void) riid;
    (void) out_value;
    return 0;
}

static int inst_create(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;
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
    return 0;
}

static anj_dm_handlers_t handlers = {
    .transaction_begin = transaction_begin,
    .transaction_end = transaction_end,
    .transaction_validate = transaction_validate,
    .inst_reset = inst_reset,
    .inst_create = inst_create,
    .res_inst_create = res_inst_create,
    .res_inst_delete = res_inst_delete,
    .res_write = res_write,
    .res_read = res_read
};

#define TEST_INIT(Anj, Obj)                              \
    anj_dm_res_t res_0[] = {                             \
        {                                                \
            .rid = 0,                                    \
            .kind = ANJ_DM_RES_RW,                       \
            .type = ANJ_DATA_TYPE_INT,                   \
        },                                               \
        {                                                \
            .rid = 6,                                    \
            .kind = ANJ_DM_RES_W,                        \
            .type = ANJ_DATA_TYPE_INT                    \
        }                                                \
    };                                                   \
    anj_riid_t res_insts[9] = { 1,                       \
                                3,                       \
                                ANJ_ID_INVALID,          \
                                ANJ_ID_INVALID,          \
                                ANJ_ID_INVALID,          \
                                ANJ_ID_INVALID,          \
                                ANJ_ID_INVALID,          \
                                ANJ_ID_INVALID,          \
                                ANJ_ID_INVALID };        \
    anj_riid_t res_insts_5[9] = { 1, ANJ_ID_INVALID };   \
    anj_riid_t rid_3_inst[9] = { ANJ_ID_INVALID };       \
    anj_dm_res_t res_1[] = {                             \
        {                                                \
            .rid = 0,                                    \
            .kind = ANJ_DM_RES_RW,                       \
            .type = ANJ_DATA_TYPE_INT,                   \
        },                                               \
        {                                                \
            .rid = 1,                                    \
            .kind = ANJ_DM_RES_RW,                       \
            .type = ANJ_DATA_TYPE_INT,                   \
        },                                               \
        {                                                \
            .rid = 2,                                    \
            .kind = ANJ_DM_RES_RW,                       \
            .type = ANJ_DATA_TYPE_DOUBLE                 \
        },                                               \
        {                                                \
            .rid = 3,                                    \
            .kind = ANJ_DM_RES_RM,                       \
            .type = ANJ_DATA_TYPE_INT,                   \
            .max_inst_count = 9,                         \
            .insts = rid_3_inst                          \
        },                                               \
        {                                                \
            .rid = 4,                                    \
            .kind = ANJ_DM_RES_RWM,                      \
            .type = ANJ_DATA_TYPE_INT,                   \
            .max_inst_count = 9,                         \
            .insts = res_insts,                          \
        },                                               \
        {                                                \
            .rid = 5,                                    \
            .kind = ANJ_DM_RES_RWM,                      \
            .type = ANJ_DATA_TYPE_INT,                   \
            .max_inst_count = 2,                         \
            .insts = res_insts_5,                        \
        },                                               \
        {                                                \
            .rid = 6,                                    \
            .kind = ANJ_DM_RES_W,                        \
            .type = ANJ_DATA_TYPE_INT                    \
        },                                               \
        {                                                \
            .rid = 7,                                    \
            .kind = ANJ_DM_RES_RW,                       \
            .type = ANJ_DATA_TYPE_STRING                 \
        }                                                \
    };                                                   \
    anj_dm_obj_inst_t obj_insts[3] = {                   \
        {                                                \
            .iid = 0,                                    \
            .res_count = 2,                              \
            .resources = res_0                           \
        },                                               \
        {                                                \
            .iid = 1,                                    \
            .res_count = 8,                              \
            .resources = res_1                           \
        },                                               \
        {                                                \
            .iid = 2,                                    \
            .res_count = 0                               \
        }                                                \
    };                                                   \
    anj_dm_obj_t Obj = {                                 \
        .oid = 1,                                        \
        .insts = obj_insts,                              \
        .handlers = &handlers,                           \
        .max_inst_count = 3                              \
    };                                                   \
    anj_t Anj = { 0 };                                   \
    _anj_dm_initialize(&Anj);                            \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&Anj, &Obj)); \
    call_counter_begin = 0;                              \
    call_counter_end = 0;                                \
    call_counter_validate = 0;                           \
    call_counter_res_write = 0;                          \
    call_value = NULL;                                   \
    call_iid = 0;                                        \
    call_rid = 0;                                        \
    call_riid = 0;                                       \
    call_result = 4;                                     \
    call_counter_reset = 0;                              \
    call_counter_res_delete = 0;                         \
    call_counter_res_create = 0;

ANJ_UNIT_TEST(dm_write_replace, write_handler) {
    TEST_INIT(anj, obj);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_rid, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_riid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_write_replace, write_no_handler) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);

    anj_io_out_entry_t record_1 = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1),
        .value.int_value = 77777
    };
    anj_io_out_entry_t record_6 = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 6),
        .value.int_value = 88888
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_1));
    ANJ_UNIT_ASSERT_TRUE(call_value == &record_1.value);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_6));
    ANJ_UNIT_ASSERT_TRUE(call_value == &record_6.value);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_reset, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_write_replace, write_string_in_chunk) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 7);

    anj_io_out_entry_t record_1 = {
        .type = ANJ_DATA_TYPE_STRING,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 7),
        .value.bytes_or_string.data = "123",
        .value.bytes_or_string.chunk_length = 3,
    };
    anj_io_out_entry_t record_2 = {
        .type = ANJ_DATA_TYPE_STRING,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 7),
        .value.bytes_or_string.data = "ABC",
        .value.bytes_or_string.offset = 3,
        .value.bytes_or_string.chunk_length = 3
    };
    anj_io_out_entry_t record_3 = {
        .type = ANJ_DATA_TYPE_STRING,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 7),
        .value.bytes_or_string.data = "DEF",
        .value.bytes_or_string.offset = 6,
        .value.bytes_or_string.chunk_length = 3
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_1));
    ANJ_UNIT_ASSERT_TRUE(call_value == &record_1.value);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_2));
    ANJ_UNIT_ASSERT_TRUE(call_value == &record_2.value);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_3));
    ANJ_UNIT_ASSERT_TRUE(call_value == &record_3.value);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_reset, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_write_replace, multi_res_write) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 3)
    };
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_TRUE(call_rid == 4);
    ANJ_UNIT_ASSERT_TRUE(call_riid == 3);
    ANJ_UNIT_ASSERT_TRUE(call_riid_inst_delete == 3);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    call_value = NULL;
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[0], 3);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[1], ANJ_ID_INVALID);

    record.path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 2);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_TRUE(call_rid == 4);
    ANJ_UNIT_ASSERT_TRUE(call_riid == 2);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_delete, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_create, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[0], 2);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[1], 3);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[2], ANJ_ID_INVALID);
}

ANJ_UNIT_TEST(dm_write_replace, multi_res_write_create) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0)
    };
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[0], 0);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[1], ANJ_ID_INVALID);
    call_value = NULL;

    record.path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 2);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[0], 0);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[2], ANJ_ID_INVALID);
    call_value = NULL;

    record.path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 8);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[0], 0);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[2], 8);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[3], ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_write_replace, error_type) {
    TEST_INIT(anj, obj);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_BOOL,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_BAD_REQUEST);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_write_replace, error_no_writable) {
    TEST_INIT(anj, obj);
    res_1[0].kind = ANJ_DM_RES_R;
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_TRUE(anj_core_ongoing_operation(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_FALSE(anj_core_ongoing_operation(&anj));

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

// Path exist but write handler returns NOT_FOUND
ANJ_UNIT_TEST(dm_write_replace, error_path_1) {
    TEST_INIT(anj, obj);
    res_write_operation_error = ANJ_DM_ERR_NOT_FOUND;
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_NOT_FOUND);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
    res_write_operation_error = 0;
}

// The same test as above but base path points to object instance
// so error should be ignored - check _anj_dm_write_entry() for details
ANJ_UNIT_TEST(dm_write_replace, error_path_2) {
    TEST_INIT(anj, obj);
    res_write_operation_error = ANJ_DM_ERR_NOT_FOUND;
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
    res_write_operation_error = 0;
}

// Instance exists but resource does not - error should be reset
// but write_handler should not be called
ANJ_UNIT_TEST(dm_write_replace, error_path_3) {
    TEST_INIT(anj, obj);
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 12)
    };
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

// Instance does not exist - error should be propagated
ANJ_UNIT_TEST(dm_write_replace, error_path_4) {
    TEST_INIT(anj, obj);
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 7, 12)
    };
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 7);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_REPLACE,
                                                  false, &path),
                          ANJ_DM_ERR_NOT_FOUND);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_write_replace, error_path_multi_instance) {
    TEST_INIT(anj, obj);
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_write_replace, error_unauthorized) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(0, 0, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_REPLACE,
                                                  false, &path),
                          ANJ_DM_ERR_UNAUTHORIZED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    path = ANJ_MAKE_RESOURCE_PATH(21, 0, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_REPLACE,
                                                  false, &path),
                          ANJ_DM_ERR_UNAUTHORIZED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    // value was initialized to 4, so if it was not changed, the test passes
    ANJ_UNIT_ASSERT_EQUAL(call_result, 4);
}

ANJ_UNIT_TEST(dm_write_replace, handler_error) {
    TEST_INIT(anj, obj);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    res_write_operation_error = ANJ_DM_ERR_BAD_REQUEST;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_BAD_REQUEST);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 1);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);

    res_write_operation_error = 0;
}

ANJ_UNIT_TEST(dm_write_replace, handler_error_2) {
    TEST_INIT(anj, obj);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);
    res_create_operation_return_error = true;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record), -1);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);

    res_create_operation_return_error = false;
}

ANJ_UNIT_TEST(dm_write_replace, handler_error_3) {
    TEST_INIT(anj, obj);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);
    validate_return_error = true;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_validate(&anj), -12);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);

    validate_return_error = false;
}

ANJ_UNIT_TEST(dm_write_replace, lack_of_inst_reset_error) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    handlers.inst_reset = NULL;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_REPLACE,
                                                  false, &path),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    handlers.inst_reset = inst_reset;
}

static anj_dm_res_t res_0_bootstrap[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_INT
    }
};

static anj_dm_obj_inst_t obj_insts_bootstrap[2] = {
    {
        .iid = 0,
        .res_count = 1,
        .resources = res_0_bootstrap
    },
    {
        .iid = ANJ_ID_INVALID,
        .res_count = 1,
        .resources = res_0_bootstrap
    }
};

static anj_dm_obj_t Obj_Bootstrap = {
    .oid = 1,
    .insts = obj_insts_bootstrap,
    .handlers = &handlers,
    .max_inst_count = 2
};

ANJ_UNIT_TEST(dm_write_replace, write_with_create_instance_level) {
    anj_t anj = { 0 };
    _anj_dm_initialize(&anj);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &Obj_Bootstrap));

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, true, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(Obj_Bootstrap.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(Obj_Bootstrap.insts[1].iid, 1);
    obj_insts_bootstrap[1].iid = ANJ_ID_INVALID;
}

ANJ_UNIT_TEST(dm_write_replace, write_with_create_resource_level) {
    anj_t anj = { 0 };
    _anj_dm_initialize(&anj);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &Obj_Bootstrap));

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 2, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 2, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, true, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(Obj_Bootstrap.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(Obj_Bootstrap.insts[1].iid, 2);
    obj_insts_bootstrap[1].iid = ANJ_ID_INVALID;
}

ANJ_UNIT_TEST(dm_write_replace, write_with_create_write_error) {
    call_counter_res_write = 0;
    anj_t anj = { 0 };
    _anj_dm_initialize(&anj);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &Obj_Bootstrap));

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1)
    };
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, true, &path));
    // resource 1 does not exist, but in _anj_dm_write_entry(), NOT FOUND error
    // is reset
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record), 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(Obj_Bootstrap.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(Obj_Bootstrap.insts[1].iid, 1);
}

ANJ_UNIT_TEST(dm_write_replace, write_with_create_error) {
    anj_t anj = { 0 };
    _anj_dm_initialize(&anj);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &Obj_Bootstrap));
    Obj_Bootstrap.max_inst_count = 1;
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_REPLACE,
                                                  true, &path),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    Obj_Bootstrap.max_inst_count = 2;
}
