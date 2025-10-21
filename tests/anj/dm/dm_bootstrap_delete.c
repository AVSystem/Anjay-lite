/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
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

#include "../../../src/anj/dm/dm_io.h"

#include <anj_unit_test.h>

static anj_dm_res_t inst_00_res[] = {
    {
        .rid = 1,
        .kind = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_BOOL
    },
    {
        .rid = 17,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_OBJLNK
    }
};
static anj_dm_res_t inst_01_res[] = {
    {
        .rid = 1,
        .kind = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_BOOL
    },
    {
        .rid = 17,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_OBJLNK
    }
};

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

    if (obj->oid == 1) {
        out_value->int_value = 11;
    }

    if (obj->oid == 0) {
        if (iid == 0 && rid == 1) {
            out_value->bool_value = false;
        }
        if (iid == 1 && rid == 1) {
            out_value->bool_value = true;
        }
        if (iid == 0 && rid == 17) {
            out_value->objlnk.iid = 0;
            out_value->objlnk.oid = 21;
        }
        if (iid == 1 && rid == 17) {
            out_value->objlnk.iid = 1;
            out_value->objlnk.oid = 21;
        }
    }
    return 0;
}

static int inst_delete(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;

    if (obj->max_inst_count == 1) {
        uint16_t *iid_ptr = (uint16_t *) &obj->insts[0].iid;
        *iid_ptr = ANJ_ID_INVALID;
    } else if (obj->max_inst_count == 2 && obj->insts[0].iid == iid) {
        uint16_t *iid_ptr = (uint16_t *) &obj->insts[0].iid;
        *iid_ptr = obj->insts[1].iid;
        iid_ptr = (uint16_t *) &obj->insts[1].iid;
        *iid_ptr = ANJ_ID_INVALID;
    } else {
        uint16_t *iid_ptr = (uint16_t *) &obj->insts[1].iid;
        *iid_ptr = ANJ_ID_INVALID;
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

static const anj_dm_handlers_t handlers = {
    .inst_delete = inst_delete,
    .res_read = res_read,
    .res_write = res_write,
};

static anj_dm_res_t obj_1_inst_1_res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    }
};

#define DELETE_TEST_INIT()                                 \
    static anj_dm_obj_inst_t obj_0_insts[] = {             \
        {                                                  \
            .iid = 0,                                      \
            .res_count = 2,                                \
            .resources = inst_00_res                       \
        },                                                 \
        {                                                  \
            .iid = 1,                                      \
            .res_count = 2,                                \
            .resources = inst_01_res                       \
        }                                                  \
    };                                                     \
    static anj_dm_obj_t obj_0 = {                          \
        .oid = 0,                                          \
        .insts = obj_0_insts,                              \
        .max_inst_count = 2,                               \
        .handlers = &handlers                              \
    };                                                     \
    static anj_dm_obj_inst_t obj_1_insts[] = {             \
        {                                                  \
            .iid = 0,                                      \
            .res_count = 1,                                \
            .resources = obj_1_inst_1_res                  \
        }                                                  \
    };                                                     \
    static anj_dm_obj_t obj_1 = {                          \
        .oid = 1,                                          \
        .insts = obj_1_insts,                              \
        .max_inst_count = 1,                               \
        .handlers = &handlers                              \
    };                                                     \
    static anj_dm_obj_inst_t obj_3_insts[] = {             \
        {                                                  \
            .iid = 44                                      \
        }                                                  \
    };                                                     \
    static anj_dm_obj_t obj_3 = {                          \
        .oid = 3,                                          \
        .insts = obj_3_insts,                              \
        .max_inst_count = 1,                               \
        .handlers = &handlers                              \
    };                                                     \
    static anj_dm_obj_inst_t obj_21_insts[] = {            \
        {                                                  \
            .iid = 0                                       \
        },                                                 \
        {                                                  \
            .iid = 1                                       \
        }                                                  \
    };                                                     \
    static anj_dm_obj_t obj_21 = {                         \
        .oid = 21,                                         \
        .insts = obj_21_insts,                             \
        .max_inst_count = 2,                               \
        .handlers = &handlers                              \
    };                                                     \
    anj_t anj = { 0 };                                     \
    _anj_dm_initialize(&anj);                              \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_0)); \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1)); \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3)); \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_21));

#define DELETE_TEST(Path)                                                    \
    ANJ_UNIT_ASSERT_SUCCESS(                                                 \
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, true, &(Path))); \
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));               \
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

ANJ_UNIT_TEST(dm_bootstrap_delete, root) {
    DELETE_TEST_INIT();
    DELETE_TEST(ANJ_MAKE_ROOT_PATH());
    // all instances should be deleted except for the bootstrap server instance,
    // related OSCORE instance and device object instance
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(obj_0.insts[0].iid == 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
#ifdef ANJ_WITH_OSCORE
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(obj_21.insts[0].iid == 1);
#endif // ANJ_WITH_OSCORE
}

ANJ_UNIT_TEST(dm_bootstrap_delete, security_instance_0) {
    DELETE_TEST_INIT();
    DELETE_TEST(ANJ_MAKE_INSTANCE_PATH(0, 0));
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(obj_0.insts[0].iid == 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
#ifdef ANJ_WITH_OSCORE
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, 1);
#endif // ANJ_WITH_OSCORE
}

ANJ_UNIT_TEST(dm_bootstrap_delete, security_instance_1) {
    DELETE_TEST_INIT();
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, true,
                                    &ANJ_MAKE_INSTANCE_PATH(0, 1)),
            ANJ_DM_ERR_BAD_REQUEST);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
#ifdef ANJ_WITH_OSCORE
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, 1);
#endif // ANJ_WITH_OSCORE
}

ANJ_UNIT_TEST(dm_bootstrap_delete, security_obj) {
    DELETE_TEST_INIT();
    DELETE_TEST(ANJ_MAKE_OBJECT_PATH(0));
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(obj_0.insts[0].iid == 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
#ifdef ANJ_WITH_OSCORE
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, 1);
#endif // ANJ_WITH_OSCORE
}

ANJ_UNIT_TEST(dm_bootstrap_delete, server_instance) {
    DELETE_TEST_INIT();
    DELETE_TEST(ANJ_MAKE_INSTANCE_PATH(1, 0));
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
#ifdef ANJ_WITH_OSCORE
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, 1);
#endif // ANJ_WITH_OSCORE
}

ANJ_UNIT_TEST(dm_bootstrap_delete, server_obj) {
    DELETE_TEST_INIT();
    DELETE_TEST(ANJ_MAKE_OBJECT_PATH(1));
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
#ifdef ANJ_WITH_OSCORE
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, 1);
#endif // ANJ_WITH_OSCORE
}

ANJ_UNIT_TEST(dm_bootstrap_delete, device_obj) {
    DELETE_TEST_INIT();
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, true,
                                                  &ANJ_MAKE_OBJECT_PATH(3)),
                          ANJ_DM_ERR_BAD_REQUEST);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, 0);
#ifdef ANJ_WITH_OSCORE
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, 1);
#endif // ANJ_WITH_OSCORE
}

ANJ_UNIT_TEST(dm_bootstrap_delete, device_instance) {
    DELETE_TEST_INIT();
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, true,
                                    &ANJ_MAKE_INSTANCE_PATH(3, 0)),
            ANJ_DM_ERR_BAD_REQUEST);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, 0);
#ifdef ANJ_WITH_OSCORE
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, 1);
#endif // ANJ_WITH_OSCORE
}

#ifdef ANJ_WITH_OSCORE
ANJ_UNIT_TEST(dm_bootstrap_delete, oscore_obj) {
    DELETE_TEST_INIT();
    DELETE_TEST(ANJ_MAKE_OBJECT_PATH(21));
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(obj_21.insts[0].iid == 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
}

ANJ_UNIT_TEST(dm_bootstrap_delete, oscore_instance_0) {
    DELETE_TEST_INIT();
    DELETE_TEST(ANJ_MAKE_INSTANCE_PATH(21, 0));
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_TRUE(obj_21.insts[0].iid == 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
}

ANJ_UNIT_TEST(dm_bootstrap_delete, oscore_instance_1) {
    DELETE_TEST_INIT();
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DELETE, true,
                                    &ANJ_MAKE_INSTANCE_PATH(0, 1)),
            ANJ_DM_ERR_BAD_REQUEST);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_EQUAL(obj_21.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_0.insts[1].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_1.insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_3.insts[0].iid, 44);
}
#endif // ANJ_WITH_OSCORE
