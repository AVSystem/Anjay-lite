/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */
#include <stdbool.h>
#include <stddef.h>

#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>

#include "../../../../src/anj/dm/dm_integration.h"
#include "../../../../src/anj/dm/dm_io.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_BOOTSTRAP

/*
 * Base configuration:
 * - Object 0 with 2 instances, each with 2 resources: 1: BootstrapServer, 10:
 * SSID
 * - Object 1 with 1 instance, with 1 resource: 0: SSID
 * - Object 3 with 1 instance
 * first instance of Security object and Server object instances are matched
 */

static anj_dm_res_t inst_00_res[] = {
    {
        .rid = 1,
        .kind = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_BOOL
    },
    {
        .rid = 10,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    }
};
static int g_rid_00_10_value = 1;

static anj_dm_res_t obj_1_inst_1_res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    }
};
static int g_rid_0_value = 1;

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) riid;

    if (obj->oid == 1) {
        out_value->int_value = g_rid_0_value;
    } else if (obj->oid == 0) {
        if (iid == 0 && rid == 1) {
            out_value->bool_value = false;
        }
        if (iid == 1 && rid == 1) {
            out_value->bool_value = true;
        }
        if (iid == 0 && rid == 10) {
            out_value->int_value = g_rid_00_10_value;
        }
        if (iid == 1 && rid == 10) {
            out_value->int_value = 0;
        }
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

static int inst_delete(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;

    if (obj->max_inst_count == 1) {
        uint16_t *iid_ptr = (uint16_t *) &obj->insts[0].iid;
        *iid_ptr = ANJ_ID_INVALID;
    } else if (obj->max_inst_count == 3 && obj->insts[0].iid == iid) {
        uint16_t *iid_ptr = (uint16_t *) &obj->insts[0].iid;
        *iid_ptr = obj->insts[1].iid;
        iid_ptr = (uint16_t *) &obj->insts[1].iid;
        *iid_ptr = obj->insts[2].iid;
        iid_ptr = (uint16_t *) &obj->insts[2].iid;
        *iid_ptr = ANJ_ID_INVALID;
    } else if (obj->max_inst_count == 3 && obj->insts[1].iid == iid) {
        uint16_t *iid_ptr = (uint16_t *) &obj->insts[1].iid;
        *iid_ptr = obj->insts[2].iid;
        iid_ptr = (uint16_t *) &obj->insts[2].iid;
        *iid_ptr = ANJ_ID_INVALID;
    } else {
        uint16_t *iid_ptr = (uint16_t *) &obj->insts[2].iid;
        *iid_ptr = ANJ_ID_INVALID;
    }
    return 0;
}

static const anj_dm_handlers_t handlers = {
    .res_read = res_read,
    .res_write = res_write,
    .inst_delete = inst_delete
};
#    define INIT_DATA_MODEL()                                  \
        static anj_dm_obj_inst_t obj_0_insts[3] = {            \
            {                                                  \
                .iid = 0,                                      \
                .res_count = 2,                                \
                .resources = inst_00_res                       \
            },                                                 \
            {                                                  \
                .iid = 1,                                      \
                .res_count = 2,                                \
                .resources = inst_00_res                       \
            },                                                 \
            {                                                  \
                .iid = ANJ_ID_INVALID,                         \
                .res_count = 2,                                \
                .resources = inst_00_res                       \
            }                                                  \
        };                                                     \
        static anj_dm_obj_t obj_0 = {                          \
            .oid = 0,                                          \
            .insts = obj_0_insts,                              \
            .max_inst_count = 3,                               \
            .handlers = &handlers                              \
        };                                                     \
        static anj_dm_obj_inst_t obj_1_insts[3] = {            \
            {                                                  \
                .iid = 1,                                      \
                .res_count = 1,                                \
                .resources = obj_1_inst_1_res                  \
            },                                                 \
            {                                                  \
                .iid = ANJ_ID_INVALID,                         \
                .res_count = 1,                                \
                .resources = obj_1_inst_1_res                  \
            },                                                 \
            {                                                  \
                .iid = ANJ_ID_INVALID,                         \
                .res_count = 1,                                \
                .resources = obj_1_inst_1_res                  \
            }                                                  \
        };                                                     \
        static anj_dm_obj_t obj_1 = {                          \
            .oid = 1,                                          \
            .insts = obj_1_insts,                              \
            .max_inst_count = 3,                               \
            .handlers = &handlers                              \
        };                                                     \
        static anj_dm_obj_inst_t obj_3_insts[] = {             \
            {                                                  \
                .iid = 7                                       \
            }                                                  \
        };                                                     \
        static anj_dm_obj_t obj_3 = {                          \
            .oid = 3,                                          \
            .insts = obj_3_insts,                              \
            .max_inst_count = 1,                               \
            .handlers = &handlers                              \
        };                                                     \
        anj_t anj = { 0 };                                     \
        _anj_dm_initialize(&anj);                              \
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_0)); \
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1)); \
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3))

ANJ_UNIT_TEST(dm_bootstrap_handlers, validation) {
    INIT_DATA_MODEL();
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_bootstrap_validation(&anj));
}

ANJ_UNIT_TEST(dm_bootstrap_handlers, validation_no_ssid_match) {
    INIT_DATA_MODEL();
    g_rid_0_value = 2;
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_bootstrap_validation(&anj));
    g_rid_0_value = 1;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_bootstrap_validation(&anj));
}

ANJ_UNIT_TEST(dm_bootstrap_handlers, validation_no_ssid_match_security_obj) {
    INIT_DATA_MODEL();
    g_rid_00_10_value = 2;
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_bootstrap_validation(&anj));
    g_rid_00_10_value = 1;
}

ANJ_UNIT_TEST(dm_bootstrap_handlers, validation_no_server_instance) {
    INIT_DATA_MODEL();
    obj_1_insts[0].iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_bootstrap_validation(&anj));
}

ANJ_UNIT_TEST(dm_bootstrap_handlers, validation_no_security_instance) {
    INIT_DATA_MODEL();
    obj_0_insts[0].iid = ANJ_ID_INVALID;
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_bootstrap_validation(&anj));
}

#endif // ANJ_WITH_BOOTSTRAP
