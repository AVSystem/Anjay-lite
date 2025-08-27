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
#include <string.h>

#define ANJ_UNIT_ENABLE_SHORT_ASSERTS
#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#include "../../../src/anj/dm/dm_io.h"
#include "../../../src/anj/observe/observe.h"
#include "../../src/anj/coap/coap.h"
#include "../../src/anj/exchange.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_OBSERVE

#    define ONE_DAY_MS 86400000

uint64_t anj_time_now(void);
uint64_t anj_time_real_now(void);
void set_mock_time(uint64_t time);

static double get_res_value_double = 7.0;
static bool get_res_value_bool = true;
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

    if (rid == 2 || rid == 9 || rid == 10) {
        out_value->bool_value = get_res_value_bool;
    } else if (rid == 1 || rid == 7 || rid == 8) {
        out_value->double_value = get_res_value_double;
    } else {
        out_value->int_value = 0;
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
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_DOUBLE,
    },
    {
        .rid = 2,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_BOOL,
    },
    {
        .rid = 7,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_DOUBLE,
    },
    {
        .rid = 8,
        .operation = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_DOUBLE,
        .max_inst_count = 1,
        .insts = (anj_riid_t[]){ 1 }
    },
    {
        .rid = 9,
        .operation = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_BOOL,
        .max_inst_count = 1,
        .insts = (anj_riid_t[]){ 1 }
    },
    {
        .rid = 10,
        .operation = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_BOOL,
        .max_inst_count = 0
    },
};
static anj_dm_obj_inst_t inst_0 = {
    .iid = 1,
    .res_count = 6,
    .resources = inst_0_res
};
static anj_dm_obj_t obj_3 = {
    .oid = 3,
    .insts = &inst_0,
    .max_inst_count = 1,
    .handlers = &handlers
};

static anj_dm_obj_t obj_0 = {
    .oid = 0,
    .handlers = &handlers,
    .insts =
            (anj_dm_obj_inst_t[])
                    {
                        {
                            .iid = 0,
                            .res_count = 1,
                            .resources =
                                    (anj_dm_res_t[])
                                            {
                                                {
                                                    .rid = 0,
                                                    .operation = ANJ_DM_RES_R,
                                                    .type = ANJ_DATA_TYPE_STRING,
                                                }
                                            }
                        }
                    },
    .max_inst_count = 1
};

void compare_attr(_anj_attr_notification_t *attr1,
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

void compare_observations(_anj_observe_ctx_t *ctx1, _anj_observe_ctx_t *ctx2) {
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_EQ(ctx1->observations[i].ssid, ctx2->observations[i].ssid);
        ASSERT_TRUE(anj_uri_path_equal(&ctx1->observations[i].path,
                                       &ctx2->observations[i].path));
        ASSERT_EQ(ctx1->observations[i].token.size,
                  ctx2->observations[i].token.size);
        for (size_t j = 0; j < ctx1->observations[i].token.size; j++) {
            ASSERT_EQ(ctx1->observations[i].token.bytes[j],
                      ctx2->observations[i].token.bytes[j]);
        }
#    ifdef ANJ_WITH_LWM2M12
        compare_attr(&ctx1->observations[i].observation_attr,
                     &ctx2->observations[i].observation_attr);
#    endif // ANJ_WITH_LWM2M12
        compare_attr(&ctx1->observations[i].effective_attr,
                     &ctx2->observations[i].effective_attr);
        ASSERT_EQ(ctx1->observations[i].observe_active,
                  ctx2->observations[i].observe_active);
        ASSERT_EQ(ctx1->observations[i].last_notify_timestamp,
                  ctx2->observations[i].last_notify_timestamp);
        ASSERT_EQ(ctx1->observations[i].next_conf_notify_timestamp,
                  ctx2->observations[i].next_conf_notify_timestamp);
        ASSERT_EQ(ctx1->observations[i].last_sent_value.double_value,
                  ctx2->observations[i].last_sent_value.double_value);
        ASSERT_EQ(ctx1->observations[i].notification_to_send,
                  ctx2->observations[i].notification_to_send);
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
        ASSERT_EQ((size_t) ctx1->observations[i].prev,
                  (size_t) ctx2->observations[i].prev);
        ASSERT_EQ(ctx1->observations[i].content_format_opt,
                  ctx2->observations[i].content_format_opt);
        ASSERT_EQ(ctx1->observations[i].accept_opt,
                  ctx2->observations[i].accept_opt);
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    }
}

#    define OBSERVE_OP(Path, Attr, Result, Msg_Code)                          \
        _anj_exchange_ctx_t exchange_ctx;                                     \
        _anj_exchange_handlers_t out_handlers;                                \
        _anj_exchange_init(&exchange_ctx, 0);                                 \
        _anj_coap_msg_t inout_msg = {                                         \
            .operation = ANJ_OP_INF_OBSERVE,                                  \
            .attr.notification_attr = Attr,                                   \
            .uri = Path,                                                      \
            .payload_size = 0,                                                \
            .accept = _ANJ_COAP_FORMAT_NOT_DEFINED,                           \
        };                                                                    \
        inout_msg.coap_binding_data.udp.message_id = 0x1111;                  \
        inout_msg.token.size = 1;                                             \
        inout_msg.token.bytes[0] = 0x22;                                      \
        uint8_t response_code;                                                \
        uint8_t out_buff[100];                                                \
        size_t out_buff_len = sizeof(out_buff);                               \
        uint8_t payload_buff[100];                                            \
        size_t payload_buff_len = sizeof(payload_buff);                       \
        ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv,         \
                                           &inout_msg, &response_code),       \
                  Result);                                                    \
        if (Msg_Code == ANJ_COAP_CODE_INTERNAL_SERVER_ERROR) {                \
            res_read_ret_val = ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;           \
        }                                                                     \
        ASSERT_EQ(_anj_exchange_new_server_request(                           \
                          &exchange_ctx, response_code, &inout_msg,           \
                          &out_handlers, payload_buff, payload_buff_len),     \
                  ANJ_EXCHANGE_STATE_MSG_TO_SEND);                            \
                                                                              \
        if (Msg_Code == ANJ_COAP_CODE_INTERNAL_SERVER_ERROR) {                \
            res_read_ret_val = 0;                                             \
        }                                                                     \
        size_t out_msg_size = 0;                                              \
        ASSERT_EQ(inout_msg.block.block_type, ANJ_OPTION_BLOCK_NOT_DEFINED);  \
        ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len,    \
                                       &out_msg_size));                       \
        uint8_t EXPECTED_POSITIVE[] =                                         \
                "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */                 \
                "\xFF\x11\x11\x22"                                            \
                "\x60"     /*observe = 0*/                                    \
                "\x61\x70" /*content format = 112*/                           \
                "\xFF";                                                       \
        uint8_t EXPECTED_NEGATIVE[] =                                         \
                "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */                 \
                "\xFF";                                                       \
        size_t EXPECTED_POSITIVE_SIZE = sizeof(EXPECTED_POSITIVE) - 1;        \
        size_t EXPECTED_NEGATIVE_SIZE = sizeof(EXPECTED_NEGATIVE) - 1;        \
                                                                              \
        uint8_t *EXPECTED = Msg_Code == ANJ_COAP_CODE_CONTENT                 \
                                    ? EXPECTED_POSITIVE                       \
                                    : EXPECTED_NEGATIVE;                      \
        size_t EXPECTED_SIZE = Msg_Code == ANJ_COAP_CODE_CONTENT              \
                                       ? EXPECTED_POSITIVE_SIZE               \
                                       : EXPECTED_NEGATIVE_SIZE;              \
                                                                              \
        EXPECTED[1] = Msg_Code;                                               \
        ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED, EXPECTED_SIZE);             \
        ASSERT_EQ(_anj_exchange_process(&exchange_ctx,                        \
                                        ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, \
                                        &inout_msg),                          \
                  ANJ_EXCHANGE_STATE_FINISHED);

#    define OBSERVE_OP_TEST(Path)                             \
        _anj_observe_server_state_t srv = {                   \
            .ssid = 1,                                        \
            .default_max_period = 77,                         \
        };                                                    \
        OBSERVE_OP(Path, (_anj_attr_notification_t) { 0 }, 0, \
                   ANJ_COAP_CODE_CONTENT)

#    ifdef ANJ_WITH_LWM2M12
#        define OBSERVE_OP_WITH_ATTR_TEST(Path, Attr, Result, Msg_Code) \
            _anj_observe_server_state_t srv = {                         \
                .ssid = 1,                                              \
                .default_min_period = 12,                               \
            };                                                          \
            OBSERVE_OP(Path, Attr, Result, Msg_Code)
#    endif // ANJ_WITH_LWM2M12

#    define OBSERVE_OP_TEST_WITH_ERROR(Path, Attr, Result, Msg_Code) \
        _anj_observe_server_state_t srv = {                          \
            .ssid = 1,                                               \
            .default_max_period = 77,                                \
        };                                                           \
        OBSERVE_OP(Path, Attr, Result, Msg_Code)

#    define TEST_INIT()                          \
        anj_t anj = { 0 };                       \
        _anj_observe_init(&anj);                 \
        _anj_dm_initialize(&anj);                \
        ASSERT_OK(anj_dm_add_obj(&anj, &obj_0)); \
        ASSERT_OK(anj_dm_add_obj(&anj, &obj_3));

ANJ_UNIT_TEST(observe_op, observe_basic) {
    TEST_INIT();
    OBSERVE_OP_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 1));

    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 1, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    ASSERT_EQ(anj.observe_ctx.observations[0].accept_opt,
              _ANJ_COAP_FORMAT_NOT_DEFINED);
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

void add_attr_storage(_anj_observe_attr_storage_t *attr,
                      anj_uri_path_t path,
                      _anj_attr_notification_t notif_attr,
                      uint16_t ssid) {
    attr->path = path;
    attr->attr = notif_attr;
    attr->ssid = ssid;
}

ANJ_UNIT_TEST(observe_op, observe_effective_attr) {
    TEST_INIT();
    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_OBJECT_PATH(3),
                     (_anj_attr_notification_t) {
                         .has_min_period = true,
                         .min_period = 20,
                     },
                     2);
    add_attr_storage(&anj.observe_ctx.attributes_storage[1],
                     ANJ_MAKE_OBJECT_PATH(4),
                     (_anj_attr_notification_t) {
                         .has_max_eval_period = true,
                         .max_eval_period = 2137,
                     },
                     1);
    add_attr_storage(&anj.observe_ctx.attributes_storage[2],
                     ANJ_MAKE_INSTANCE_PATH(3, 1),
                     (_anj_attr_notification_t) {
                         .has_min_period = true,
                         .min_period = 10,
                         .has_min_eval_period = true,
                         .min_eval_period = 10,
                     },
                     1);
    add_attr_storage(&anj.observe_ctx.attributes_storage[3],
                     ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 1, 1),
                     (_anj_attr_notification_t) {
                         .has_min_period = true,
                         .min_period = 5,
                     },
                     1);
    add_attr_storage(&anj.observe_ctx.attributes_storage[4],
                     ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                     (_anj_attr_notification_t) {
                         .has_step = true,
                         .step = 2,
                         .has_min_eval_period = true,
                         .min_eval_period = 11,
                     },
                     1);

    OBSERVE_OP_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 1));

    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 1, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 10,
                     .has_min_eval_period = true,
                     .min_eval_period = 11,
                     .has_step = true,
                     .step = 2,
                 });
    ASSERT_EQ(anj.observe_ctx.observations[0].last_sent_value.double_value,
              7.0);
}

ANJ_UNIT_TEST(observe_op, observe_existing_records) {
    TEST_INIT();
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[2].ssid = 2;
    OBSERVE_OP_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 1));

    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[1].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 1, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].token.bytes[0], 0x22);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

ANJ_UNIT_TEST(observe_op, observe_wrong_default_pmax) {
    TEST_INIT();
    _anj_observe_server_state_t srv = {
        .ssid = 1,
        .default_max_period = 5,
    };

    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                     (_anj_attr_notification_t) {
                         .has_min_period = true,
                         .min_period = 10
                     },
                     1);

    OBSERVE_OP(ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
               (_anj_attr_notification_t) { 0 }, 0, ANJ_COAP_CODE_CONTENT)

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 10,
                     .has_max_period = false,
                     .max_period = 0
                 });
}

ANJ_UNIT_TEST(observe_op, observe_inactive_to_active) {
    TEST_INIT();
    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                     (_anj_attr_notification_t) {
                         .has_min_eval_period = true,
                         .min_eval_period = 20,
                         .has_max_eval_period = true,
                         .max_eval_period = 10
                     },
                     1);
    OBSERVE_OP_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 1));

    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 1, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_eval_period = true,
                     .min_eval_period = 20,
                     .has_max_eval_period = true,
                     .max_eval_period = 10
                 });
    ASSERT_FALSE(anj.observe_ctx.observations[0].observe_active);
    inout_msg.operation = ANJ_OP_DM_WRITE_ATTR;
    inout_msg.attr.notification_attr = (_anj_attr_notification_t) {
        .has_min_eval_period = true,
        .min_eval_period = 20,
        .has_max_eval_period = true,
        .max_eval_period = 30
    };
    ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv, &inout_msg,
                                       &response_code),
              0);

    ASSERT_EQ(_anj_exchange_new_server_request(&exchange_ctx, response_code,
                                               &inout_msg, &out_handlers,
                                               payload_buff, payload_buff_len),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&exchange_ctx,
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &inout_msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_eval_period = true,
                     .min_eval_period = 20,
                     .has_max_eval_period = true,
                     .max_eval_period = 30
                 });
    ASSERT_TRUE(anj.observe_ctx.observations[0].observe_active);
}

ANJ_UNIT_TEST(observe_op, observe_update_record) {
    TEST_INIT();
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1);
    OBSERVE_OP_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 1));

    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 1, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

#    ifdef ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(observe_op, observe_with_attr) {
    TEST_INIT();
    OBSERVE_OP_WITH_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                              ((_anj_attr_notification_t) {
                                  .has_max_period = true,
                                  .max_period = 22,
                                  .has_min_eval_period = true,
                                  .min_eval_period = 3,
                                  .has_max_eval_period = true,
                                  .max_eval_period = 4,
                              }),
                              0, ANJ_COAP_CODE_CONTENT);

    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 1, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    compare_attr(&anj.observe_ctx.observations[0].observation_attr,
                 &inout_msg.attr.notification_attr);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_max_period = true,
                     .max_period = 22,
                     .has_min_eval_period = true,
                     .min_eval_period = 3,
                     .has_max_eval_period = true,
                     .max_eval_period = 4,
                 });
}

ANJ_UNIT_TEST(observe_op, observe_ignore_attached_attr) {
    TEST_INIT();
    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_RESOURCE_PATH(3, 1, 7),
                     (_anj_attr_notification_t) {
                         .has_less_than = true,
                         .less_than = 3300,
                     },
                     1);
    OBSERVE_OP_WITH_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 7),
                              ((_anj_attr_notification_t) {
                                  .has_greater_than = true,
                                  .greater_than = 5000,
                              }),
                              0, ANJ_COAP_CODE_CONTENT);

    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 1, 7)));
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_greater_than = true,
                     .greater_than = 5000
                 });
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

ANJ_UNIT_TEST(observe_op, observe_with_step_when_path_to_res_inst) {
    TEST_INIT();
    OBSERVE_OP_WITH_ATTR_TEST(ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 8, 1),
                              ((_anj_attr_notification_t) {
                                  .has_min_period = true,
                                  .min_period = 5,
                                  .has_step = true,
                                  .step = 2,
                              }),
                              0, ANJ_COAP_CODE_CONTENT);

    ASSERT_TRUE(
            anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                               &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 8, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 5,
                     .has_step = true,
                     .step = 2
                 });
}

ANJ_UNIT_TEST(observe_op, observe_with_edge_when_path_to_res_inst) {
    TEST_INIT();
    OBSERVE_OP_WITH_ATTR_TEST(ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 9, 1),
                              ((_anj_attr_notification_t) {
                                  .has_min_period = true,
                                  .min_period = 5,
                                  .has_edge = true,
                                  .edge = 1,
                              }),
                              0, ANJ_COAP_CODE_CONTENT);

    ASSERT_TRUE(
            anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                               &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 9, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 5,
                     .has_edge = true,
                     .edge = 1
                 });
}

ANJ_UNIT_TEST(observe_op, observe_ignore_edge_with_multi_res) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_min_period = true,
        .min_period = 10,
        .has_edge = true,
        .edge = 1
    };
    OBSERVE_OP_WITH_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 9), attr, 0,
                              ANJ_COAP_CODE_CONTENT);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 10,
                 });
}

ANJ_UNIT_TEST(observe_op, observe_ignore_step_with_multi_res) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_min_period = true,
        .min_period = 10,
        .has_step = true,
        .step = 1
    };
    OBSERVE_OP_WITH_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 8), attr, 0,
                              ANJ_COAP_CODE_CONTENT);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 10,
                 });
}

ANJ_UNIT_TEST(observe_op, observe_check_timestamps) {
    TEST_INIT();
    _anj_observe_ctx_t ctx_ref;
    memset(&ctx_ref, 0, sizeof(ctx_ref));

    _anj_attr_notification_t observe_attr = {
        .has_max_period = true,
        .max_period = 22,
    };
    ctx_ref.observations[0].observe_active = true;
    ctx_ref.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1);
    ctx_ref.observations[0].ssid = 1;
    ctx_ref.observations[0].token.size = 1;
    ctx_ref.observations[0].token.bytes[0] = 0x22;
    ctx_ref.observations[0].last_notify_timestamp = anj_time_real_now();
    ctx_ref.observations[0].next_conf_notify_timestamp =
            anj_time_real_now() + ONE_DAY_MS;
    ctx_ref.observations[0].effective_attr = (_anj_attr_notification_t) {
        .has_max_period = true,
        .max_period = 22,
    };
    ctx_ref.observations[0].observation_attr = (_anj_attr_notification_t) {
        .has_max_period = true,
        .max_period = 22,
    };
#        ifdef ANJ_WITH_OBSERVE_COMPOSITE
    ctx_ref.observations[0].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    ctx_ref.observations[0].content_format_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
#        endif // ANJ_WITH_OBSERVE_COMPOSITE
    OBSERVE_OP_WITH_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 1), observe_attr, 0,
                              ANJ_COAP_CODE_CONTENT);

    compare_observations(&anj.observe_ctx, &ctx_ref);
    set_mock_time(10000);

    inout_msg.operation = ANJ_OP_INF_OBSERVE;
    inout_msg.attr.notification_attr = observe_attr;
    inout_msg.uri = ANJ_MAKE_RESOURCE_PATH(3, 1, 1);
    inout_msg.payload_size = 0;
    ctx_ref.observations[0].last_notify_timestamp = anj_time_real_now();
    ctx_ref.observations[0].next_conf_notify_timestamp =
            anj_time_real_now() + ONE_DAY_MS;
    ASSERT_OK(_anj_observe_new_request(&anj, &out_handlers, &srv, &inout_msg,
                                       &response_code));
    ASSERT_EQ(_anj_exchange_new_server_request(&exchange_ctx, response_code,
                                               &inout_msg, &out_handlers,
                                               payload_buff, payload_buff_len),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&exchange_ctx,
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &inout_msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    compare_observations(&anj.observe_ctx, &ctx_ref);
    // go back to 0 value after test execution
    set_mock_time(0);
}

ANJ_UNIT_TEST(observe_op, observe_wrong_observe_attr_pmax) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_min_period = true,
        .min_period = 10,
        .has_max_period = true,
        .max_period = 9,
    };
    OBSERVE_OP_WITH_ATTR_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 1), attr, 0,
                              ANJ_COAP_CODE_CONTENT);

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    // we are do not ignoring max period at this point
    compare_attr(&anj.observe_ctx.observations[0].effective_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 10,
                     .has_max_period = true,
                     .max_period = 9
                 });
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

#    endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(observe_op, observe_block) {
    TEST_INIT();
    _anj_exchange_ctx_t exchange_ctx;
    _anj_exchange_handlers_t out_handlers;
    _anj_exchange_init(&exchange_ctx, 0);
    _anj_observe_server_state_t srv = {
        .ssid = 1,
        .default_max_period = 77,
    };
    _anj_coap_msg_t inout_msg = {
        .operation = ANJ_OP_INF_OBSERVE,
        .uri = ANJ_MAKE_INSTANCE_PATH(3, 1),
        .payload_size = 0,
    };
    inout_msg.coap_binding_data.udp.message_id = 0x1111;
    inout_msg.token.size = 1;
    inout_msg.token.bytes[0] = 0x22;
    uint8_t response_code;
    uint8_t out_buff[100];
    size_t out_buff_len = sizeof(out_buff);
    uint8_t payload_buff[16];
    // make sure that payload will fit in 2 blocks
    inst_0.res_count -= 4;
    ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv, &inout_msg,
                                       &response_code),
              0);
    ASSERT_EQ(_anj_exchange_new_server_request(&exchange_ctx, response_code,
                                               &inout_msg, &out_handlers,
                                               payload_buff, 16),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    size_t out_msg_size = 0;
    ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len,
                                   &out_msg_size));
    uint8_t EXPECTED[] =
            "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
            "\xFF\x11\x11\x22"
            "\x60"     /*observe = 0*/
            "\x61\x70" /*content format = 112*/
            "\xb1\x08" // block2 0
            "\xFF"
            "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01";
    EXPECTED[1] = ANJ_COAP_CODE_CONTENT;
    // don't check payload
    ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED, sizeof(EXPECTED) - 17);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(EXPECTED) - 1);

    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_INSTANCE_PATH(3, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);

    ASSERT_EQ(inout_msg.block.number, 0);
    ASSERT_EQ(inout_msg.block.block_type, ANJ_OPTION_BLOCK_2);

    ASSERT_EQ(_anj_exchange_process(&exchange_ctx,
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &inout_msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    inout_msg.operation = ANJ_OP_INF_OBSERVE;
    inout_msg.block.block_type = ANJ_OPTION_BLOCK_2;
    inout_msg.block.number = 1;
    inout_msg.block.more_flag = false;
    inout_msg.payload_size = 0;
    inout_msg.coap_binding_data.udp.message_id++;
    ASSERT_EQ(_anj_exchange_process(&exchange_ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
                                    &inout_msg),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(inout_msg.block.number, 1);
    ASSERT_EQ(inout_msg.block.block_type, ANJ_OPTION_BLOCK_2);
    ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len,
                                   &out_msg_size));
    uint8_t EXPECTED2[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x12\x22"
                          "\x60"     /*observe = 0*/
                          "\x61\x70" /*content format = 112*/
                          "\xb1\x10" // block2 1
                          "\xFF";
    inst_0.res_count += 4;
    EXPECTED2[1] = ANJ_COAP_CODE_CONTENT;
    // don't check payload
    ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED2, sizeof(EXPECTED2) - 1);
    ASSERT_EQ(_anj_exchange_process(&exchange_ctx,
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &inout_msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
}

ANJ_UNIT_TEST(observe_op, observe_err_no_space) {
    TEST_INIT();
    anj.observe_ctx.observations[0].ssid = 2;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[4].ssid = 1;
    OBSERVE_OP_TEST_WITH_ERROR(ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 8, 1),
                               NULL, -1, ANJ_COAP_CODE_INTERNAL_SERVER_ERROR);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 2);
}

ANJ_UNIT_TEST(observe_op, observe_build_msg_error) {
    TEST_INIT();
    OBSERVE_OP_TEST_WITH_ERROR(ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 8, 1),
                               NULL, 0, ANJ_COAP_CODE_INTERNAL_SERVER_ERROR);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
}

ANJ_UNIT_TEST(observe_op, observe_root_path) {
    TEST_INIT();
    OBSERVE_OP_TEST_WITH_ERROR(ANJ_MAKE_ROOT_PATH(), NULL, -1,
                               ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
}

#    ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(observe_op, observe_err_not_found) {
    TEST_INIT();
    OBSERVE_OP_TEST_WITH_ERROR(ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 21, 1, 1),
                               &((_anj_attr_notification_t) {
                                   .has_min_period = true,
                                   .min_period = 20,
                               }),
                               -1, ANJ_COAP_CODE_NOT_FOUND);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
}

ANJ_UNIT_TEST(observe_op, observe_err_not_allowed) {
    TEST_INIT();
    OBSERVE_OP_TEST_WITH_ERROR(ANJ_MAKE_RESOURCE_PATH(3, 1, 10),
                               &((_anj_attr_notification_t) {
                                   .has_min_period = true,
                                   .min_period = 20,
                               }),
                               -1, ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
}

ANJ_UNIT_TEST(observe_op, observe_err_unauthorized) {
    TEST_INIT();
    // Check if we get UNAUTHORIZED even if the object is not registered
    OBSERVE_OP_TEST_WITH_ERROR(ANJ_MAKE_RESOURCE_PATH(0, 0, 0),
                               &((_anj_attr_notification_t) {
                                   .has_min_period = true,
                                   .min_period = 20,
                               }),
                               -1, ANJ_COAP_CODE_UNAUTHORIZED);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
}

ANJ_UNIT_TEST(observe_op, observe_update_record_error) {
    TEST_INIT();
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].observation_attr.has_min_period = true;
    anj.observe_ctx.observations[0].observation_attr.min_period = 5;
    OBSERVE_OP_TEST_WITH_ERROR(ANJ_MAKE_RESOURCE_PATH(3, 1, 1), NULL, -1,
                               ANJ_COAP_CODE_METHOD_NOT_ALLOWED);

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 1, 1)));
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    compare_attr(&anj.observe_ctx.observations[0].observation_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 5,
                 });
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

ANJ_UNIT_TEST(observe_op, observe_wrong_observe_attr) {
    TEST_INIT();
    _anj_attr_notification_t attr = {
        .has_min_eval_period = true,
        .min_eval_period = 20,
        .has_max_eval_period = true,
        .max_eval_period = 10
    };
    OBSERVE_OP_TEST_WITH_ERROR(ANJ_MAKE_RESOURCE_PATH(3, 1, 1), attr, -1,
                               ANJ_COAP_CODE_BAD_REQUEST);

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

#    endif // ANJ_WITH_LWM2M12

#    define CANCEL_OBSERVE_OP_TEST(Path, Result, Expected)                    \
        _anj_exchange_ctx_t exchange_ctx;                                     \
        _anj_exchange_handlers_t out_handlers;                                \
        _anj_exchange_init(&exchange_ctx, 0);                                 \
        _anj_observe_server_state_t srv = {                                   \
            .ssid = 1,                                                        \
        };                                                                    \
        _anj_coap_msg_t inout_msg = {                                         \
            .operation = ANJ_OP_INF_CANCEL_OBSERVE,                           \
            .uri = Path,                                                      \
            .payload_size = 0,                                                \
            .accept = _ANJ_COAP_FORMAT_NOT_DEFINED,                           \
        };                                                                    \
        inout_msg.coap_binding_data.udp.message_id = 0x1111;                  \
        inout_msg.token.size = 1;                                             \
        inout_msg.token.bytes[0] = 0x22;                                      \
        uint8_t response_code;                                                \
        uint8_t out_buff[100];                                                \
        size_t out_buff_len = sizeof(out_buff);                               \
        uint8_t payload_buff[100];                                            \
        size_t payload_buff_len = sizeof(payload_buff);                       \
        ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv,         \
                                           &inout_msg, &response_code),       \
                  Result);                                                    \
        if ((uint8_t) Expected[1] == ANJ_COAP_CODE_INTERNAL_SERVER_ERROR) {   \
            res_read_ret_val = ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;           \
        }                                                                     \
        ASSERT_EQ(_anj_exchange_new_server_request(                           \
                          &exchange_ctx, response_code, &inout_msg,           \
                          &out_handlers, payload_buff, payload_buff_len),     \
                  ANJ_EXCHANGE_STATE_MSG_TO_SEND);                            \
        if ((uint8_t) Expected[1] == ANJ_COAP_CODE_INTERNAL_SERVER_ERROR) {   \
            res_read_ret_val = 0;                                             \
        }                                                                     \
        size_t out_msg_size = 0;                                              \
        ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len,    \
                                       &out_msg_size));                       \
        ASSERT_EQ_BYTES_SIZED(out_buff, Expected, sizeof(Expected) - 1);      \
        ASSERT_EQ(_anj_exchange_process(&exchange_ctx,                        \
                                        ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, \
                                        &inout_msg),                          \
                  ANJ_EXCHANGE_STATE_FINISHED);

ANJ_UNIT_TEST(observe_op, observe_cancel) {
    TEST_INIT();
    anj.observe_ctx.observations[0].ssid = 2;
    anj.observe_ctx.observations[0].path =
            ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 8, 1);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path =
            ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 8, 1);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[3].ssid = 1;
    CANCEL_OBSERVE_OP_TEST(ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 8, 1), 0,
                           "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                           "\x45\x11\x11\x22\xc1\x70\xFF")
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 1);
}

ANJ_UNIT_TEST(observe_op, observe_cancel_not_found) {
    TEST_INIT();
    anj.observe_ctx.observations[0].ssid = 2;
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x33;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[3].ssid = 1;
    CANCEL_OBSERVE_OP_TEST(ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 77, 1),
                           -1,
                           "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                           "\x84\x11\x11\x22")
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 1);
}

ANJ_UNIT_TEST(observe_op, observe_cancel_build_msg_error) {
    TEST_INIT();
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].path =
            ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 8, 1);
    uint8_t EXPECTED[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                         "\xFF\x11\x11\x22";
    EXPECTED[1] = ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    CANCEL_OBSERVE_OP_TEST(ANJ_MAKE_RESOURCE_PATH(3, 1, 1), 0, EXPECTED)
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
}

#endif // ANJ_WITH_OBSERVE
