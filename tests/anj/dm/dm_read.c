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

#include "../../../src/anj/dm/dm_core.h"
#include "../../../src/anj/dm/dm_io.h"
#include "../../../src/anj/io/io.h"

#include <anj_unit_test.h>

static const anj_dm_obj_t *call_obj;
static anj_iid_t called_iid;
static anj_rid_t called_rid;
static anj_riid_t called_riid;
static _anj_op_t call_operation;
static anj_res_value_t callback_value;

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    call_obj = obj;
    called_iid = iid;
    called_rid = rid;
    called_riid = riid;
    if (obj->oid == 10 && rid == 0) {
        out_value->int_value = 37;
    } else if (obj->oid == 10 && rid == 1) {
        out_value->int_value = 21;
    } else if (iid == 0) {
        *out_value = callback_value;
    } else if (rid == 0) {
        *out_value = callback_value;
    } else if (rid == 1) {
        out_value->int_value = 17;
    } else if (rid == 2) {
        out_value->int_value = 18;
    } else if (rid == 4) {
        out_value->int_value = (riid == 0) ? 33 : 44;
    } else if (rid == 5) {
        *out_value = callback_value;
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
    (void) rid;
    (void) riid;
    (void) value;
    return 0;
}

static anj_dm_res_t res_0[] = {
    {
        .rid = 0,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    },
    {
        .rid = 6,
        .operation = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT,
    }
};
static anj_riid_t res_insts[9] = { 0,
                                   1,
                                   ANJ_ID_INVALID,
                                   ANJ_ID_INVALID,
                                   ANJ_ID_INVALID,
                                   ANJ_ID_INVALID,
                                   ANJ_ID_INVALID,
                                   ANJ_ID_INVALID,
                                   ANJ_ID_INVALID };
static anj_riid_t res_insts_2[9] = { 0,
                                     ANJ_ID_INVALID,
                                     ANJ_ID_INVALID,
                                     ANJ_ID_INVALID,
                                     ANJ_ID_INVALID,
                                     ANJ_ID_INVALID,
                                     ANJ_ID_INVALID,
                                     ANJ_ID_INVALID,
                                     ANJ_ID_INVALID };
static anj_dm_res_t res_1[] = {
    {
        .rid = 0,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    },
    {
        .rid = 1,
        .operation = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_INT,
    },
    {
        .rid = 2,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    },
    {
        .rid = 3,
        .operation = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_INT,
        .max_inst_count = 0,
    },
    {
        .rid = 4,
        .operation = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_INT,
        .max_inst_count = 9,
        .insts = res_insts
    },
    {
        .rid = 5,
        .operation = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_INT,
        .max_inst_count = 9,
        .insts = res_insts_2
    }
};
static anj_dm_res_t res_4[] = {
    {
        .rid = 0,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    },
    {
        .rid = 1,
        .operation = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_INT,
    },
};

static anj_dm_res_t res_sec[] = {
    {
        .rid = 0,
        .operation = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_STRING,
    },
    {
        .rid = 1,
        .operation = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_BOOL,
    }
};
static anj_dm_res_t res21[] = {
    {
        .rid = 0,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    },
    {
        .rid = 1,
        .type = ANJ_DATA_TYPE_INT,
        .operation = ANJ_DM_RES_RW
    },
};

static anj_dm_obj_inst_t obj0_inst = {
    .iid = 0,
    .res_count = 2,
    .resources = res_sec
};

static anj_dm_obj_inst_t obj1_insts[3] = {
    {
        .iid = 0,
        .res_count = 2,
        .resources = res_0
    },
    {
        .iid = 1,
        .res_count = 6,
        .resources = res_1
    },
    {
        .iid = 2,
    }
};
static anj_dm_obj_inst_t obj2_insts[1] = {
    {
        .iid = 0,
        .res_count = 2,
        .resources = res_4
    }
};
static anj_dm_obj_inst_t insts21 = {
    .iid = 0,
    .resources = res21,
    .res_count = 2,
};

static anj_dm_handlers_t handlers = {
    .res_read = res_read,
    .res_write = res_write,
};
static anj_dm_obj_t obj0 = {
    .oid = 0,
    .insts = &obj0_inst,
    .max_inst_count = 1,
    .handlers = &handlers
};
static anj_dm_obj_t obj1 = {
    .oid = 1,
    .insts = obj1_insts,
    .handlers = &handlers,
    .max_inst_count = 3
};
static anj_dm_obj_t obj2 = {
    .oid = 10,
    .insts = obj2_insts,
    .handlers = &handlers,
    .max_inst_count = 1
};
static anj_dm_obj_t obj21 = {
    .oid = 21,
    .insts = &insts21,
    .max_inst_count = 1,
    .handlers = &handlers
};

#define READ_INIT(Anj)                                    \
    anj_t Anj = { 0 };                                    \
    _anj_dm_initialize(&Anj);                             \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&Anj, &obj0)); \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&Anj, &obj1)); \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj2)); \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj21));

#define VERIFY_ENTRY(Out, Path, Value)                         \
    ANJ_UNIT_ASSERT_TRUE(anj_uri_path_equal(&Out.path, Path)); \
    ANJ_UNIT_ASSERT_EQUAL(Out.value.int_value, Value);         \
    ANJ_UNIT_ASSERT_EQUAL(Out.type, ANJ_DATA_TYPE_INT);

ANJ_UNIT_TEST(dm_read, read_res_instance) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));

    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(1, out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    VERIFY_ENTRY(record, &path, 33);

    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));

    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(1, out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    VERIFY_ENTRY(record, &path, 44);

    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 5, 0);
    callback_value.int_value = 222;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));

    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(1, out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    ;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    VERIFY_ENTRY(record, &path, 222);

    ANJ_UNIT_ASSERT_TRUE(call_obj == &obj1);
    ANJ_UNIT_ASSERT_TRUE(called_iid == 1);
    ANJ_UNIT_ASSERT_TRUE(called_rid == 5);
    ANJ_UNIT_ASSERT_TRUE(called_riid == 0);
}

ANJ_UNIT_TEST(dm_read, read_res_error) {
    READ_INIT(anj);

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(2, 1, 4, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj), ANJ_DM_ERR_NOT_FOUND);
    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 2, 4, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj), ANJ_DM_ERR_NOT_FOUND);
    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 6, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj), ANJ_DM_ERR_NOT_FOUND);
    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 4);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj), ANJ_DM_ERR_NOT_FOUND);
    path = ANJ_MAKE_RESOURCE_PATH(1, 0, 6);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false,
                                                  &path),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
}

ANJ_UNIT_TEST(dm_read, empty_read) {
    READ_INIT(anj);
    size_t out_res_count = 0;
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 3);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));
    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(0, out_res_count);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
}

ANJ_UNIT_TEST(dm_read, read_res) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(1, 1, 4);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));
    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(2, out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0), 33);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1), 44);

    path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));
    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(1, out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    VERIFY_ENTRY(record, &path, 17);
}

ANJ_UNIT_TEST(dm_read, read_inst) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    callback_value.int_value = 999;
    anj_uri_path_t path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));
    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 6);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 0), 999);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 1), 17);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 2), 18);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0), 33);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1), 44);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 5, 0), 999);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    callback_value.int_value = 7;
    path = ANJ_MAKE_INSTANCE_PATH(1, 0);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));
    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 1);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 0, 0), 7);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
}

ANJ_UNIT_TEST(dm_read, read_obj) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    callback_value.int_value = 225;
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));
    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 7);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 0, 0), 225);
    callback_value.int_value = 7;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 0), 7);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 1), 17);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 2), 18);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0), 33);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1), 44);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 5, 0), 7);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
}

ANJ_UNIT_TEST(dm_read, read_obj_error) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    callback_value.int_value = 225;
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false,
                                                  &path),
                          ANJ_DM_ERR_UNAUTHORIZED);

    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj), ANJ_DM_ERR_UNAUTHORIZED);
    path = ANJ_MAKE_OBJECT_PATH(21);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false,
                                                  &path),
                          ANJ_DM_ERR_UNAUTHORIZED);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj), ANJ_DM_ERR_UNAUTHORIZED);
}

ANJ_UNIT_TEST(dm_read, bootstrap_read_obj) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    callback_value.int_value = 225;
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, true, &path));
    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 7);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 0, 0), 225);
    callback_value.int_value = 7;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 0), 7);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 1), 17);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 2), 18);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0), 33);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1), 44);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 5, 0), 7);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
}

ANJ_UNIT_TEST(dm_read, bootstrap_read_obj_error) {
    READ_INIT(anj);

    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(3);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, true,
                                                  &path),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    path = ANJ_MAKE_OBJECT_PATH(2);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, true,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj), ANJ_DM_ERR_NOT_FOUND);
    path = ANJ_MAKE_INSTANCE_PATH(1, 4);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, true,
                                                  &path),
                          ANJ_DM_ERR_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj), ANJ_DM_ERR_NOT_FOUND);
    path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, true,
                                                  &path),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    path = ANJ_MAKE_OBJECT_PATH(0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, true,
                                                  &path),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, true, &path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
}

ANJ_UNIT_TEST(dm_read, get_res_val) {
    READ_INIT(anj);
    anj_res_value_t out_value;

    callback_value.int_value = 3333;
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));
    path = ANJ_MAKE_RESOURCE_PATH(1, 0, 0);
    anj_data_type_t type = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_get_resource_value(&anj, &path, &out_value, &type, NULL));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(out_value.int_value, 3333);
    path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_res_read(&anj, &path, &out_value));
    ANJ_UNIT_ASSERT_EQUAL(out_value.int_value, 17);
    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_res_read(&anj, &path, &out_value));
    ANJ_UNIT_ASSERT_EQUAL(out_value.int_value, 33);
    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 5, 0);
    callback_value.int_value = 3331;
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_res_read(&anj, &path, &out_value));
    ANJ_UNIT_ASSERT_EQUAL(out_value.int_value, 3331);

    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 5, 2);
    ANJ_UNIT_ASSERT_EQUAL(anj_dm_res_read(&anj, &path, &out_value),
                          ANJ_DM_ERR_NOT_FOUND);
    path = ANJ_MAKE_RESOURCE_PATH(1, 1, 8);
    ANJ_UNIT_ASSERT_EQUAL(anj_dm_res_read(&anj, &path, &out_value),
                          ANJ_DM_ERR_NOT_FOUND);
    path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_resource_value(&anj, &path, &out_value,
                                                     NULL, NULL),
                          ANJ_DM_ERR_BAD_REQUEST);
    path = ANJ_MAKE_OBJECT_PATH(2);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_resource_value(&anj, &path, &out_value,
                                                     NULL, NULL),
                          ANJ_DM_ERR_BAD_REQUEST);
    path = ANJ_MAKE_RESOURCE_PATH(1, 1, 5);
    ANJ_UNIT_ASSERT_EQUAL(anj_dm_res_read(&anj, &path, &out_value),
                          ANJ_DM_ERR_BAD_REQUEST);
    path = ANJ_MAKE_RESOURCE_PATH(1, 0, 6);
    ANJ_UNIT_ASSERT_EQUAL(anj_dm_res_read(&anj, &path, &out_value),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
}

ANJ_UNIT_TEST(dm_read, get_res_type) {
    READ_INIT(anj);
    anj_data_type_t out_type = 0;
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));
    path = ANJ_MAKE_RESOURCE_PATH(1, 0, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_resource_type(&anj, &path, &out_type));
    ANJ_UNIT_ASSERT_EQUAL(ANJ_DATA_TYPE_INT, out_type);
    path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 5, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_resource_type(&anj, &path, &out_type));
    ANJ_UNIT_ASSERT_EQUAL(ANJ_DATA_TYPE_INT, out_type);

    path = ANJ_MAKE_RESOURCE_PATH(1, 1, 8);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_resource_type(&anj, &path, &out_type),
                          ANJ_DM_ERR_NOT_FOUND);
    path = ANJ_MAKE_INSTANCE_PATH(1, 1);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_resource_type(&anj, &path, &out_type),
                          ANJ_DM_ERR_BAD_REQUEST);
    path = ANJ_MAKE_OBJECT_PATH(2);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_resource_type(&anj, &path, &out_type),
                          ANJ_DM_ERR_BAD_REQUEST);
}

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
ANJ_UNIT_TEST(dm_read, composite_read) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    callback_value.int_value = 755;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ_COMP, false, NULL));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_count_readable_res_if_allowed(
            &anj, &ANJ_MAKE_INSTANCE_PATH(1, 0), &out_res_count));
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_count_readable_res_if_allowed(
            &anj, &ANJ_MAKE_INSTANCE_PATH(1, 1), &out_res_count));
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 6);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_count_readable_res_if_allowed(
            &anj, &ANJ_MAKE_ROOT_PATH(), &out_res_count));
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 9);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_composite_next_path(&anj, &ANJ_MAKE_INSTANCE_PATH(1, 0)),
            0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 0, 0), 755);
    callback_value.int_value = 7;
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_composite_next_path(&anj, &ANJ_MAKE_INSTANCE_PATH(1, 1)),
            0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 0), 7);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 1), 17);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 2), 18);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0), 33);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1), 44);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 5, 0), 7);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_composite_next_path(&anj, &ANJ_MAKE_OBJECT_PATH(10)), 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(10, 0, 0), 37);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(10, 0, 1), 21);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
}

ANJ_UNIT_TEST(dm_read, composite_read_non_existing_path) {
    READ_INIT(anj);
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ_COMP, false, NULL));
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_composite_next_path(&anj, &ANJ_MAKE_RESOURCE_PATH(1, 0, 6)),
            ANJ_DM_ERR_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_end(&anj),
                          ANJ_DM_ERR_METHOD_NOT_ALLOWED);
}

ANJ_UNIT_TEST(dm_read, composite_read_no_records) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ_COMP, false, NULL));
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_composite_next_path(&anj, &ANJ_MAKE_INSTANCE_PATH(1, 2)),
            _ANJ_DM_NO_RECORD);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
}

ANJ_UNIT_TEST(dm_read, composite_read_no_instances) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    obj0.max_inst_count = 0;
    obj1.max_inst_count = 0;
    obj2.max_inst_count = 0;
    obj21.max_inst_count = 0;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ_COMP, false, NULL));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_count_readable_res_if_allowed(
            &anj, &ANJ_MAKE_ROOT_PATH(), &out_res_count));
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_composite_next_path(&anj,
                                                      &ANJ_MAKE_ROOT_PATH()),
                          _ANJ_DM_NO_RECORD);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    obj0.max_inst_count = 1;
    obj1.max_inst_count = 3;
    obj2.max_inst_count = 1;
    obj21.max_inst_count = 1;
}

ANJ_UNIT_TEST(dm_read, composite_read_non_null_path) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_READ_COMP, false, &ANJ_MAKE_ROOT_PATH()));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_composite_next_path(&anj, &ANJ_MAKE_INSTANCE_PATH(1, 1)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_count_readable_res_if_allowed(
            &anj, &ANJ_MAKE_INSTANCE_PATH(1, 1), &out_res_count));
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 6);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    anj_uri_path_t path = {
        .ids = { 0, 0, 0, 0 },
        .uri_len = 0
    };

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ_COMP, false, &path));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_composite_next_path(&anj, &ANJ_MAKE_INSTANCE_PATH(1, 1)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_count_readable_res_if_allowed(
            &anj, &ANJ_MAKE_INSTANCE_PATH(1, 1), &out_res_count));
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 6);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
}

ANJ_UNIT_TEST(dm_read, composite_root_read_empty_objects) {
    READ_INIT(anj);
    anj_io_out_entry_t record = { 0 };
    size_t out_res_count = 0;

    callback_value.int_value = 755;

    anj_dm_obj_t obj5 = {
        .oid = 5,
        .max_inst_count = 0
    };
    anj_dm_obj_t obj6 = {
        .oid = 6,
        .max_inst_count = 0
    };
    anj_dm_obj_t obj15 = {
        .oid = 15,
        .max_inst_count = 0
    };

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj5));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj6));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj15));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ_COMP, false, NULL));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_count_readable_res_if_allowed(
            &anj, &ANJ_MAKE_ROOT_PATH(), &out_res_count));
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 9);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_composite_next_path(&anj, &ANJ_MAKE_ROOT_PATH()));
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);

    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 0, 0), 755);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 0), 755);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 1), 17);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(1, 1, 2), 18);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 0), 33);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 4, 1), 44);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 5, 0), 755);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record), 0);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(10, 0, 0), 37);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &record),
                          _ANJ_DM_LAST_RECORD);
    VERIFY_ENTRY(record, &ANJ_MAKE_RESOURCE_PATH(10, 0, 1), 21);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
}
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
