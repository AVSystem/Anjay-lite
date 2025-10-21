/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#include "../../../src/anj/dm/dm_io.h"

#include <anj_unit_test.h>

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) riid;

    if (obj->oid == 1) {
        if (iid == 1) {
            out_value->int_value = 11;
        } else {
            out_value->int_value = 22;
        }
    } else if (obj->oid == 0) {
        if (iid == 0 && rid == 0) {
            out_value->bytes_or_string.data = "DDD";
        }
        if (iid == 0 && rid == 1) {
            out_value->bool_value = true;
        }
        if (iid == 0 && rid == 10) {
            out_value->int_value = 99;
        }
        if (iid == 1 && rid == 0) {
            out_value->bytes_or_string.data = "SSS";
        }
        if (iid == 1 && rid == 1) {
            out_value->bool_value = false;
        }
        if (iid == 1 && rid == 10) {
            out_value->int_value = 199;
        }
        if (rid == 17) {
            out_value->objlnk.oid = 21;
            out_value->objlnk.iid = 0;
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
static const anj_dm_handlers_t handlers = {
    .res_read = res_read,
    .res_write = res_write,
};

static anj_dm_res_t inst_00_res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_STRING
    },
    {
        .rid = 1,
        .kind = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_BOOL
    },
    {
        .rid = 10,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 17,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_OBJLNK
    }
};
static anj_dm_res_t inst_01_res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_STRING
    },
    {
        .rid = 1,
        .kind = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_BOOL
    },
    {
        .rid = 10,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 17,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_OBJLNK
    }
};

static anj_dm_obj_inst_t obj_0_insts[] = {
    {
        .iid = 0,
        .res_count = 4,
        .resources = inst_00_res
    },
    {
        .iid = 1,
        .res_count = 4,
        .resources = inst_01_res
    },
};
static anj_dm_obj_t obj_0 = {
    .oid = 0,
    .insts = obj_0_insts,
    .max_inst_count = 2,
    .handlers = &handlers
};

static anj_dm_res_t inst_1_res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    },
    {
        .rid = 1,
        .kind = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT,
    }
};
static anj_dm_res_t inst_2_res[] = {
    {
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    },
    {
        .rid = 1,
        .kind = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT,
    }
};

static anj_dm_obj_inst_t obj_1_insts[] = {
    {
        .iid = 1,
        .res_count = 2,
        .resources = inst_1_res
    },
    {
        .iid = 2,
        .res_count = 2,
        .resources = inst_2_res
    }
};
static anj_dm_obj_t obj_1 = {
    .oid = 1,
    .version = "1.1",
    .insts = obj_1_insts,
    .max_inst_count = 2,
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
    .handlers = &handlers,
    .max_inst_count = 1
};
static anj_dm_obj_t obj_5 = {
    .oid = 5
};
static anj_dm_obj_t obj_55 = {
    .oid = 55,
    .version = "1.2"
};

static anj_dm_obj_inst_t obj_21_insts[1] = {
    {
        .iid = 0
    }
};
static anj_dm_obj_t obj_21 = {
    .oid = 21,
    .insts = obj_21_insts,
    .max_inst_count = 1,
    .handlers = &handlers,
};

typedef struct {
    anj_uri_path_t path;
    const char *version;
    const uint16_t ssid;
    const char *uri;
} boot_discover_record_t;

#define BOOTSTRAP_DISCOVER_TEST(Path, Idx_start, Idx_end)                     \
    anj_t anj = { 0 };                                                        \
    _anj_dm_initialize(&anj);                                                 \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_0));                    \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1));                    \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));                    \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_5));                    \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_55));                   \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_21));                   \
    ANJ_UNIT_ASSERT_SUCCESS(                                                  \
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_DISCOVER, true, &Path));  \
    for (size_t idx = Idx_start; idx <= Idx_end; idx++) {                     \
        anj_uri_path_t out_path;                                              \
        const char *out_version;                                              \
        const uint16_t *out_ssid;                                             \
        const char *out_uri;                                                  \
        int res = _anj_dm_get_bootstrap_discover_record(                      \
                &anj, &out_path, &out_version, &out_ssid, &out_uri);          \
        ANJ_UNIT_ASSERT_TRUE(                                                 \
                anj_uri_path_equal(&out_path, &boot_disc_records[idx].path)); \
        if (out_version && boot_disc_records[idx].version) {                  \
            ANJ_UNIT_ASSERT_SUCCESS(                                          \
                    strcmp(out_version, boot_disc_records[idx].version));     \
        } else {                                                              \
            ANJ_UNIT_ASSERT_TRUE(!out_version                                 \
                                 && !boot_disc_records[idx].version);         \
        }                                                                     \
        if (out_ssid && boot_disc_records[idx].ssid) {                        \
            ANJ_UNIT_ASSERT_TRUE(*out_ssid == boot_disc_records[idx].ssid);   \
        } else {                                                              \
            ANJ_UNIT_ASSERT_TRUE(!out_ssid && !boot_disc_records[idx].ssid);  \
        }                                                                     \
        if (out_uri && boot_disc_records[idx].uri) {                          \
            ANJ_UNIT_ASSERT_SUCCESS(                                          \
                    strcmp(out_uri, boot_disc_records[idx].uri));             \
        } else {                                                              \
            ANJ_UNIT_ASSERT_TRUE(!out_uri && !boot_disc_records[idx].uri);    \
        }                                                                     \
        if (idx == Idx_end) {                                                 \
            ANJ_UNIT_ASSERT_EQUAL(res, _ANJ_DM_LAST_RECORD);                  \
        } else {                                                              \
            ANJ_UNIT_ASSERT_EQUAL(res, 0);                                    \
        }                                                                     \
    }                                                                         \
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

/**
 * 0:
 *    0
 *       0 "DDD"
 *       1 true
 *       10 99
 *       17 21:0
 *    1
 *       0 "SSS"
 *       1 false
 *       10 199
 *       17 21:0
 * 1: version = "1.1"
 *    1
 *       0 SSID = 11
 *       1
 *    2
 *       0 SSID = 22
 *       1
 * 3:
 *    0
 * 5
 * 21:
 *    0
 * 55: version = "1.2"
 */
// clang-format off
static boot_discover_record_t boot_disc_records [12] = {
    {.path = ANJ_MAKE_OBJECT_PATH(0)},
    {.path = ANJ_MAKE_INSTANCE_PATH(0,0)},
    {.path = ANJ_MAKE_INSTANCE_PATH(0,1), .ssid = 199, .uri = "SSS"},
    {.path = ANJ_MAKE_OBJECT_PATH(1), .version = "1.1"},
    {.path = ANJ_MAKE_INSTANCE_PATH(1,1), .ssid = 11},
    {.path = ANJ_MAKE_INSTANCE_PATH(1,2), .ssid = 22},
    {.path = ANJ_MAKE_OBJECT_PATH(3)},
    {.path = ANJ_MAKE_INSTANCE_PATH(3,0)},
    {.path = ANJ_MAKE_OBJECT_PATH(5)},
    {.path = ANJ_MAKE_OBJECT_PATH(21)},
#ifdef ANJ_WITH_OSCORE
    {.path = ANJ_MAKE_INSTANCE_PATH(21,0), .ssid = 199},
#else 
    {.path = ANJ_MAKE_INSTANCE_PATH(21,0)},
#endif // ANJ_WITH_OSCORE
    {.path = ANJ_MAKE_OBJECT_PATH(55), .version = "1.2"}
};
// clang-format on

ANJ_UNIT_TEST(dm_bootstrap_discover, root) {
    anj_uri_path_t path = ANJ_MAKE_ROOT_PATH();
    BOOTSTRAP_DISCOVER_TEST(path, 0, 11);
}

ANJ_UNIT_TEST(dm_bootstrap_discover, object_0) {
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(0);
    BOOTSTRAP_DISCOVER_TEST(path, 0, 2);
}

ANJ_UNIT_TEST(dm_bootstrap_discover, object_1) {
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(1);
    BOOTSTRAP_DISCOVER_TEST(path, 3, 5);
}

ANJ_UNIT_TEST(dm_bootstrap_discover, object_3) {
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(3);
    BOOTSTRAP_DISCOVER_TEST(path, 6, 7);
}

ANJ_UNIT_TEST(dm_bootstrap_discover, object_5) {
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(5);
    BOOTSTRAP_DISCOVER_TEST(path, 8, 8);
}

ANJ_UNIT_TEST(dm_bootstrap_discover, object_21) {
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(21);
    BOOTSTRAP_DISCOVER_TEST(path, 9, 10);
}

ANJ_UNIT_TEST(dm_bootstrap_discover, object_55) {
    anj_uri_path_t path = ANJ_MAKE_OBJECT_PATH(55);
    BOOTSTRAP_DISCOVER_TEST(path, 11, 11);
}
