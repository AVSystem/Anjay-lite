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
#include <stdint.h>

#define ANJ_UNIT_ENABLE_SHORT_ASSERTS
#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#include "../../../../src/anj/coap/coap.h"
#include "../../../../src/anj/dm/dm_io.h"
#include "../../../../src/anj/exchange.h"
#include "../../../../src/anj/observe/observe.h"
#include "../../../../src/anj/observe/observe_internal.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_OBSERVE

static double get_res_value_double = 0;
static bool get_res_value_bool = 0;
static int res_read_ret_val = 0;

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

    if (rid == 1) {
        out_value->bool_value = get_res_value_bool;
    } else {
        out_value->double_value = get_res_value_double;
    }
    return res_read_ret_val;
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
static anj_dm_handlers_t handlers = {
    .res_read = res_read,
    .res_write = res_write,
};
static anj_dm_res_t inst_0_res[] = {
    {
        .rid = 1,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_DOUBLE,
    },
    {
        .rid = 2,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_BOOL,
    },
    {
        .rid = 3,
        .kind = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_INT,
        .max_inst_count = 1,
        .insts = (anj_riid_t[]){ 0 }
    },
    {
        .rid = 4,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    },
    {
        .rid = 5,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_STRING,
    },
    {
        .rid = 6,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_UINT,
    },
    {
        .rid = 7,
        .kind = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_UINT,
    }
};
static anj_dm_obj_inst_t inst_0 = {
    .iid = 0,
    .res_count = 7,
    .resources = inst_0_res
};
static anj_dm_obj_t obj_3 = {
    .oid = 3,
    .handlers = &handlers,
    .insts = &inst_0,
    .max_inst_count = 1
};

#    define TEST_INIT()           \
        anj_t anj = { 0 };        \
        _anj_observe_init(&anj);  \
        _anj_dm_initialize(&anj); \
        ASSERT_OK(anj_dm_add_obj(&anj, &obj_3));

ANJ_UNIT_TEST(write_attr, attach_to_riid) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_less_than = true,
    };
    ASSERT_OK(_anj_observe_attributes_apply_condition(
            &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 3, 0), &attr));
}

#    ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(write_attr, types_mismatch_edge_on_int) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_edge = true,
    };
    ASSERT_EQ(_anj_observe_attributes_apply_condition(
                      &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 4), &attr),
              ANJ_COAP_CODE_BAD_REQUEST);
}
#    endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(write_attr, types_mismatch_step_on_bool) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_step = true,
    };
    ASSERT_EQ(_anj_observe_attributes_apply_condition(
                      &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 2), &attr),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(write_attr, types_mismatch_gt_on_string) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_greater_than = true,
    };
    ASSERT_EQ(_anj_observe_attributes_apply_condition(
                      &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 5), &attr),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(write_attr, types_ok_lt_on_uint) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_less_than = true,
    };
    ASSERT_OK(_anj_observe_attributes_apply_condition(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 6), &attr));
}

ANJ_UNIT_TEST(write_attr, types_ok_lt_on_int) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_less_than = true,
    };
    ASSERT_OK(_anj_observe_attributes_apply_condition(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 4), &attr));
}

ANJ_UNIT_TEST(write_attr, types_ok_lt_on_double) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_less_than = true,
    };
    ASSERT_OK(_anj_observe_attributes_apply_condition(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1), &attr));
}

static void compare_attr(_anj_attr_notification_t *attr1,
                         _anj_attr_notification_t *attr2) {
    ASSERT_EQ(attr1->has_less_than, attr2->has_less_than);
    if (attr1->has_less_than) {
        ASSERT_EQ(attr1->less_than, attr2->less_than);
    }
    ASSERT_EQ(attr1->has_greater_than, attr2->has_greater_than);
    if (attr1->has_greater_than) {
        ASSERT_EQ(attr1->greater_than, attr2->greater_than);
    }
    ASSERT_EQ(attr1->has_step, attr2->has_step);
    if (attr1->has_step) {
        ASSERT_EQ(attr1->step, attr2->step);
    }
    ASSERT_EQ(attr1->has_min_period, attr2->has_min_period);
    if (attr1->has_min_period) {
        ASSERT_EQ(attr1->min_period, attr2->min_period);
    }
    ASSERT_EQ(attr1->has_max_period, attr2->has_max_period);
    if (attr1->has_max_period) {
        ASSERT_EQ(attr1->max_period, attr2->max_period);
    }
    ASSERT_EQ(attr1->has_min_eval_period, attr2->has_min_eval_period);
    if (attr1->has_min_eval_period) {
        ASSERT_EQ(attr1->min_eval_period, attr2->min_eval_period);
    }
    ASSERT_EQ(attr1->has_max_eval_period, attr2->has_max_eval_period);
    if (attr1->has_max_eval_period) {
        ASSERT_EQ(attr1->max_eval_period, attr2->max_eval_period);
    }
#    ifdef ANJ_WITH_LWM2M12
    ASSERT_EQ(attr1->has_edge, attr2->has_edge);
    if (attr1->has_edge) {
        ASSERT_EQ(attr1->edge, attr2->edge);
    }
    ASSERT_EQ(attr1->has_con, attr2->has_con);
    if (attr1->has_con) {
        ASSERT_EQ(attr1->con, attr2->con);
    }
    ASSERT_EQ(attr1->has_hqmax, attr2->has_hqmax);
    if (attr1->has_hqmax) {
        ASSERT_EQ(attr1->hqmax, attr2->hqmax);
    }
#    endif // ANJ_WITH_LWM2M12
}

#    define WRITE_ATTR_TEST(Path, Attr, Msg_Code)                             \
        int result = Msg_Code == ANJ_COAP_CODE_CHANGED ? 0 : -1;              \
        _anj_observe_server_state_t srv = {                                   \
            .ssid = 1                                                         \
        };                                                                    \
        _anj_exchange_handlers_t out_handlers = { 0 };                        \
        _anj_exchange_init(&anj.exchange_ctx);                                \
        _anj_coap_msg_t inout_msg = {                                         \
            .operation = ANJ_OP_DM_WRITE_ATTR,                                \
            .uri = Path,                                                      \
            .attr.notification_attr = Attr,                                   \
            .payload_size = 0,                                                \
        };                                                                    \
        inout_msg.coap_binding_data.message_id = 0x1111;                      \
        inout_msg.token.size = 1;                                             \
        inout_msg.token.bytes[0] = 0x22;                                      \
        uint8_t response_code;                                                \
        uint8_t out_buff[100];                                                \
        size_t out_buff_len = sizeof(out_buff);                               \
        uint8_t payload_buff[100];                                            \
        size_t payload_buff_len = sizeof(payload_buff);                       \
        ASSERT_FALSE(anj_core_ongoing_operation(&anj));                       \
        ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv,         \
                                           &inout_msg, &response_code),       \
                  result);                                                    \
        ASSERT_EQ(_anj_exchange_new_server_request(                           \
                          &anj.exchange_ctx, response_code, &inout_msg,       \
                          &out_handlers, payload_buff, payload_buff_len),     \
                  ANJ_EXCHANGE_STATE_MSG_TO_SEND);                            \
        size_t out_msg_size = 0;                                              \
        /* Write-Attributes can't be blockwise, so the flag is not set */     \
        ASSERT_FALSE(anj_core_ongoing_operation(&anj));                       \
        ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len,    \
                                       &out_msg_size));                       \
        uint8_t EXPECTED[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */    \
                             "\xFF\x11\x11\x22";                              \
        EXPECTED[1] = Msg_Code;                                               \
        ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED, sizeof(EXPECTED) - 1);      \
        ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(EXPECTED) - 1);            \
        ASSERT_EQ(_anj_exchange_process(&anj.exchange_ctx,                    \
                                        ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, \
                                        &inout_msg),                          \
                  ANJ_EXCHANGE_STATE_FINISHED);                               \
        ASSERT_FALSE(anj_core_ongoing_operation(&anj));

ANJ_UNIT_TEST(write_attr, write_basic) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                    ((_anj_attr_notification_t) {
                        .has_less_than = true,
                        .less_than = 10,
                    }),
                    ANJ_COAP_CODE_CHANGED);
    compare_attr(&_anj_observe_get_attr_from_path(
                          &anj.observe_ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1), 1)
                          ->attr,
                 &inout_msg.attr.notification_attr);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 1)));
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 1);
}

ANJ_UNIT_TEST(write_attr, write_basic_instance) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_INSTANCE_PATH(3, 0),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = 10,
                    }),
                    ANJ_COAP_CODE_CHANGED);
    compare_attr(&_anj_observe_get_attr_from_path(
                          &anj.observe_ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0), 1)
                          ->attr,
                 &inout_msg.attr.notification_attr);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[0].path,
                                   &ANJ_MAKE_INSTANCE_PATH(3, 0)));
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 1);
}

ANJ_UNIT_TEST(write_attr, write_basic_object) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_OBJECT_PATH(3),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = 10,
                    }),
                    ANJ_COAP_CODE_CHANGED);
    compare_attr(&_anj_observe_get_attr_from_path(&anj.observe_ctx,
                                                  &ANJ_MAKE_OBJECT_PATH(3), 1)
                          ->attr,
                 &inout_msg.attr.notification_attr);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[0].path,
                                   &ANJ_MAKE_OBJECT_PATH(3)));
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 1);
}

ANJ_UNIT_TEST(write_attr, write_all_attributes_except_edge) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                    ((_anj_attr_notification_t) {
                        .has_less_than = true,
                        .less_than = 10,
                        .has_greater_than = true,
                        .greater_than = 15,
                        .has_step = true,
                        .step = 1,
                        .has_min_period = true,
                        .min_period = 1,
                        .has_max_period = true,
                        .max_period = 2,
                        .has_min_eval_period = true,
                        .min_eval_period = 3,
                        .has_max_eval_period = true,
                        .max_eval_period = 4,
#    ifdef ANJ_WITH_LWM2M12
                        .has_con = true,
                        .con = 1,
                        .has_hqmax = true,
                        .hqmax = 7,
#    endif // ANJ_WITH_LWM2M12
                    }),
                    ANJ_COAP_CODE_CHANGED);
    compare_attr(&_anj_observe_get_attr_from_path(
                          &anj.observe_ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1), 1)
                          ->attr,
                 &inout_msg.attr.notification_attr);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 1)));
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[1].ssid, 0);
}

ANJ_UNIT_TEST(write_attr, write_all_attributes_possible_with_edge) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 0, 2),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = 1,
                        .has_max_period = true,
                        .max_period = 2,
                        .has_min_eval_period = true,
                        .min_eval_period = 3,
                        .has_max_eval_period = true,
                        .max_eval_period = 4,
#    ifdef ANJ_WITH_LWM2M12
                        .has_edge = true,
                        .edge = 1,
                        .has_con = true,
                        .con = 1,
                        .has_hqmax = true,
                        .hqmax = 7,
#    endif // ANJ_WITH_LWM2M12
                    }),
                    ANJ_COAP_CODE_CHANGED);
    compare_attr(&_anj_observe_get_attr_from_path(
                          &anj.observe_ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 2), 1)
                          ->attr,
                 &inout_msg.attr.notification_attr);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 2)));
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[1].ssid, 0);
}

ANJ_UNIT_TEST(write_attr, write_attr_bad_request) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 0, 5),
                    ((_anj_attr_notification_t) {
                        .has_less_than = true,
                        .less_than = 10,
                    }),
                    ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 0);
}

ANJ_UNIT_TEST(write_attr, write_attr_bad_request_2) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_INSTANCE_PATH(3, 0),
                    ((_anj_attr_notification_t) {
                        .has_less_than = true,
                        .less_than = 10,
                    }),
                    ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 0);
}

ANJ_UNIT_TEST(write_attr, write_attr_empty_attr_no_record) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                    ((_anj_attr_notification_t) {}), ANJ_COAP_CODE_CHANGED);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 0);
}

ANJ_UNIT_TEST(write_attr, write_attr_not_found) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 0, 8),
                    ((_anj_attr_notification_t) {
                        .has_less_than = true,
                        .less_than = 10,
                    }),
                    ANJ_COAP_CODE_NOT_FOUND);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 0);
}

ANJ_UNIT_TEST(write_attr, write_attr_not_allowed) {
    TEST_INIT();
    uint16_t temp = inst_0.res_count;
    inst_0.res_count = 0;
    WRITE_ATTR_TEST(ANJ_MAKE_INSTANCE_PATH(3, 0),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = 10,
                    }),
                    ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 0);
    inst_0.res_count = temp;
}

ANJ_UNIT_TEST(write_attr, write_attr_not_allowed_no_readable) {
    TEST_INIT();
    WRITE_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 0, 7),
                    ((_anj_attr_notification_t) {
                        .has_less_than = true,
                        .less_than = 10,
                    }),
                    ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 0);
}

ANJ_UNIT_TEST(write_attr, write_attr_second_record) {
    TEST_INIT();
    anj.observe_ctx.attributes_storage[0].attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 20,
    };
    anj.observe_ctx.attributes_storage[0].ssid = 2;
    anj.observe_ctx.attributes_storage[0].path =
            ANJ_MAKE_RESOURCE_PATH(3, 0, 1);

    WRITE_ATTR_TEST(ANJ_MAKE_INSTANCE_PATH(3, 0),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = 10,
                    }),
                    ANJ_COAP_CODE_CHANGED);
    compare_attr(&_anj_observe_get_attr_from_path(
                          &anj.observe_ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0), 1)
                          ->attr,
                 &inout_msg.attr.notification_attr);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[1].path,
                                   &ANJ_MAKE_INSTANCE_PATH(3, 0)));
    ASSERT_EQ(anj.observe_ctx.attributes_storage[1].ssid, 1);

    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 2);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 1)));
    compare_attr(&_anj_observe_get_attr_from_path(
                          &anj.observe_ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1), 2)
                          ->attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 20,
                 });
}

ANJ_UNIT_TEST(write_attr, write_attr_second_record_same_path_different_ssid) {
    TEST_INIT();
    anj.observe_ctx.attributes_storage[0].attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 20,
    };
    anj.observe_ctx.attributes_storage[0].ssid = 2;
    anj.observe_ctx.attributes_storage[0].path =
            ANJ_MAKE_RESOURCE_PATH(3, 0, 1);

    WRITE_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = 10,
                    }),
                    ANJ_COAP_CODE_CHANGED);
    compare_attr(&_anj_observe_get_attr_from_path(
                          &anj.observe_ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1), 1)
                          ->attr,
                 &inout_msg.attr.notification_attr);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[1].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 1)));
    ASSERT_EQ(anj.observe_ctx.attributes_storage[1].ssid, 1);

    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 2);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 1)));
    compare_attr(&_anj_observe_get_attr_from_path(
                          &anj.observe_ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1), 2)
                          ->attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 20,
                 });
}

ANJ_UNIT_TEST(write_attr, write_attr_no_space) {
    TEST_INIT();
    anj.observe_ctx.attributes_storage[0].ssid = 2;
    anj.observe_ctx.attributes_storage[1].ssid = 2;
    anj.observe_ctx.attributes_storage[2].ssid = 2;
    anj.observe_ctx.attributes_storage[3].ssid = 2;
    anj.observe_ctx.attributes_storage[4].ssid = 2;

    WRITE_ATTR_TEST(ANJ_MAKE_INSTANCE_PATH(3, 0),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = 10,
                    }),
                    ANJ_COAP_CODE_INTERNAL_SERVER_ERROR);

    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[1].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[2].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[3].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[4].ssid, 2);
}

ANJ_UNIT_TEST(write_attr, write_attr_remove_some_attr) {
    TEST_INIT();
    anj.observe_ctx.attributes_storage[0].ssid = 1;
    anj.observe_ctx.attributes_storage[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.attributes_storage[0].attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 20,
        .has_max_period = true,
        .max_period = 50,
        .has_step = true,
        .step = 1.5
    };
    WRITE_ATTR_TEST(ANJ_MAKE_INSTANCE_PATH(3, 0),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = ANJ_ATTR_UINT_NONE,
                        .has_step = true,
                        .step = ANJ_ATTR_DOUBLE_NONE
                    }),
                    ANJ_COAP_CODE_CHANGED);

    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 1);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.attributes_storage[0].path,
                                   &ANJ_MAKE_INSTANCE_PATH(3, 0)));
    compare_attr(&_anj_observe_get_attr_from_path(
                          &anj.observe_ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0), 1)
                          ->attr,
                 &(_anj_attr_notification_t) {
                     .has_max_period = true,
                     .max_period = 50,
                 });
}

ANJ_UNIT_TEST(write_attr, write_attr_remove_all_attr) {
    TEST_INIT();
    anj.observe_ctx.attributes_storage[0].ssid = 1;
    anj.observe_ctx.attributes_storage[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.attributes_storage[0].attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 20,
        .has_max_period = true,
        .max_period = 50,
        .has_step = true,
        .step = 1.5
    };
    WRITE_ATTR_TEST(ANJ_MAKE_INSTANCE_PATH(3, 0),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = ANJ_ATTR_UINT_NONE,
                        .has_max_period = true,
                        .max_period = ANJ_ATTR_UINT_NONE,
                        .has_step = true,
                        .step = ANJ_ATTR_DOUBLE_NONE
                    }),
                    ANJ_COAP_CODE_CHANGED);

    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 0);
}

ANJ_UNIT_TEST(write_attr, write_attr_refresh_effective_attrs) {
    TEST_INIT();
    anj.observe_ctx.attributes_storage[0].ssid = 1;
    anj.observe_ctx.attributes_storage[0].path =
            ANJ_MAKE_RESOURCE_PATH(3, 0, 4);
    anj.observe_ctx.attributes_storage[0].attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 20,
    };
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 4);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].effective_attr.has_min_period = true;
    anj.observe_ctx.observations[0].effective_attr.min_period = 20;
    WRITE_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 0, 4),
                    ((_anj_attr_notification_t) {
                        .has_min_period = true,
                        .min_period = 4,
                    }),
                    ANJ_COAP_CODE_CHANGED);

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 4)));
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 4,
                 });
}

#    ifdef ANJ_WITH_DISCOVER_ATTR
ANJ_UNIT_TEST(discover_attr, get_obj_attr) {
    TEST_INIT();

    anj.observe_ctx.attributes_storage[0] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_OBJECT_PATH(3),
        .attr.has_min_period = true,
        .attr.min_period = 2,
    };
    anj.observe_ctx.attributes_storage[1] = (_anj_observe_attr_storage_t) {
        .ssid = 2,
        .path = ANJ_MAKE_OBJECT_PATH(3),
        .attr.has_min_period = true,
        .attr.min_period = 10
    };

    _anj_attr_notification_t attr;
    _anj_attr_notification_t expected = {
        .has_min_period = true,
        .min_period = 10
    };
    ASSERT_OK(_anj_observe_get_attr_storage(&anj, 2, true,
                                            &ANJ_MAKE_OBJECT_PATH(3), &attr));
    compare_attr(&attr, &expected);
    ASSERT_OK(_anj_observe_get_attr_storage(&anj, 2, false,
                                            &ANJ_MAKE_OBJECT_PATH(3), &attr));
    compare_attr(&attr, &expected);
    ASSERT_FAIL(_anj_observe_get_attr_storage(&anj, 3, true,
                                              &ANJ_MAKE_OBJECT_PATH(3), &attr));
    ASSERT_FAIL(_anj_observe_get_attr_storage(&anj, 2, false,
                                              &ANJ_MAKE_OBJECT_PATH(2), &attr));
}

ANJ_UNIT_TEST(discover_attr, get_instance_attr) {
    TEST_INIT();

    anj.observe_ctx.attributes_storage[0] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_OBJECT_PATH(3),
        .attr.has_min_period = true,
        .attr.min_period = 2,
#        ifdef ANJ_WITH_LWM2M12
        .attr.has_con = true,
#        endif // ANJ_WITH_LWM2M12
    };
    anj.observe_ctx.attributes_storage[1] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_INSTANCE_PATH(3, 1),
        .attr.has_min_period = true,
        .attr.min_period = 10,
        .attr.has_max_period = true,
    };

    _anj_attr_notification_t attr;

    // only attributes related to instance should be returned
    _anj_attr_notification_t expected_1 = {
        .has_min_period = true,
        .min_period = 10,
        .has_max_period = true
    };
    ASSERT_OK(_anj_observe_get_attr_storage(
            &anj, 1, false, &ANJ_MAKE_INSTANCE_PATH(3, 1), &attr));
    compare_attr(&attr, &expected_1);

    // path from request, return also object attributes
    _anj_attr_notification_t expected_2 = {
        .has_min_period = true,
        .min_period = 10,
        .has_max_period = true,
#        ifdef ANJ_WITH_LWM2M12
        .has_con = true
#        endif // ANJ_WITH_LWM2M12
    };
    ASSERT_OK(_anj_observe_get_attr_storage(
            &anj, 1, true, &ANJ_MAKE_INSTANCE_PATH(3, 1), &attr));
    compare_attr(&attr, &expected_2);

    ASSERT_FAIL(_anj_observe_get_attr_storage(
            &anj, 2, true, &ANJ_MAKE_INSTANCE_PATH(3, 1), &attr));
}

ANJ_UNIT_TEST(discover_attr, get_resource_attr) {
    TEST_INIT();

    anj.observe_ctx.attributes_storage[0] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_OBJECT_PATH(3),
        .attr.has_min_period = true,
        .attr.min_period = 2,
#        ifdef ANJ_WITH_LWM2M12
        .attr.has_con = true,
#        endif // ANJ_WITH_LWM2M12
    };
    anj.observe_ctx.attributes_storage[1] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_INSTANCE_PATH(3, 1),
        .attr.has_min_period = true,
        .attr.min_period = 10,
        .attr.has_max_period = true,
    };
    anj.observe_ctx.attributes_storage[2] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
        .attr.has_min_period = true,
        .attr.min_period = 7,
        .attr.has_step = true,
    };

    _anj_attr_notification_t attr;

    // only attributes related to resource should be returned
    _anj_attr_notification_t expected_1 = {
        .has_min_period = true,
        .min_period = 7,
        .has_step = true
    };
    ASSERT_OK(_anj_observe_get_attr_storage(
            &anj, 1, false, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), &attr));
    compare_attr(&attr, &expected_1);

    // path from request, return also object and instance attributes
    _anj_attr_notification_t expected_2 = {
        .has_min_period = true,
        .min_period = 7,
        .has_max_period = true,
        .has_step = true,
#        ifdef ANJ_WITH_LWM2M12
        .has_con = true
#        endif // ANJ_WITH_LWM2M12
    };
    ASSERT_OK(_anj_observe_get_attr_storage(
            &anj, 1, true, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), &attr));
    compare_attr(&attr, &expected_2);

    ASSERT_FAIL(_anj_observe_get_attr_storage(
            &anj, 2, true, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), &attr));
    ASSERT_FAIL(_anj_observe_get_attr_storage(
            &anj, 1, false, &ANJ_MAKE_RESOURCE_PATH(3, 1, 2), &attr));
    ASSERT_FAIL(_anj_observe_get_attr_storage(
            &anj, 1, false, &ANJ_MAKE_RESOURCE_PATH(3, 2, 1), &attr));
}

ANJ_UNIT_TEST(discover_attr, get_resource_instance_attr) {
    TEST_INIT();

    anj.observe_ctx.attributes_storage[0] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_OBJECT_PATH(3),
        .attr.has_min_period = true,
        .attr.min_period = 2,
#        ifdef ANJ_WITH_LWM2M12
        .attr.has_con = true,
#        endif // ANJ_WITH_LWM2M12
    };
    anj.observe_ctx.attributes_storage[1] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_INSTANCE_PATH(3, 1),
        .attr.has_min_period = true,
        .attr.min_period = 10,
        .attr.has_max_period = true,
    };
    anj.observe_ctx.attributes_storage[2] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
        .attr.has_min_period = true,
        .attr.min_period = 7,
        .attr.has_step = true,
    };
    anj.observe_ctx.attributes_storage[3] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 1, 1),
        .attr.has_less_than = true,
    };

    _anj_attr_notification_t attr;

    // only attributes related to resource instance should be returned
    _anj_attr_notification_t expected_1 = {
        .has_less_than = true,
    };
    ASSERT_OK(_anj_observe_get_attr_storage(
            &anj, 1, false, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 1, 1),
            &attr));
    compare_attr(&attr, &expected_1);

    // path from request, return also object and instance attributes
    _anj_attr_notification_t expected_2 = {
        .has_min_period = true,
        .min_period = 7,
        .has_max_period = true,
        .has_step = true,
#        ifdef ANJ_WITH_LWM2M12
        .has_con = true,
#        endif // ANJ_WITH_LWM2M12
        .has_less_than = true
    };
    ASSERT_OK(_anj_observe_get_attr_storage(
            &anj, 1, true, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 1, 1),
            &attr));
    compare_attr(&attr, &expected_2);
}
#    endif // ANJ_WITH_DISCOVER_ATTR

#endif // ANJ_WITH_OBSERVE
