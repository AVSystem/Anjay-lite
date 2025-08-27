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

#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/utils.h>

#ifdef ANJ_WITH_OBSERVE
#    include "../../../src/anj/observe/observe.h"
#endif // ANJ_WITH_OBSERVE

#include "../../../src/anj/dm/dm_integration.h"
#include "../../../src/anj/dm/dm_io.h"
#include "../../../src/anj/io/io.h"
#include "../../src/anj/coap/coap.h"
#include "../../src/anj/exchange.h"

#include <anj_unit_test.h>

#define MAX_OBJECT_COUNT 6

enum transaction_end_result {
    TRANSACTION_END_RESULT_OBJ_0,
    TRANSACTION_END_RESULT_OBJ_1,
    TRANSACTION_END_RESULT_OBJ_2,
    TRANSACTION_END_RESULT_OBJ_3,
    TRANSACTION_END_RESULT_OBJ_4,
    TRANSACTION_END_RESULT_OBJ_21
};

static anj_dm_res_t obj_2_new_inst_res[2] = {
    {
        .rid = 1,
        .operation = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 4,
        .operation = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_STRING
    }
};

static int inst_create(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
    (void) anj;

    anj_dm_obj_inst_t *inst = (anj_dm_obj_inst_t *) obj->insts;
    // only one scenario of use
    inst[1] = inst[0];
    inst[0].iid = iid;
    inst[0].resources = obj_2_new_inst_res;
    inst[0].res_count = 2;
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

static char res_4_buff[100] = { 0 };
static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) iid;
    (void) riid;
    (void) out_value;
    if (rid == 4) {
        out_value->bytes_or_string.data = res_4_buff;
    } else if (riid == 1 && obj->oid != 222) {
        out_value->int_value = 6;
    } else if (riid == 2) {
        out_value->int_value = 7;
    } else if (rid == 0) {
        out_value->int_value = (iid == 1) ? 1 : 3;
    } else if (rid == 1) {
        out_value->int_value = (iid == 1) ? 2 : 4;
    }
    return 0;
}

static int res_execute_counter;
static size_t res_execute_arg_len;
static const char *res_execute_arg;
static int res_execute(anj_t *anj,
                       const anj_dm_obj_t *obj,
                       anj_iid_t iid,
                       anj_rid_t rid,
                       const char *execute_arg,
                       size_t execute_arg_len) {
    (void) anj;
    (void) obj;
    (void) iid;
    (void) rid;
    res_execute_arg = execute_arg;
    res_execute_arg_len = execute_arg_len;
    res_execute_counter++;
    return 0;
}

static int res_inst_delete(anj_t *anj,
                           const anj_dm_obj_t *obj,
                           anj_iid_t iid,
                           anj_rid_t rid,
                           anj_riid_t riid) {
    (void) anj;
    (void) obj;
    (void) iid;
    (void) rid;
    (void) riid;

    anj_riid_t *insts;
    if (obj->oid == 222) {
        insts = (anj_riid_t *) obj->insts[0].resources[0].insts;
    } else {
        insts = (anj_riid_t *) obj->insts[1].resources[rid].insts;
    }

    if (insts[0] == riid) {
        insts[0] = insts[1];
        insts[1] = ANJ_ID_INVALID;
    } else if (insts[1] == riid) {
        insts[1] = ANJ_ID_INVALID;
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

    anj_dm_res_t *res;
    if (obj->oid == 222) {
        res = (anj_dm_res_t *) &obj->insts[0].resources[0];
    } else {
        res = (anj_dm_res_t *) &obj->insts[1].resources[rid];
    }
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

static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value);

static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj);
static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj);
static void transaction_end(anj_t *anj, const anj_dm_obj_t *obj, int result);

static const anj_dm_handlers_t handlers = {
    .inst_create = inst_create,
    .inst_delete = inst_delete,
    .res_execute = res_execute,
    .res_inst_delete = res_inst_delete,
    .res_inst_create = res_inst_create,
    .res_write = res_write,
    .res_read = res_read,
    .transaction_begin = transaction_begin,
    .transaction_validate = transaction_validate,
    .transaction_end = transaction_end
};

static anj_riid_t res_insts[] = { 1, 2 };

static anj_dm_res_t inst_1_res[] = {
    {
        .rid = 0,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 1,
        .operation = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT
    }
};

static anj_dm_res_t inst_2_res[] = {
    {
        .rid = 0,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 1,
        .operation = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT
    },
    {
        .rid = 2,
        .operation = ANJ_DM_RES_RWM,
        .type = ANJ_DATA_TYPE_INT,
        .max_inst_count = 2,
        .insts = res_insts,
    },
    {
        .rid = 3,
        .operation = ANJ_DM_RES_WM,
        .type = ANJ_DATA_TYPE_INT,
        .max_inst_count = 0,
    },
    {
        .rid = 4,
        .operation = ANJ_DM_RES_RW,
        .type = ANJ_DATA_TYPE_STRING
    },
    {
        .rid = 5,
        .operation = ANJ_DM_RES_E
    }
};

static anj_dm_obj_inst_t obj_1_insts[2] = {
    {
        .iid = 1,
        .res_count = 2,
        .resources = inst_1_res
    },
    {
        .iid = 2,
        .res_count = 6,
        .resources = inst_2_res
    }
};

static anj_dm_obj_t obj_0 = {
    .oid = 0,
    .insts = &(anj_dm_obj_inst_t) {
        .iid = 0,
        .res_count = 1,
        .resources =
                (anj_dm_res_t[])
                        {
                            {
                                .rid = 0,
                                .operation = ANJ_DM_RES_R,
                                .type = ANJ_DATA_TYPE_INT
                            }
                        }
    },
    .max_inst_count = 1,
    .handlers = &handlers
};

static anj_dm_obj_t obj_1 = {
    .oid = 111,
    .version = "1.1",
    .insts = obj_1_insts,
    .max_inst_count = 2,
    .handlers = &handlers
};

static anj_riid_t obj_2_res_insts[] = { 1, ANJ_ID_INVALID };
static anj_dm_res_t obj_2_res = {
    .rid = 2,
    .operation = ANJ_DM_RES_RWM,
    .type = ANJ_DATA_TYPE_INT,
    .max_inst_count = 2,
    .insts = obj_2_res_insts
};

static anj_dm_obj_inst_t obj_2_insts[2] = {
    {
        .iid = 1,
        .res_count = 1,
        .resources = &obj_2_res
    },
    {
        .iid = ANJ_ID_INVALID,
        .res_count = 1,
        .resources = obj_2_new_inst_res
    }
};

static void reset_obj_2_insts(void) {
    obj_2_insts[0].iid = 1;
    obj_2_insts[1].iid = ANJ_ID_INVALID;
}

static anj_dm_obj_t obj_2 = {
    .oid = 222,
    .insts = obj_2_insts,
    .max_inst_count = 2,
    .handlers = &handlers
};

static anj_dm_obj_t obj_4 = {
    .oid = 444,
    .insts = &(anj_dm_obj_inst_t) {
        .iid = 0,
        .res_count = 1,
        .resources =
                (anj_dm_res_t[])
                        {
                            {
                                .rid = 0,
                                .operation = ANJ_DM_RES_R,
                                .type = ANJ_DATA_TYPE_INT
                            }
                        }
    },
    .max_inst_count = 1,
    .handlers = &handlers
};

static anj_dm_obj_t obj_21 = {
    .oid = 21,
    .insts = &(anj_dm_obj_inst_t) {
        .iid = 0,
        .res_count = 1,
        .resources =
                (anj_dm_res_t[])
                        {
                            {
                                .rid = 0,
                                .operation = ANJ_DM_RES_R,
                                .type = ANJ_DATA_TYPE_INT
                            }
                        }
    },
    .max_inst_count = 1,
    .handlers = &handlers
};

#ifdef ANJ_WITH_EXTERNAL_DATA
static bool opened;
static bool closed;

static char *ptr_for_callback = NULL;
static size_t ext_data_size = 0;
static size_t external_data_handler_call_count = 0;
static size_t external_data_handler_when_error = 0;
static int external_data_handler(void *buffer,
                                 size_t *inout_size,
                                 size_t offset,
                                 void *user_args) {
    (void) user_args;
    assert(&ptr_for_callback);
    ANJ_UNIT_ASSERT_TRUE(opened);
    external_data_handler_call_count++;
    if (external_data_handler_call_count == external_data_handler_when_error) {
        return -1;
    }
    size_t bytes_to_copy = ANJ_MIN(ext_data_size, *inout_size);
    memcpy(buffer, &ptr_for_callback[offset], bytes_to_copy);
    ext_data_size -= bytes_to_copy;
    *inout_size = bytes_to_copy;
    if (ext_data_size) {
        return ANJ_IO_NEED_NEXT_CALL;
    }
    return 0;
}

static int external_data_open(void *user_args) {
    (void) user_args;
    external_data_handler_call_count = 0;
    ANJ_UNIT_ASSERT_FALSE(opened);
    opened = true;

    return 0;
}

static void external_data_close(void *user_args) {
    (void) user_args;
    ANJ_UNIT_ASSERT_FALSE(closed);
    closed = true;
}

static int res_read_external(anj_t *anj,
                             const anj_dm_obj_t *obj,
                             anj_iid_t iid,
                             anj_rid_t rid,
                             anj_riid_t riid,
                             anj_res_value_t *out_value) {
    (void) anj;
    (void) iid;
    (void) riid;
    (void) out_value;
    if (rid == 1) {
        out_value->external_data.get_external_data = external_data_handler;
        out_value->external_data.open_external_data = external_data_open;
        out_value->external_data.close_external_data = external_data_close;
    } else if (rid == 2) {
        out_value->int_value = 3;
    }
    return 0;
}

static const anj_dm_handlers_t handlers_external = {
    .res_read = res_read_external,
};

static anj_dm_res_t obj_3_res[] = {
    {
        .rid = 1,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_EXTERNAL_STRING,
    },
    {
        .rid = 2,
        .operation = ANJ_DM_RES_R,
        .type = ANJ_DATA_TYPE_INT,
    }
};

static anj_dm_obj_inst_t obj_3_insts = {
    .iid = 1,
    .res_count = 2,
    .resources = obj_3_res
};

static anj_dm_obj_t obj_3 = {
    .oid = 333,
    .insts = &obj_3_insts,
    .max_inst_count = 1,
    .handlers = &handlers_external
};

#endif // ANJ_WITH_EXTERNAL_DATA

static int write_value;
static int write_value2;
static int write_value3;
static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) anj;
    (void) obj;
    (void) iid;

    ASSERT_TRUE(obj != &obj_0 && obj != &obj_21);

    if (rid == 4) {
        return anj_dm_write_string_chunked(value, res_4_buff,
                                           sizeof(res_4_buff), NULL);
    } else if (iid == 2 && rid == 2 && (riid == 2 || riid == 5)) {
        write_value2 = value->int_value;
    } else if (iid == 1 && rid == 2 && (riid == 1 || riid == 2)) {
        write_value3 = value->int_value;
    } else {
        write_value = value->int_value;
    }

    return 0;
}

static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;

    ASSERT_TRUE(obj != &obj_0 && obj != &obj_21);

    return 0;
}

static bool validation_error = false;
static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj) {
    ASSERT_TRUE(obj != &obj_0 && obj != &obj_21);

    if (validation_error) {
        return ANJ_DM_ERR_BAD_REQUEST;
    }
    return 0;
}

static int transaction_end_results[MAX_OBJECT_COUNT];
static void transaction_end(anj_t *anj, const anj_dm_obj_t *obj, int result) {
    (void) anj;

    ASSERT_TRUE(obj != &obj_0 && obj != &obj_21);

    if (obj == &obj_0) {
        transaction_end_results[TRANSACTION_END_RESULT_OBJ_0] = result;
    } else if (obj == &obj_1) {
        transaction_end_results[TRANSACTION_END_RESULT_OBJ_1] = result;
    } else if (obj == &obj_2) {
        transaction_end_results[TRANSACTION_END_RESULT_OBJ_2] = result;
#ifdef ANJ_WITH_EXTERNAL_DATA
    } else if (obj == &obj_3) {
        transaction_end_results[TRANSACTION_END_RESULT_OBJ_3] = result;
#endif // ANJ_WITH_EXTERNAL_DATA
    } else if (obj == &obj_4) {
        transaction_end_results[TRANSACTION_END_RESULT_OBJ_4] = result;
    } else if (obj == &obj_21) {
        transaction_end_results[TRANSACTION_END_RESULT_OBJ_21] = result;
    }
}

#define DEFAULT_TRANSACTION_END_RESULT 0xFFFFFFFF

#define SET_UP()                                           \
    uint8_t payload[512];                                  \
    size_t payload_len = sizeof(payload);                  \
    _anj_coap_msg_t msg;                                   \
    memset(&msg, 0, sizeof(msg));                          \
    msg.token.size = 1;                                    \
    msg.token.bytes[0] = 0x01;                             \
    msg.coap_binding_data.udp.message_id = 0x1111;         \
    anj_t anj = { 0 };                                     \
    _anj_dm_initialize(&anj);                              \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_0)); \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1)); \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_2)); \
                                                           \
    _anj_exchange_ctx_t exchange_ctx;                      \
    _anj_exchange_init(&exchange_ctx, 0);                  \
    uint8_t response_code;                                 \
    _anj_exchange_handlers_t handlers;                     \
    write_value = 0;                                       \
    write_value2 = 0;                                      \
    write_value3 = 0;                                      \
    memset(&transaction_end_results, 0xFF, sizeof(transaction_end_results))

#define PROCESS_REQUEST(Bootstrap)                                             \
    _anj_dm_process_request(&anj, &msg, Bootstrap ? _ANJ_SSID_BOOTSTRAP : 1,   \
                            &response_code, &handlers);                        \
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_server_request(                    \
                                  &exchange_ctx, response_code, &msg,          \
                                  &handlers, payload, payload_len),            \
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);                     \
    ANJ_UNIT_ASSERT_EQUAL(                                                     \
            _anj_exchange_process(&exchange_ctx,                               \
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg), \
            ANJ_EXCHANGE_STATE_FINISHED);

#define PROCESS_REQUEST_BLOCK()                                                \
    _anj_dm_process_request(&anj, &msg, 1, &response_code, &handlers);         \
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_server_request(                    \
                                  &exchange_ctx, response_code, &msg,          \
                                  &handlers, payload, payload_len),            \
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);                     \
    ANJ_UNIT_ASSERT_EQUAL(                                                     \
            _anj_exchange_process(&exchange_ctx,                               \
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg), \
            ANJ_EXCHANGE_STATE_WAITING_MSG);

#define PROCESS_REQUEST_WITH_ERROR(Error_Code)                                 \
    _anj_dm_process_request(&anj, &msg, 1, &response_code, &handlers);         \
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_server_request(                    \
                                  &exchange_ctx, response_code, &msg,          \
                                  &handlers, payload, payload_len),            \
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);                     \
    ANJ_UNIT_ASSERT_EQUAL(                                                     \
            _anj_exchange_process(&exchange_ctx,                               \
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg), \
            ANJ_EXCHANGE_STATE_FINISHED);

static void verify_payload(const char *expected,
                           size_t expected_len,
                           _anj_coap_msg_t *msg) {
    uint8_t out_buff[500];
    size_t out_msg_size = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
            msg, out_buff, sizeof(out_buff), &out_msg_size));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(out_buff, expected, expected_len);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, expected_len);
}

// register is only client operation here
ANJ_UNIT_TEST(dm_integration, register_operation) {
    SET_UP();
    msg.operation = ANJ_OP_REGISTER;
    msg.attr.register_attr.has_endpoint = true;
    msg.attr.register_attr.endpoint = "name";

    int arg_val;
    int *arg = &arg_val;
    _anj_dm_process_register_update_payload(&anj, &handlers);
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_client_request(&exchange_ctx, &msg,
                                                           &handlers, payload,
                                                           payload_len),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_WAITING_MSG);

    char expected[] = "\x48"                             // Confirmable, tkl8
                      "\x02\x00\x00"                     // POST 0x02, msg id
                      "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                      "\xb2\x72\x64"                     // uri path /rd
                      "\x11\x28" // content_format: application/link-format
                      "\x37\x65\x70\x3d\x6e\x61\x6d\x65" // uri-query ep=name
                      "\xFF"
                      "</111>;ver=1.1,</111/1>,</111/2>,</222>,</222/1>";

    uint8_t out_buff[200];
    size_t out_msg_size = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
            &msg, out_buff, sizeof(out_buff), &out_msg_size));
#define MSG_ID_OFFSET 2
    expected[MSG_ID_OFFSET] =
            (uint8_t) (msg.coap_binding_data.udp.message_id >> 8);
    expected[MSG_ID_OFFSET + 1] =
            (uint8_t) msg.coap_binding_data.udp.message_id;
#define TOKEN_OFFSET 4
    memcpy(&expected[TOKEN_OFFSET], msg.token.bytes, msg.token.size);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(out_buff, expected, sizeof(expected) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(expected) - 1);

    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_CREATED;
    msg.payload_size = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_FINISHED);
}

ANJ_UNIT_TEST(dm_integration, update_operation_with_payload) {
    SET_UP();
    msg.operation = ANJ_OP_UPDATE;
    msg.location_path.location[0] = "name";
    msg.location_path.location_len[0] = 4;
    msg.location_path.location_count = 1;

    int arg_val;
    int *arg = &arg_val;
    _anj_dm_process_register_update_payload(&anj, &handlers);
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_client_request(&exchange_ctx, &msg,
                                                           &handlers, payload,
                                                           payload_len),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_WAITING_MSG);

    char expected[] = "\x48"                             // Confirmable, tkl
                      "\x02\x00\x00"                     // POST 0x02, msg id
                      "\x00\x00\x00\x00\x00\x00\x00\x00" // token
                      "\xb4\x6e\x61\x6d\x65"             // uri path /name
                      "\x11\x28" // content_format: application/link-format
                      "\xFF"
                      "</111>;ver=1.1,</111/1>,</111/2>,</222>,</222/1>";

    uint8_t out_buff[200];
    size_t out_msg_size = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_coap_encode_udp(
            &msg, out_buff, sizeof(out_buff), &out_msg_size));
#define MSG_ID_OFFSET 2
    expected[MSG_ID_OFFSET] =
            (uint8_t) (msg.coap_binding_data.udp.message_id >> 8);
    expected[MSG_ID_OFFSET + 1] =
            (uint8_t) msg.coap_binding_data.udp.message_id;
#define TOKEN_OFFSET 4
    memcpy(&expected[TOKEN_OFFSET], msg.token.bytes, msg.token.size);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(out_buff, expected, sizeof(expected) - 1);
    ANJ_UNIT_ASSERT_EQUAL(out_msg_size, sizeof(expected) - 1);

    msg.operation = ANJ_OP_RESPONSE;
    msg.coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    msg.msg_code = ANJ_COAP_CODE_CREATED;
    msg.payload_size = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_FINISHED);
}
#ifdef ANJ_WITH_OBSERVE

static void discover_test(anj_uri_path_t *path,
                          char *expected_payload,
                          size_t expected_payload_len,
                          uint8_t depth) {
    SET_UP();
    _anj_observe_init(&anj);
    msg.operation = ANJ_OP_DM_DISCOVER;
    msg.accept = _ANJ_COAP_FORMAT_LINK_FORMAT;
    msg.uri = *path;
    if (depth != UINT8_MAX) {
        msg.attr.discover_attr.has_depth = true;
        msg.attr.discover_attr.depth = depth;
    }
    PROCESS_REQUEST(false);
    char header[] = "\x61"             // ACK, tkl 1
                    "\x45\x11\x11\x01" // content, msg_id token
                    "\xC1\x28\xFF";    // content_format: link format
    char expected[512];
    size_t header_len = sizeof(header) - 1;
    memcpy(expected, header, header_len);
    memcpy(expected + header_len, expected_payload, expected_payload_len);
    verify_payload(expected, header_len + expected_payload_len, &msg);
}

ANJ_UNIT_TEST(dm_integration, discover_operation_object) {
    char expected[] = "</111>;ver=1.1,</111/1>,</111/1/0>,</111/1/1>,</111/"
                      "2>,</111/2/0>,</111/2/1>,</111/2/2>;dim=2,</111/2/"
                      "3>;dim=0,</111/2/4>,</111/2/5>";
    discover_test(&ANJ_MAKE_OBJECT_PATH(111), expected, sizeof(expected) - 1,
                  UINT8_MAX);
}

ANJ_UNIT_TEST(dm_integration, discover_operation_instance) {
    char expected[] = "</111/2>,</111/2/0>,</111/2/1>,</111/2/2>;dim=2,</111/2/"
                      "3>;dim=0,</111/2/4>,</111/2/5>";
    discover_test(&ANJ_MAKE_INSTANCE_PATH(111, 2), expected,
                  sizeof(expected) - 1, UINT8_MAX);
}

ANJ_UNIT_TEST(dm_integration, discover_operation_instance_with_max_depth) {
    char expected[] =
            "</111/2>,</111/2/0>,</111/2/1>,</111/2/2>;dim=2,</111/2/2/1>,</"
            "111/2/2/2>,</111/2/3>;dim=0,</111/2/4>,</111/2/5>";
    discover_test(&ANJ_MAKE_INSTANCE_PATH(111, 2), expected,
                  sizeof(expected) - 1, 3);
}

ANJ_UNIT_TEST(dm_integration, discover_operation_resource) {
    char expected[] = "</111/2/0>";
    discover_test(&ANJ_MAKE_RESOURCE_PATH(111, 2, 0), expected,
                  sizeof(expected) - 1, UINT8_MAX);
}

ANJ_UNIT_TEST(dm_integration, discover_operation_multi_resource) {
    char expected[] = "</111/2/2>;dim=2";
    discover_test(&ANJ_MAKE_RESOURCE_PATH(111, 2, 2), expected,
                  sizeof(expected) - 1, 0);
}

ANJ_UNIT_TEST(dm_integration,
              discover_operation_multi_resource_with_instances_in_payload) {
    char expected[] = "</111/2/2>;dim=2,</111/2/2/1>,</111/2/2/2>";
    discover_test(&ANJ_MAKE_RESOURCE_PATH(111, 2, 2), expected,
                  sizeof(expected) - 1, UINT8_MAX);
}

ANJ_UNIT_TEST(dm_integration, discover_operation_with_attr) {
    SET_UP();
    _anj_observe_init(&anj);
    anj.observe_ctx.attributes_storage[0] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_OBJECT_PATH(111),
        .attr.has_min_period = true,
        .attr.min_period = 2,
        .attr.has_con = true,
        .attr.con = 1,
    };
    anj.observe_ctx.attributes_storage[1] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_INSTANCE_PATH(111, 1),
        .attr.has_min_period = true,
        .attr.min_period = 10,
        .attr.has_max_period = true,
        .attr.max_period = 20,
    };
    anj.observe_ctx.attributes_storage[2] = (_anj_observe_attr_storage_t) {
        .ssid = 1,
        .path = ANJ_MAKE_RESOURCE_PATH(111, 1, 1),
        .attr.has_step = true,
        .attr.step = 1,
    };

    msg.operation = ANJ_OP_DM_DISCOVER;
    msg.uri = ANJ_MAKE_INSTANCE_PATH(111, 1);
    PROCESS_REQUEST(false)
    char expected[] =
            "\x61"             // ACK, tkl 1
            "\x45\x11\x11\x01" // content, msg_id token
            "\xC1\x28"         // content_format: link format
            "\xFF"
            "</111/1>;pmin=10;pmax=20;con=1,</111/1/0>,</111/1/1>;st=1";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}
#endif // ANJ_WITH_OBSERVE

ANJ_UNIT_TEST(dm_integration, read_operation) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_OBJECT_PATH(111);
    PROCESS_REQUEST(false)
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senmlcbor
                      "\xFF"
                      "\x85\xA3"
                      "\x21\x64\x2F\x31\x31\x31" // /111
                      "\x00\x64\x2F\x31\x2F\x30" // /1/0
                      "\x02\x01"                 // 1
                      "\xA2"
                      "\x00\x64\x2F\x32\x2F\x30" // /2/0
                      "\x02\x03"                 // 3
                      "\xA2"
                      "\x00\x66\x2F\x32\x2F\x32\x2F\x31" // /2/2/1
                      "\x02\x06"                         // 6
                      "\xA2"
                      "\x00\x66\x2F\x32\x2F\x32\x2F\x32" // /2/2/2
                      "\x02\x07"                         // 7
                      "\xA2"
                      "\x00\x64\x2F\x32\x2F\x34" // /2/4
                      "\x03\x60";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_operation_block) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_OBJECT_PATH(111);
    payload_len = 32;
    PROCESS_REQUEST_BLOCK();
    char expected[] =
            "\x61"             // ACK, tkl 1
            "\x45\x11\x11\x01" // content, msg_id token
            "\xC1\x70"         // content_format: senmlcbor
            "\xB1\x09"         // block2 0, size 32, more
            "\xFF"
            "\x85\xA3\x21\x64\x2F\x31\x31\x31\x00\x64\x2F\x31\x2F\x30\x02\x01"
            "\xA2\x00\x64\x2F\x32\x2F\x30\x02\x03\xA2\x00\x66\x2F\x32\x2F\x32";
    verify_payload(expected, sizeof(expected) - 1, &msg);

    msg.operation = ANJ_OP_DM_READ;
    msg.payload_size = 0;
    msg.block.number++;
    msg.coap_binding_data.udp.message_id++;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_FINISHED);
    char expected2[] =
            "\x61"             // ACK, tkl 1
            "\x45\x11\x12\x01" // content, msg_id token
            "\xC1\x70"         // content_format: senmlcbor
            "\xB1\x11"         // block2 1, size 32
            "\xFF"
            "\x2F\x31\x02\x06\xA2\x00\x66\x2F\x32\x2F\x32\x2F\x32\x02"
            "\x07\xA2\x00\x64\x2F\x32\x2F\x34\x03\x60";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
}

// for string/bytes resource during block transfer data model must store
// resource value (anj_io_out_entry_t)
ANJ_UNIT_TEST(dm_integration, read_operation_string_resource_block) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ;
    msg.accept = _ANJ_COAP_FORMAT_PLAINTEXT;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 2, 4);
    strcpy(res_4_buff, "abcdefghijklmnoprstuwxyz");
    payload_len = 16;
    PROCESS_REQUEST_BLOCK();
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC0"             // content_format: plaintext
                      "\xB1\x08"         // block2 0, size 16, more
                      "\xFF"
                      "abcdefghijklmnop";
    verify_payload(expected, sizeof(expected) - 1, &msg);

    msg.operation = ANJ_OP_DM_READ;
    msg.payload_size = 0;
    msg.block.number++;
    msg.coap_binding_data.udp.message_id++;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_FINISHED);
    char expected2[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x12\x01" // content, msg_id token
                       "\xC0"             // content_format: plaintext
                       "\xB1\x10"         // block2 1, size 16
                       "\xFF"
                       "rstuwxyz";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
}

// handle_read_payload_result() logic check
void check_if_anj_io_fails_if_resource_size_is_changed() {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 2, 4);
    payload_len = 16;
    _anj_dm_process_request(&anj, &msg, 1, &response_code, &handlers);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_new_server_request(&exchange_ctx, response_code, &msg,
                                             &handlers, payload, payload_len),
            ANJ_EXCHANGE_STATE_MSG_TO_SEND);
}

ANJ_UNIT_TEST(dm_integration, read_operation_block_resource_size_changes) {
    memset(res_4_buff, 0, sizeof(res_4_buff));
    for (int i = 0; i < sizeof(res_4_buff) - 1; i++) {
        res_4_buff[i] = 'a';
        check_if_anj_io_fails_if_resource_size_is_changed();
    }
    memset(res_4_buff, 0, sizeof(res_4_buff));
}

ANJ_UNIT_TEST(dm_integration, read_operation_block_with_termination) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_OBJECT_PATH(111);
    payload_len = 32;
    PROCESS_REQUEST_BLOCK();
    msg.operation = ANJ_OP_DM_READ;
    msg.payload_size = 0;
    msg.block.number++;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    _anj_exchange_terminate(&exchange_ctx);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.op_in_progress, false);
}

ANJ_UNIT_TEST(dm_integration, empty_read_senml_cbor) {
    SET_UP();
    obj_1_insts[0].res_count = 0;
    msg.operation = ANJ_OP_DM_READ;
    msg.uri = ANJ_MAKE_INSTANCE_PATH(111, 1);
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF\x80";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    obj_1_insts[0].res_count = 2;
}

ANJ_UNIT_TEST(dm_integration, empty_read_lwm2m_cbor) {
    SET_UP();
    obj_1_insts[0].res_count = 0;
    msg.operation = ANJ_OP_DM_READ;
    msg.uri = ANJ_MAKE_INSTANCE_PATH(111, 1);
    msg.accept = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC2\x2D\x18"     // content_format: lwm2m_cbor
                      "\xFF\xBF\xFF";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    obj_1_insts[0].res_count = 2;
}

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
ANJ_UNIT_TEST(dm_integration, read_composite) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();

    char input_payload[] = {
        "\x82"           /* array(2) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x68/111/1/0"   /* text(8) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/1" /* text(10) */
    };

    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senmlcbor
                      "\xFF"
                      "\x82"
                      "\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01"
                      "\xA2"
                      "\x00\x6A/222/1/2/1"
                      "\x02\x00";

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_block1) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    msg.block = (_anj_block_t) {
        .block_type = ANJ_OPTION_BLOCK_1,
        .number = 0,
        .size = 16,
        .more_flag = true
    };

    char input_payload[] = {
        "\x82"           /* array(2) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x65/1111"      /* text(5) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/1" /* text(10) */
    };

    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = 16;
    msg.coap_binding_data.udp.message_id++;

    payload_len = 16;
    PROCESS_REQUEST_BLOCK();
    char expected1[] = "\x61"             // ACK, tkl 1
                       "\x5F\x11\x12\x01" // continue, msg_id token
                       "\xd1\x0e\x08";    // block1 0 more
    verify_payload(expected1, sizeof(expected1) - 1, &msg);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.number = 1;
    msg.block.more_flag = false;
    msg.payload = (uint8_t *) &input_payload[16];
    msg.payload_size = sizeof(input_payload) - 1 - 16;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected2[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x13\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xD1\x02\x10"     // block1 1
                       "\xFF"
                       "\x81\xA2"
                       "\x00\x6a/222/1/2/1"
                       "\x02\x00";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_FINISHED);
}

ANJ_UNIT_TEST(dm_integration,
              read_composite_block2_first_read_exactly_fills_buf) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();

    char input_payload[] = {
        "\x81"         /* array(1) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x68/111/2/2" /* text(8) */
    };

    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    msg.coap_binding_data.udp.message_id++;

    payload_len = 16;
    PROCESS_REQUEST_BLOCK();
    char expected1[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x12\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x08"         // block2 0 more
                       "\xFF"
                       "\x82\xA2"
                       "\x00\x6A/111/2/2/1"
                       "\x02\x06";

    verify_payload(expected1, sizeof(expected1) - 1, &msg);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_2;
    msg.block.number = 1;
    msg.block.more_flag = false;
    msg.payload_size = 0;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected2[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x13\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x10"         // block2 1
                       "\xFF"
                       "\xA2"
                       "\x00\x6A/111/2/2/2"
                       "\x02\x07";

    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_FINISHED);
}

ANJ_UNIT_TEST(dm_integration, read_composite_block2) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();

    char input_payload[] = {
        "\x81"       /* array(1) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/111/2" /* text(6) */
    };

    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    msg.coap_binding_data.udp.message_id++;

    payload_len = 32;
    PROCESS_REQUEST_BLOCK();
    char expected1[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x12\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x09"         // block2 0 more
                       "\xFF"
                       "\x84\xA2"
                       "\x00\x68/111/2/0"
                       "\x02\x03"
                       "\xA2"
                       "\x00\x6A/111/2/2/1"
                       "\x02\x06"
                       "\xA2"
                       "\x00\x6A";
    verify_payload(expected1, sizeof(expected1) - 1, &msg);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_2;
    msg.block.number = 1;
    msg.block.more_flag = false;
    msg.payload_size = 0;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected2[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x13\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x11"         // block2 1
                       "\xFF"
                       "/111/2/2/2"
                       "\x02\x07"
                       "\xA2"
                       "\x00\x68/111/2/4"
                       "\x03\x60";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_FINISHED);
}

ANJ_UNIT_TEST(dm_integration, read_composite_block_both_way) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    msg.block = (_anj_block_t) {
        .block_type = ANJ_OPTION_BLOCK_1,
        .number = 0,
        .size = 16,
        .more_flag = true
    };

    char input_payload[] = {
        "\x82"           /* array(2) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x64/111"       /* text(4) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/1" /* text(10) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = 16;
    msg.coap_binding_data.udp.message_id++;

    payload_len = 16;
    PROCESS_REQUEST_BLOCK();
    char expected1[] = "\x61"             // ACK, tkl 1
                       "\x5F\x11\x12\x01" // continue, msg_id token
                       "\xd1\x0e\x08";    // block1 0 more
    verify_payload(expected1, sizeof(expected1) - 1, &msg);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.number = 1;
    msg.block.more_flag = false;
    msg.payload = (uint8_t *) &input_payload[16];
    msg.payload_size = sizeof(input_payload) - 1 - 16;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected2[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x13\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x08"         // block2 0 more
                       "\x41\x10"         // block1 1
                       "\xFF"
                       "\x86\xA2"
                       "\x00\x68/111/1/0"
                       "\x02\x01"
                       "\xA2"
                       "\x00";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_WAITING_MSG);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_2;
    msg.block.number = 1;
    msg.block.more_flag = false;
    msg.payload_size = 0;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected3[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x14\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x18"         // block2 1 more
                       "\xFF"
                       "\x68/111/2/0"
                       "\x02\x03"
                       "\xA2"
                       "\x00\x6a/1";
    verify_payload(expected3, sizeof(expected3) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_WAITING_MSG);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_2;
    msg.block.number = 2;
    msg.block.more_flag = false;
    msg.payload_size = 0;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected4[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x15\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x28"         // block2 2 more
                       "\xFF"
                       "11/2/2/1"
                       "\x02\x06"
                       "\xA2"
                       "\x00\x6a/11";

    verify_payload(expected4, sizeof(expected4) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_WAITING_MSG);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_2;
    msg.block.number = 3;
    msg.block.more_flag = false;
    msg.payload_size = 0;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected5[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x16\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x38"         // block2 3 more
                       "\xFF"
                       "1/2/2/2"
                       "\x02\x07"
                       "\xA2"
                       "\x00\x68/111";

    verify_payload(expected5, sizeof(expected5) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_WAITING_MSG);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_2;
    msg.block.number = 4;
    msg.block.more_flag = false;
    msg.payload_size = 0;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected6[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x17\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x48"         // block2 4 more
                       "\xFF"
                       "/2/4"
                       "\x03\x60"
                       "\xA2"
                       "\x00\x6a/222/1/";

    verify_payload(expected6, sizeof(expected6) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_WAITING_MSG);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_2;
    msg.block.number = 5;
    msg.block.more_flag = false;
    msg.payload_size = 0;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected7[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x18\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senmlcbor
                       "\xB1\x50"         // block2 5
                       "\xFF"
                       "2/1"
                       "\x02\x00";

    verify_payload(expected7, sizeof(expected7) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_FINISHED);
}

ANJ_UNIT_TEST(dm_integration, read_composite_block_with_termination) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x82"           /* array(2) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x68/111/1/0"   /* text(8) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/1" /* text(10) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    payload_len = 16;
    PROCESS_REQUEST_BLOCK();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.payload_size = 0;
    msg.block.number++;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    _anj_exchange_terminate(&exchange_ctx);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.op_in_progress, false);
}

ANJ_UNIT_TEST(dm_integration, read_composite_root) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x81"  /* array(1) */
        "\xA1"  /* map(1) */
        "\x00"  /* unsigned(0) => SenML Name */
        "\x61/" /* text(1) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF"
                      "\x86\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01"
                      "\xA2"
                      "\x00\x68/111/2/0"
                      "\x02\x03"
                      "\xA2"
                      "\x00\x6A/111/2/2/1"
                      "\x02\x06"
                      "\xA2"
                      "\x00\x6A/111/2/2/2"
                      "\x02\x07"
                      "\xA2"
                      "\x00\x68/111/2/4"
                      "\x03\x60"
                      "\xA2"
                      "\x00\x6A/222/1/2/1"
                      "\x02\x00";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_skip_security_and_oscore) {
    SET_UP();
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_21));

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x87"        /* array(7) */
        "\xA1"        /* map(1) */
        "\x00"        /* unsigned(0) => SenML Name */
        "\x64/0/0"    /* text(4) */
        "\xA1"        /* map(1) */
        "\x00"        /* unsigned(0) => SenML Name */
        "\x66/0/0/0"  /* text(6) */
        "\xA1"        /* map(1) */
        "\x00"        /* unsigned(0) => SenML Name */
        "\x62/0"      /* text(2) */
        "\xA1"        /* map(1) */
        "\x00"        /* unsigned(0) => SenML Name */
        "\x61/"       /* text(1) */
        "\xA1"        /* map(1) */
        "\x00"        /* unsigned(0) => SenML Name */
        "\x63/21"     /* text(3) */
        "\xA1"        /* map(1) */
        "\x00"        /* unsigned(0) => SenML Name */
        "\x65/21/0"   /* text(5) */
        "\xA1"        /* map(1) */
        "\x00"        /* unsigned(0) => SenML Name */
        "\x67/21/0/0" /* text(7) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF"
                      "\x86\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01"
                      "\xA2"
                      "\x00\x68/111/2/0"
                      "\x02\x03"
                      "\xA2"
                      "\x00\x6A/111/2/2/1"
                      "\x02\x06"
                      "\xA2"
                      "\x00\x6A/111/2/2/2"
                      "\x02\x07"
                      "\xA2"
                      "\x00\x68/111/2/4"
                      "\x03\x60"
                      "\xA2"
                      "\x00\x6A/222/1/2/1"
                      "\x02\x00";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_root_block2) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x81"  /* array(1) */
        "\xA1"  /* map(1) */
        "\x00"  /* unsigned(0) => SenML Name */
        "\x61/" /* text(1) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;

    payload_len = 64;
    PROCESS_REQUEST_BLOCK();
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xB1\x0A"         // block2 0 more
                      "\xFF"
                      "\x86\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01"
                      "\xA2"
                      "\x00\x68/111/2/0"
                      "\x02\x03"
                      "\xA2"
                      "\x00\x6A/111/2/2/1"
                      "\x02\x06"
                      "\xA2"
                      "\x00\x6A/111/2/2/2"
                      "\x02\x07"
                      "\xA2"
                      "\x00\x68/111";
    verify_payload(expected, sizeof(expected) - 1, &msg);

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_2;
    msg.block.number = 1;
    msg.block.more_flag = false;
    msg.payload_size = 0;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected2[] = "\x61"             // ACK, tkl 1
                       "\x45\x11\x12\x01" // content, msg_id token
                       "\xC1\x70"         // content_format: senml_cbor
                       "\xB1\x12"         // block2 1
                       "\xFF"
                       "/2/4"
                       "\x03\x60"
                       "\xA2"
                       "\x00\x6A/222/1/2/1"
                       "\x02\x00";
    verify_payload(expected2, sizeof(expected2) - 1, &msg);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_FINISHED);
}

ANJ_UNIT_TEST(dm_integration, read_composite_root_with_other_paths) {
    SET_UP();
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_4));

    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x83"           /* array(1) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/111/2/2/1" /* text(1) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x61/"          /* text(1) */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x64/444"       /* text(1) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF"
                      "\x89\xA2"
                      "\x00\x6A/111/2/2/1"
                      "\x02\x06"
                      "\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01"
                      "\xA2"
                      "\x00\x68/111/2/0"
                      "\x02\x03"
                      "\xA2"
                      "\x00\x6A/111/2/2/1"
                      "\x02\x06"
                      "\xA2"
                      "\x00\x6A/111/2/2/2"
                      "\x02\x07"
                      "\xA2"
                      "\x00\x68/111/2/4"
                      "\x03\x60"
                      "\xA2"
                      "\x00\x6A/222/1/2/1"
                      "\x02\x00"
                      "\xA2"
                      "\x00\x68/444/0/0"
                      "\x02\x03"
                      "\xA2"
                      "\x00\x68/444/0/0"
                      "\x02\x03";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_duplicate_path) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();

    char input_payload[] = {
        "\x82"         /* array(2) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x68/111/1/0" /* text(8) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x68/111/1/0" /* text(8) */
    };

    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senmlcbor
                      "\xFF"
                      "\x82"
                      "\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01"
                      "\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01";

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_duplicate_path_lwm2m) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();

    char input_payload[] = {
        "\x82"         /* array(2) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x68/111/1/0" /* text(8) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x68/111/1/0" /* text(8) */
    };

    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC2\x2D\x18"     // content_format: lwm2m_cbor
                      "\xFF"
                      "\xBF"
                      "\x18\x6F"
                      "\xBF"
                      "\x01"
                      "\xBF"
                      "\x00"
                      "\x01"
                      "\x00"
                      "\x01"
                      "\xFF\xFF\xFF";

    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_root_empty) {
    SET_UP();
    obj_1.max_inst_count = 0;
    obj_2.max_inst_count = 0;
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x81"  /* array(1) */
        "\xA1"  /* map(1) */
        "\x00"  /* unsigned(0) => SenML Name */
        "\x61/" /* text(1) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF\x80";
    obj_1.max_inst_count = 2;
    obj_2.max_inst_count = 2;
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_double_root) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x82"  /* array(2) */
        "\xA1"  /* map(1) */
        "\x00"  /* unsigned(0) => SenML Name */
        "\x61/" /* text(1) */
        "\xA1"  /* map(1) */
        "\x00"  /* unsigned(0) => SenML Name */
        "\x61/" /* text(1) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF"
                      "\x8C\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01"
                      "\xA2"
                      "\x00\x68/111/2/0"
                      "\x02\x03"
                      "\xA2"
                      "\x00\x6A/111/2/2/1"
                      "\x02\x06"
                      "\xA2"
                      "\x00\x6A/111/2/2/2"
                      "\x02\x07"
                      "\xA2"
                      "\x00\x68/111/2/4"
                      "\x03\x60"
                      "\xA2"
                      "\x00\x6A/222/1/2/1"
                      "\x02\x00"
                      "\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01"
                      "\xA2"
                      "\x00\x68/111/2/0"
                      "\x02\x03"
                      "\xA2"
                      "\x00\x6A/111/2/2/1"
                      "\x02\x06"
                      "\xA2"
                      "\x00\x6A/111/2/2/2"
                      "\x02\x07"
                      "\xA2"
                      "\x00\x68/111/2/4"
                      "\x03\x60"
                      "\xA2"
                      "\x00\x6A/222/1/2/1"
                      "\x02\x00";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_empty) {
    SET_UP();
    obj_1_insts[0].res_count = 0;
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x81"       /* array(1) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/111/1" /* text(6) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF\x80";
    verify_payload(expected, sizeof(expected) - 1, &msg);
    obj_1_insts[0].res_count = 2;
}

ANJ_UNIT_TEST(dm_integration, read_composite_write_only) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x81"         /* array(1) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x68/111/1/1" /* text(8) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF\x80";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_unexsiting) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x81"       /* array(1) */
        "\xA1"       /* map(1) */
        "\x00"       /* unsigned(0) => SenML Name */
        "\x66/6/6/6" /* text(6) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF\x80";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_one_path_exists) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x82"         /* array(2) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x66/6/6/6"   /* text(6) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x68/111/1/0" /* text(8) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF"
                      "\x81"
                      "\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, read_composite_unauthorized) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ_COMP;
    msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x82"         /* array(2) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x66/0/0/0"   /* text(6) */
        "\xA1"         /* map(1) */
        "\x00"         /* unsigned(0) => SenML Name */
        "\x68/111/1/0" /* text(8) */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x70"         // content_format: senml_cbor
                      "\xFF"
                      "\x81"
                      "\xA2"
                      "\x00\x68/111/1/0"
                      "\x02\x01";
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, write_composite) {
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    // {[111, 1, 1]: 123, [111, 2, 2, 2]: 321, [222, 1, 2, 1]: 456}
    char input_payload[] = {
        "\xA3"         // map(3)
        "\x83"         // array(3)
        "\x18\x6F"     // unsigned(111)
        "\x01"         // unsigned(1)
        "\x01"         // unsigned(1)
        "\x18\x7B"     // unsigned(123)
        "\x84"         // array(4)
        "\x18\x6F"     // unsigned(111)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\x41" // unsigned(321)
        "\x84"         // array(4)
        "\x18\xDE"     // unsigned(222)
        "\x01"         // unsigned(1)
        "\x02"         // unsigned(2)
        "\x01"         // unsigned(1)
        "\x19\x01\xC8" // unsigned(456)
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    ASSERT_EQ(write_value, 123);
    ASSERT_EQ(write_value2, 321);
    ASSERT_EQ(write_value3, 456);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1], 0);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2], 0);
}

ANJ_UNIT_TEST(dm_integration, write_composite_unauthorized) {
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    // {[111, 1, 1]: 123, [111, 2, 2, 2]: 321, [222, 1, 2, 1]: 456, [0, 0, 0]:
    // "coap://localhost:9000"}
    char input_payload[] = {
        "\xA4"         // map(4)
        "\x83"         // array(3)
        "\x18\x6F"     // unsigned(111)
        "\x01"         // unsigned(1)
        "\x01"         // unsigned(1)
        "\x18\x7B"     // unsigned(123)
        "\x84"         // array(4)
        "\x18\x6F"     // unsigned(111)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\x41" // unsigned(321)
        "\x84"         // array(4)
        "\x18\xDE"     // unsigned(222)
        "\x01"         // unsigned(1)
        "\x02"         // unsigned(2)
        "\x01"         // unsigned(1)
        "\x19\x01\xC8" // unsigned(456)
        "\x83"         // array(3)
        "\x15"         // unsigned(0)
        "\x00"         // unsigned(0)
        "\x00"         // unsigned(0)
        "\x75"         // text(21)
        "\x63\x6F\x61\x70\x3A\x2F\x2F\x6C\x6F\x63\x61\x6C\x68\x6F\x73\x74\x3A"
        "\x39\x30\x30\x30" // "coap://localhost:9000"
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x81\x11\x11\x01"; // unauthorized, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    // it is user resposibility to restore previous values if transaction_end
    // indicates that an error has occurred
    ASSERT_EQ(write_value, 123);
    ASSERT_EQ(write_value2, 321);
    ASSERT_EQ(write_value3, 456);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1],
              ANJ_DM_ERR_UNAUTHORIZED);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2],
              ANJ_DM_ERR_UNAUTHORIZED);
    // transaction_end will not be called for object 0 because there will be
    // error before calling transaction_begin
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_0],
              DEFAULT_TRANSACTION_END_RESULT);
}

ANJ_UNIT_TEST(dm_integration, write_composite_unauthorized_in_the_middle) {
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    // {[111, 1, 1]: 123, [111, 2, 2, 2]: 321, [0, 0, 0]:
    // "coap://localhost:9000", [222, 1, 2, 1]: 456}
    char input_payload[] = {
        "\xA4"         // map(4)
        "\x83"         // array(3)
        "\x18\x6F"     // unsigned(111)
        "\x01"         // unsigned(1)
        "\x01"         // unsigned(1)
        "\x18\x7B"     // unsigned(123)
        "\x84"         // array(4)
        "\x18\x6F"     // unsigned(111)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\x41" // unsigned(321)
        "\x83"         // array(3)
        "\x00"         // unsigned(0)
        "\x00"         // unsigned(0)
        "\x00"         // unsigned(0)
        "\x75"         // text(21)
        "\x63\x6F\x61\x70\x3A\x2F\x2F\x6C\x6F\x63\x61\x6C\x68\x6F\x73\x74\x3A"
        "\x39\x30\x30\x30" // "coap://localhost:9000"
        "\x84"             // array(4)
        "\x18\xDE"         // unsigned(222)
        "\x01"             // unsigned(1)
        "\x02"             // unsigned(2)
        "\x01"             // unsigned(1)
        "\x19\x01\xC8"     // unsigned(456)
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x81\x11\x11\x01"; // unauthorized, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    // it is user resposibility to restore previous values if transaction_end
    // indicates that an error has occurred
    ASSERT_EQ(write_value, 123);
    ASSERT_EQ(write_value2, 321);
    ASSERT_EQ(write_value3, 0);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1],
              ANJ_DM_ERR_UNAUTHORIZED);
    // transaction_end will not be called for objects 0 and 2 because there will
    // be error before calling transaction_begin
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2],
              DEFAULT_TRANSACTION_END_RESULT);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_0],
              DEFAULT_TRANSACTION_END_RESULT);
}

ANJ_UNIT_TEST(dm_integration, write_composite_nonexistent_path) {
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    // {[111, 1, 1]: 123, [111, 2, 2, 2]: 321, [888, 1, 2]: 666, [222, 1, 2, 1]:
    // 456}
    char input_payload[] = {
        "\xA4"         // map(4)
        "\x83"         // array(3)
        "\x18\x6F"     // unsigned(111)
        "\x01"         // unsigned(1)
        "\x01"         // unsigned(1)
        "\x18\x7B"     // unsigned(123)
        "\x84"         // array(4)
        "\x18\x6F"     // unsigned(111)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\x41" // unsigned(321)
        "\x83"         // array(3)
        "\x19\x03\x78" // unsigned(888)
        "\x01"         // unsigned(1)
        "\x02"         // unsigned(2)
        "\x19\x02\x9A" // unsigned(666)
        "\x84"         // array(4)
        "\x18\xDE"     // unsigned(222)
        "\x01"         // unsigned(1)
        "\x02"         // unsigned(2)
        "\x01"         // unsigned(1)
        "\x19\x01\xC8" // unsigned(456)
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x84\x11\x11\x01"; // not found, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    // it is user resposibility to restore previous values if transaction_end
    // indicates that an error has occurred
    ASSERT_EQ(write_value, 123);
    ASSERT_EQ(write_value2, 321);
    // an error will occur before entering the value for /222/1/2/1
    ASSERT_EQ(write_value3, 0);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1],
              ANJ_DM_ERR_NOT_FOUND);
    // transaction_end will not be called for object 2 because there will be
    // error before calling transaction_begin
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2],
              DEFAULT_TRANSACTION_END_RESULT);
}

ANJ_UNIT_TEST(dm_integration,
              write_composite_nonexistent_resource_instance_create) {
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    // {[111, 1, 1]: 123, [222, 1, 2, 2]: 456, [111, 2, 2, 2]: 321}
    char input_payload[] = {
        "\xA3"         // map(3)
        "\x83"         // array(3)
        "\x18\x6F"     // unsigned(111)
        "\x01"         // unsigned(1)
        "\x01"         // unsigned(1)
        "\x18\x7B"     // unsigned(123)
        "\x84"         // array(4)
        "\x18\xDE"     // unsigned(222)
        "\x01"         // unsigned(1)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\xC8" // unsigned(456)
        "\x84"         // array(4)
        "\x18\x6F"     // unsigned(111)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\x41" // unsigned(321)
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    ASSERT_EQ(write_value, 123);
    ASSERT_EQ(write_value2, 321);
    ASSERT_EQ(write_value3, 456);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1], 0);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2], 0);

    // /222/1/2/2
    ASSERT_EQ(obj_2_insts[0].resources[0].insts[1], 2);

    // restore initial state
    obj_2_res_insts[1] = ANJ_ID_INVALID;
}

ANJ_UNIT_TEST(dm_integration,
              write_composite_nonexistent_resource_instance_not_enough_space) {
    obj_2_res.max_inst_count = 1;

    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    // {[111, 1, 1]: 123, [222, 1, 2, 2]: 456, [111, 2, 2, 2]: 321}
    char input_payload[] = {
        "\xA3"         // map(3)
        "\x83"         // array(3)
        "\x18\x6F"     // unsigned(111)
        "\x01"         // unsigned(1)
        "\x01"         // unsigned(1)
        "\x18\x7B"     // unsigned(123)
        "\x84"         // array(4)
        "\x18\xDE"     // unsigned(222)
        "\x01"         // unsigned(1)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\xC8" // unsigned(456)
        "\x84"         // array(4)
        "\x18\x6F"     // unsigned(111)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\x41" // unsigned(321)
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\xA0\x11\x11\x01"; // internal error, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    // it is user resposibility to restore previous values if transaction_end
    // indicates that an error has occurred
    ASSERT_EQ(write_value, 123);
    // an error will occur before entering the value for /222/1/2/2 and
    // /111/2/2/2
    ASSERT_EQ(write_value2, 0);
    ASSERT_EQ(write_value3, 0);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1],
              _ANJ_DM_ERR_MEMORY);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2],
              _ANJ_DM_ERR_MEMORY);

    obj_2_res.max_inst_count = 2;
}

ANJ_UNIT_TEST(dm_integration,
              write_composite_delete_resource_instances_senml_etch_cbor) {
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x83"           /* array(3) */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/1" /* text(6) existing resource instance*/
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/7" /* text(6) nonexistent resource instance */
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
        /* A missing value should also cause resource instance deletion */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/111/2/2/2" /* text(6) existing resource instance */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1], 0);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2], 0);

    // /222/1/2/1
    ASSERT_EQ(obj_2_insts[0].resources[0].insts[0], ANJ_ID_INVALID);
    // /111/2/2/2
    ASSERT_EQ(obj_1_insts[1].resources[2].insts[1], ANJ_ID_INVALID);
    // /111/2/2/1 still exists
    ASSERT_EQ(obj_1_insts[1].resources[2].insts[0], 1);

    // restore initial state
    obj_2_res_insts[0] = 1;
    res_insts[0] = 1;
    res_insts[1] = 2;
}

ANJ_UNIT_TEST(dm_integration,
              write_composite_delete_resource_instances_lwm2m_cbor) {
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\xA3"     // map(3)
        "\x84"     // array(4)
        "\x18\xDE" // unsigned(222)
        "\x01"     // unsigned(1)
        "\x02"     // unsigned(2)
        "\x01"     // unsigned(1)
        "\xF6"     // primitive(22)
        "\x84"     // array(4)
        "\x18\xDE" // unsigned(222)
        "\x01"     // unsigned(1)
        "\x02"     // unsigned(2)
        "\x07"     // unsigned(7)
        "\xF6"     // primitive(22)
        "\x84"     // array(4)
        "\x18\x6F" // unsigned(111)
        "\x02"     // unsigned(2)
        "\x02"     // unsigned(2)
        "\x02"     // unsigned(2)
        "\xF6"     // primitive(22)
    };

    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1], 0);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2], 0);

    // /222/1/2/1
    ASSERT_EQ(obj_2_insts[0].resources[0].insts[0], ANJ_ID_INVALID);
    // /111/2/2/2
    ASSERT_EQ(obj_1_insts[1].resources[2].insts[1], ANJ_ID_INVALID);
    // /111/2/2/1 still exists
    ASSERT_EQ(obj_1_insts[1].resources[2].insts[0], 1);

    // restore initial state
    obj_2_res_insts[0] = 1;
    res_insts[0] = 1;
    res_insts[1] = 2;
}

ANJ_UNIT_TEST(dm_integration, write_composite_try_delete_resource) {
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x83"           /* array(3) */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x68/111/2/1"   /* text(6) existing resource */
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/7" /* text(6) nonexistent resource instance */
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
        /* A missing value should also cause resource instance deletion */
        "\xA1"           /* map(1) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/111/2/2/2" /* text(6) existing resource instance */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x80\x11\x11\x01"; // bad request, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    // transaction_end will not be called for object 1 because there will be
    // error before calling transaction_begin
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1],
              DEFAULT_TRANSACTION_END_RESULT);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2],
              DEFAULT_TRANSACTION_END_RESULT);
}

ANJ_UNIT_TEST(dm_integration,
              write_composite_try_delete_resource_that_doesnt_exist) {
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x83"           /* array(3) */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/1" /* text(6) existing resource instance*/
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x68/222/1/9"   /* text(6) nonexistent resource */
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/111/2/2/2" /* text(6) existing resource instance */
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x80\x11\x11\x01"; // bad request, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    // transaction_end will not be called for object 1 because there will be
    // error before calling transaction_begin
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1],
              DEFAULT_TRANSACTION_END_RESULT);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2],
              ANJ_DM_ERR_BAD_REQUEST);

    // restore initial state
    obj_2_res_insts[0] = 1;
}

ANJ_UNIT_TEST(dm_integration,
              write_composite_delete_resource_instances_not_writable) {
    obj_2_res.operation = ANJ_DM_RES_RM;

    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_SENML_ETCH_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    char input_payload[] = {
        "\x83"           /* array(3) */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/1" /* text(6) existing resource instance*/
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/222/1/2/7" /* text(6) nonexistent resource instance */
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
        "\xA2"           /* map(2) */
        "\x00"           /* unsigned(0) => SenML Name */
        "\x6A/111/2/2/2" /* text(6) existing resource instance */
        "\x02"           /* unsigned(2) => SenML Value */
        "\xF6"           /* null */
    };
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = sizeof(input_payload) - 1;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x85\x11\x11\x01"; // not allowed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    // transaction_end will not be called for object 1 because there will be
    // error before calling transaction_begin
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1],
              DEFAULT_TRANSACTION_END_RESULT);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2],
              ANJ_DM_ERR_METHOD_NOT_ALLOWED);

    // restore initial state
    obj_2_res_insts[0] = 1;
    obj_2_res.operation = ANJ_DM_RES_RWM;
}

ANJ_UNIT_TEST(dm_integration, write_composite_block) {
    const int block_size = 16;
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    // {[111, 1, 1]: 123, [111, 2, 2, 2]: 321, [222, 1, 2, 1]: 456}
    char input_payload[] = {
        "\xA3"         // map(3)
        "\x83"         // array(3)
        "\x18\x6F"     // unsigned(111)
        "\x01"         // unsigned(1)
        "\x01"         // unsigned(1)
        "\x18\x7B"     // unsigned(123)
        "\x84"         // array(4)
        "\x18\x6F"     // unsigned(111)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\x41" // unsigned(321)
        "\x84"         // array(4)
        "\x18\xDE"     // unsigned(222)
        "\x01"         // unsigned(1)
        "\x02"         // unsigned(2)
        "\x01"         // unsigned(1)
        "\x19\x01\xC8" // unsigned(456)
    };
    msg.block.block_type = ANJ_OPTION_BLOCK_1;
    msg.block.number = 0;
    msg.block.size = block_size;
    msg.block.more_flag = true;
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = block_size;
    PROCESS_REQUEST_BLOCK();
    char expected1[] = "\x61"             // ACK, tkl 1
                       "\x5F\x11\x11\x01" // continue, msg_id token
                       "\xd1\x0e\x08";    // block1 0 more
    verify_payload(expected1, sizeof(expected1) - 1, &msg);

    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_1;
    msg.block.number = 1;
    msg.block.size = block_size;
    msg.block.more_flag = false;
    msg.payload = (uint8_t *) &input_payload[block_size];
    msg.payload_size = sizeof(input_payload) - block_size - 1;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected2[] = "\x61"             // ACK, tkl 1
                       "\x44\x11\x12\x01" // changed, msg_id token
                       "\xd1\x0e\x10";    // block1 1
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
    ASSERT_EQ(write_value, 123);
    ASSERT_EQ(write_value2, 321);
    ASSERT_EQ(write_value3, 456);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1], 0);
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2], 0);
}

ANJ_UNIT_TEST(dm_integration, write_composite_block_nonexistent_path) {
    const int block_size = 16;
    SET_UP();
    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.uri = ANJ_MAKE_ROOT_PATH();
    // {[111, 1, 1]: 123, [111, 2, 2, 2]: 321, [888, 1, 2]: 666, [222, 1, 2, 1]:
    // 456}
    char input_payload[] = {
        "\xA4"         // map(4)
        "\x83"         // array(3)
        "\x18\x6F"     // unsigned(111)
        "\x01"         // unsigned(1)
        "\x01"         // unsigned(1)
        "\x18\x7B"     // unsigned(123)
        "\x84"         // array(4)
        "\x18\x6F"     // unsigned(111)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x02"         // unsigned(2)
        "\x19\x01\x41" // unsigned(321)
        "\x83"         // array(3)
        "\x19\x03\x78" // unsigned(888)
        "\x01"         // unsigned(1)
        "\x02"         // unsigned(2)
        "\x19\x02\x9A" // unsigned(666)
        "\x84"         // array(4)
        "\x18\xDE"     // unsigned(222)
        "\x01"         // unsigned(1)
        "\x02"         // unsigned(2)
        "\x01"         // unsigned(1)
        "\x19\x01\xC8" // unsigned(456)
    };
    msg.block.block_type = ANJ_OPTION_BLOCK_1;
    msg.block.number = 0;
    msg.block.size = block_size;
    msg.block.more_flag = true;
    msg.payload = (uint8_t *) input_payload;
    msg.payload_size = block_size;
    PROCESS_REQUEST_BLOCK();
    char expected1[] = "\x61"             // ACK, tkl 1
                       "\x5F\x11\x11\x01" // continue, msg_id token
                       "\xd1\x0e\x08";    // block1 0 more
    verify_payload(expected1, sizeof(expected1) - 1, &msg);

    msg.operation = ANJ_OP_DM_WRITE_COMP;
    msg.block.block_type = ANJ_OPTION_BLOCK_1;
    msg.block.number = 1;
    msg.block.size = block_size;
    msg.block.more_flag = false;
    msg.payload = (uint8_t *) &input_payload[block_size];
    msg.payload_size = sizeof(input_payload) - block_size - 1;
    msg.coap_binding_data.udp.message_id++;

    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);

    char expected2[] = "\x61"              // ACK, tkl 1
                       "\x84\x11\x12\x01"; // not found, msg_id token

    verify_payload(expected2, sizeof(expected2) - 1, &msg);
    // it is user resposibility to restore previous values if transaction_end
    // indicates that an error has occurred
    ASSERT_EQ(write_value, 123);
    ASSERT_EQ(write_value2, 321);
    // an error will occur before entering the value for /222/1/2/1
    ASSERT_EQ(write_value3, 0);

    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_1],
              ANJ_DM_ERR_NOT_FOUND);
    // transaction_end will not be called for object 2 because there will be
    // error before calling transaction_begin
    ASSERT_EQ(transaction_end_results[TRANSACTION_END_RESULT_OBJ_2],
              DEFAULT_TRANSACTION_END_RESULT);
}
#endif // ANJ_WITH_COMPOSITE_OPERATIONS

ANJ_UNIT_TEST(dm_integration, execute_operation) {
    SET_UP();
    res_execute_counter = 0;
    msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    msg.operation = ANJ_OP_DM_EXECUTE;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 2, 5);
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(res_execute_counter, 1);
    res_execute_counter = 0;
}

ANJ_UNIT_TEST(dm_integration, execute_operation_with_payload) {
    SET_UP();
    res_execute_counter = 0;
    msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    msg.operation = ANJ_OP_DM_EXECUTE;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 2, 5);
    msg.payload = (uint8_t *) "test";
    msg.payload_size = 4;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(res_execute_counter, 1);
    ANJ_UNIT_ASSERT_EQUAL(res_execute_arg_len, 4);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(res_execute_arg, "test", 4);
    res_execute_counter = 0;
}

ANJ_UNIT_TEST(dm_integration, bootstrap_discover_operation) {
    SET_UP();
    msg.operation = ANJ_OP_DM_DISCOVER;
    msg.accept = _ANJ_COAP_FORMAT_LINK_FORMAT;
    msg.uri = ANJ_MAKE_OBJECT_PATH(222);
    PROCESS_REQUEST(true);

#ifdef ANJ_WITH_LWM2M12
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x28"         // content_format: link format
                      "\xFF"
                      "</>;lwm2m=1.2,</222>,</222/1>";
    verify_payload(expected, sizeof(expected) - 1, &msg);
#else  // ANJ_WITH_LWM2M12
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x45\x11\x11\x01" // content, msg_id token
                      "\xC1\x28"         // content_format: link format
                      "\xFF"
                      "</>;lwm2m=1.1,</222>,</222/1>";
    verify_payload(expected, sizeof(expected) - 1, &msg);
#endif // ANJ_WITH_LWM2M12
}

ANJ_UNIT_TEST(dm_integration, delete_operation) {
    SET_UP();
    obj_2_insts[0].iid = 0;
    obj_2_insts[1].iid = 1;
    msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    msg.operation = ANJ_OP_DM_DELETE;
    msg.uri = ANJ_MAKE_INSTANCE_PATH(222, 0);
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x42\x11\x11\x01"; // deleted, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[1].iid, ANJ_ID_INVALID);
    reset_obj_2_insts();
}

ANJ_UNIT_TEST(dm_integration, write_update_operation) {
    SET_UP();
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_TLV;
    msg.operation = ANJ_OP_DM_WRITE_PARTIAL_UPDATE;
    msg.uri = ANJ_MAKE_INSTANCE_PATH(111, 1);
    msg.payload = (uint8_t *) "\xC1\x01\x2A";
    msg.payload_size = 3;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(write_value, 42);
}

ANJ_UNIT_TEST(dm_integration, write_update_operation_block) {
    SET_UP();
    msg.content_format = _ANJ_COAP_FORMAT_PLAINTEXT;
    msg.operation = ANJ_OP_DM_WRITE_PARTIAL_UPDATE;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 2, 4);
    msg.block = (_anj_block_t) {
        .block_type = ANJ_OPTION_BLOCK_1,
        .number = 0,
        .size = 16,
        .more_flag = true
    };
    msg.payload = (uint8_t *) "\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31"
                              "\x31\x31\x31\x31";
    msg.payload_size = 16;
    PROCESS_REQUEST_BLOCK();
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x5F\x11\x11\x01" // continue, msg_id token
                      "\xd1\x0e\x08";    // block1 0 more
    verify_payload(expected, sizeof(expected) - 1, &msg);

    msg.operation = ANJ_OP_DM_WRITE_PARTIAL_UPDATE;
    msg.payload = (uint8_t *) "\x32\x33\x34\x35\x36\x37\x38\x39";
    msg.payload_size = 8;
    msg.block.number++;
    msg.block.more_flag = false;
    msg.coap_binding_data.udp.message_id++;
    ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                &msg),
                          ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_exchange_process(&exchange_ctx,
                                  ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION, &msg),
            ANJ_EXCHANGE_STATE_FINISHED);

    char expected2[] = "\x61"             // ACK, tkl 1
                       "\x44\x11\x12\x01" // changed, msg_id token
                       "\xd1\x0e\x10";    // block1 1
    verify_payload(expected2, sizeof(expected2) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            res_4_buff,
            "\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31"
            "\x32\x33\x34\x35\x36\x37\x38\x39",
            24);
    ANJ_UNIT_ASSERT_EQUAL(strlen(res_4_buff), 24);
}

ANJ_UNIT_TEST(dm_integration, write_replace_operation) {
    SET_UP();
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_TLV;
    msg.operation = ANJ_OP_DM_WRITE_REPLACE;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 1, 1);
    msg.payload = (uint8_t *) "\xC1\x01\x0A";
    msg.payload_size = 3;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(write_value, 10);
}

ANJ_UNIT_TEST(dm_integration, write_replace_operation_on_resource_instance) {
    SET_UP();
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.operation = ANJ_OP_DM_WRITE_REPLACE;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 2, 2);
    // 111/2/2/3 : 5
    msg.payload = (uint8_t *) "\xA1\x18\x6F\xA1\x02\xA1\x02\xA1\x03\x05";
    msg.payload_size = 10;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(write_value, 5);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], 3);

    res_insts[0] = 1;
    res_insts[1] = 2;
}

ANJ_UNIT_TEST(dm_integration,
              write_replace_operation_on_two_resource_instance) {
    SET_UP();
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.operation = ANJ_OP_DM_WRITE_REPLACE;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 2, 2);
    // {[111, 2, 2]: {3: 5, 5: 10}}
    msg.payload = (uint8_t *) "\xA1\x83\x18\x6F\x02\x02\xA2\x03\x05\x05\x0A";
    msg.payload_size = 11;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(write_value, 5);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], 3);
    ANJ_UNIT_ASSERT_EQUAL(write_value2, 10);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[1], 5);

    res_insts[0] = 1;
    res_insts[1] = 2;
}

ANJ_UNIT_TEST(dm_integration, create_with_write) {
    SET_UP();
    msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    msg.operation = ANJ_OP_DM_CREATE;
    msg.uri = ANJ_MAKE_OBJECT_PATH(222);
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_TLV;
    msg.payload = (uint8_t *) "\x03\x00\xC1\x01\x2B";
    msg.payload_size = 5;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x41\x11\x11\x01"; // created, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(write_value, 43);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[1].iid, 1);
    reset_obj_2_insts();
}

// Coiote can send TLV message with only instance ID provided
ANJ_UNIT_TEST(dm_integration, create_with_empty_write) {
    SET_UP();
    msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    msg.operation = ANJ_OP_DM_CREATE;
    msg.uri = ANJ_MAKE_OBJECT_PATH(222);
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_TLV;
    msg.payload = (uint8_t *) "\x00\x00";
    msg.payload_size = 2;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x41\x11\x11\x01"; // created, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[1].iid, 1);
    reset_obj_2_insts();
}

ANJ_UNIT_TEST(dm_integration, create_with_write_no_iid_specify) {
    SET_UP();
    msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    msg.operation = ANJ_OP_DM_CREATE;
    msg.uri = ANJ_MAKE_OBJECT_PATH(222);
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_TLV;
    msg.payload = (uint8_t *) "\xC1\x01\x2A";
    msg.payload_size = 3;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x41\x11\x11\x01" // created, msg_id token
                      "\x83\x32\x32\x32" // location-path /222
                      "\x01\x30";        // location-path /0
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(write_value, 42);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[1].iid, 1);
    reset_obj_2_insts();
}

ANJ_UNIT_TEST(dm_integration, create_without_payload) {
    SET_UP();
    msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    msg.operation = ANJ_OP_DM_CREATE;
    msg.uri = ANJ_MAKE_OBJECT_PATH(222);
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_TLV;
    msg.payload = NULL;
    msg.payload_size = 0;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"             // ACK, tkl 1
                      "\x41\x11\x11\x01" // created, msg_id token
                      "\x83\x32\x32\x32" // location-path /222
                      "\x01\x30";        // location-path /0
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[0].iid, 0);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[1].iid, 1);
    reset_obj_2_insts();
}

ANJ_UNIT_TEST(dm_integration, format_error) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ;
    msg.uri = ANJ_MAKE_OBJECT_PATH(222);
    msg.content_format = 333;
    msg.accept = _ANJ_COAP_FORMAT_NOT_DEFINED - 1;
    PROCESS_REQUEST_WITH_ERROR(ANJ_COAP_CODE_NOT_ACCEPTABLE);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x86\x11\x11\x01"; // unsupported format, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, not_found_error) {
    SET_UP();
    msg.operation = ANJ_OP_DM_READ;
    msg.uri = ANJ_MAKE_INSTANCE_PATH(222, 2);
    msg.content_format = 333;
    PROCESS_REQUEST_WITH_ERROR(ANJ_COAP_CODE_NOT_FOUND);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x84\x11\x11\x01"; // not found, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
}

ANJ_UNIT_TEST(dm_integration, validation_error) {
    SET_UP();
    validation_error = true;
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_TLV;
    msg.operation = ANJ_OP_DM_WRITE_PARTIAL_UPDATE;
    msg.uri = ANJ_MAKE_INSTANCE_PATH(111, 1);
    msg.payload = (uint8_t *) "\xC1\x01\x2A";
    msg.payload_size = 3;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x80\x11\x11\x01"; // bad reqeust, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    validation_error = false;
}

#ifdef ANJ_WITH_EXTERNAL_DATA
ANJ_UNIT_TEST(dm_integration, read_external_string) {
    // successfully send external string, string split between two messages
    {
        SET_UP();
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));
        external_data_handler_when_error = 0;

        msg.operation = ANJ_OP_DM_READ;
        msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
        msg.uri = ANJ_MAKE_INSTANCE_PATH(333, 1);
        payload_len = 32;

        ptr_for_callback = "012345678901234567890123456789";
        ext_data_size = sizeof("012345678901234567890123456789") - 1;
        opened = false;
        closed = false;

        PROCESS_REQUEST_BLOCK()
        char expected[] = "\x61"             // ACK, tkl 1
                          "\x45\x11\x11\x01" // content, msg_id token
                          "\xC1\x70"         // content_format: senml_cbor
                          "\xB1\x09"         // block2 0 more
                          "\xFF"
                          "\x82\xA3"
                          "\x21\x66/333/1"
                          "\x00\x62/1"
                          "\x03"
                          "\x7f"
                          "\x6E"
                          "01234567890123\x60";
        verify_payload(expected, sizeof(expected) - 1, &msg);
        ANJ_UNIT_ASSERT_FALSE(closed);

        msg.operation = ANJ_OP_DM_READ;
        msg.payload_size = 0;
        msg.block.number++;
        msg.coap_binding_data.udp.message_id++;
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                    ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                    &msg),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected2[] = "\x61"             // ACK, tkl 1
                           "\x45\x11\x12\x01" // content, msg_id token
                           "\xC1\x70"         // content_format: senmlcbor
                           "\xB1\x11"         // block2 1, size 32
                           "\xFF"
                           "\x70"
                           "4567890123456789"
                           "\xFF"
                           "\xA2"
                           "\x00\x62/2"
                           "\x02\x03";
        verify_payload(expected2, sizeof(expected2) - 1, &msg);
        ANJ_UNIT_ASSERT_TRUE(closed);

        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_FINISHED);
    }
    // successfully send external string, whole string in first message
    {
        SET_UP();
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));
        external_data_handler_when_error = 0;

        msg.operation = ANJ_OP_DM_READ;
        msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
        msg.uri = ANJ_MAKE_INSTANCE_PATH(333, 1);
        payload_len = 32;

        ptr_for_callback = "01234567890123";
        ext_data_size = sizeof("01234567890123") - 1;
        opened = false;
        closed = false;

        PROCESS_REQUEST_BLOCK()
        char expected[] = "\x61"             // ACK, tkl 1
                          "\x45\x11\x11\x01" // content, msg_id token
                          "\xC1\x70"         // content_format: senml_cbor
                          "\xB1\x09"         // block2 0 more
                          "\xFF"
                          "\x82\xA3"
                          "\x21\x66/333/1"
                          "\x00\x62/1"
                          "\x03"
                          "\x7f"
                          "\x6E"
                          "01234567890123\xFF";
        verify_payload(expected, sizeof(expected) - 1, &msg);

        ANJ_UNIT_ASSERT_TRUE(closed);

        msg.operation = ANJ_OP_DM_READ;
        msg.payload_size = 0;
        msg.block.number++;
        msg.coap_binding_data.udp.message_id++;
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                    ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                    &msg),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected2[] = "\x61"             // ACK, tkl 1
                           "\x45\x11\x12\x01" // content, msg_id token
                           "\xC1\x70"         // content_format: senmlcbor
                           "\xB1\x11"         // block2 1, size 32
                           "\xFF"
                           "\xA2"
                           "\x00\x62/2"
                           "\x02\x03";
        verify_payload(expected2, sizeof(expected2) - 1, &msg);

        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_FINISHED);
    }
    // try send external string, exchange terminated
    {
        SET_UP();
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));
        external_data_handler_when_error = 0;

        msg.operation = ANJ_OP_DM_READ;
        msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
        msg.uri = ANJ_MAKE_INSTANCE_PATH(333, 1);
        payload_len = 32;

        ptr_for_callback = "012345678901234";
        ext_data_size = sizeof("012345678901234") - 1;
        opened = false;
        closed = false;

        PROCESS_REQUEST_BLOCK()
        char expected[] = "\x61"             // ACK, tkl 1
                          "\x45\x11\x11\x01" // content, msg_id token
                          "\xC1\x70"         // content_format: senml_cbor
                          "\xB1\x09"         // block2 0 more
                          "\xFF"
                          "\x82\xA3"
                          "\x21\x66/333/1"
                          "\x00\x62/1"
                          "\x03"
                          "\x7f"
                          "\x6E"
                          "01234567890123\x60";
        verify_payload(expected, sizeof(expected) - 1, &msg);

        ANJ_UNIT_ASSERT_FALSE(closed);
        _anj_exchange_terminate(&exchange_ctx);
        ANJ_UNIT_ASSERT_TRUE(closed);
    }
    // try send external string, external data handler fails the first time it
    // is called
    {
        SET_UP();
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));
        external_data_handler_when_error = 1;

        msg.operation = ANJ_OP_DM_READ;
        msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
        msg.uri = ANJ_MAKE_INSTANCE_PATH(333, 1);
        payload_len = 32;

        ptr_for_callback = "012345678901234567890123456789";
        ext_data_size = sizeof("012345678901234567890123456789") - 1;
        opened = false;
        closed = false;

        _anj_dm_process_request(&anj, &msg, 1, &response_code, &handlers);
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_new_server_request(
                                      &exchange_ctx, response_code, &msg,
                                      &handlers, payload, payload_len),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected[] =
                "\x61"              // ACK, tkl 1
                "\xa0\x11\x11\x01"; // internal server error, msg_id token
        verify_payload(expected, sizeof(expected) - 1, &msg);
        ANJ_UNIT_ASSERT_TRUE(closed);

        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_FINISHED);
    }
    // try send external string, external data handler fails the second time it
    // is called
    {
        SET_UP();
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));
        external_data_handler_when_error = 2;

        msg.operation = ANJ_OP_DM_READ;
        msg.accept = _ANJ_COAP_FORMAT_SENML_CBOR;
        msg.uri = ANJ_MAKE_INSTANCE_PATH(333, 1);
        payload_len = 32;

        ptr_for_callback = "012345678901234567890123456789";
        ext_data_size = sizeof("012345678901234567890123456789") - 1;
        opened = false;
        closed = false;

        PROCESS_REQUEST_BLOCK()
        char expected[] = "\x61"             // ACK, tkl 1
                          "\x45\x11\x11\x01" // content, msg_id token
                          "\xC1\x70"         // content_format: senml_cbor
                          "\xB1\x09"         // block2 0 more
                          "\xFF"
                          "\x82\xA3"
                          "\x21\x66/333/1"
                          "\x00\x62/1"
                          "\x03"
                          "\x7f"
                          "\x6E"
                          "01234567890123\x60";
        verify_payload(expected, sizeof(expected) - 1, &msg);
        ANJ_UNIT_ASSERT_FALSE(closed);

        msg.operation = ANJ_OP_DM_READ;
        msg.payload_size = 0;
        msg.block.number++;
        msg.coap_binding_data.udp.message_id++;
        ANJ_UNIT_ASSERT_EQUAL(_anj_exchange_process(&exchange_ctx,
                                                    ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                    &msg),
                              ANJ_EXCHANGE_STATE_MSG_TO_SEND);

        char expected2[] =
                "\x61"              // ACK, tkl 1
                "\xa0\x11\x12\x01"; // internal server error, msg_id token
        verify_payload(expected2, sizeof(expected2) - 1, &msg);
        ANJ_UNIT_ASSERT_TRUE(closed);

        ANJ_UNIT_ASSERT_EQUAL(
                _anj_exchange_process(&exchange_ctx,
                                      ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                      &msg),
                ANJ_EXCHANGE_STATE_FINISHED);
    }
}
#endif // ANJ_WITH_EXTERNAL_DATA

#ifdef ANJ_WITH_OBSERVE_COMPOSITE
ANJ_UNIT_TEST(dm_integration, delete_operation_with_observation_removed) {
    SET_UP();
    // delete observation related to the deleted instance
    anj.observe_ctx.observations[0].ssid = 2;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(222, 0);
    anj.observe_ctx.observations[0].observe_active = true;
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_INSTANCE_PATH(111, 0);
    anj.observe_ctx.observations[1].observe_active = true;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(222, 0, 1);
    anj.observe_ctx.observations[2].observe_active = true;
    anj.observe_ctx.observations[3].ssid = 2;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_RESOURCE_PATH(111, 1, 0);
    anj.observe_ctx.observations[3].observe_active = true;
    anj.observe_ctx.observations[4].ssid = 2;
    anj.observe_ctx.observations[4].path = ANJ_MAKE_INSTANCE_PATH(222, 0);
    anj.observe_ctx.observations[4].observe_active = true;
    anj.observe_ctx.observations[4].prev = &anj.observe_ctx.observations[4];

    obj_2_insts[1].iid = 1;
    obj_2_insts[0].iid = 0;

    msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    msg.operation = ANJ_OP_DM_DELETE;
    msg.uri = ANJ_MAKE_INSTANCE_PATH(222, 0);
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x42\x11\x11\x01"; // deleted, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[0].iid, 1);
    ANJ_UNIT_ASSERT_EQUAL(obj_2_insts[1].iid, ANJ_ID_INVALID);

    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].ssid, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].ssid, 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].ssid, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[3].ssid, 2);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[4].ssid, 2);
    reset_obj_2_insts();
}

ANJ_UNIT_TEST(dm_integration, create_with_write_with_observation_set_to_send) {
    SET_UP();
    // prev field is used to set the composite observation which can exist even
    // when instance does not exist
    anj.observe_ctx.observations[0].ssid = 2;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_INSTANCE_PATH(222, 0);
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[1].ssid = 2;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_RESOURCE_PATH(222, 0, 4);
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_INSTANCE_PATH(222, 0);
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[2];
    anj.observe_ctx.observations[3].ssid = 2;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_OBJECT_PATH(222);
    anj.observe_ctx.observations[3].observe_active = true;
    obj_2_insts[1].res_count++;

    msg.content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    msg.operation = ANJ_OP_DM_CREATE;
    msg.uri = ANJ_MAKE_OBJECT_PATH(222);
    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_TLV;
    msg.payload = (uint8_t *) "\x03\x00\xC1\x01\x2B";
    msg.payload_size = 5;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x41\x11\x11\x01"; // created, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(write_value, 43);
    obj_2_insts[1].res_count--;

    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].notification_to_send,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[3].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[3].notification_to_send,
                          true);
}
#endif // ANJ_WITH_OBSERVE_COMPOSITE

ANJ_UNIT_TEST(dm_integration, write_replace_operation_with_observation_update) {
    SET_UP();

    anj.observe_ctx.observations[0].ssid = 2;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_OBJECT_PATH(111);
    anj.observe_ctx.observations[0].observe_active = true;
    anj.observe_ctx.observations[1].ssid = 2;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_INSTANCE_PATH(111, 1);
    anj.observe_ctx.observations[1].observe_active = true;
    anj.observe_ctx.observations[2].ssid = 2;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(111, 1, 1);
    anj.observe_ctx.observations[2].observe_active = true;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_INSTANCE_PATH(111, 1);
    anj.observe_ctx.observations[3].observe_active = true;
    anj.observe_ctx.observations[4].ssid = 1;
    anj.observe_ctx.observations[4].path = ANJ_MAKE_RESOURCE_PATH(111, 1, 1);
    anj.observe_ctx.observations[4].observe_active = true;

    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_TLV;
    msg.operation = ANJ_OP_DM_WRITE_REPLACE;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 1, 1);
    msg.payload = (uint8_t *) "\xC1\x01\x0A";
    msg.payload_size = 3;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);

    // write operation is coming from the server with ssid = 1, so observations
    // with ssid = 1 should be ignored
    ANJ_UNIT_ASSERT_EQUAL(write_value, 10);

    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].ssid, 2);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].ssid, 2);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].ssid, 2);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[3].ssid, 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[3].notification_to_send,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[4].ssid, 1);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[4].notification_to_send,
                          false);
}

#ifdef ANJ_WITH_OBSERVE_COMPOSITE
ANJ_UNIT_TEST(
        dm_integration,
        write_replace_operation_on_resource_instance_with_observations_update) {
    SET_UP();

    anj.observe_ctx.observations[0].ssid = 2;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_OBJECT_PATH(111);
    anj.observe_ctx.observations[0].observe_active = true;
    anj.observe_ctx.observations[1].ssid = 2;
    anj.observe_ctx.observations[1].path =
            ANJ_MAKE_RESOURCE_INSTANCE_PATH(111, 2, 2, 3);
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[4];
    anj.observe_ctx.observations[2].ssid = 2;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(111, 2, 2);
    anj.observe_ctx.observations[2].observe_active = true;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_INSTANCE_PATH(111, 2);
    anj.observe_ctx.observations[3].observe_active = true;
    anj.observe_ctx.observations[4].ssid = 2;
    anj.observe_ctx.observations[4].path =
            ANJ_MAKE_RESOURCE_INSTANCE_PATH(111, 2, 2, 2);
    anj.observe_ctx.observations[4].prev = &anj.observe_ctx.observations[1];

    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.operation = ANJ_OP_DM_WRITE_REPLACE;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 2, 2);
    // 111/2/2/3 : 5
    msg.payload = (uint8_t *) "\xA1\x18\x6F\xA1\x02\xA1\x02\xA1\x03\x05";
    msg.payload_size = 10;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(write_value, 5);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], 3);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[1], ANJ_ID_INVALID);

    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[3].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[3].notification_to_send,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[4].observe_active,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[4].notification_to_send,
                          false);
    res_insts[0] = 1;
    res_insts[1] = 2;
}

ANJ_UNIT_TEST(
        dm_integration,
        write_update_operation_on_resource_instance_with_observations_update) {
    SET_UP();

    for (int i = 0; i < 5; i++) {
        anj.observe_ctx.observations[i].ssid = 2;
        anj.observe_ctx.observations[i].observe_active = true;
    }
    anj.observe_ctx.observations[0].path = ANJ_MAKE_OBJECT_PATH(111);
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path =
            ANJ_MAKE_RESOURCE_INSTANCE_PATH(111, 2, 2, 1);
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(111, 2, 2);
    anj.observe_ctx.observations[3].path = ANJ_MAKE_INSTANCE_PATH(111, 1);
    anj.observe_ctx.observations[4].path =
            ANJ_MAKE_RESOURCE_INSTANCE_PATH(111, 2, 2, 2);

    msg.content_format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
    msg.operation = ANJ_OP_DM_WRITE_PARTIAL_UPDATE;
    msg.uri = ANJ_MAKE_RESOURCE_PATH(111, 2, 2);
    // 111/2/2/1 : 2
    msg.payload = (uint8_t *) "\xA1\x18\x6F\xA1\x02\xA1\x02\xA1\x01\x02";
    msg.payload_size = 10;
    PROCESS_REQUEST(false);
    char expected[] = "\x61"              // ACK, tkl 1
                      "\x44\x11\x11\x01"; // changed, msg_id token
    verify_payload(expected, sizeof(expected) - 1, &msg);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[0], 1);
    ANJ_UNIT_ASSERT_EQUAL(res_insts[1], 2);
    ANJ_UNIT_ASSERT_EQUAL(write_value, 2);

    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].notification_to_send,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[3].notification_to_send,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[4].notification_to_send,
                          false);
}
#endif // ANJ_WITH_OBSERVE_COMPOSITE

ANJ_UNIT_TEST(dm_integration, delete_object_observation_update) {
    anj_t anj = { 0 };
    _anj_dm_initialize(&anj);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_2));

    anj.observe_ctx.observations[0].ssid = 2;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_OBJECT_PATH(111);
    anj.observe_ctx.observations[0].observe_active = true;
    anj.observe_ctx.observations[1].ssid = 2;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_INSTANCE_PATH(111, 1);
    anj.observe_ctx.observations[1].observe_active = true;
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_RESOURCE_PATH(111, 1, 1);
    anj.observe_ctx.observations[2].observe_active = true;
    anj.observe_ctx.observations[3].ssid = 1;
    anj.observe_ctx.observations[3].path = ANJ_MAKE_INSTANCE_PATH(222, 1);
    anj.observe_ctx.observations[3].observe_active = true;

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_remove_obj(&anj, 111));
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].ssid, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].ssid, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].ssid, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[3].ssid, 1);
}

#ifdef ANJ_WITH_OBSERVE_COMPOSITE
ANJ_UNIT_TEST(dm_integration, add_object_observation_update) {
    anj_t anj = { 0 };
    _anj_dm_initialize(&anj);

    anj.observe_ctx.observations[0].ssid = 1;
    anj.observe_ctx.observations[0].path = ANJ_MAKE_RESOURCE_PATH(111, 2, 2);
    anj.observe_ctx.observations[0].prev = &anj.observe_ctx.observations[1];
    anj.observe_ctx.observations[1].ssid = 1;
    anj.observe_ctx.observations[1].path = ANJ_MAKE_INSTANCE_PATH(111, 2);
    anj.observe_ctx.observations[1].prev = &anj.observe_ctx.observations[0];
    anj.observe_ctx.observations[2].ssid = 1;
    anj.observe_ctx.observations[2].path = ANJ_MAKE_INSTANCE_PATH(222, 2);
    anj.observe_ctx.observations[2].prev = &anj.observe_ctx.observations[0];

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1));

    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].observe_active, true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[0].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[1].notification_to_send,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].observe_active,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(anj.observe_ctx.observations[2].notification_to_send,
                          false);
}
#endif // ANJ_WITH_OBSERVE_COMPOSITE

// TODO: add this test during
// https://gitlab.avsystem.com/iot/embedded/embedded-project-tracker/-/issues/4769
// implementation
// ANJ_UNIT_TEST(write_replace_operation_on_resource_instance_with_observations_update)
// {
// }
