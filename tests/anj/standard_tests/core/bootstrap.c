/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>

#include "../../../../src/anj/coap/coap.h"
#include "../../../../src/anj/core/bootstrap.h"
#include "../../../../src/anj/dm/dm_io.h"
#include "../../../../src/anj/exchange.h"
#include "../mock/time_api_mock.h"

#include <anj_unit_test.h>

static anj_dm_res_t server_ssid = {
    .rid = 0,
    .kind = ANJ_DM_RES_R,
    .type = ANJ_DATA_TYPE_INT,
};

static anj_dm_obj_inst_t server_insts[2] = {
    {
        .iid = 0,
        .res_count = 1,
        .resources = &server_ssid
    },
    {
        .iid = ANJ_ID_INVALID,
        .res_count = 1,
        .resources = &server_ssid
    }
};

static int g_ssid = 1;
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
        out_value->int_value = 1;
        return 0;
    }

    if (iid == 0 && rid == 1) {
        out_value->bool_value = true;
    }
    if (iid == 1 && rid == 1) {
        out_value->bool_value = false;
    }
    if (iid == 0 && rid == 10) {
        out_value->int_value = 0;
    }
    if (iid == 1 && rid == 10) {
        out_value->int_value = g_ssid;
    }
    return 0;
}

static int inst_delete(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;

    anj_dm_obj_inst_t *insts = (anj_dm_obj_inst_t *) obj->insts;
    if (insts[0].iid == iid) {
        insts[0] = insts[1];
        insts[1].iid = ANJ_ID_INVALID;
    } else if (insts[1].iid == iid) {
        insts[1].iid = ANJ_ID_INVALID;
    }
    return 0;
}
static anj_dm_handlers_t handlers_server = {
    .inst_delete = inst_delete,
    .res_read = res_read,
};
static anj_dm_obj_t mock_server = {
    .oid = 1,
    .insts = server_insts,
    .handlers = &handlers_server,
    .max_inst_count = 1
};

static anj_dm_res_t res_obj_0[] = {
    {
        // bootstrap_server
        .rid = 1,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_BOOL,
    },
    {
        // ssid
        .rid = 10,
        .kind = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    }
};

static anj_dm_obj_inst_t security_insts[2] = {
    {
        .iid = 0,
        .res_count = 2,
        .resources = res_obj_0
    },
    {
        .iid = 1,
        .res_count = 2,
        .resources = res_obj_0
    }
};

static anj_dm_handlers_t handlers_security = {
    .inst_delete = inst_delete,
    .res_read = res_read,
};
static anj_dm_obj_t mock_security = {
    .oid = 0,
    .insts = security_insts,
    .handlers = &handlers_security,
    .max_inst_count = 2
};

#define TEST_INIT()                                  \
    anj_t anj = { 0 };                               \
    const char endpoint[] = "test";                  \
    _anj_dm_initialize(&anj);                        \
    ASSERT_OK(anj_dm_add_obj(&anj, &mock_server));   \
    ASSERT_OK(anj_dm_add_obj(&anj, &mock_security)); \
    mock_time_reset();                               \
    _anj_bootstrap_ctx_init(&anj, endpoint,          \
                            anj_time_duration_new(247, ANJ_TIME_UNIT_S));

#define PROCESS_BOOTSTRAP_REQUEST(Exchange_result)                        \
    _anj_coap_msg_t request = { 0 };                                      \
    _anj_exchange_handlers_t exchange_handlers;                           \
    ASSERT_EQ(_anj_bootstrap_process(&anj, &request, &exchange_handlers), \
              _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND);                        \
    exchange_handlers.completion(exchange_handlers.arg, NULL,             \
                                 Exchange_result);                        \
    ASSERT_EQ(_anj_bootstrap_process(&anj, &request, &exchange_handlers), \
              Exchange_result ? _ANJ_BOOTSTRAP_ERR_EXCHANGE_ERROR         \
                              : _ANJ_BOOTSTRAP_IN_PROGRESS);

#define PROCESS_BOOTSTRAP_FINISH(Exchange_result, Response_code, Final_result) \
    uint8_t response_code;                                                     \
    _anj_bootstrap_finish_request(&anj, &response_code, &exchange_handlers);   \
    ASSERT_EQ(response_code, Response_code);                                   \
    ASSERT_EQ(_anj_bootstrap_process(&anj, &request, &exchange_handlers),      \
              _ANJ_BOOTSTRAP_IN_PROGRESS);                                     \
    exchange_handlers.completion(exchange_handlers.arg, NULL,                  \
                                 Exchange_result);                             \
    ASSERT_EQ(_anj_bootstrap_process(&anj, &request, &exchange_handlers),      \
              Final_result);

ANJ_UNIT_TEST(bootstrap, bootstrap_request) {
    TEST_INIT();
    PROCESS_BOOTSTRAP_REQUEST(0);

    // check Bootstrap-Request message only once, in this test
    uint8_t msg_buffer[100];
    size_t msg_size;
    request.token.size = 2;
    request.token.bytes[0] = 0x01;
    request.token.bytes[1] = 0x01;
    request.coap_binding_data.message_id = 0x0404;
    ASSERT_OK(_anj_coap_encode_udp(&request, msg_buffer, sizeof(msg_buffer),
                                   &msg_size));
    uint8_t EXPECTED[] =
            "\x42"                              // Confirmable, tkl 2
            "\x02\x04\x04"                      // POST 0x02, msg id 0404
            "\x01\x01"                          // token
            "\xb2\x62\x73"                      // uri path /bs
            "\x47\x65\x70\x3d\x74\x65\x73\x74"  // uri-query ep=test
            "\x07\x70\x63\x74\x3d\x31\x31\x32"; // uri-query pct=112
    ASSERT_EQ_BYTES_SIZED(msg_buffer, EXPECTED, sizeof(EXPECTED) - 1);
    ANJ_UNIT_ASSERT_EQUAL(msg_size, sizeof(EXPECTED) - 1);

    PROCESS_BOOTSTRAP_FINISH(0, ANJ_COAP_CODE_CHANGED, _ANJ_BOOTSTRAP_FINISHED);
}

ANJ_UNIT_TEST(bootstrap, bootstrap_request_exchange_failed) {
    TEST_INIT();
    PROCESS_BOOTSTRAP_REQUEST(-1);
}

ANJ_UNIT_TEST(bootstrap, bootstrap_finish_exchange_failed) {
    TEST_INIT();
    PROCESS_BOOTSTRAP_REQUEST(0);
    PROCESS_BOOTSTRAP_FINISH(-1, ANJ_COAP_CODE_CHANGED,
                             _ANJ_BOOTSTRAP_ERR_EXCHANGE_ERROR);
}

ANJ_UNIT_TEST(bootstrap, bootstrap_data_model_validation_error) {
    TEST_INIT();
    PROCESS_BOOTSTRAP_REQUEST(0);
    g_ssid = 2;
    PROCESS_BOOTSTRAP_FINISH(0, ANJ_COAP_CODE_NOT_ACCEPTABLE,
                             _ANJ_BOOTSTRAP_ERR_DATA_MODEL_VALIDATION);
    g_ssid = 1;
}

ANJ_UNIT_TEST(bootstrap, bootstrap_timeout) {
    TEST_INIT();
    PROCESS_BOOTSTRAP_REQUEST(0);
    ASSERT_EQ(_anj_bootstrap_process(&anj, &request, &exchange_handlers),
              _ANJ_BOOTSTRAP_IN_PROGRESS);
    mock_time_advance(
            anj_time_duration_add(anj.bootstrap_ctx.bootstrap_lifetime,
                                  anj_time_duration_new(1, ANJ_TIME_UNIT_S)));

    ASSERT_EQ(_anj_bootstrap_process(&anj, &request, &exchange_handlers),
              _ANJ_BOOTSTRAP_ERR_BOOTSTRAP_TIMEOUT);
}

ANJ_UNIT_TEST(bootstrap, bootstrap_finish_without_ongoing_bootstrap) {
    TEST_INIT();
    _anj_coap_msg_t request = { 0 };
    _anj_exchange_handlers_t exchange_handlers;
    uint8_t response_code;
    // Bootstrap-Finish received out of order, next _anj_bootstrap_process
    // should start new bootstrap
    _anj_bootstrap_finish_request(&anj, &response_code, &exchange_handlers);
    ASSERT_EQ(response_code, ANJ_COAP_CODE_NOT_ACCEPTABLE);
    ASSERT_EQ(_anj_bootstrap_process(&anj, &request, &exchange_handlers),
              _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND);
}

ANJ_UNIT_TEST(bootstrap, bootstrap_network_error) {
    TEST_INIT();
    PROCESS_BOOTSTRAP_REQUEST(0);
    _anj_bootstrap_connection_lost(&anj);
    ASSERT_EQ(_anj_bootstrap_process(&anj, &request, &exchange_handlers),
              _ANJ_BOOTSTRAP_ERR_NETWORK);
}
