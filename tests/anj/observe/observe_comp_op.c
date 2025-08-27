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

void compare_attr(_anj_attr_notification_t *attr1,
                  _anj_attr_notification_t *attr2);

void compare_observations(_anj_observe_ctx_t *ctx1, _anj_observe_ctx_t *ctx2);

void add_attr_storage(_anj_observe_attr_storage_t *attr,
                      anj_uri_path_t path,
                      _anj_attr_notification_t notif_attr,
                      uint16_t ssid);

static double get_res_value_double = 7.0;
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

    if (rid == 3 || rid == 4 || rid == 5 || rid == 9 || rid == 10) {
        out_value->bool_value = true;
    } else if (rid == 1 || rid == 2 || rid == 7 || rid == 8) {
        out_value->double_value = get_res_value_double;
    } else {
        out_value->int_value = 1;
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
        .type = ANJ_DATA_TYPE_DOUBLE,
    },
    {
        .rid = 3,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_BOOL,
    },
    {
        .rid = 4,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_BOOL,
    },
    {
        .rid = 5,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_BOOL,
    },
    {
        .rid = 6,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
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
        .insts = (anj_riid_t[]){ 0 }
    },
    {
        .rid = 10,
        .operation = ANJ_DM_RES_WM,
        .type = ANJ_DATA_TYPE_BOOL,
        .max_inst_count = 0
    },
};
static anj_dm_obj_inst_t inst_0 = {
    .iid = 0,
    .res_count = 10,
    .resources = inst_0_res
};
static anj_dm_obj_t obj_3 = {
    .oid = 3,
    .insts = &inst_0,
    .max_inst_count = 1,
    .handlers = &handlers
};
static anj_dm_obj_t obj_4 = {
    .oid = 4,
    .max_inst_count = 0
};

#    define TEST_INIT()                          \
        anj_t anj = { 0 };                       \
        _anj_observe_init(&anj);                 \
        _anj_dm_initialize(&anj);                \
        ASSERT_OK(anj_dm_add_obj(&anj, &obj_3)); \
        ASSERT_OK(anj_dm_add_obj(&anj, &obj_4));

#    ifdef ANJ_WITH_OBSERVE_COMPOSITE

#        define OBSERVE_COMP_OP(Attr, Result, Msg_Code, Payload, Format,       \
                                Accept, AlreadyProcessed)                      \
            _anj_exchange_ctx_t exchange_ctx;                                  \
            _anj_exchange_handlers_t out_handlers;                             \
            _anj_exchange_init(&exchange_ctx, 0);                              \
            _anj_coap_msg_t inout_msg = {                                      \
                .operation = ANJ_OP_INF_OBSERVE_COMP,                          \
                .payload = Payload,                                            \
                .payload_size = sizeof(Payload) - 1,                           \
                .attr.notification_attr = Attr,                                \
                .content_format = Format,                                      \
                .accept = Accept                                               \
            };                                                                 \
            inout_msg.coap_binding_data.udp.message_id = 0x1111;               \
            inout_msg.token.size = 1;                                          \
            inout_msg.token.bytes[0] = 0x22;                                   \
            uint8_t response_code;                                             \
            uint8_t out_buff[1200];                                            \
            size_t out_buff_len = sizeof(out_buff);                            \
            uint8_t payload_buff[1024];                                        \
            size_t payload_buff_len = sizeof(payload_buff);                    \
            ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv,      \
                                               &inout_msg, &response_code),    \
                      Result);                                                 \
            ASSERT_EQ(_anj_exchange_new_server_request(                        \
                              &exchange_ctx, response_code, &inout_msg,        \
                              &out_handlers, payload_buff, payload_buff_len),  \
                      ANJ_EXCHANGE_STATE_MSG_TO_SEND);                         \
            size_t out_msg_size = 0;                                           \
            ASSERT_EQ(anj.observe_ctx.already_processed, AlreadyProcessed);    \
            ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len, \
                                           &out_msg_size));                    \
            uint8_t EXPECTED_POSITIVE[] =                                      \
                    "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */              \
                    "\xFF\x11\x11\x22"                                         \
                    "\x60"     /*observe = 0*/                                 \
                    "\x61\x70" /*content format = 112*/                        \
                    "\xFF";                                                    \
            uint8_t EXPECTED_NEGATIVE[] =                                      \
                    "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */              \
                    "\xFF\x11\x11\x22";                                        \
            size_t EXPECTED_POSITIVE_SIZE = sizeof(EXPECTED_POSITIVE) - 1;     \
            size_t EXPECTED_NEGATIVE_SIZE = sizeof(EXPECTED_NEGATIVE) - 1;     \
                                                                               \
            uint8_t *EXPECTED = Msg_Code == ANJ_COAP_CODE_CONTENT              \
                                        ? EXPECTED_POSITIVE                    \
                                        : EXPECTED_NEGATIVE;                   \
            size_t EXPECTED_SIZE = Msg_Code == ANJ_COAP_CODE_CONTENT           \
                                           ? EXPECTED_POSITIVE_SIZE            \
                                           : EXPECTED_NEGATIVE_SIZE;           \
                                                                               \
            EXPECTED[1] = Msg_Code;                                            \
            ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED, EXPECTED_SIZE);          \
            ASSERT_EQ(_anj_exchange_process(                                   \
                              &exchange_ctx,                                   \
                              ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,            \
                              &inout_msg),                                     \
                      ANJ_EXCHANGE_STATE_FINISHED);

#        define OBSERVE_COMP_OP_TEST_WITH_SETTABLE_ALREADY_PROCESS(          \
                Payload, AlreadyProcessed)                                   \
            set_mock_time(0);                                                \
            _anj_observe_ctx_t ctx_ref;                                      \
            memset(&ctx_ref, 0, sizeof(ctx_ref));                            \
                                                                             \
            size_t records_number = *(uint8_t *) Payload - 0x80;             \
            /* Default configuration, change it in test itself if needed */  \
            for (size_t i = 0; i < records_number; i++) {                    \
                ctx_ref.observations[i].ssid = 1;                            \
                ctx_ref.observations[i].token.size = 1;                      \
                ctx_ref.observations[i].token.bytes[0] = 0x22;               \
                ctx_ref.observations[i].path =                               \
                        ANJ_MAKE_RESOURCE_PATH(3, 0, 2 + i);                 \
                ctx_ref.observations[i].observe_active = true;               \
                ctx_ref.observations[i].prev =                               \
                        &anj.observe_ctx                                     \
                                 .observations[(i == 0 ? records_number : i) \
                                               - 1];                         \
                ctx_ref.observations[i].last_notify_timestamp =              \
                        anj_time_real_now();                                 \
                ctx_ref.observations[i].next_conf_notify_timestamp =         \
                        anj_time_real_now() + ONE_DAY_MS;                    \
                ctx_ref.observations[i].effective_attr =                     \
                        (_anj_attr_notification_t) { 0 };                    \
                ctx_ref.observations[i].content_format_opt =                 \
                        _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;                    \
                ctx_ref.observations[i].accept_opt =                         \
                        _ANJ_COAP_FORMAT_SENML_CBOR;                         \
            }                                                                \
                                                                             \
            _anj_observe_server_state_t srv = {                              \
                .ssid = 1,                                                   \
                .default_max_period = 77,                                    \
            };                                                               \
            OBSERVE_COMP_OP((_anj_attr_notification_t) { 0 }, 0,             \
                            ANJ_COAP_CODE_CONTENT, Payload,                  \
                            _ANJ_COAP_FORMAT_SENML_ETCH_CBOR,                \
                            _ANJ_COAP_FORMAT_SENML_CBOR, AlreadyProcessed)

#        define OBSERVE_COMP_OP_TEST(Payload)                   \
            OBSERVE_COMP_OP_TEST_WITH_SETTABLE_ALREADY_PROCESS( \
                    Payload, *(uint8_t *) Payload - 0x80)

#        ifdef ANJ_WITH_LWM2M12
#            define OBSERVE_COMP_OP_WITH_ATTR_TEST(Attr, Result, Msg_Code,   \
                                                   Payload)                  \
                set_mock_time(0);                                            \
                _anj_observe_ctx_t ctx_ref;                                  \
                memset(&ctx_ref, 0, sizeof(ctx_ref));                        \
                                                                             \
                size_t records_number = *(uint8_t *) Payload - 0x80;         \
                                                                             \
                /* Default configuration, change it in test itself if needed \
                 */                                                          \
                for (size_t i = 0; i < records_number; i++) {                \
                    ctx_ref.observations[i].ssid = 1;                        \
                    ctx_ref.observations[i].token.size = 1;                  \
                    ctx_ref.observations[i].token.bytes[0] = 0x22;           \
                    ctx_ref.observations[i].path =                           \
                            ANJ_MAKE_RESOURCE_PATH(3, 0, 2 + i);             \
                    ctx_ref.observations[i].observe_active = true;           \
                    ctx_ref.observations[i].prev =                           \
                            &anj.observe_ctx.observations                    \
                                     [(i == 0 ? records_number : i) - 1];    \
                    ctx_ref.observations[i].last_notify_timestamp =          \
                            anj_time_real_now();                             \
                    ctx_ref.observations[i].next_conf_notify_timestamp =     \
                            anj_time_real_now() + ONE_DAY_MS;                \
                    ctx_ref.observations[i].effective_attr = Attr;           \
                    ctx_ref.observations[i].observation_attr = Attr;         \
                    ctx_ref.observations[i].content_format_opt =             \
                            _ANJ_COAP_FORMAT_SENML_CBOR;                     \
                    ctx_ref.observations[i].accept_opt =                     \
                            _ANJ_COAP_FORMAT_SENML_CBOR;                     \
                }                                                            \
                                                                             \
                _anj_observe_server_state_t srv = {                          \
                    .ssid = 1,                                               \
                    .default_min_period = 12,                                \
                };                                                           \
                OBSERVE_COMP_OP(Attr, Result, Msg_Code, Payload,             \
                                _ANJ_COAP_FORMAT_SENML_CBOR,                 \
                                _ANJ_COAP_FORMAT_SENML_CBOR,                 \
                                Msg_Code == ANJ_COAP_CODE_CONTENT            \
                                        ? records_number                     \
                                        : 0)
#        endif // ANJ_WITH_LWM2M12

#        define OBSERVE_COMP_OP_TEST_ERROR(Attr, Result, Msg_Code, Payload, \
                                           Format, Accept)                  \
            _anj_observe_ctx_t ctx_ref;                                     \
            memset(&ctx_ref, 0, sizeof(ctx_ref));                           \
                                                                            \
            _anj_observe_server_state_t srv = {                             \
                .ssid = 1,                                                  \
                .default_max_period = 77,                                   \
            };                                                              \
            OBSERVE_COMP_OP(Attr, Result, Msg_Code, Payload, Format, Accept, 0)

ANJ_UNIT_TEST(observe_comp_op, composite_observation_one_records) {
    TEST_INIT();
    uint8_t payload[] = {
        "\x81"       /* array(1) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST(payload);
    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, composite_observation_two_records) {
    TEST_INIT();
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST(payload);
    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, composite_observation_four_records) {
    TEST_INIT();
    uint8_t payload[] = {
        "\x84"       /* array(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/4" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/5" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST(payload);
    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(
        observe_comp_op,
        composite_observation_test_removing_paths_that_dont_exist_in_data_model) {
    uint8_t payload1[] = {
        "\x84"       /* array(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/4" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/5" /* text(6) */
    };

    for (size_t i = 1; i < 6; i++) {
        TEST_INIT();
        // for res_count = 1, only rid 1 is present in data model
        // for res_count = 2, rid 1 and 2 are present in data model..
        inst_0.res_count = i;
        OBSERVE_COMP_OP_TEST_WITH_SETTABLE_ALREADY_PROCESS(payload1, i - 1);
        compare_observations(&anj.observe_ctx, &ctx_ref);
    }
    inst_0.res_count = 10;

    uint8_t payload2[] = {
        "\x85"       /* array(5) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/4" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/5" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/6" /* text(6) */
    };

    for (size_t i = 1; i < 7; i++) {
        TEST_INIT();
        inst_0.res_count = i;
        OBSERVE_COMP_OP_TEST_WITH_SETTABLE_ALREADY_PROCESS(payload2, i - 1);
        compare_observations(&anj.observe_ctx, &ctx_ref);
    }
    inst_0.res_count = 10;
}

#        define REQUEST_BLOCK_TRANSFER(First, Last, Expected)                  \
            if (First) {                                                       \
                ASSERT_EQ(_anj_exchange_new_server_request(                    \
                                  &exchange_ctx, response_code, &inout_msg,    \
                                  &out_handlers, payload_buff, 100),           \
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);                     \
                                                                               \
            } else {                                                           \
                ASSERT_EQ(_anj_exchange_process(&exchange_ctx,                 \
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,    \
                                                &inout_msg),                   \
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);                     \
            }                                                                  \
                                                                               \
            ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len, \
                                           &out_msg_size));                    \
                                                                               \
            ASSERT_EQ_BYTES_SIZED(out_buff, Expected, sizeof(Expected) - 1);   \
                                                                               \
            ASSERT_EQ(_anj_exchange_process(                                   \
                              &exchange_ctx,                                   \
                              ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,            \
                              &inout_msg),                                     \
                      Last ? ANJ_EXCHANGE_STATE_FINISHED                       \
                           : ANJ_EXCHANGE_STATE_WAITING_MSG);

ANJ_UNIT_TEST(observe_comp_op, composite_observation_four_records_block) {
    _anj_observe_ctx_t ctx_ref;
    memset(&ctx_ref, 0, sizeof(ctx_ref));
    TEST_INIT();
    _anj_exchange_ctx_t exchange_ctx;
    _anj_exchange_handlers_t out_handlers;
    _anj_exchange_init(&exchange_ctx, 0);

    _anj_observe_server_state_t srv = {
        .ssid = 1,
        .default_max_period = 77,
    };
    size_t block_size = 16;
    uint8_t payload[] = {
        "\x84"       /* array(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/4" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/5" /* text(6) */
    };

    _anj_coap_msg_t inout_msg = {
        .operation = ANJ_OP_INF_OBSERVE_COMP,
        .payload = payload,
        .payload_size = 16,
        .content_format = _ANJ_COAP_FORMAT_SENML_CBOR,
        .accept = _ANJ_COAP_FORMAT_SENML_CBOR,
        .block.block_type = ANJ_OPTION_BLOCK_1,
        .block.size = block_size,
        .block.number = 0,
        .block.more_flag = true
    };
    inout_msg.coap_binding_data.udp.message_id = 0x1111;
    inout_msg.token.size = 1;
    inout_msg.token.bytes[0] = 0x22;
    uint8_t response_code;
    uint8_t out_buff[100];
    size_t out_buff_len = sizeof(out_buff);
    uint8_t payload_buff[100];
    ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv, &inout_msg,
                                       &response_code),
              0);

    size_t out_msg_size = 0;
    uint8_t EXPECTED1[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x11\x22"
                          "\xD1\x0E\x08"; // block1 0 more
    EXPECTED1[1] = ANJ_COAP_CODE_CONTINUE;

    REQUEST_BLOCK_TRANSFER(true, false, EXPECTED1);
    inout_msg.operation = ANJ_OP_INF_OBSERVE_COMP;
    inout_msg.payload = &payload[block_size];
    inout_msg.payload_size = 16;
    inout_msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.block.number = 1;
    inout_msg.coap_binding_data.udp.message_id++;

    uint8_t EXPECTED2[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x12\x22"
                          "\xD1\x0E\x18"; // block1 1 more
    EXPECTED2[1] = ANJ_COAP_CODE_CONTINUE;

    REQUEST_BLOCK_TRANSFER(false, false, EXPECTED2);
    inout_msg.operation = ANJ_OP_INF_OBSERVE_COMP;
    inout_msg.payload = &payload[2 * block_size];
    inout_msg.payload_size = sizeof(payload) - 2 * block_size - 1;
    inout_msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.block.number = 2;
    inout_msg.block.more_flag = false;
    inout_msg.coap_binding_data.udp.message_id++;

    uint8_t EXPECTED3[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x13\x22"
                          "\x60"         /*observe = 0*/
                          "\x61\x70"     /*content format = 112*/
                          "\xD1\x02\x20" /*block1 2*/
                          "\xFF";
    EXPECTED3[1] = ANJ_COAP_CODE_CONTENT;

    REQUEST_BLOCK_TRANSFER(false, true, EXPECTED3);

    for (size_t i = 0; i < 4; i++) {
        ctx_ref.observations[i].ssid = 1;
        ctx_ref.observations[i].token.size = 1;
        ctx_ref.observations[i].token.bytes[0] = 0x22;
        ctx_ref.observations[i].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2 + i);
        ctx_ref.observations[i].observe_active = true;
        ctx_ref.observations[i].prev =
                &anj.observe_ctx.observations[(i == 0 ? 4 : i) - 1];
        ctx_ref.observations[i].last_notify_timestamp = anj_time_real_now();
        ctx_ref.observations[i].next_conf_notify_timestamp =
                anj_time_real_now() + ONE_DAY_MS;
        ctx_ref.observations[i].effective_attr =
                (_anj_attr_notification_t) { 0 };
        ctx_ref.observations[i].content_format_opt =
                _ANJ_COAP_FORMAT_SENML_CBOR;
        ctx_ref.observations[i].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    }

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

#        ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(observe_comp_op,
              composite_observation_two_records_block_with_attribute) {
    _anj_observe_ctx_t ctx_ref;
    memset(&ctx_ref, 0, sizeof(ctx_ref));
    TEST_INIT();
    _anj_exchange_ctx_t exchange_ctx;
    _anj_exchange_handlers_t out_handlers;
    _anj_exchange_init(&exchange_ctx, 0);

    _anj_observe_server_state_t srv = {
        .ssid = 1,
        .default_max_period = 77,
    };
    size_t block_size = 16;
    uint8_t payload[] = {
        "\x82"       /* array(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    _anj_coap_msg_t inout_msg = {
        .operation = ANJ_OP_INF_OBSERVE_COMP,
        .payload = payload,
        .payload_size = 16,
        .content_format = _ANJ_COAP_FORMAT_SENML_CBOR,
        .accept = _ANJ_COAP_FORMAT_SENML_CBOR,
        .block.block_type = ANJ_OPTION_BLOCK_1,
        .block.size = block_size,
        .block.number = 0,
        .block.more_flag = true
    };
    inout_msg.coap_binding_data.udp.message_id = 0x1111;
    inout_msg.token.size = 1;
    inout_msg.token.bytes[0] = 0x22;
    inout_msg.attr.notification_attr = (_anj_attr_notification_t) {
        .has_max_period = true,
        .max_period = 420
    };
    uint8_t response_code;
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[1].ssid = 1;
    uint8_t out_buff[100];
    size_t out_buff_len = sizeof(out_buff);
    uint8_t payload_buff[100];
    ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv, &inout_msg,
                                       &response_code),
              0);

    size_t out_msg_size = 0;
    uint8_t EXPECTED1[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x11\x22"
                          "\xD1\x0E\x08"; // block1 0 more
    EXPECTED1[1] = ANJ_COAP_CODE_CONTINUE;

    REQUEST_BLOCK_TRANSFER(true, false, EXPECTED1);
    inout_msg.operation = ANJ_OP_INF_OBSERVE_COMP;
    inout_msg.payload = &payload[block_size];
    inout_msg.payload_size = sizeof(payload) - block_size - 1;
    inout_msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.block.number = 1;
    inout_msg.block.more_flag = false;
    inout_msg.coap_binding_data.udp.message_id++;

    uint8_t EXPECTED2[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x12\x22"
                          "\x60"         /*observe = 0*/
                          "\x61\x70"     /*content format = 112*/
                          "\xD1\x02\x10" /*block1 1*/
                          "\xFF";
    EXPECTED2[1] = ANJ_COAP_CODE_CONTENT;

    REQUEST_BLOCK_TRANSFER(false, true, EXPECTED2);

    ctx_ref.observations[0].ssid = 1;
    ctx_ref.observations[1].ssid = 1;
    for (size_t i = 2; i < 4; i++) {
        ctx_ref.observations[i].ssid = 1;
        ctx_ref.observations[i].token.size = 1;
        ctx_ref.observations[i].token.bytes[0] = 0x22;
        ctx_ref.observations[i].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2 + i - 2);
        ctx_ref.observations[i].observe_active = true;
        ctx_ref.observations[i].prev =
                &anj.observe_ctx.observations[(i == 2 ? 4 : i) - 1];
        ctx_ref.observations[i].last_notify_timestamp = anj_time_real_now();
        ctx_ref.observations[i].next_conf_notify_timestamp =
                anj_time_real_now() + ONE_DAY_MS;
        ctx_ref.observations[i].effective_attr = (_anj_attr_notification_t) {
            .has_max_period = true,
            .max_period = 420
        };
        ctx_ref.observations[i].observation_attr = (_anj_attr_notification_t) {
            .has_max_period = true,
            .max_period = 420,
        };
        ctx_ref.observations[i].content_format_opt =
                _ANJ_COAP_FORMAT_SENML_CBOR;
        ctx_ref.observations[i].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    }
    compare_observations(&anj.observe_ctx, &ctx_ref);
}
#        endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(observe_comp_op, observe_composite_effective_attr) {
    TEST_INIT();
    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_OBJECT_PATH(3),
                     (_anj_attr_notification_t) {
                         .has_min_period = true,
                         .min_period = 20,
                     },
                     2);
    add_attr_storage(&anj.observe_ctx.attributes_storage[1],
                     ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
                     (_anj_attr_notification_t) {
                         .has_max_eval_period = true,
                         .max_eval_period = 2137,
                         .has_max_period = true,
                         .max_period = 100,
                     },
                     1);
    add_attr_storage(&anj.observe_ctx.attributes_storage[2],
                     ANJ_MAKE_INSTANCE_PATH(3, 0),
                     (_anj_attr_notification_t) {
                         .has_min_period = true,
                         .min_period = 10,
                         .has_min_eval_period = true,
                         .min_eval_period = 10,
                     },
                     1);
    add_attr_storage(&anj.observe_ctx.attributes_storage[3],
                     ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 9, 1),
                     (_anj_attr_notification_t) {
                         .has_min_period = true,
                         .min_period = 5,
                         .has_step = true,
                         .step = 2,
                     },
                     1);
    add_attr_storage(&anj.observe_ctx.attributes_storage[4],
                     ANJ_MAKE_RESOURCE_PATH(3, 0, 9),
                     (_anj_attr_notification_t) {
                         .has_min_eval_period = true,
                         .min_eval_period = 11,
                     },
                     1);
    uint8_t payload[] = {
        "\x83"       /* array(3) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/9" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x64/3/0"   /* text(4) */
    };
    OBSERVE_COMP_OP_TEST(payload);

    ctx_ref.observations[0].effective_attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 10,
        .has_min_eval_period = true,
        .min_eval_period = 11,
    };
    ctx_ref.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 9);

    ctx_ref.observations[1].effective_attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 10,
        .has_min_eval_period = true,
        .min_eval_period = 10,
        .has_max_eval_period = true,
        .max_eval_period = 2137,
        .has_max_period = true,
        .max_period = 100
    };

    ctx_ref.observations[2].effective_attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 10,
        .has_min_eval_period = true,
        .min_eval_period = 10
    };
    ctx_ref.observations[2].path = ANJ_MAKE_INSTANCE_PATH(3, 0);

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_existing_records) {
    TEST_INIT();

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[2].ssid = 2;
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST(payload);

    ctx_ref.observations[3] = ctx_ref.observations[1];
    ctx_ref.observations[1] = ctx_ref.observations[0];
    ctx_ref.observations[1].prev = &anj.observe_ctx.observations[3];
    ctx_ref.observations[3].prev = &anj.observe_ctx.observations[1];
    memset(&ctx_ref.observations[0], 0, sizeof(ctx_ref.observations[0]));
    memset(&ctx_ref.observations[2], 0, sizeof(ctx_ref.observations[0]));
    ctx_ref.observations[0].ssid = 1;
    ctx_ref.observations[2].ssid = 2;

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_inactive_to_active) {
    TEST_INIT();
    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_RESOURCE_PATH(3, 0, 2),
                     (_anj_attr_notification_t) {
                         .has_min_eval_period = true,
                         .min_eval_period = 20,
                         .has_max_eval_period = true,
                         .max_eval_period = 10
                     },
                     1);
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    OBSERVE_COMP_OP_TEST(payload);

    ctx_ref.observations[0].observe_active = false;
    ctx_ref.observations[0].effective_attr = (_anj_attr_notification_t) {
        .has_min_eval_period = true,
        .min_eval_period = 20,
        .has_max_eval_period = true,
        .max_eval_period = 10
    };

    compare_observations(&anj.observe_ctx, &ctx_ref);

    inout_msg.operation = ANJ_OP_DM_WRITE_ATTR;
    inout_msg.uri = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    _anj_attr_notification_t new_attr = {
        .has_min_eval_period = true,
        .min_eval_period = 20,
        .has_max_eval_period = true,
        .max_eval_period = 30
    };

    inout_msg.attr.notification_attr = new_attr;

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
    ctx_ref.observations[0].effective_attr = (_anj_attr_notification_t) {
        .has_min_eval_period = true,
        .min_eval_period = 20,
        .has_max_eval_period = true,
        .max_eval_period = 30
    };
    ctx_ref.observations[0].observe_active = true;
    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_update_record_one_path) {
    TEST_INIT();
    uint8_t payload[] = {
        "\x81"       /* array(1) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
    };
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x24;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[0].prev = NULL;
    anj.observe_ctx.observations[0].observe_active = true;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 4);
    anj.observe_ctx.observations[1].prev = NULL;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].observe_active = true;
    OBSERVE_COMP_OP_TEST(payload);

    ctx_ref.observations[2] = ctx_ref.observations[0];
    ctx_ref.observations[2].prev = &anj.observe_ctx.observations[2];
    memset(&ctx_ref.observations[0], 0, sizeof(ctx_ref.observations[0]));
    ctx_ref.observations[0].ssid = 1;
    ctx_ref.observations[0].token.size = 1;
    ctx_ref.observations[0].token.bytes[0] = 0x24;
    ctx_ref.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    ctx_ref.observations[0].prev = NULL;
    ctx_ref.observations[0].observe_active = true;
    ctx_ref.observations[1].ssid = 1;
    ctx_ref.observations[1].token.size = 1;
    ctx_ref.observations[1].token.bytes[0] = 0x23;
    ctx_ref.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 4);
    ctx_ref.observations[1].prev = NULL;

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_update_record_three_paths) {
    TEST_INIT();
    uint8_t payload[] = {
        "\x83"       /* array(3) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/4" /* text(6) */
    };

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[3];
    anj.observe_ctx.observations[0].observe_active = true;
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[1].prev = NULL;
    anj.observe_ctx.observations[1].observe_active = true;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].observe_active = true;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 4);
    anj.observe_ctx.observations[3].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[3].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;
    anj.observe_ctx.observations[3].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].observe_active = true;
    OBSERVE_COMP_OP_TEST(payload);

    ctx_ref.observations[0].prev = &anj.observe_ctx.observations[3];
    ctx_ref.observations[3] = ctx_ref.observations[2];
    ctx_ref.observations[2] = ctx_ref.observations[1];
    ctx_ref.observations[2].prev = &anj.observe_ctx.observations[0];
    ctx_ref.observations[3].prev = &anj.observe_ctx.observations[2];
    memset(&ctx_ref.observations[1], 0, sizeof(ctx_ref.observations[1]));
    ctx_ref.observations[1].ssid = 1;
    ctx_ref.observations[1].token.size = 1;
    ctx_ref.observations[1].token.bytes[0] = 0x23;
    ctx_ref.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    ctx_ref.observations[1].prev = NULL;
    ctx_ref.observations[1].observe_active = true;

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_wrong_pmax_for_one_path) {
    TEST_INIT();
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_RESOURCE_PATH(3, 0, 2),
                     (_anj_attr_notification_t) {
                         .has_min_period = true,
                         .min_period = 20,
                         .has_max_period = true,
                         .max_period = 10
                     },
                     1);

    OBSERVE_COMP_OP_TEST(payload);
    // we are do not ignoring max period at this point
    ctx_ref.observations[0].observe_active = true;
    ctx_ref.observations[0].effective_attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 20,
        .has_max_period = true,
        .max_period = 10
    };
    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_bad_attributes_for_one_path) {
    TEST_INIT();
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_RESOURCE_PATH(3, 0, 2),
                     (_anj_attr_notification_t) {
                         .has_min_eval_period = true,
                         .min_eval_period = 20,
                         .has_max_eval_period = true,
                         .max_eval_period = 10
                     },
                     1);

    OBSERVE_COMP_OP_TEST(payload);

    ctx_ref.observations[0].observe_active = false;
    ctx_ref.observations[0].effective_attr = (_anj_attr_notification_t) {
        .has_min_eval_period = true,
        .min_eval_period = 20,
        .has_max_eval_period = true,
        .max_eval_period = 10
    };
    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_bad_attributes_for_all_paths) {
    TEST_INIT();
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_INSTANCE_PATH(3, 0),
                     (_anj_attr_notification_t) {
                         .has_min_eval_period = true,
                         .min_eval_period = 20,
                         .has_max_eval_period = true,
                         .max_eval_period = 10
                     },
                     1);

    OBSERVE_COMP_OP_TEST(payload);

    _anj_attr_notification_t attr = {
        .has_min_eval_period = true,
        .min_eval_period = 20,
        .has_max_eval_period = true,
        .max_eval_period = 10
    };

    ctx_ref.observations[0].observe_active = false;
    ctx_ref.observations[0].effective_attr = attr;
    ctx_ref.observations[1].observe_active = false;
    ctx_ref.observations[1].effective_attr = attr;
    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_wrong_pmax_for_all_paths) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_INSTANCE_PATH(3, 0),
                     (_anj_attr_notification_t) {
                         .has_min_period = true,
                         .min_period = 20,
                         .has_max_period = true,
                         .max_period = 10
                     },
                     1);

    OBSERVE_COMP_OP_TEST(payload);
    // we are do not ignoring max period at this point
    ctx_ref.observations[0].observe_active = true;
    ctx_ref.observations[0].effective_attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 20,
        .has_max_period = true,
        .max_period = 10
    };
    ctx_ref.observations[1].observe_active = true;
    ctx_ref.observations[1].effective_attr = (_anj_attr_notification_t) {
        .has_min_period = true,
        .min_period = 20,
        .has_max_period = true,
        .max_period = 10
    };
    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_ignore_step_with_multi_res) {
    TEST_INIT();
    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_RESOURCE_PATH(3, 0, 8),
                     (_anj_attr_notification_t) {
                         .has_min_eval_period = true,
                         .min_eval_period = 10,
                         .has_step = true,
                         .step = 2,
                     },
                     1);
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/8" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST(payload);
    ctx_ref.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 8);
    ctx_ref.observations[0].effective_attr.has_min_eval_period = true;
    ctx_ref.observations[0].effective_attr.min_eval_period = 10;

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

#        ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(observe_comp_op, observe_ignore_edge_with_multi_res) {
    TEST_INIT();
    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_RESOURCE_PATH(3, 0, 9),
                     (_anj_attr_notification_t) {
                         .has_min_eval_period = true,
                         .min_eval_period = 10,
                         .has_edge = true,
                         .edge = true,
                     },
                     1);
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/9" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST(payload);
    ctx_ref.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 9);
    ctx_ref.observations[0].effective_attr.has_min_eval_period = true;
    ctx_ref.observations[0].effective_attr.min_eval_period = 10;

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_with_attr) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_WITH_ATTR_TEST(((_anj_attr_notification_t) {
                                       .has_max_period = true,
                                       .max_period = 22,
                                       .has_min_eval_period = true,
                                       .min_eval_period = 3,
                                       .has_max_eval_period = true,
                                       .max_eval_period = 4,
                                   }),
                                   0, ANJ_COAP_CODE_CONTENT, payload);

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_ignore_attached_attr) {
    TEST_INIT();

    add_attr_storage(&anj.observe_ctx.attributes_storage[0],
                     ANJ_MAKE_INSTANCE_PATH(3, 0),
                     (_anj_attr_notification_t) {
                         .has_min_eval_period = true,
                         .min_eval_period = 3300,
                     },
                     1);
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    OBSERVE_COMP_OP_WITH_ATTR_TEST(((_anj_attr_notification_t) {
                                       .has_max_eval_period = true,
                                       .max_eval_period = 5000,
                                   }),
                                   0, ANJ_COAP_CODE_CONTENT, payload);

    compare_observations(&anj.observe_ctx, &ctx_ref);
}
#        endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(observe_comp_op, composite_observation_root_path) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x81"  /* array(1) */
        "\xA1"  /* map(1) */
        "\x00"  /* unsigned(0) => SenML Name */
        "\x61/" /* text(1) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(NULL, 0, ANJ_COAP_CODE_METHOD_NOT_ALLOWED,
                               payload, _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);
}

ANJ_UNIT_TEST(observe_comp_op, composite_observation_too_much_records) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x86"       /* array(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/4" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/5" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/6" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/7" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(NULL, 0, ANJ_COAP_CODE_INTERNAL_SERVER_ERROR,
                               payload, _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }
}

#        ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(observe_comp_op, observe_err_not_found) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"        /* array(2) */
        "\xA1"        /* map(1) */
        "\x00"        /* unsigned(0) => SenML Name */
        "\x67/3/0/11" /* text(7) */
        "\xA1"        /* map(1) */
        "\x00"        /* unsigned(0) => SenML Name */
        "\x66/3/0/3"  /* text(6) */
    };
    // without attributes is_any_resource_readable is not called
    OBSERVE_COMP_OP_TEST_ERROR(&((_anj_attr_notification_t) {
                                   .has_min_period = true,
                                   .min_period = 20,
                               }),
                               0, ANJ_COAP_CODE_NOT_FOUND, payload,
                               _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_err_not_allowed) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x62/4"     /* text(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(&((_anj_attr_notification_t) {
                                   .has_min_period = true,
                                   .min_period = 20,
                               }),
                               0, ANJ_COAP_CODE_METHOD_NOT_ALLOWED, payload,
                               _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op,
              observe_failure_in_the_middle_of_addition_due_to_cb) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x84"       /* array(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x62/4"     /* text(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/5" /* text(6) */
    };

    OBSERVE_COMP_OP_TEST_ERROR(&((_anj_attr_notification_t) {
                                   .has_min_period = true,
                                   .min_period = 20,
                               }),
                               0, ANJ_COAP_CODE_METHOD_NOT_ALLOWED, payload,
                               _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }
}
#        endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(observe_comp_op, observe_build_msg_error) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(NULL, 0,
                               ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT,
                               payload, _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_JSON);

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }
}

ANJ_UNIT_TEST(observe_comp_op, observe_bad_payload) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x83"       /* array(3) <- wrong array size */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(NULL, 0, ANJ_COAP_CODE_BAD_REQUEST, payload,
                               _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }
}

ANJ_UNIT_TEST(observe_comp_op, observe_bad_format) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(NULL, -1, ANJ_COAP_CODE_BAD_REQUEST, payload,
                               _ANJ_COAP_FORMAT_PLAINTEXT,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_payload_with_value) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA2"       /* map(2) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\x02"       /* unsigned(2) => SenML Value */
        "\x18\x2A"   /* unsigned(42) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(NULL, 0, ANJ_COAP_CODE_BAD_REQUEST, payload,
                               _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op, observe_unsupported_format) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(NULL, -1,
                               ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT,
                               payload, _ANJ_COAP_FORMAT_SENML_JSON,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    compare_observations(&anj.observe_ctx, &ctx_ref);
}

ANJ_UNIT_TEST(observe_comp_op,
              observe_failure_in_the_middle_of_addition_due_to_payload) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x84"       /* array(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x67/3/0/4" /* text(6) <- wrong size */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/5" /* text(6) */
    };

    OBSERVE_COMP_OP_TEST_ERROR(NULL, 0, ANJ_COAP_CODE_BAD_REQUEST, payload,
                               _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }
}

ANJ_UNIT_TEST(observe_comp_op,
              composite_observation_two_records_different_accept_options) {
    TEST_INIT();
    uint8_t payload1[] = {
        "\x81"       /* array(1) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST(payload1);

    uint8_t payload2[] = {
        "\x81"       /* array(1) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    uint16_t accept_option = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    inout_msg.token.bytes[0] = 0x23;
    inout_msg.operation = ANJ_OP_INF_OBSERVE_COMP;
    inout_msg.accept = accept_option;
    inout_msg.payload = payload2;
    inout_msg.payload_size = sizeof(payload2) - 1;
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
    ASSERT_EQ(anj.observe_ctx.observations[0].accept_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[1].accept_opt, accept_option);
}

ANJ_UNIT_TEST(observe_comp_op,
              observe_update_record_error_wrong_accept_option) {
    TEST_INIT();

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(NULL, -1, ANJ_COAP_CODE_METHOD_NOT_ALLOWED,
                               payload, _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_JSON);

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 2)));
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[1].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 3)));
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    ASSERT_EQ(anj.observe_ctx.observations[1].token.bytes[0], 0x22);
    ASSERT_EQ((size_t) anj.observe_ctx.observations[0].prev,
              (size_t) &anj.observe_ctx.observations[1]);
    ASSERT_EQ((size_t) anj.observe_ctx.observations[1].prev,
              (size_t) &anj.observe_ctx.observations[0]);
    ASSERT_EQ(anj.observe_ctx.observations[0].content_format_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[1].content_format_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[0].accept_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[1].accept_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

#        ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(observe_comp_op, observe_update_record_error_wrong_attributes) {
    TEST_INIT();

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].observation_attr.has_min_period = true;
    anj.observe_ctx.observations[0].observation_attr.min_period = 5;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].observation_attr.has_min_period = true;
    anj.observe_ctx.observations[1].observation_attr.min_period = 5;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(NULL, -1, ANJ_COAP_CODE_METHOD_NOT_ALLOWED,
                               payload, _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 2)));
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[1].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 3)));
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    ASSERT_EQ(anj.observe_ctx.observations[1].token.bytes[0], 0x22);
    ASSERT_EQ((size_t) anj.observe_ctx.observations[0].prev,
              (size_t) &anj.observe_ctx.observations[1]);
    ASSERT_EQ((size_t) anj.observe_ctx.observations[1].prev,
              (size_t) &anj.observe_ctx.observations[0]);
    compare_attr(&anj.observe_ctx.observations[0].observation_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 5,
                 });
    compare_attr(&anj.observe_ctx.observations[1].observation_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 5,
                 });
    ASSERT_EQ(anj.observe_ctx.observations[0].content_format_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[1].content_format_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[0].accept_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[1].accept_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

ANJ_UNIT_TEST(observe_comp_op, observe_update_record_error_wrong_format) {
    TEST_INIT();

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].observation_attr.has_min_period = true;
    anj.observe_ctx.observations[0].observation_attr.min_period = 5;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].observation_attr.has_min_period = true;
    anj.observe_ctx.observations[1].observation_attr.min_period = 5;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    OBSERVE_COMP_OP_TEST_ERROR(((_anj_attr_notification_t) {
                                   .has_min_period = true,
                                   .min_period = 5,
                               }),
                               -1, ANJ_COAP_CODE_METHOD_NOT_ALLOWED, payload,
                               _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_JSON);

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[0].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 2)));
    ASSERT_TRUE(anj_uri_path_equal(&anj.observe_ctx.observations[1].path,
                                   &ANJ_MAKE_RESOURCE_PATH(3, 0, 3)));
    ASSERT_EQ(anj.observe_ctx.observations[0].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].token.size, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].token.bytes[0], 0x22);
    ASSERT_EQ(anj.observe_ctx.observations[1].token.bytes[0], 0x22);
    ASSERT_EQ((size_t) anj.observe_ctx.observations[0].prev,
              (size_t) &anj.observe_ctx.observations[1]);
    ASSERT_EQ((size_t) anj.observe_ctx.observations[1].prev,
              (size_t) &anj.observe_ctx.observations[0]);
    compare_attr(&anj.observe_ctx.observations[0].observation_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 5,
                 });
    compare_attr(&anj.observe_ctx.observations[1].observation_attr,
                 &(_anj_attr_notification_t) {
                     .has_min_period = true,
                     .min_period = 5,
                 });
    ASSERT_EQ(anj.observe_ctx.observations[0].content_format_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[1].content_format_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[0].accept_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[1].accept_opt,
              _ANJ_COAP_FORMAT_SENML_CBOR);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
}
#        endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(observe_comp_op, observe_bad_attribute_epmin_epmax) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    _anj_attr_notification_t attr = {
        .has_min_eval_period = true,
        .min_eval_period = 20,
        .has_max_eval_period = true,
        .max_eval_period = 10
    };
    OBSERVE_COMP_OP_TEST_ERROR(attr, -1, ANJ_COAP_CODE_BAD_REQUEST, payload,
                               _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }
}

ANJ_UNIT_TEST(observe_comp_op, observe_bad_attribute_st) {
    TEST_INIT();

    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };
    _anj_attr_notification_t attr = {
        .has_min_period = true,
        .min_period = 10,
        .has_step = true,
        .step = 2,
    };
    OBSERVE_COMP_OP_TEST_ERROR(attr, -1, ANJ_COAP_CODE_BAD_REQUEST, payload,
                               _ANJ_COAP_FORMAT_SENML_CBOR,
                               _ANJ_COAP_FORMAT_SENML_CBOR);

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }
}

ANJ_UNIT_TEST(observe_comp_op, observe_block) {
    TEST_INIT();
    _anj_observe_ctx_t ctx_ref = { 0 };
    _anj_exchange_ctx_t exchange_ctx;
    _anj_exchange_handlers_t out_handlers;
    _anj_exchange_init(&exchange_ctx, 0);
    _anj_observe_server_state_t srv = {
        .ssid = 1,
        .default_max_period = 77,
    };
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    _anj_coap_msg_t inout_msg = {
        .operation = ANJ_OP_INF_OBSERVE_COMP,
        .payload = payload,
        .payload_size = sizeof(payload) - 1,
        .content_format = _ANJ_COAP_FORMAT_SENML_CBOR,
        .accept = _ANJ_COAP_FORMAT_SENML_CBOR
    };
    inout_msg.coap_binding_data.udp.message_id = 0x1111;
    inout_msg.token.size = 1;
    inout_msg.token.bytes[0] = 0x22;
    uint8_t response_code;
    uint8_t out_buff[40];
    size_t out_buff_len = sizeof(out_buff);
    uint8_t payload_buff[16];
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
    // payload is not checked here
    ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED, sizeof(EXPECTED) - 17);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(EXPECTED) - 1);
    ASSERT_EQ(anj.observe_ctx.already_processed, 1);

    ctx_ref.observations[0].ssid = 1;
    ctx_ref.observations[0].token.size = 1;
    ctx_ref.observations[0].token.bytes[0] = 0x22;
    ctx_ref.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    ctx_ref.observations[0].observe_active = true;
    ctx_ref.observations[0].prev = &anj.observe_ctx.observations[1];
    ctx_ref.observations[0].last_notify_timestamp = anj_time_real_now();
    ctx_ref.observations[0].next_conf_notify_timestamp =
            anj_time_real_now() + ONE_DAY_MS;
    ctx_ref.observations[0].content_format_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    ctx_ref.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;

    ctx_ref.observations[1].ssid = 1;
    ctx_ref.observations[1].token.size = 1;
    ctx_ref.observations[1].token.bytes[0] = 0x22;
    ctx_ref.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    ctx_ref.observations[1].observe_active = true;
    ctx_ref.observations[1].prev = &anj.observe_ctx.observations[0];
    ctx_ref.observations[1].last_notify_timestamp = anj_time_real_now();
    ctx_ref.observations[1].next_conf_notify_timestamp =
            anj_time_real_now() + ONE_DAY_MS;
    ctx_ref.observations[1].content_format_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    ctx_ref.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;

    compare_observations(&anj.observe_ctx, &ctx_ref);

    ASSERT_EQ(inout_msg.block.number, 0);
    ASSERT_EQ(inout_msg.block.block_type, ANJ_OPTION_BLOCK_2);

    ASSERT_EQ(_anj_exchange_process(&exchange_ctx,
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &inout_msg),
              ANJ_EXCHANGE_STATE_WAITING_MSG);
    inout_msg.operation = ANJ_OP_INF_OBSERVE_COMP;
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
    ASSERT_EQ(anj.observe_ctx.already_processed, 2);
    uint8_t EXPECTED2[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x12\x22"
                          "\x60"     /*observe = 0*/
                          "\x61\x70" /*content format = 112*/
                          "\xb1\x10" // block2 1
                          "\xFF";
    EXPECTED2[1] = ANJ_COAP_CODE_CONTENT;
    ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED2, sizeof(EXPECTED2) - 1);
    ASSERT_EQ(_anj_exchange_process(&exchange_ctx,
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &inout_msg),
              ANJ_EXCHANGE_STATE_FINISHED);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
}

#        define CANCEL_COMP_OBSERVE_OP_TEST(Result, Msg_Code, Payload, Format) \
            _anj_exchange_ctx_t exchange_ctx;                                  \
            _anj_exchange_handlers_t out_handlers;                             \
            _anj_exchange_init(&exchange_ctx, 0);                              \
            _anj_observe_server_state_t srv = {                                \
                .ssid = 1,                                                     \
            };                                                                 \
            _anj_coap_msg_t inout_msg = {                                      \
                .operation = ANJ_OP_INF_CANCEL_OBSERVE_COMP,                   \
                .payload = Payload,                                            \
                .payload_size = sizeof(Payload) - 1,                           \
                .content_format = Format,                                      \
                .accept = _ANJ_COAP_FORMAT_SENML_CBOR                          \
            };                                                                 \
            inout_msg.coap_binding_data.udp.message_id = 0x1111;               \
            inout_msg.token.size = 1;                                          \
            inout_msg.token.bytes[0] = 0x22;                                   \
            uint8_t response_code;                                             \
            uint8_t out_buff[1024];                                            \
            size_t out_buff_len = sizeof(out_buff);                            \
            uint8_t payload_buff[1024];                                        \
            size_t payload_buff_len = sizeof(payload_buff);                    \
            ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv,      \
                                               &inout_msg, &response_code),    \
                      Result);                                                 \
            ASSERT_EQ(_anj_exchange_new_server_request(                        \
                              &exchange_ctx, response_code, &inout_msg,        \
                              &out_handlers, payload_buff, payload_buff_len),  \
                      ANJ_EXCHANGE_STATE_MSG_TO_SEND);                         \
            ASSERT_EQ(anj.observe_ctx.already_processed,                       \
                      Msg_Code == ANJ_COAP_CODE_CONTENT                        \
                              ? *(uint8_t *) Payload - 0x80                    \
                              : 0);                                            \
            size_t out_msg_size = 0;                                           \
            ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len, \
                                           &out_msg_size));                    \
            uint8_t EXPECTED_POSITIVE[] =                                      \
                    "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */              \
                    "\xFF\x11\x11\x22"                                         \
                    "\xc1\x70" /*content format = 112*/                        \
                    "\xFF";                                                    \
            uint8_t EXPECTED_NEGATIVE[] =                                      \
                    "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */              \
                    "\xFF\x11\x11\x22";                                        \
            size_t EXPECTED_POSITIVE_SIZE = sizeof(EXPECTED_POSITIVE) - 1;     \
            size_t EXPECTED_NEGATIVE_SIZE = sizeof(EXPECTED_NEGATIVE) - 1;     \
                                                                               \
            uint8_t *EXPECTED = Msg_Code == ANJ_COAP_CODE_CONTENT              \
                                        ? EXPECTED_POSITIVE                    \
                                        : EXPECTED_NEGATIVE;                   \
            size_t EXPECTED_SIZE = Msg_Code == ANJ_COAP_CODE_CONTENT           \
                                           ? EXPECTED_POSITIVE_SIZE            \
                                           : EXPECTED_NEGATIVE_SIZE;           \
                                                                               \
            EXPECTED[1] = Msg_Code;                                            \
                                                                               \
            ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED, EXPECTED_SIZE);          \
            ASSERT_EQ(_anj_exchange_process(                                   \
                              &exchange_ctx,                                   \
                              ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,            \
                              &inout_msg),                                     \
                      ANJ_EXCHANGE_STATE_FINISHED);

ANJ_UNIT_TEST(observe_comp_op, observe_cancel_four_paths) {
    TEST_INIT();

    // this payload is unused
    uint8_t payload[] = {
        "\x84"       /* array(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x64/3/0"   /* text(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/1" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[3];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 1);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[3].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[3].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;

    CANCEL_COMP_OBSERVE_OP_TEST(0, ANJ_COAP_CODE_CONTENT, payload,
                                _ANJ_COAP_FORMAT_SENML_CBOR)

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }
}

ANJ_UNIT_TEST(observe_comp_op, observe_cancel_four_paths_block) {
    _anj_observe_ctx_t ctx_ref;
    memset(&ctx_ref, 0, sizeof(ctx_ref));
    TEST_INIT();
    _anj_exchange_ctx_t exchange_ctx;
    _anj_exchange_handlers_t out_handlers;
    _anj_exchange_init(&exchange_ctx, 0);

    _anj_observe_server_state_t srv = {
        .ssid = 1,
        .default_max_period = 77,
    };
    size_t block_size = 16;
    // this payload is unused
    uint8_t payload[] = {
        "\x84"       /* array(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/4" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/5" /* text(6) */
    };

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[3];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 4);
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 5);
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[3].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[3].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;

    _anj_coap_msg_t inout_msg = {
        .operation = ANJ_OP_INF_CANCEL_OBSERVE_COMP,
        .payload = payload,
        .payload_size = 16,
        .content_format = _ANJ_COAP_FORMAT_SENML_CBOR,
        .accept = _ANJ_COAP_FORMAT_SENML_CBOR,
        .block.block_type = ANJ_OPTION_BLOCK_1,
        .block.size = block_size,
        .block.number = 0,
        .block.more_flag = true
    };
    inout_msg.coap_binding_data.udp.message_id = 0x1111;
    inout_msg.token.size = 1;
    inout_msg.token.bytes[0] = 0x22;
    uint8_t response_code;
    uint8_t out_buff[100];
    size_t out_buff_len = sizeof(out_buff);
    uint8_t payload_buff[100];
    ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv, &inout_msg,
                                       &response_code),
              0);

    size_t out_msg_size = 0;
    uint8_t EXPECTED1[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x11\x22"
                          "\xD1\x0E\x08"; // block1 0 more
    EXPECTED1[1] = ANJ_COAP_CODE_CONTINUE;

    REQUEST_BLOCK_TRANSFER(true, false, EXPECTED1);
    inout_msg.operation = ANJ_OP_INF_CANCEL_OBSERVE_COMP;
    inout_msg.payload = &payload[block_size];
    inout_msg.payload_size = 16;
    inout_msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.block.number = 1;
    inout_msg.coap_binding_data.udp.message_id++;

    uint8_t EXPECTED2[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x12\x22"
                          "\xD1\x0E\x18"; // block1 1 more
    EXPECTED2[1] = ANJ_COAP_CODE_CONTINUE;

    REQUEST_BLOCK_TRANSFER(false, false, EXPECTED2);
    inout_msg.operation = ANJ_OP_INF_CANCEL_OBSERVE_COMP;
    inout_msg.payload = &payload[2 * block_size];
    inout_msg.payload_size = sizeof(payload) - 2 * block_size - 1;
    inout_msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.block.number = 2;
    inout_msg.block.more_flag = false;
    inout_msg.coap_binding_data.udp.message_id++;

    uint8_t EXPECTED3[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x13\x22"
                          "\xC1\x70"     /*content format = 112*/
                          "\xD1\x02\x20" /*block1 2*/
                          "\xFF";
    EXPECTED3[1] = ANJ_COAP_CODE_CONTENT;

    REQUEST_BLOCK_TRANSFER(false, true, EXPECTED3);

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }
}

ANJ_UNIT_TEST(observe_comp_op, observe_cancel_two_paths_block) {
    _anj_observe_ctx_t ctx_ref;
    memset(&ctx_ref, 0, sizeof(ctx_ref));
    TEST_INIT();
    _anj_exchange_ctx_t exchange_ctx;
    _anj_exchange_handlers_t out_handlers;
    _anj_exchange_init(&exchange_ctx, 0);

    _anj_observe_server_state_t srv = {
        .ssid = 1,
        .default_max_period = 77,
    };
    size_t block_size = 16;
    // this payload is unused
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/1" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x64/3/2"   /* text(4) */
    };

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 1);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[3];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_INSTANCE_PATH(3, 2);
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_INSTANCE_PATH(3, 3);
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[3].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[3].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;

    _anj_coap_msg_t inout_msg = {
        .operation = ANJ_OP_INF_CANCEL_OBSERVE_COMP,
        .payload = payload,
        .payload_size = 16,
        .content_format = _ANJ_COAP_FORMAT_SENML_CBOR,
        .accept = _ANJ_COAP_FORMAT_SENML_CBOR,
        .block.block_type = ANJ_OPTION_BLOCK_1,
        .block.size = block_size,
        .block.number = 0,
        .block.more_flag = true
    };
    inout_msg.coap_binding_data.udp.message_id = 0x1111;
    inout_msg.token.size = 1;
    inout_msg.token.bytes[0] = 0x22;
    uint8_t response_code;
    uint8_t out_buff[100];
    size_t out_buff_len = sizeof(out_buff);
    uint8_t payload_buff[100];
    ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv, &inout_msg,
                                       &response_code),
              0);

    size_t out_msg_size = 0;
    uint8_t EXPECTED1[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x11\x22"
                          "\xD1\x0E\x08"; // block1 0 more
    EXPECTED1[1] = ANJ_COAP_CODE_CONTINUE;

    REQUEST_BLOCK_TRANSFER(true, false, EXPECTED1);

    inout_msg.operation = ANJ_OP_INF_CANCEL_OBSERVE_COMP;
    inout_msg.payload = &payload[block_size];
    inout_msg.payload_size = sizeof(payload) - block_size - 1;
    inout_msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    inout_msg.block.number = 1;
    inout_msg.block.more_flag = false;
    inout_msg.coap_binding_data.udp.message_id++;

    uint8_t EXPECTED2[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                          "\xFF\x11\x12\x22"
                          "\xC1\x70"     /*content format = 112*/
                          "\xD1\x02\x10" /*block1 1*/
                          "\xFF";
    EXPECTED2[1] = ANJ_COAP_CODE_CONTENT;

    REQUEST_BLOCK_TRANSFER(false, true, EXPECTED2);

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

ANJ_UNIT_TEST(observe_comp_op, observe_cancel_not_found) {
    TEST_INIT();

    // this payload is unused
    uint8_t payload[] = {
        "\x84"       /* array(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x64/4/0"   /* text(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/4/0/1" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/4/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/4/0/3" /* text(6) */
    };

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[3];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 1);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[3].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[3].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;

    CANCEL_COMP_OBSERVE_OP_TEST(-1, ANJ_COAP_CODE_NOT_FOUND, payload,
                                _ANJ_COAP_FORMAT_SENML_CBOR)

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 1);
}

ANJ_UNIT_TEST(observe_comp_op, observe_cancel_build_msg_error) {
    TEST_INIT();
    // this payload is unused
    uint8_t payload[] = {
        "\x84"     /* array(4) */
        "\xA1"     /* map(1) */
        "\x00"     /* unsigned(0) => SenML Name */
        "\x64/3/0" /* text(4) */
        "\xA1"     /* map(1) */
        "\x00"     /* unsigned(0) => SenML Name */
        "\x64/3/1" /* text(4) */
        "\xA1"     /* map(1) */
        "\x00"     /* unsigned(0) => SenML Name */
        "\x64/3/2" /* text(6) */
        "\xA1"     /* map(1) */
        "\x00"     /* unsigned(0) => SenML Name */
        "\x64/3/3" /* text(4) */
    };

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[3];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt =
            _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_INSTANCE_PATH(3, 1);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt =
            _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_INSTANCE_PATH(3, 2);
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].accept_opt =
            _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_INSTANCE_PATH(3, 3);
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[3].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[3].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].accept_opt =
            _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;

    CANCEL_COMP_OBSERVE_OP_TEST(0, ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT,
                                payload, _ANJ_COAP_FORMAT_SENML_JSON)

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 1);
}

ANJ_UNIT_TEST(observe_comp_op, observe_cancel_two_paths_different_ssid) {
    TEST_INIT();

    // this payload is unused
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/1" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    anj.observe_ctx.observations[0].ssid = 2;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 1);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[3];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[2].ssid = 2;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[3].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[3].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;

    CANCEL_COMP_OBSERVE_OP_TEST(0, ANJ_COAP_CODE_CONTENT, payload,
                                _ANJ_COAP_FORMAT_SENML_CBOR)

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

ANJ_UNIT_TEST(observe_comp_op, observe_cancel_two_paths_different_token) {
    TEST_INIT();

    // this payload is unused
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x64/3/0"   /* text(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
    };

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 1);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[3];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 3);
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[3].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[3].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;

    CANCEL_COMP_OBSERVE_OP_TEST(0, ANJ_COAP_CODE_CONTENT, payload,
                                _ANJ_COAP_FORMAT_SENML_CBOR)

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

ANJ_UNIT_TEST(observe_comp_op, observe_cancel_one_path) {
    TEST_INIT();

    // this payload is unused
    uint8_t payload[] = {
        "\x81"       /* array(1) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/1" /* text(6) */
    };

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x24;
    anj.observe_ctx.observations[0].prev = NULL;
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 2);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[3];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 1);
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 5);
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[3].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[3].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[3].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;

    CANCEL_COMP_OBSERVE_OP_TEST(0, ANJ_COAP_CODE_CONTENT, payload,
                                _ANJ_COAP_FORMAT_SENML_CBOR)

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 1);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

// we don't care about payload in the case of Cancel Observation-Composite
// operation
ANJ_UNIT_TEST(observe_comp_op, observe_cancel_bad_format) {
    TEST_INIT();

    // this payload is unused
    uint8_t payload[] = {
        "\x82"     /* array(2) */
        "\xA1"     /* map(1) */
        "\x00"     /* unsigned(0) => SenML Name */
        "\x64/3/0" /* text(4) */
        "\xA1"     /* map(1) */
        "\x00"     /* unsigned(0) => SenML Name */
        "\x62/3"   /* text(2) */
    };
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_OBJECT_PATH(3);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;

    CANCEL_COMP_OBSERVE_OP_TEST(0, ANJ_COAP_CODE_CONTENT, payload,
                                _ANJ_COAP_FORMAT_PLAINTEXT)

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
}

// we don't care about payload in the case of Cancel Observation-Composite
// operation
ANJ_UNIT_TEST(observe_comp_op, observe_cancel_unsupported_format) {
    TEST_INIT();

    // this payload is unused, it is not even plain text
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x64/3/0"   /* text(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/1" /* text(6) */
    };
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 1);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;

    CANCEL_COMP_OBSERVE_OP_TEST(0, ANJ_COAP_CODE_CONTENT, payload,
                                _ANJ_COAP_FORMAT_PLAINTEXT)

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
}

ANJ_UNIT_TEST(observe_comp_op,
              observe_cancel_composite_and_add_normal_observe) {
    TEST_INIT();

    // this payload is unused
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x64/3/0"   /* text(4) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/1" /* text(6) */
    };

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[0].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[0].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(3, 0, 1);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_SENML_CBOR;

    CANCEL_COMP_OBSERVE_OP_TEST(0, ANJ_COAP_CODE_CONTENT, payload,
                                _ANJ_COAP_FORMAT_SENML_CBOR)

    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ASSERT_FALSE(anj.observe_ctx.observations[i].ssid);
    }

    _anj_coap_msg_t request_observe = {
        .operation = ANJ_OP_INF_OBSERVE,
        .uri = ANJ_MAKE_RESOURCE_PATH(3, 0, 2),
        .payload_size = 0,
    };

    ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv,
                                       &request_observe, &response_code),
              0);
    ASSERT_EQ(_anj_exchange_new_server_request(&exchange_ctx, response_code,
                                               &request_observe, &out_handlers,
                                               payload_buff, payload_buff_len),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ASSERT_EQ(_anj_exchange_process(&exchange_ctx,
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &request_observe),
              ANJ_EXCHANGE_STATE_FINISHED);

    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 1);
    ASSERT_EQ((size_t) anj.observe_ctx.observations[0].prev, 0);
}
#    else  // ANJ_WITH_OBSERVE_COMPOSITE
ANJ_UNIT_TEST(observe_comp_op, observe_comp_turn_off) {
    TEST_INIT();
    _anj_observe_ctx_t ctx_ref;
    _anj_exchange_ctx_t exchange_ctx;
    _anj_exchange_handlers_t out_handlers;
    _anj_exchange_init(&exchange_ctx, 0);

    _anj_observe_server_state_t srv = {
        .ssid = 1,
        .default_max_period = 77,
    };
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    _anj_coap_msg_t inout_msg = {
        .operation = ANJ_OP_INF_OBSERVE_COMP,
        .payload = payload,
        .payload_size = sizeof(payload) - 1,
        .content_format = _ANJ_COAP_FORMAT_SENML_CBOR,
        .accept = _ANJ_COAP_FORMAT_SENML_CBOR
    };
    inout_msg.coap_binding_data.udp.message_id = 0x1111;
    inout_msg.token.size = 1;
    inout_msg.token.bytes[0] = 0x22;
    uint8_t response_code;
    uint8_t out_buff[100];
    size_t out_buff_len = sizeof(out_buff);
    uint8_t payload_buff[100];
    size_t payload_buff_len = sizeof(payload_buff);
    ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv, &inout_msg,
                                       &response_code),
              -1);
    ASSERT_EQ(_anj_exchange_new_server_request(&exchange_ctx, response_code,
                                               &inout_msg, &out_handlers,
                                               payload_buff, payload_buff_len),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    size_t out_msg_size = 0;
    ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len,
                                   &out_msg_size));
    uint8_t EXPECTED[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                         "\xFF\x11\x11\x22";
    EXPECTED[1] = ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
    ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(EXPECTED) - 1);
    ASSERT_EQ(_anj_exchange_process(&exchange_ctx,
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &inout_msg),
              ANJ_EXCHANGE_STATE_FINISHED);
}

ANJ_UNIT_TEST(observe_comp_op, cancel_observe_comp_turn_off) {
    TEST_INIT();
    _anj_observe_ctx_t ctx_ref;
    _anj_exchange_ctx_t exchange_ctx;
    _anj_exchange_handlers_t out_handlers;
    _anj_exchange_init(&exchange_ctx, 0);

    _anj_observe_server_state_t srv = {
        .ssid = 1,
        .default_max_period = 77,
    };
    uint8_t payload[] = {
        "\x82"       /* array(2) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/2" /* text(6) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/3/0/3" /* text(6) */
    };

    _anj_coap_msg_t inout_msg = {
        .operation = ANJ_OP_INF_CANCEL_OBSERVE_COMP,
        .payload = payload,
        .payload_size = sizeof(payload) - 1,
        .content_format = _ANJ_COAP_FORMAT_SENML_CBOR,
        .accept = _ANJ_COAP_FORMAT_SENML_CBOR
    };
    inout_msg.coap_binding_data.udp.message_id = 0x1111;
    inout_msg.token.size = 1;
    inout_msg.token.bytes[0] = 0x22;
    uint8_t response_code;
    uint8_t out_buff[100];
    size_t out_buff_len = sizeof(out_buff);
    uint8_t payload_buff[100];
    size_t payload_buff_len = sizeof(payload_buff);
    ASSERT_EQ(_anj_observe_new_request(&anj, &out_handlers, &srv, &inout_msg,
                                       &response_code),
              -1);
    ASSERT_EQ(_anj_exchange_new_server_request(&exchange_ctx, response_code,
                                               &inout_msg, &out_handlers,
                                               payload_buff, payload_buff_len),
              ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    size_t out_msg_size = 0;
    ASSERT_OK(_anj_coap_encode_udp(&inout_msg, out_buff, out_buff_len,
                                   &out_msg_size));
    uint8_t EXPECTED[] = "\x61" /* Ver = 1, Type = 2 (ACK), TKL = 1 */
                         "\xFF\x11\x11\x22";
    EXPECTED[1] = ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
    ASSERT_EQ_BYTES_SIZED(out_buff, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(EXPECTED) - 1);
    ASSERT_EQ(_anj_exchange_process(&exchange_ctx,
                                    ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                    &inout_msg),
              ANJ_EXCHANGE_STATE_FINISHED);
}
#    endif // ANJ_WITH_OBSERVE_COMPOSITE

ANJ_UNIT_TEST(observe_comp_op, remove_observations) {
    TEST_INIT();
    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[0].token.size = 1;
    anj.observe_ctx.observations[0].token.bytes[0] = 0x24;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_INSTANCE_PATH(3, 1);
    anj.observe_ctx.observations[1].token.size = 1;
    anj.observe_ctx.observations[1].token.bytes[0] = 0x23;
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[1].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[1].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    anj.observe_ctx.observations[2].token.size = 1;
    anj.observe_ctx.observations[2].token.bytes[0] = 0x22;
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[2].content_format_opt =
            _ANJ_COAP_FORMAT_SENML_CBOR;
    anj.observe_ctx.observations[2].accept_opt = _ANJ_COAP_FORMAT_NOT_DEFINED;
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    anj.observe_ctx.observations[3].ssid = 2;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_INSTANCE_PATH(3, 3);
    anj.observe_ctx.observations[3].token.size = 1;
    anj.observe_ctx.observations[3].token.bytes[0] = 0x23;

    _anj_observe_remove_all_observations(&anj, 1);
    ASSERT_EQ(anj.observe_ctx.observations[0].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[1].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[2].ssid, 0);
    ASSERT_EQ(anj.observe_ctx.observations[3].ssid, 2);
    ASSERT_EQ(anj.observe_ctx.observations[4].ssid, 0);
}

#endif // ANJ_WITH_OBSERVE
