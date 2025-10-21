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
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#include "../../../src/anj/dm/dm_io.h"
#include "../../../src/anj/io/io.h"

#include <anj_unit_test.h>

static int call_counter_begin;
static int call_counter_end;
static int call_counter_validate;
static int call_counter_res_write;
static int call_counter_res_create;
static anj_iid_t call_iid;
static anj_rid_t call_rid;
static anj_riid_t call_riid;
static bool inst_transaction_end_return_eror;
static bool res_write_operation_return_eror;
static bool res_create_operation_return_eror;
static const anj_res_value_t *call_value;
static anj_dm_transaction_result_t call_result;

static char string_buffer[20] = { 0 };
static size_t buffer_size = 20;
static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) anj;
    (void) obj;
    call_rid = rid;
    call_iid = iid;
    call_riid = riid;
    call_value = value;
    call_counter_res_write++;
    if (res_write_operation_return_eror) {
        return -1;
    }
    if (iid == 1 && rid == 7) {
        return anj_dm_write_string_chunked(value, string_buffer, buffer_size,
                                           NULL);
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
    if (res_create_operation_return_eror) {
        return -2;
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
    if (inst_transaction_end_return_eror) {
        return -1;
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
    (void) iid;
    (void) rid;
    (void) riid;
    (void) out_value;
    return 0;
}

static anj_dm_handlers_t handlers = {
    .transaction_begin = transaction_begin,
    .transaction_end = transaction_end,
    .transaction_validate = transaction_validate,
    .res_inst_create = res_inst_create,
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
    call_rid = 0;                                        \
    call_riid = 0;                                       \
    call_iid = 0;                                        \
    call_result = 4;

ANJ_UNIT_TEST(dm_write_update, write_handler) {
    TEST_INIT(anj, obj);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_rid, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_riid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(call_iid, 1);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_write_update, write_string_in_chunk) {
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
        .value.bytes_or_string.chunk_length = 3,
        .value.bytes_or_string.full_length_hint = 9
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_3));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 3);
    ANJ_UNIT_ASSERT_SUCCESS(strcmp(string_buffer, "123ABCDEF"));
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_write_update, multi_res_write) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1)
    };
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_EQUAL(call_rid, 4);
    ANJ_UNIT_ASSERT_EQUAL(call_riid, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_iid, 1);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    call_value = NULL;

    record.path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 3);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(call_rid, 4);
    ANJ_UNIT_ASSERT_EQUAL(call_riid, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_iid, 1);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 2);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[0], 1);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[1], 3);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[2], ANJ_ID_INVALID);
}

ANJ_UNIT_TEST(dm_write_update, multi_res_write_create) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0)
    };
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[0], 0);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[1], 1);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[2], 3);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[3], ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    call_value = NULL;

    record.path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 2);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[0], 0);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[1], 1);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[2], 2);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[3], 3);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[4], ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    call_value = NULL;

    record.path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 8);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[0], 0);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[1], 1);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[2], 2);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[3], 3);
    ANJ_UNIT_ASSERT_EQUAL(res_1[4].insts[4], 8);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_write_update, error_type) {
    TEST_INIT(anj, obj);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_BOOL,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_BAD_REQUEST);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_write_update, error_no_writable) {
    TEST_INIT(anj, obj);
    res_1[0].kind = ANJ_DM_RES_R;
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_write_update, error_path) {
    TEST_INIT(anj, obj);
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 12)
    };
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_NOT_FOUND);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_write_update, error_path_multi_instance) {
    TEST_INIT(anj, obj);
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_write_update, error_unauthorized) {
    TEST_INIT(anj, obj);

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(0, 0, 0);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &path),
            ANJ_DM_ERR_UNAUTHORIZED);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_write_update, handler_error) {
    TEST_INIT(anj, obj);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 0);
    res_write_operation_return_eror = true;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record), -1);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 1);
    ANJ_UNIT_ASSERT_TRUE(call_value == &record.value);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
    res_write_operation_return_eror = false;
}

ANJ_UNIT_TEST(dm_write_update, handler_error_2) {
    TEST_INIT(anj, obj);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_INT,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0)
    };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);
    res_create_operation_return_eror = true;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record), -2);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
    res_create_operation_return_eror = false;
}

ANJ_UNIT_TEST(dm_write_update, string_in_chunk_error) {
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
    buffer_size = 7;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record_2));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_write_entry(&anj, &record_3),
                          ANJ_DM_ERR_INTERNAL);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);

    ANJ_UNIT_ASSERT_EQUAL(call_counter_begin, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_end, 1);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_validate, 0);
    ANJ_UNIT_ASSERT_EQUAL(call_counter_res_write, 3);
    ANJ_UNIT_ASSERT_EQUAL(call_result, ANJ_DM_TRANSACTION_FAILURE);
    buffer_size = 20;
}
