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
#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#ifdef ANJ_WITH_OBSERVE_COMPOSITE
#    include "../../../src/anj/observe/observe_internal.h"
#endif // ANJ_WITH_OBSERVE_COMPOSITE
#include "../../../src/anj/dm/dm_io.h"
#include "../../../src/anj/observe/observe.h"
#include "../../src/anj/coap/coap.h"
#include "../../src/anj/exchange.h"

#include "../mock/time_api_mock.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_OBSERVE

#    define ONE_DAY anj_time_duration_new(1, ANJ_TIME_UNIT_DAY)
#    define ONE_DAY_MS (24 * 60 * 60 * 1000)

static uint64_t mock_time_value = 0;
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

    if (rid == 0) {
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
        .rid = 0,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_BOOL,
    },
    {
        .rid = 1,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_DOUBLE,
    },
    {
        .rid = 2,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_DOUBLE,
    },
    {
        .rid = 3,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_DOUBLE,
    },
    {
        .rid = 4,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_DOUBLE,
    },
    {
        .rid = 6,
        .kind = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_DOUBLE,
        .max_inst_count = 1,
        .insts = (anj_riid_t[]){ 1 }
    },
    {
        .rid = 7,
        .kind = ANJ_DM_RES_WM,
        .type = ANJ_DATA_TYPE_DOUBLE,
        .max_inst_count = 1,
        .insts = (anj_riid_t[]){ 1 }
    }
};
static anj_dm_obj_inst_t inst_0 = {
    .iid = 0,
    .res_count = 2,
    .resources = inst_0_res
};
static anj_dm_obj_t obj_3 = {
    .oid = 3,
    .insts = &inst_0,
    .max_inst_count = 1,
    .handlers = &handlers
};

static anj_dm_res_t inst_0_res_obj_4[] = {
    {
        .rid = 1,
        .kind = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_DOUBLE,
        .max_inst_count = 1,
        .insts = (anj_riid_t[]){ 1 }
    }
};
static anj_dm_obj_inst_t inst_0_obj_4 = {
    .iid = 0,
    .res_count = 1,
    .resources = inst_0_res_obj_4
};
static anj_dm_obj_t obj_4 = {
    .oid = 4,
    .insts = &inst_0_obj_4,
    .max_inst_count = 1,
    .handlers = &handlers
};
#    define NOTIFICATION_INIT()                                         \
        inst_0.res_count = 2;                                           \
        _anj_exchange_ctx_t exchange_ctx;                               \
        _anj_exchange_init(&exchange_ctx);                              \
        exchange_ctx.msg_id = 0;                                        \
        uint16_t message_id = 1;                                        \
        anj_t anj = { 0 };                                              \
        _anj_dm_initialize(&anj);                                       \
        anj_dm_add_obj(&anj, &obj_3);                                   \
        anj_dm_add_obj(&anj, &obj_4);                                   \
        _anj_exchange_handlers_t out_handlers = { 0 };                  \
        _anj_observe_server_state_t srv = {                             \
            .ssid = 1,                                                  \
            .default_max_period = 77,                                   \
        };                                                              \
        _anj_coap_msg_t out_msg = { 0 };                                \
        anj_time_duration_t time_to_next_call = ANJ_TIME_DURATION_ZERO; \
                                                                        \
        const size_t payload_buff_size = 1024;                          \
        uint8_t payload[payload_buff_size];                             \
        const size_t out_buff_size = 1024;                              \
        uint8_t out_buff[out_buff_size];                                \
        size_t out_msg_size;                                            \
        mock_time_reset();                                              \
        set_res_value_double(0.0);                                      \
        set_res_value_bool(false);                                      \
        anj_time_real_t time_real_zero = anj_time_real_now();

#    define INIT_OBSERVE_MODULE() _anj_observe_init(&anj);

#    define anj_process(expected_time_to_next_call, Token, token_size)        \
        ASSERT_TRUE(token_size == 1 || token_size == 0);                      \
        ASSERT_OK(anj_observe_time_to_next_notification(&anj, &srv,           \
                                                        &time_to_next_call)); \
        ASSERT_EQ(time_to_next_call.us, expected_time_to_next_call.us);       \
        ASSERT_OK(_anj_observe_process(&anj, &out_handlers, &srv, &out_msg)); \
                                                                              \
        ASSERT_EQ(out_msg.token.size, token_size);                            \
        if (token_size) {                                                     \
            ASSERT_EQ(out_msg.token.bytes[0], Token);                         \
        }

#    define anj_exchange(Confirmable)                                         \
        ASSERT_EQ(_anj_exchange_new_client_request(&exchange_ctx, &out_msg,   \
                                                   &out_handlers, payload,    \
                                                   payload_buff_size),        \
                  ANJ_EXCHANGE_STATE_MSG_TO_SEND);                            \
        ASSERT_OK(_anj_coap_encode_udp(&out_msg, out_buff, out_buff_size,     \
                                       &out_msg_size));                       \
        ASSERT_EQ(_anj_exchange_process(&exchange_ctx,                        \
                                        ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, \
                                        &out_msg),                            \
                  Confirmable ? ANJ_EXCHANGE_STATE_WAITING_MSG                \
                              : ANJ_EXCHANGE_STATE_FINISHED);                 \
        if (Confirmable) {                                                    \
            /* Server response */                                             \
            if ((int) Confirmable == -1) {                                    \
                out_msg.operation = ANJ_OP_COAP_RESET;                        \
            } else {                                                          \
                out_msg.operation = ANJ_OP_RESPONSE;                          \
            }                                                                 \
            out_msg.msg_code = ANJ_COAP_CODE_EMPTY;                           \
            out_msg.payload_size = 0;                                         \
            out_msg.coap_binding_data.udp.type =                              \
                    ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;                        \
            ASSERT_EQ(_anj_exchange_process(&exchange_ctx,                    \
                                            ANJ_EXCHANGE_EVENT_NEW_MSG,       \
                                            &out_msg),                        \
                      ANJ_EXCHANGE_STATE_FINISHED);                           \
        }

// payload is not checked
#    define check_out_buff(Confirmable, Token, Observe_number, Content_Format) \
        {                                                                      \
            uint8_t EXPECTED_POSITIVE[] =                                      \
                    "\x51" /* Ver = 1, Type = 1 (Non-con), TKL = 1 */          \
                    "\x45\xFF\xFF\x21"                                         \
                    "\x61"                                                     \
                    "\xFF"     /*observe = xx*/                                \
                    "\x61\x70" /*content format = xx*/                         \
                    "\xFF";                                                    \
            if (Confirmable) {                                                 \
                EXPECTED_POSITIVE[0] =                                         \
                        0x41; /* Ver = 1, Type = 0 (Con), TKL = 1 */           \
            }                                                                  \
            EXPECTED_POSITIVE[2] = message_id >> 8;                            \
            EXPECTED_POSITIVE[3] = message_id & 0x00FF;                        \
            EXPECTED_POSITIVE[4] = Token;                                      \
            EXPECTED_POSITIVE[6] = Observe_number;                             \
            EXPECTED_POSITIVE[8] = Content_Format;                             \
            ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED_POSITIVE,                 \
                                  sizeof(EXPECTED_POSITIVE) - 1);              \
            memset(&out_msg, 0, sizeof(out_msg));                              \
            message_id++;                                                      \
        }

static void set_res_value_double(double val) {
    get_res_value_double = val;
}

static void set_res_value_bool(bool val) {
    get_res_value_bool = val;
}

static void setup_observations(_anj_observe_ctx_t *ctx,
                               anj_uri_path_t *paths,
                               size_t path_count,
                               _anj_attr_notification_t *effective_attr) {
    for (size_t i = 0; i < path_count; i++) {
        ctx->observations[i].ssid = 1;
        ctx->observations[i].token.bytes[0] = 0x21 + i;
        ctx->observations[i].token.size = 1;
        ctx->observations[i].path = paths[i];
        ctx->observations[i].effective_attr = *effective_attr;
        ctx->observations[i].observe_active = true;
        ctx->observations[i].last_notify_timestamp = anj_time_real_now();
        if (anj_uri_path_equal(&paths[i], &ANJ_MAKE_RESOURCE_PATH(3, 0, 0))) {
            ctx->observations[i].last_sent_value.bool_value =
                    get_res_value_bool;
        } else {
            ctx->observations[i].last_sent_value.double_value =
                    get_res_value_double;
        }
        ctx->observations[i].next_conf_notify_timestamp =
                anj_time_real_add(anj_time_real_now(), ONE_DAY);
        ;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
        ctx->observations[i].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
        ctx->observations[i].content_format_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    }
}

static void check_observations(_anj_observe_ctx_t *ctx,
                               anj_uri_path_t *paths,
                               size_t path_count,
                               bool confirmable,
                               anj_time_real_t timestamp) {
    size_t path_counter = 0;
    for (size_t i = 0;
         i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER && path_counter < path_count;
         i++) {
        if (!anj_uri_path_equal(&ctx->observations[i].path,
                                &paths[path_counter])) {
            continue;
        }
        path_counter++;
        ASSERT_TRUE(anj_time_real_eq(ctx->observations[i].last_notify_timestamp,
                                     anj_time_real_now()));
        if (confirmable) {
            ASSERT_TRUE(anj_time_real_eq(
                    ctx->observations[i].next_conf_notify_timestamp,
                    anj_time_real_add(anj_time_real_now(), ONE_DAY)));
        } else {
            // unfortunately it will be not true after sending first confirmable
            // notification or when mock_time_value != 0 during
            // setup_observations call
            ASSERT_TRUE(anj_time_real_eq(
                    ctx->observations[i].next_conf_notify_timestamp,
                    timestamp));
        }
    }

    ASSERT_EQ(path_counter, path_count);
}

ANJ_UNIT_TEST(notification_op, notification_max_period) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();

    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_max_period = true,
        .max_period = 10
    };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_op, notification_change) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = { 0 };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(0.1);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
}

#    define GT_LS_TEST                                                         \
        srv.default_max_period = 0;                                            \
        setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),     \
                           &effective_attributes);                             \
                                                                               \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        set_res_value_double(20.0);                                            \
        ASSERT_OK(anj_observe_data_model_changed(                              \
                &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),                        \
                ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));                    \
                                                                               \
        anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);     \
        anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);     \
                                                                               \
        mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));      \
                                                                               \
        anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);      \
        anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);      \
                                                                               \
        mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));      \
                                                                               \
        anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);                          \
                                                                               \
        anj_exchange(false);                                                   \
        check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);           \
        check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),     \
                           false, anj_time_real_add(time_real_zero, ONE_DAY)); \
                                                                               \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        mock_time_advance(anj_time_duration_new(1000, ANJ_TIME_UNIT_MS));      \
                                                                               \
        set_res_value_double(11.0);                                            \
        ASSERT_OK(anj_observe_data_model_changed(                              \
                &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),                        \
                ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));                    \
                                                                               \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        set_res_value_double(9.0);                                             \
        ASSERT_OK(anj_observe_data_model_changed(                              \
                &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),                        \
                ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));                    \
                                                                               \
        anj_process(anj_time_duration_new(9000, ANJ_TIME_UNIT_MS), 0, 0);      \
        anj_process(anj_time_duration_new(9000, ANJ_TIME_UNIT_MS), 0, 0);      \
                                                                               \
        mock_time_advance(anj_time_duration_new(4000, ANJ_TIME_UNIT_MS));      \
                                                                               \
        set_res_value_double(11.0);                                            \
        ASSERT_OK(anj_observe_data_model_changed(                              \
                &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),                        \
                ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));                    \
                                                                               \
        anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);      \
        anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);      \
                                                                               \
        mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));      \
                                                                               \
        anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);                          \
                                                                               \
        anj_exchange(false);                                                   \
        check_out_buff(false, 0x21, 2, _ANJ_COAP_FORMAT_SENML_CBOR);           \
        check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),     \
                           false, anj_time_real_add(time_real_zero, ONE_DAY)); \
                                                                               \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        mock_time_advance(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS));     \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        /* value equals threshold - notification should not be sent */         \
        set_res_value_double(10.0);                                            \
        ASSERT_OK(anj_observe_data_model_changed(                              \
                &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),                        \
                ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));                    \
                                                                               \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        /* set "manually" last value to 10.0 (equall to gt/ls attribute) */    \
        anj.observe_ctx.observations[0].last_sent_value.double_value = 10.0;   \
                                                                               \
        set_res_value_double(9.0);                                             \
        ASSERT_OK(anj_observe_data_model_changed(                              \
                &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),                        \
                ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));                    \
        anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);                          \
        anj_exchange(false);                                                   \
        check_out_buff(false, 0x21, 3, _ANJ_COAP_FORMAT_SENML_CBOR);           \
        check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),     \
                           false, anj_time_real_add(time_real_zero, ONE_DAY)); \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        mock_time_advance(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS));     \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        /* set "manually" last value to 10.0 (equall to gt/ls attribute) */    \
        anj.observe_ctx.observations[0].last_sent_value.double_value = 10.0;   \
                                                                               \
        set_res_value_double(11.0);                                            \
        ASSERT_OK(anj_observe_data_model_changed(                              \
                &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),                        \
                ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));                    \
        anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);                          \
        anj_exchange(false);                                                   \
        check_out_buff(false, 0x21, 4, _ANJ_COAP_FORMAT_SENML_CBOR);           \
        check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),     \
                           false, anj_time_real_add(time_real_zero, ONE_DAY)); \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        mock_time_advance(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS));     \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        /* set "manually" last value to 10.0 (equall to gt/ls attribute) */    \
        anj.observe_ctx.observations[0].last_sent_value.double_value = 10.0;   \
                                                                               \
        /* last value equals current - no notification should be sent */       \
        set_res_value_double(10.0);                                            \
        ASSERT_OK(anj_observe_data_model_changed(                              \
                &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),                        \
                ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));                    \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);                          \
                                                                               \
        mock_time_advance(anj_time_duration_new(50000, ANJ_TIME_UNIT_MS));     \
                                                                               \
        anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

ANJ_UNIT_TEST(notification_op, notification_change_gt) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_min_period = true,
        .min_period = 10,
        .has_greater_than = true,
        .greater_than = 10
    };

    GT_LS_TEST;
}

ANJ_UNIT_TEST(notification_op, notification_change_ls) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_min_period = true,
        .min_period = 10,
        .has_greater_than = true,
        .greater_than = 2137,
        .has_less_than = true,
        .less_than = 10
    };

    GT_LS_TEST;
}

ANJ_UNIT_TEST(notification_op, notification_change_step) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_min_period = true,
        .min_period = 10,
        .has_greater_than = true,
        .greater_than = 2137,
        .has_less_than = true,
        .less_than = 420,
        .has_step = true,
        .step = 10
    };

    srv.default_max_period = 0;
    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    mock_time_advance(anj_time_duration_new(1000, ANJ_TIME_UNIT_MS));

    set_res_value_double(11.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    set_res_value_double(10.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(9000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(9000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(4000, ANJ_TIME_UNIT_MS));

    set_res_value_double(11.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(false);
    check_out_buff(false, 0x21, 2, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    mock_time_advance(anj_time_duration_new(50000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
}

#    ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(notification_op, notification_change_edge_falling) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 0) };
    _anj_attr_notification_t effective_attributes = {
        .has_edge = true,
        .edge = 0
    };

    srv.default_max_period = 0;
    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    set_res_value_bool(true);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    set_res_value_bool(false);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    anj.observe_ctx.observations[0].last_sent_value.bool_value = true;

    set_res_value_bool(false);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
}

ANJ_UNIT_TEST(notification_op, notification_change_edge_raising) {
    NOTIFICATION_INIT();
    set_res_value_bool(true);
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 0) };
    _anj_attr_notification_t effective_attributes = {
        .has_edge = true,
        .edge = 1
    };

    srv.default_max_period = 0;
    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    set_res_value_bool(true);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    set_res_value_bool(false);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    anj.observe_ctx.observations[0].last_sent_value.bool_value = false;

    set_res_value_bool(true);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
}

ANJ_UNIT_TEST(notification_op,
              notification_confirmable_from_effective_attr_enabled) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_con = true,
        .con = 1
    };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(true);
    check_out_buff(true, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), true,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_op,
              notification_confirmable_from_effective_attr_disabled) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_con = true,
        .con = 0
    };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_op, notification_confirmable_from_server) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = { 0 };
    srv.default_con = 1;

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(true);
    check_out_buff(true, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), true,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_op, notification_confirmable_after_24h) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = { 0 };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(
            anj_time_duration_new(ONE_DAY_MS - 77000, ANJ_TIME_UNIT_MS));
    // non-con notification
    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS));
    // con notification
    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(true);
    check_out_buff(true, 0x21, 2, _ANJ_COAP_FORMAT_SENML_CBOR);
    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), true,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS));
    // non-con notification
    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(false);
    ASSERT_TRUE(anj_time_real_eq(
            anj.observe_ctx.observations[0].next_conf_notify_timestamp,
            anj_time_real_add(time_real_zero,
                              anj_time_duration_mul(ONE_DAY, 2))));
}

ANJ_UNIT_TEST(notification_op, notification_confirmable_get_reset) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = { 0 };
    srv.default_con = 1;

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    anj_exchange(-1);
    check_out_buff(true, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
}
#    endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(notification_op,
              notification_call_data_model_changed_more_than_once) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_step = true,
        .step = 10
    };

    srv.default_max_period = 0;
    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    set_res_value_double(11.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    set_res_value_double(5.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    // Even though 5.0 doesn't meet the "Change Value Condition", we send
    // notifications because the previously reported value met them
    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].last_sent_value.double_value,
              get_res_value_double);
}

ANJ_UNIT_TEST(notification_op, observe_number_overflow) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = { 0 };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);
    anj.observe_ctx.observations[0].observe_number = 0xFFFFFE;

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(0.1);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);
    anj_exchange(false);
    uint8_t EXPECTED_POSITIVE1[] =
            "\x51" /* Ver = 1, Type = 1 (Non-con), TKL = 1 */
            "\x45\x00\x01\x21"
            "\x63"
            "\xFF\xFF\xFF" /*observe = 0xFFFFFF*/
            "\x61\x70"     /*content format = 112*/
            "\xFF";
    EXPECTED_POSITIVE1[2] = message_id >> 8;
    EXPECTED_POSITIVE1[3] = message_id & 0x00FF;
    ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED_POSITIVE1,
                          sizeof(EXPECTED_POSITIVE1) - 1);
    memset(&out_msg, 0, sizeof(out_msg));
    message_id++;

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));
    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);
    anj_exchange(false);
    uint8_t EXPECTED_POSITIVE2[] =
            "\x51" /* Ver = 1, Type = 1 (Non-con), TKL = 1 */
            "\x45\x00\x01\x21"
            "\x60"     /*observe = 0x00*/
            "\x61\x70" /*content format = 112*/
            "\xFF";
    EXPECTED_POSITIVE2[2] = message_id >> 8;
    EXPECTED_POSITIVE2[3] = message_id & 0x00FF;
    ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED_POSITIVE2,
                          sizeof(EXPECTED_POSITIVE2) - 1);
    memset(&out_msg, 0, sizeof(out_msg));
    message_id++;

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_op, notification_change_with_more_than_one_server) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                               ANJ_MAKE_INSTANCE_PATH(3, 0),
                               ANJ_MAKE_OBJECT_PATH(3) };
    _anj_attr_notification_t effective_attributes = { 0 };

    srv.default_max_period = 0;
    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj.observe_ctx.observations[1].ssid = 2;
    anj.observe_ctx.observations[2].ssid = 3;

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    set_res_value_double(20.0);
    // server with SSID 1 changed resource value
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 1));

    // change time only to tests timestamps
    mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));

    srv.ssid = 2;
    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
    anj_exchange(false);
    check_out_buff(false, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    srv.ssid = 3;
    anj_process(ANJ_TIME_DURATION_ZERO, 0x23, 1);
    anj_exchange(false);
    check_out_buff(false, 0x23, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    check_observations(&anj.observe_ctx,
                       &(anj_uri_path_t[2]){ ANJ_MAKE_INSTANCE_PATH(3, 0),
                                             ANJ_MAKE_OBJECT_PATH(3) }[0],
                       2, false, anj_time_real_add(time_real_zero, ONE_DAY));

    srv.ssid = 1;
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    srv.ssid = 2;
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    srv.ssid = 3;
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
}

ANJ_UNIT_TEST(notification_op, notification_change_added_object_instance) {
    /* There should never be a situation where we have a standard observation on
     * a non-existent path. In this case, we have such an unrealistic situation
     * to test the code. */
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 1, 5),
                               ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                               ANJ_MAKE_INSTANCE_PATH(3, 0),
                               ANJ_MAKE_OBJECT_PATH(3) };
    _anj_attr_notification_t effective_attributes = { 0 };

    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    srv.default_max_period = 0;
    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    ASSERT_OK(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(3, 0),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED, 0));
    // skip /3/0/1/5
    // skip /3/0/1

    anj_process(ANJ_TIME_DURATION_ZERO, 0x23, 1);
    anj_exchange(false);
    check_out_buff(false, 0x23, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    anj_process(ANJ_TIME_DURATION_ZERO, 0x24, 1);
    anj_exchange(false);
    check_out_buff(false, 0x24, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
}

ANJ_UNIT_TEST(notification_op, notification_change_added_resource_instance) {
    /* There should never be a situation where we have a standard observation on
     * a non-existent path. In this case, we have such an unrealistic situation
     * to test the code. */
    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 1),
                               ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 2),
                               ANJ_MAKE_RESOURCE_PATH(3, 0, 6),
                               ANJ_MAKE_INSTANCE_PATH(3, 0),
                               ANJ_MAKE_OBJECT_PATH(3) };
    _anj_attr_notification_t effective_attributes = { 0 };

    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    inst_0.res_count = 7;
    srv.default_max_period = 0;
    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 1),
            ANJ_OBSERVE_CHANGE_TYPE_ADDED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);
    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    // skip /3/0/8/1

    anj_process(ANJ_TIME_DURATION_ZERO, 0x23, 1);
    anj_exchange(false);
    check_out_buff(false, 0x23, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    anj_process(ANJ_TIME_DURATION_ZERO, 0x24, 1);
    anj_exchange(false);
    check_out_buff(false, 0x24, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    anj_process(ANJ_TIME_DURATION_ZERO, 0x25, 1);
    anj_exchange(false);
    check_out_buff(false, 0x25, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(time_real_zero, ONE_DAY));

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
}

ANJ_UNIT_TEST(notification_op, notification_change_deleted) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    srv.default_max_period = 0;
    setup_observations(&anj.observe_ctx,
                       &(anj_uri_path_t[3]){ ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                                             ANJ_MAKE_INSTANCE_PATH(3, 1),
                                             ANJ_MAKE_OBJECT_PATH(3) }[0],
                       3, &(_anj_attr_notification_t) { 0 });

    anj.observe_ctx.attributes_storage[0].ssid = 1;
    anj.observe_ctx.attributes_storage[0].path =
            ANJ_MAKE_RESOURCE_PATH(3, 1, 1);

    anj.observe_ctx.attributes_storage[1].ssid = 2;
    anj.observe_ctx.attributes_storage[1].path = ANJ_MAKE_INSTANCE_PATH(3, 1);

    anj.observe_ctx.attributes_storage[2].ssid = 3;
    anj.observe_ctx.attributes_storage[2].path = ANJ_MAKE_INSTANCE_PATH(3, 1);

    anj.observe_ctx.attributes_storage[3].ssid = 3;
    anj.observe_ctx.attributes_storage[3].path = ANJ_MAKE_OBJECT_PATH(3);

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);

    mock_time_advance(anj_time_duration_new(4000, ANJ_TIME_UNIT_MS));

    ASSERT_OK(
            anj_observe_data_model_changed(&anj, &ANJ_MAKE_INSTANCE_PATH(3, 1),
                                           ANJ_OBSERVE_CHANGE_TYPE_DELETED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 1);
    ASSERT_TRUE(anj_time_real_eq(
            anj.observe_ctx.observations[0].last_notify_timestamp,
            time_real_zero));
    ASSERT_TRUE(anj_time_real_eq(
            anj.observe_ctx.observations[1].last_notify_timestamp,
            time_real_zero));
    ASSERT_TRUE(anj_time_real_eq(
            anj.observe_ctx.observations[2].last_notify_timestamp,
            time_real_zero));

    ASSERT_EQ(anj.observe_ctx.attributes_storage[0].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[3].ssid, 3);

    ASSERT_OK(anj_observe_data_model_changed(&anj, &ANJ_MAKE_OBJECT_PATH(3),
                                             ANJ_OBSERVE_CHANGE_TYPE_DELETED,
                                             0));

    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.attributes_storage[3].ssid, 0);
}

// check if we get correct time_to_next_call when there are two observations
// with different max period attributes
ANJ_UNIT_TEST(notification_op, time_to_next_call_different_max_period) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    setup_observations(&anj.observe_ctx,
                       (anj_uri_path_t[2]){ ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                                            ANJ_MAKE_RESOURCE_PATH(3, 1, 2) },
                       2,
                       &(_anj_attr_notification_t) {
                           .has_max_period = true,
                           .max_period = 10
                       });
    anj.observe_ctx.observations[1].effective_attr.max_period = 5;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(6000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
}

// check if we get correct time_to_next_call when there are two observations
// with and without max period attribute, max_period for latter one goes from
// server object instance
ANJ_UNIT_TEST(notification_op,
              time_to_next_call_different_default_max_period_smaller) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    setup_observations(&anj.observe_ctx,
                       (anj_uri_path_t[2]){ ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                                            ANJ_MAKE_RESOURCE_PATH(3, 1, 2) },
                       2,
                       &(_anj_attr_notification_t) {
                           .has_max_period = true,
                           .max_period = 2137
                       });
    anj.observe_ctx.observations[1].effective_attr.has_max_period = false;

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(78000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
}

// check if we get correct time_to_next_call when there are two observations
// with and without max period attribute, max_period for latter one goes from
// server object instance
ANJ_UNIT_TEST(notification_op,
              time_to_next_call_different_default_max_period_bigger) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    setup_observations(&anj.observe_ctx,
                       (anj_uri_path_t[2]){ ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                                            ANJ_MAKE_RESOURCE_PATH(3, 1, 2) },
                       2,
                       &(_anj_attr_notification_t) {
                           .has_max_period = true,
                           .max_period = 10
                       });
    anj.observe_ctx.observations[1].effective_attr.has_max_period = false;

    anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);
}

// check if we get correct time_to_next_call when there are two observations
// with different min period attributes
ANJ_UNIT_TEST(notification_op, time_to_next_call_different_min_period) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    setup_observations(&anj.observe_ctx,
                       (anj_uri_path_t[2]){ ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                                            ANJ_MAKE_RESOURCE_PATH(3, 0, 1) },
                       2,
                       &(_anj_attr_notification_t) {
                           .has_min_period = true,
                           .min_period = 10,
                           .has_greater_than = true,
                           .greater_than = 10.0
                       });
    anj.observe_ctx.observations[1].effective_attr.min_period = 5;

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(6000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
}

// check if we get correct time_to_next_call when there are two observations
// with and without min period attribute, min_period for latter one goes from
// server object instance
ANJ_UNIT_TEST(notification_op,
              time_to_next_call_different_default_min_period_smaller) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    srv.default_min_period = 10;
    setup_observations(&anj.observe_ctx,
                       (anj_uri_path_t[2]){ ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                                            ANJ_MAKE_RESOURCE_PATH(3, 0, 1) },
                       2,
                       &(_anj_attr_notification_t) {
                           .has_min_period = true,
                           .min_period = 2137,
                           .has_greater_than = true,
                           .greater_than = 10.0
                       });
    anj.observe_ctx.observations[1].effective_attr.has_min_period = false;

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
}

// check if we get correct time_to_next_call when there are two observations
// with and without min period attribute, min_period for latter one goes from
// server object instance
ANJ_UNIT_TEST(notification_op,
              time_to_next_call_different_default_min_period_bigger) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    srv.default_min_period = 2137;
    setup_observations(&anj.observe_ctx,
                       (anj_uri_path_t[2]){ ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                                            ANJ_MAKE_RESOURCE_PATH(3, 0, 1) },
                       2,
                       &(_anj_attr_notification_t) {
                           .has_min_period = true,
                           .min_period = 10,
                           .has_greater_than = true,
                           .greater_than = 10.0
                       });
    anj.observe_ctx.observations[1].effective_attr.has_min_period = false;

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(10000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);
}

ANJ_UNIT_TEST(notification_op, time_notification_for_different_servers) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    setup_observations(&anj.observe_ctx,
                       &(anj_uri_path_t[3]){ ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                                             ANJ_MAKE_INSTANCE_PATH(3, 1),
                                             ANJ_MAKE_OBJECT_PATH(3) }[0],
                       3, &(_anj_attr_notification_t) { 0 });
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[1].ssid = 2;
    anj.observe_ctx.observations[2].ssid = 3;
    anj.observe_ctx.observations[0].effective_attr.has_max_period = true;
    anj.observe_ctx.observations[1].effective_attr.has_max_period = true;
    anj.observe_ctx.observations[2].effective_attr.has_max_period = true;
    anj.observe_ctx.observations[0].effective_attr.max_period = 100;
    anj.observe_ctx.observations[1].effective_attr.max_period = 50;
    anj.observe_ctx.observations[2].effective_attr.max_period = 20;

    srv.ssid = 1;
    anj_process(anj_time_duration_new(100000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(100000, ANJ_TIME_UNIT_MS), 0, 0);

    srv.ssid = 2;
    anj_process(anj_time_duration_new(50000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(50000, ANJ_TIME_UNIT_MS), 0, 0);

    srv.ssid = 3;
    anj_process(anj_time_duration_new(20000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(20000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_op, read_callback_failed) {
    NOTIFICATION_INIT();
    _anj_observe_init(&anj);

    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                               ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_greater_than = true,
        .greater_than = 10.0
    };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(20.0);
    res_read_ret_val = ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    ASSERT_EQ(anj_observe_data_model_changed(
                      &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                      ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0),
              ANJ_COAP_CODE_INTERNAL_SERVER_ERROR);
    res_read_ret_val = 0;
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
}

ANJ_UNIT_TEST(notification_op, build_callback_failed) {
    NOTIFICATION_INIT();
    _anj_observe_init(&anj);

    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                               ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = { 0 };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);
    res_read_ret_val = ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    ASSERT_EQ(_anj_exchange_new_client_request(&exchange_ctx, &out_msg,
                                               &out_handlers, payload,
                                               payload_buff_size),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
    ASSERT_EQ(_anj_exchange_new_client_request(&exchange_ctx, &out_msg,
                                               &out_handlers, payload,
                                               payload_buff_size),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    res_read_ret_val = 0;
    memset(&out_msg, 0, sizeof(out_msg));
    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
}

ANJ_UNIT_TEST(notification_op, time_update) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();

    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = { 0 };

    mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);
    anj_time_real_t observation_timestamp_start = anj_time_real_now();

    /* So now last_notify_timestamp is set to 5000 but the current time is 2500
     */
    mock_time_advance(anj_time_duration_new(-2500, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);
    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    check_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths), false,
                       anj_time_real_add(observation_timestamp_start, ONE_DAY));

    anj_process(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_op, no_active_observation_for_server_ssid) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();

    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                               ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_max_period = true,
        .max_period = 5
    };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);
    anj.observe_ctx.observations[0].ssid = 10;
    anj.observe_ctx.observations[1].observe_active = false;

    anj_process(ANJ_TIME_DURATION_INVALID, 0, 0);
}

ANJ_UNIT_TEST(notification_op, dont_check_observation_without_ssid) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();

    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };
    _anj_attr_notification_t effective_attributes = {
        .has_max_period = true,
        .max_period = 5
    };

    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);
    anj.observe_ctx.observations[0].ssid = 0;

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, false);
}

ANJ_UNIT_TEST(notification_op,
              call_observation_api_in_the_middle_of_notification_handling) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();

    anj_uri_path_t paths[] = { ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
                               ANJ_MAKE_RESOURCE_PATH(3, 0, 1) };

    _anj_attr_notification_t effective_attributes = { 0 };
    setup_observations(&anj.observe_ctx, paths, ANJ_ARRAY_SIZE(paths),
                       &effective_attributes);

    mock_time_advance(anj_time_duration_new(77000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x21, 1);

    _anj_observe_observation_t *processing_observation =
            anj.observe_ctx.processing_observation;
    ASSERT_EQ((intptr_t) processing_observation,
              (intptr_t) &anj.observe_ctx.observations[0]);

    ASSERT_OK(anj_observe_time_to_next_notification(&anj, &srv,
                                                    &time_to_next_call));

    ASSERT_EQ((intptr_t) processing_observation,
              (intptr_t) anj.observe_ctx.processing_observation);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    ASSERT_EQ((intptr_t) processing_observation,
              (intptr_t) anj.observe_ctx.processing_observation);

    anj_exchange(false);
    check_out_buff(false, 0x21, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
}

#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
#        define SET_COMPOSITE_OBSERVATION()                              \
            inst_0.res_count = 7;                                        \
            anj.observe_ctx.observations[0].ssid = 1;                    \
            anj.observe_ctx.observations[0].token.size = 1;              \
            anj.observe_ctx.observations[0].token.bytes[0] = 0x22;       \
            anj.observe_ctx.observations[0].path =                       \
                    ANJ_MAKE_RESOURCE_PATH(3, 0, 2);                     \
            anj.observe_ctx.observations[0].prev =                       \
                    &anj.observe_ctx.observations[4];                    \
            anj.observe_ctx.observations[0].observe_active = true;       \
            anj.observe_ctx.observations[0].content_format_opt =         \
                    _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;                    \
            anj.observe_ctx.observations[0].accept_opt =                 \
                    _ANJ_COAP_FORMAT_SENML_CBOR;                         \
            anj.observe_ctx.observations[0].effective_attr =             \
                    (_anj_attr_notification_t) {                         \
                        .has_max_period = true,                          \
                        .max_period = 20                                 \
                    };                                                   \
            anj.observe_ctx.observations[0].next_conf_notify_timestamp = \
                    anj_time_real_add(time_real_zero, ONE_DAY);          \
            anj.observe_ctx.observations[0].last_notify_timestamp =      \
                    time_real_zero;                                      \
            anj.observe_ctx.observations[1].ssid = 1;                    \
            anj.observe_ctx.observations[1].token.size = 1;              \
            anj.observe_ctx.observations[1].token.bytes[0] = 0x23;       \
            anj.observe_ctx.observations[1].path =                       \
                    ANJ_MAKE_RESOURCE_PATH(3, 0, 2);                     \
            anj.observe_ctx.observations[1].prev = NULL;                 \
            anj.observe_ctx.observations[1].observe_active = true;       \
            anj.observe_ctx.observations[1].effective_attr =             \
                    (_anj_attr_notification_t) {                         \
                        .has_max_period = true,                          \
                        .max_period = 15                                 \
                    };                                                   \
            anj.observe_ctx.observations[1].next_conf_notify_timestamp = \
                    anj_time_real_add(time_real_zero, ONE_DAY);          \
            anj.observe_ctx.observations[1].last_notify_timestamp =      \
                    time_real_zero;                                      \
            anj.observe_ctx.observations[2].ssid = 1;                    \
            anj.observe_ctx.observations[2].token.size = 1;              \
            anj.observe_ctx.observations[2].token.bytes[0] = 0x22;       \
            anj.observe_ctx.observations[2].path =                       \
                    ANJ_MAKE_RESOURCE_PATH(3, 0, 3);                     \
            anj.observe_ctx.observations[2].prev =                       \
                    &anj.observe_ctx.observations[0];                    \
            anj.observe_ctx.observations[2].content_format_opt =         \
                    _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;                    \
            anj.observe_ctx.observations[2].accept_opt =                 \
                    _ANJ_COAP_FORMAT_SENML_CBOR;                         \
            anj.observe_ctx.observations[2].observe_active = true;       \
            anj.observe_ctx.observations[2].effective_attr =             \
                    (_anj_attr_notification_t) {                         \
                        .has_max_period = true,                          \
                        .max_period = 5                                  \
                    };                                                   \
            anj.observe_ctx.observations[2].next_conf_notify_timestamp = \
                    anj_time_real_add(time_real_zero, ONE_DAY);          \
            anj.observe_ctx.observations[2].last_notify_timestamp =      \
                    time_real_zero;                                      \
            anj.observe_ctx.observations[3].ssid = 1;                    \
            anj.observe_ctx.observations[3].token.size = 1;              \
            anj.observe_ctx.observations[3].token.bytes[0] = 0x22;       \
            anj.observe_ctx.observations[3].path =                       \
                    ANJ_MAKE_RESOURCE_PATH(4, 0, 1);                     \
            anj.observe_ctx.observations[3].prev =                       \
                    &anj.observe_ctx.observations[2];                    \
            anj.observe_ctx.observations[3].content_format_opt =         \
                    _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;                    \
            anj.observe_ctx.observations[3].accept_opt =                 \
                    _ANJ_COAP_FORMAT_SENML_CBOR;                         \
            anj.observe_ctx.observations[3].observe_active = true;       \
            anj.observe_ctx.observations[3].effective_attr =             \
                    (_anj_attr_notification_t) {                         \
                        .has_max_period = true,                          \
                        .max_period = 10                                 \
                    };                                                   \
            anj.observe_ctx.observations[3].next_conf_notify_timestamp = \
                    anj_time_real_add(time_real_zero, ONE_DAY);          \
            anj.observe_ctx.observations[3].last_notify_timestamp =      \
                    time_real_zero;                                      \
            anj.observe_ctx.observations[4].ssid = 1;                    \
            anj.observe_ctx.observations[4].token.size = 1;              \
            anj.observe_ctx.observations[4].token.bytes[0] = 0x22;       \
            anj.observe_ctx.observations[4].path =                       \
                    ANJ_MAKE_RESOURCE_INSTANCE_PATH(4, 0, 1, 1);         \
            anj.observe_ctx.observations[4].prev =                       \
                    &anj.observe_ctx.observations[3];                    \
            anj.observe_ctx.observations[4].content_format_opt =         \
                    _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;                    \
            anj.observe_ctx.observations[4].accept_opt =                 \
                    _ANJ_COAP_FORMAT_SENML_CBOR;                         \
            anj.observe_ctx.observations[4].observe_active = true;       \
            anj.observe_ctx.observations[4].effective_attr =             \
                    (_anj_attr_notification_t) {                         \
                        .has_max_period = true,                          \
                        .max_period = 10,                                \
                        .has_less_than = true                            \
                    };                                                   \
            anj.observe_ctx.observations[4].next_conf_notify_timestamp = \
                    anj_time_real_add(anj_time_real_now(), ONE_DAY);     \
            anj.observe_ctx.observations[4].last_notify_timestamp =      \
                    time_real_zero;

#        define CHECK_COMPOSITE_OBSERVATION(check_all_last_sent_val,           \
                                            Observe_number, confirmable)       \
            {                                                                  \
                ASSERT_EQ(anj.observe_ctx.observations[0].observe_number,      \
                          Observe_number);                                     \
                ASSERT_EQ(anj.observe_ctx.observations[1].observe_number, 0);  \
                ASSERT_EQ(anj.observe_ctx.observations[2].observe_number,      \
                          Observe_number);                                     \
                ASSERT_EQ(anj.observe_ctx.observations[3].observe_number,      \
                          Observe_number);                                     \
                ASSERT_EQ(anj.observe_ctx.observations[4].observe_number,      \
                          Observe_number);                                     \
                                                                               \
                ASSERT_TRUE(anj_time_real_eq(                                  \
                        anj.observe_ctx.observations[0].last_notify_timestamp, \
                        anj_time_real_now()));                                 \
                ASSERT_TRUE(anj_time_real_eq(                                  \
                        anj.observe_ctx.observations[1].last_notify_timestamp, \
                        time_real_zero));                                      \
                ASSERT_TRUE(anj_time_real_eq(                                  \
                        anj.observe_ctx.observations[2].last_notify_timestamp, \
                        anj_time_real_now()));                                 \
                ASSERT_TRUE(anj_time_real_eq(                                  \
                        anj.observe_ctx.observations[3].last_notify_timestamp, \
                        anj_time_real_now()));                                 \
                ASSERT_EQ(anj.observe_ctx.observations[2]                      \
                                  .last_sent_value.double_value,               \
                          get_res_value_double);                               \
                ASSERT_EQ(anj.observe_ctx.observations[4]                      \
                                  .last_sent_value.double_value,               \
                          get_res_value_double);                               \
                if (check_all_last_sent_val) {                                 \
                    ASSERT_EQ(anj.observe_ctx.observations[0]                  \
                                      .last_sent_value.double_value,           \
                              0);                                              \
                    ASSERT_EQ(anj.observe_ctx.observations[1]                  \
                                      .last_sent_value.double_value,           \
                              0);                                              \
                    ASSERT_EQ(anj.observe_ctx.observations[3]                  \
                                      .last_sent_value.double_value,           \
                              0);                                              \
                }                                                              \
                anj_time_real_t next_conf_notify_timestamp =                   \
                        confirmable                                            \
                                ? anj_time_real_add(anj_time_real_now(),       \
                                                    ONE_DAY)                   \
                                : anj_time_real_add(time_real_zero, ONE_DAY);  \
                ASSERT_TRUE(                                                   \
                        anj_time_real_eq(anj.observe_ctx.observations[0]       \
                                                 .next_conf_notify_timestamp,  \
                                         next_conf_notify_timestamp));         \
                ASSERT_TRUE(                                                   \
                        anj_time_real_eq(anj.observe_ctx.observations[2]       \
                                                 .next_conf_notify_timestamp,  \
                                         next_conf_notify_timestamp));         \
                ASSERT_TRUE(                                                   \
                        anj_time_real_eq(anj.observe_ctx.observations[3]       \
                                                 .next_conf_notify_timestamp,  \
                                         next_conf_notify_timestamp));         \
                ASSERT_TRUE(                                                   \
                        anj_time_real_eq(anj.observe_ctx.observations[4]       \
                                                 .next_conf_notify_timestamp,  \
                                         next_conf_notify_timestamp));         \
            }

#        define CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(state)         \
            ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, \
                      state);                                               \
            ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, \
                      state);                                               \
            ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, \
                      state);                                               \
            ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, \
                      state);

#        define CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(id)        \
            ASSERT_EQ(anj.observe_ctx.observations[0].ssid, id); \
            ASSERT_EQ(anj.observe_ctx.observations[2].ssid, id); \
            ASSERT_EQ(anj.observe_ctx.observations[3].ssid, id); \
            ASSERT_EQ(anj.observe_ctx.observations[4].ssid, id);

ANJ_UNIT_TEST(notification_comp_op, notification_max_period) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(false);

    CHECK_COMPOSITE_OBSERVATION(true, 1, false);

    check_out_buff(false, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op, notification_confirmable_after_24h) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();
    // turn off second observation
    anj.observe_ctx.observations[1].ssid = 0;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(
            anj_time_duration_new(ONE_DAY_MS - 5000, ANJ_TIME_UNIT_MS));
    // non-con notification
    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(false);
    CHECK_COMPOSITE_OBSERVATION(true, 1, false);
    check_out_buff(false, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    // ONE_DAY + some random value
    mock_time_advance(anj_time_duration_new(5000 + 123, ANJ_TIME_UNIT_MS));
    // con notification
    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(true);
    CHECK_COMPOSITE_OBSERVATION(true, 2, true);
    check_out_buff(true, 0x22, 2, _ANJ_COAP_FORMAT_SENML_CBOR);

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(6000 - 123, ANJ_TIME_UNIT_MS));
    // non-con notification
    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(false);
    ASSERT_TRUE(anj_time_real_eq(
            anj.observe_ctx.observations[0].next_conf_notify_timestamp,
            anj_time_real_add(time_real_zero,
                              anj_time_duration_new(2 * ONE_DAY_MS + 123,
                                                    ANJ_TIME_UNIT_MS))));
}

/* Check if last_sent_value is updated even if "Change Value Condition" wasn't
 * met during creation of notification, notification was created because of pmax
 */
ANJ_UNIT_TEST(notification_comp_op, notification_max_period_with_not_met_gt) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[0].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[0].effective_attr.greater_than = 5.0;
    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 5.0;
    anj.observe_ctx.observations[4].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[4].effective_attr.greater_than = 5.0;
    anj.observe_ctx.observations[4].effective_attr.has_less_than = false;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(1.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 1, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    mock_time_advance(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(false);

    /* last_sent_value should be updated even if "Change Value Condition" is not
     * the reason for creating the notification */
    CHECK_COMPOSITE_OBSERVATION(false, 1, false);
    ASSERT_EQ(anj.observe_ctx.observations[0].last_sent_value.double_value,
              get_res_value_double);
    ASSERT_EQ(anj.observe_ctx.observations[2].last_sent_value.double_value,
              get_res_value_double);
    ASSERT_EQ(anj.observe_ctx.observations[4].last_sent_value.double_value,
              get_res_value_double);

    check_out_buff(false, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op, notification_gt_in_one_observation) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();
    anj.observe_ctx.observations[1].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[1].effective_attr.greater_than = 5.0;

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    // change mock time for timestamp test
    mock_time_advance(anj_time_duration_new(50, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(false);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    CHECK_COMPOSITE_OBSERVATION(true, 1, false);

    check_out_buff(false, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
}
#        ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(notification_comp_op,
              notification_con_in_one_observation_disabled) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;

    anj.observe_ctx.observations[3].effective_attr.has_con = true;
    anj.observe_ctx.observations[3].effective_attr.con = 0;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    // change mock time for timestamp test
    mock_time_advance(anj_time_duration_new(50, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(false);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    CHECK_COMPOSITE_OBSERVATION(true, 1, false);

    check_out_buff(false, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op,
              notification_con_in_one_observation_enabled) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;

    anj.observe_ctx.observations[3].effective_attr.has_con = true;
    anj.observe_ctx.observations[3].effective_attr.con = 1;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    // change mock time for timestamp test
    mock_time_advance(anj_time_duration_new(50, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(true);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    CHECK_COMPOSITE_OBSERVATION(true, 1, true);

    check_out_buff(true, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op,
              notification_con_in_two_observations_enabled_and_disabled) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[0].effective_attr.has_con = true;
    anj.observe_ctx.observations[0].effective_attr.con = 0;

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;

    anj.observe_ctx.observations[3].effective_attr.has_con = true;
    anj.observe_ctx.observations[3].effective_attr.con = 1;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    // change mock time for timestamp test
    mock_time_advance(anj_time_duration_new(50, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(true);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);
    CHECK_COMPOSITE_OBSERVATION(true, 1, true);

    check_out_buff(true, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op, notification_con_from_server) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    srv.default_con = true;

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    // change mock time for timestamp test
    mock_time_advance(anj_time_duration_new(50, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(true);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);
    CHECK_COMPOSITE_OBSERVATION(true, 1, true);

    check_out_buff(true, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
}
#        endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(notification_comp_op,
              notification_call_data_model_added_and_then_changed) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();
    anj.observe_ctx.observations[0].path =
            ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 1);
    // composite with just one path
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[0].effective_attr.has_step = true;
    anj.observe_ctx.observations[0].effective_attr.step = 10;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 1),
            ANJ_OBSERVE_CHANGE_TYPE_ADDED, 0));

    set_res_value_double(5.0);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 1),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    // Even though 5.0 doesn't meet the "Change Value Condition", we send
    // notifications because previously we reported that the resource instance
    // was added
    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].last_sent_value.double_value,
              get_res_value_double);
    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, true);
}

ANJ_UNIT_TEST(notification_comp_op, added_new_object) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();
    anj.observe_ctx.observations[0].path = ANJ_MAKE_OBJECT_PATH(3);

    // to check if observation have proper last_sent_value
    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(&anj, &ANJ_MAKE_OBJECT_PATH(3),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    /* Standard observations above reported path should already exist */
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    // change mock time for timestamp test
    mock_time_advance(anj_time_duration_new(50, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(false);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[2].last_sent_value.double_value,
              get_res_value_double);
    ASSERT_EQ(anj.observe_ctx.observations[4].last_sent_value.double_value,
              get_res_value_double);
    CHECK_COMPOSITE_OBSERVATION(false, 1, false);

    check_out_buff(false, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op, added_new_object_instance) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    // to check if observation have proper last_sent_value
    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(3, 0),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED,
                                             0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    /* Standard observations above reported path should already exist */
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    // change mock time for timestamp test
    mock_time_advance(anj_time_duration_new(50, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(false);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[2].last_sent_value.double_value,
              get_res_value_double);
    ASSERT_EQ(anj.observe_ctx.observations[4].last_sent_value.double_value,
              get_res_value_double);
    CHECK_COMPOSITE_OBSERVATION(false, 1, false);

    check_out_buff(false, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op, added_new_resource_instance) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    // to check if observation have proper last_sent_value
    anj.observe_ctx.observations[0].effective_attr.has_step = true;
    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 2, 5),
            ANJ_OBSERVE_CHANGE_TYPE_ADDED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, true);

    // change mock time for timestamp test
    mock_time_advance(anj_time_duration_new(50, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(false);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[0].last_sent_value.double_value,
              get_res_value_double);
    CHECK_COMPOSITE_OBSERVATION(false, 1, false);

    check_out_buff(false, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);
    anj_process(ANJ_TIME_DURATION_ZERO, 0x23, 1);
}

ANJ_UNIT_TEST(
        notification_comp_op,
        added_new_resource_and_removed_attributes_which_cause_that_observation_is_now_active) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[0].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[0].effective_attr.has_less_than = true;
    anj.observe_ctx.observations[0].effective_attr.greater_than = 5.0;
    anj.observe_ctx.observations[0].effective_attr.less_than = 25.0;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 6);

    // effective attributes are incorrect (lt > gt) so observation will be
    // deactivated
    _anj_observe_verify_effective_attributes(&anj.observe_ctx.observations[0]);
    ASSERT_EQ(anj.observe_ctx.observations[0].observe_active, false);

    // in this function we will call
    // _anj_observe_check_if_value_condition_attributes_should_be_disabled and
    // because read_resource will indicate that the resource is multi-instance
    // the gt and lt attributes will be removed
    ASSERT_OK(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(3, 0),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED,
                                             0));

    ASSERT_EQ(anj.observe_ctx.observations[0].observe_active, true);
}

ANJ_UNIT_TEST(notification_comp_op,
              added_new_resource_but_not_removed_attributes) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[0].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[0].effective_attr.has_less_than = true;
    anj.observe_ctx.observations[0].effective_attr.greater_than = 5.0;
    anj.observe_ctx.observations[0].effective_attr.less_than = 25.0;

    // effective attributes are incorrect (lt > gt) so observation will be
    // deactivated
    _anj_observe_verify_effective_attributes(&anj.observe_ctx.observations[0]);
    ASSERT_EQ(anj.observe_ctx.observations[0].observe_active, false);

    // in this function we will call
    // _anj_observe_check_if_value_condition_attributes_should_be_disabled but
    // read_resource will indicate that the resource is not multi-instance
    ASSERT_OK(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(3, 0),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED,
                                             0));

    ASSERT_EQ(anj.observe_ctx.observations[0].observe_active, false);
}

#        ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(notification_comp_op,
              ignore_change_value_attr_if_path_to_multi_res) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[0].effective_attr.has_step = true;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 6);
    anj.observe_ctx.observations[2].effective_attr.has_edge = true;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 6);
    anj.observe_ctx.observations[3].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_RESOURCE_PATH(4, 0, 1);

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    ASSERT_OK(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(3, 0),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED,
                                             0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
    // Attributes gt/lt/st/edge should be disable at this step if the path
    // points to a multi-instance resource, and the path didn't exist in the
    // data model when composite observation was added
    ASSERT_EQ(anj.observe_ctx.observations[0].effective_attr.has_step, false);
    ASSERT_EQ(anj.observe_ctx.observations[2].effective_attr.has_edge, false);
    ASSERT_EQ(anj.observe_ctx.observations[3].effective_attr.has_greater_than,
              true);
}

ANJ_UNIT_TEST(notification_comp_op, notification_con_get_reset) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    srv.default_con = true;

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    // change mock time for timestamp test
    mock_time_advance(anj_time_duration_new(50, ANJ_TIME_UNIT_MS));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    anj_exchange(-1);

    check_out_buff(true, 0x22, 1, _ANJ_COAP_FORMAT_SENML_CBOR);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);

    anj_process(anj_time_duration_new(14950, ANJ_TIME_UNIT_MS), 0, 0);
}
#        endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(notification_comp_op, unrelated_paths) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(4, 0, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(4, 0, 3, 5),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);

    ASSERT_OK(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(4, 1),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED,
                                             0));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);

    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(4, 0, 3, 5),
            ANJ_OBSERVE_CHANGE_TYPE_ADDED, 0));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);

    ASSERT_OK(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(3, 2),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED,
                                             0));

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);

    ASSERT_OK(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(3, 0),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED,
                                             0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);
}

ANJ_UNIT_TEST(notification_comp_op, read_failed_in_model_changed_when_changed) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    res_read_ret_val = -1;
    ASSERT_EQ(anj_observe_data_model_changed(
                      &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
                      ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0),
              ANJ_COAP_CODE_BAD_REQUEST);
    res_read_ret_val = 0;

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);

    anj_process(anj_time_duration_new(15000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op, read_failed_in_process_when_changed) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();
    res_read_ret_val = -1;

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 2),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    // read_resource callback will be called from this function
    ASSERT_EQ(_anj_observe_process(&anj, &out_handlers, &srv, &out_msg),
              ANJ_COAP_CODE_BAD_REQUEST);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, true);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);

    // there is no change value condition attributes for observations[1] so we
    // do not call read_resource callback
    anj_process(ANJ_TIME_DURATION_ZERO, 0x23, 1);
    res_read_ret_val = 0;
}

ANJ_UNIT_TEST(notification_comp_op, read_failed_in_model_changed_when_added) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(20.0);
    res_read_ret_val = -1;
    ASSERT_EQ(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(3, 0),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED,
                                             0),
              ANJ_COAP_CODE_BAD_REQUEST);
    res_read_ret_val = 0;

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);

    anj_process(anj_time_duration_new(15000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op, read_failed_in_process_when_added) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();
    res_read_ret_val = -1;

    anj.observe_ctx.observations[0].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[0].effective_attr.greater_than = 25.0;

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    /* read_resource is not called here, we expect that /3/0/2
     * already exists, and since multi-instance resources cannot have "Change
     * Value Condition" attributes there is no need for checking those
     * attributes */
    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 2, 5),
            ANJ_OBSERVE_CHANGE_TYPE_ADDED, 0));

    // read_resource callback will be called in this function
    ASSERT_EQ(_anj_observe_process(&anj, &out_handlers, &srv, &out_msg),
              ANJ_COAP_CODE_BAD_REQUEST);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, true);

    // there is no change value condition attributes so we do not call
    // read_resource callback
    anj_process(ANJ_TIME_DURATION_ZERO, 0x23, 1);
    res_read_ret_val = 0;
}

ANJ_UNIT_TEST(notification_comp_op,
              is_any_readable_return_not_found_in_model_changed_when_added) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[2].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[2].effective_attr.greater_than = 25.0;
    anj.observe_ctx.observations[2].last_sent_value.int_value = 777;
    // /3/0/11 doesn't exist
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 11);

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    /* is_any_resource_readable may return ANJ_COAP_CODE_NOT_FOUND, it means
     * that path doesn't exists */
    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(&anj,
                                             &ANJ_MAKE_INSTANCE_PATH(3, 0),
                                             ANJ_OBSERVE_CHANGE_TYPE_ADDED,
                                             0));

    CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);

    /* Since is_any_resource_readable returns ANJ_COAP_CODE_NOT_FOUND we do not
     * update last_sent_value */
    ASSERT_EQ(anj.observe_ctx.observations[2].last_sent_value.int_value, 777);
}

ANJ_UNIT_TEST(notification_comp_op,
              is_any_readable_return_not_found_in_process_when_added) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[0].effective_attr.has_greater_than = true;
    anj.observe_ctx.observations[0].effective_attr.greater_than = 25.0;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 17);

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    /* is_any_resource_readable is not called here, we expect that /3/0/17
     * already exists, and since multi-instance resources cannot have "Change
     * Value Condition" attributes there is no need for checking those
     * attributes */
    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 17, 5),
            ANJ_OBSERVE_CHANGE_TYPE_ADDED, 0));

    CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    ASSERT_EQ(anj.observe_ctx.observations[0].last_sent_value.double_value, 0);

    /* is_any_resource_readable is called here, but in real-life scenario it
     * wouldn't be (at least for anj.observe_ctx.observations[0]) because
     * multi-instance resources cannot have "Change Value Condition" attributes
     */
    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
}

ANJ_UNIT_TEST(notification_comp_op, build_callback_failed) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(20.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);
    res_read_ret_val = -1;
    ASSERT_EQ(_anj_exchange_new_client_request(&exchange_ctx, &out_msg,
                                               &out_handlers, payload,
                                               payload_buff_size),
              ANJ_EXCHANGE_STATE_FINISHED);
    res_read_ret_val = 0;
    CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_NOTIF_STATE(false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    memset(&out_msg, 0, sizeof(out_msg));
    anj_process(anj_time_duration_new(15000, ANJ_TIME_UNIT_MS), 0, 0);
}

ANJ_UNIT_TEST(notification_comp_op,
              is_any_readable_return_not_found_in_process_when_changed) {
    NOTIFICATION_INIT();
    INIT_OBSERVE_MODULE();
    SET_COMPOSITE_OBSERVATION();

    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 17);

    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);
    anj_process(anj_time_duration_new(5000, ANJ_TIME_UNIT_MS), 0, 0);

    set_res_value_double(30.0);
    ASSERT_OK(anj_observe_data_model_changed(
            &anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 17),
            ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED, 0));

    ASSERT_EQ(anj.observe_ctx.observations[0].notification_to_send, true);
    ASSERT_EQ(anj.observe_ctx.observations[2].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[3].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[4].notification_to_send, false);
    ASSERT_EQ(anj.observe_ctx.observations[1].notification_to_send, false);

    /* is_any_resource_readable may return FLUF_COAP_CODE_NOT_FOUND, it means
     * that path doesn't exists, but it should never happen in such a situation
     */
    anj_process(ANJ_TIME_DURATION_ZERO, 0x22, 1);

    CHECK_ALL_COMPOSITE_OBSERVATIONS_SSID(1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
}

#    endif // ANJ_WITH_OBSERVE_COMPOSITE
#endif     // ANJ_WITH_OBSERVE
