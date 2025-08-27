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

#include "../../../src/anj/coap/coap.h"
#include "../../../src/anj/dm/dm_integration.h"
#include "../../../src/anj/dm/dm_io.h"
#include "../../src/anj/exchange.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_OBSERVE

static int res_execute(anj_t *anj,
                       const anj_dm_obj_t *obj,
                       anj_iid_t iid,
                       anj_rid_t rid,
                       const char *execute_arg,
                       size_t execute_arg_len) {
    (void) obj;
    (void) iid;
    (void) rid;
    (void) execute_arg;
    (void) execute_arg_len;
    return 0;
}

static anj_dm_res_t inst_1_res[] = {
    {
        .rid = 1,
        .operation = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_INT
    }
};

static anj_riid_t res_insts[] = { 1, 2 };
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
        .insts = res_insts
    },
    {
        .rid = 3,
        .operation = ANJ_DM_RES_WM,
        .type = ANJ_DATA_TYPE_INT,
        .max_inst_count = 0,
        .insts = res_insts
    },
    {
        .rid = 4,
        .operation = ANJ_DM_RES_W,
        .type = ANJ_DATA_TYPE_STRING
    },
    {
        .rid = 5,
        .operation = ANJ_DM_RES_E
    }
};

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) obj;
    (void) iid;

    if (rid == 0) {
        out_value->int_value = 3;
    }
    if (rid == 2) {
        out_value->int_value = (riid == 1) ? 6 : 7;
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
    (void) riid;
    (void) value;
    return 0;
}

static anj_dm_handlers_t handlers = {
    .res_execute = res_execute,
    .res_read = res_read,
    .res_write = res_write,
};

static anj_dm_obj_inst_t obj_1_insts[2] = {
    {
        .iid = 1,
        .res_count = 1,
        .resources = inst_1_res
    },
    {
        .iid = 2,
        .res_count = 6,
        .resources = inst_2_res
    }
};

static anj_dm_obj_t obj_1 = {
    .oid = 11,
    .insts = obj_1_insts,
    .max_inst_count = 2,
    .handlers = &handlers,
};

static anj_dm_obj_inst_t obj_2_insts[1] = {
    {
        .iid = 2,
        .res_count = 6,
        .resources = inst_2_res
    }
};

static anj_dm_obj_t obj_2 = {
    .oid = 12,
    .insts = obj_2_insts,
    .max_inst_count = 1,
    .handlers = &handlers,
};

static anj_dm_obj_inst_t obj_3_insts[1] = {
    {
        .iid = 1,
    }
};
static anj_dm_obj_t obj_3 = {
    .oid = 13,
    .insts = obj_3_insts,
    .max_inst_count = 1,
    .handlers = &handlers,
};

#    define SET_UP()                                           \
        uint8_t buff[512];                                     \
        size_t buff_len = sizeof(buff);                        \
        _anj_coap_msg_t msg;                                   \
        memset(&msg, 0, sizeof(msg));                          \
        anj_t anj = { 0 };                                     \
        _anj_dm_initialize(&anj);                              \
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_1)); \
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_2)); \
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&anj, &obj_3));

ANJ_UNIT_TEST(dm_observe_handlers, is_any_resource_readable) {
    SET_UP();
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_OBJECT_PATH(11)),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_OBJECT_PATH(222)),
                          ANJ_COAP_CODE_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_RESOURCE_PATH(11, 2, 5)),
                          ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_RESOURCE_PATH(11, 2, 1)),
                          ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_RESOURCE_PATH(11, 2, 0)),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_INSTANCE_PATH(11, 3)),
                          ANJ_COAP_CODE_NOT_FOUND);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_INSTANCE_PATH(11, 1)),
                          ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_RESOURCE_PATH(11, 2, 2)),
                          0);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_observe_is_any_resource_readable(
                    &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(11, 2, 2, 1)),
            0);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_observe_is_any_resource_readable(
                    &anj, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(11, 2, 2, 11)),
            ANJ_COAP_CODE_NOT_FOUND);

    inst_2_res[3].max_inst_count = 1;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_RESOURCE_PATH(11, 2, 3)),
                          ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
    inst_2_res[3].max_inst_count = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_is_any_resource_readable(
                                  &anj, &ANJ_MAKE_RESOURCE_PATH(11, 2, 3)),
                          ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
}

ANJ_UNIT_TEST(dm_observe_handlers, read_resource) {
    SET_UP();
    anj_res_value_t value;
    anj_data_type_t type;
    bool res_multi;
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_observe_read_resource(&anj, &value, &type, &res_multi,
                                          &ANJ_MAKE_RESOURCE_PATH(11, 2, 0)),
            0);
    ANJ_UNIT_ASSERT_EQUAL(value.int_value, 3);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);

    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_observe_read_resource(&anj, &value, &type, &res_multi,
                                          &ANJ_MAKE_RESOURCE_PATH(11, 2, 1)),
            ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
    res_multi = false;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_observe_read_resource(&anj, &value, &type, &res_multi,
                                          &ANJ_MAKE_RESOURCE_PATH(11, 2, 2)));
    ANJ_UNIT_ASSERT_TRUE(res_multi);

    value.int_value = 0;
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_observe_read_resource(&anj, &value, NULL, &res_multi,
                                          &ANJ_MAKE_RESOURCE_PATH(11, 2, 0)),
            0);
    ANJ_UNIT_ASSERT_EQUAL(value.int_value, 3);

    type = 0;
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_dm_observe_read_resource(&anj, NULL, &type, NULL,
                                          &ANJ_MAKE_RESOURCE_PATH(11, 2, 0)),
            0);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
}

#    define VERIFY_PAYLOAD(Payload, Buff, Len)                     \
        do {                                                       \
            ANJ_UNIT_ASSERT_EQUAL(Len, sizeof(Payload) - 1);       \
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(Payload, Buff, Len); \
        } while (0)

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_single_resource) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    size_t already_process = 0;
    const anj_uri_path_t *path = &ANJ_MAKE_RESOURCE_PATH(11, 2, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      &path,
                                                      1,
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      false));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR);
    VERIFY_PAYLOAD("\xBF\x0B\xBF\x02\xBF\x00"
                   "\x03\xFF\xFF\xFF",
                   buff, out_len);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_single_resource_set_format) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_PLAINTEXT;
    size_t already_process = 0;
    const anj_uri_path_t *path = &ANJ_MAKE_RESOURCE_PATH(11, 2, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      &path,
                                                      1,
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      false));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_PLAINTEXT);
    VERIFY_PAYLOAD("3", buff, out_len);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_several_records) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    size_t already_process = 0;
    const anj_uri_path_t *path = &ANJ_MAKE_INSTANCE_PATH(11, 2);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      &path,
                                                      1,
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      false));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR);
    VERIFY_PAYLOAD("\xBF\x0B\xBF\x02\xBF\x00\x03\x02\xBF\x01\x06\x02\x07\xFF"
                   "\xFF\xFF\xFF",
                   buff, out_len);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_block) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    size_t already_process = 0;
    const anj_uri_path_t *path = &ANJ_MAKE_INSTANCE_PATH(11, 2);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_build_msg(&anj,
                                                    &path,
                                                    1,
                                                    &already_process,
                                                    buff,
                                                    &out_len,
                                                    16,
                                                    &format,
                                                    false),
                          _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED);
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, 16);

    format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      &path,
                                                      1,
                                                      &already_process,
                                                      &(buff[16]),
                                                      &out_len,
                                                      16,
                                                      &format,
                                                      false));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, 1);
    VERIFY_PAYLOAD("\xBF\x0B\xBF\x02\xBF\x00\x03\x02\xBF\x01\x06\x02\x07\xFF"
                   "\xFF\xFF\xFF",
                   buff, 17);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_path_to_object_without_instances) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    size_t already_process = 0;
    const anj_uri_path_t *path = &ANJ_MAKE_OBJECT_PATH(13);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      &path,
                                                      1,
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      false));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR);
    VERIFY_PAYLOAD("\xBF\xFF", buff, out_len);
}

ANJ_UNIT_TEST(dm_observe_handlers,
              build_msg_path_to_object_instance_without_readable_resources) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    size_t already_process = 0;
    const anj_uri_path_t *path = &ANJ_MAKE_INSTANCE_PATH(11, 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      &path,
                                                      1,
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      false));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, 2);
    VERIFY_PAYLOAD("\xBF\xFF", buff, 2);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_path_to_unreadable_resource) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    size_t already_process = 0;
    const anj_uri_path_t *path = &ANJ_MAKE_RESOURCE_PATH(11, 1, 1);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_build_msg(&anj,
                                                    &path,
                                                    1,
                                                    &already_process,
                                                    buff,
                                                    &out_len,
                                                    buff_len,
                                                    &format,
                                                    false),
                          ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_error_dm) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    size_t already_process = 0;
    const anj_uri_path_t *path = &ANJ_MAKE_INSTANCE_PATH(11, 17);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_build_msg(&anj,
                                                    &path,
                                                    1,
                                                    &already_process,
                                                    buff,
                                                    &out_len,
                                                    buff_len,
                                                    &format,
                                                    false),
                          ANJ_COAP_CODE_NOT_FOUND);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_error_anj) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_SENML_JSON;
    size_t already_process = 0;
    const anj_uri_path_t *path = &ANJ_MAKE_RESOURCE_PATH(11, 2, 0);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_build_msg(&anj,
                                                    &path,
                                                    1,
                                                    &already_process,
                                                    buff,
                                                    &out_len,
                                                    buff_len,
                                                    &format,
                                                    false),
                          ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT);

    format = _ANJ_COAP_FORMAT_PLAINTEXT;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      &path,
                                                      1,
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      false));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_PLAINTEXT);
    VERIFY_PAYLOAD("3", buff, out_len);
}
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
ANJ_UNIT_TEST(dm_observe_handlers, build_msg_composite) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_SENML_CBOR;
    size_t already_process = 0;
    const anj_uri_path_t *path[2] = { &ANJ_MAKE_RESOURCE_PATH(11, 2, 0),
                                      &ANJ_MAKE_INSTANCE_PATH(12, 2) };
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      path,
                                                      ANJ_ARRAY_SIZE(path),
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      true));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
    VERIFY_PAYLOAD("\x84"          /* array(4) */
                   "\xA2"          /* map(2) */
                   "\x00"          /* unsigned(0) => SenML Name */
                   "\x67/11/2/0"   /* text(7) */
                   "\x02"          /* unsigned(2) => SenML Value*/
                   "\x03"          /* Value */
                   "\xA2"          /* map(2) */
                   "\x00"          /* unsigned(0) => SenML Name */
                   "\x67/12/2/0"   /* text(7) */
                   "\x02"          /* unsigned(2) => SenML Value*/
                   "\x03"          /* Value */
                   "\xA2"          /* map(2) */
                   "\x00"          /* unsigned(0) => SenML Name */
                   "\x69/12/2/2/1" /* text(9) */
                   "\x02"          /* unsigned(2) => SenML Value*/
                   "\x06"          /* Value */
                   "\xA2"          /* map(2) */
                   "\x00"          /* unsigned(0) => SenML Name */
                   "\x69/12/2/2/2" /* text(9) */
                   "\x02"          /* unsigned(2) => SenML Value*/
                   "\x07",         /* Value */
                   buff, out_len);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_composite_block) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_SENML_CBOR;
    size_t already_process = 0;
    const anj_uri_path_t *path[2] = { &ANJ_MAKE_RESOURCE_PATH(11, 2, 0),
                                      &ANJ_MAKE_INSTANCE_PATH(12, 2) };
    size_t buff_size = 16;

    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_build_msg(&anj,
                                                    path,
                                                    ANJ_ARRAY_SIZE(path),
                                                    &already_process,
                                                    buff,
                                                    &out_len,
                                                    buff_size,
                                                    &format,
                                                    true),
                          _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED);
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, buff_size);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_build_msg(&anj,
                                                    path,
                                                    ANJ_ARRAY_SIZE(path),
                                                    &already_process,
                                                    &buff[out_len],
                                                    &out_len,
                                                    buff_size,
                                                    &format,
                                                    true),
                          _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED);
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, buff_size);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_build_msg(&anj,
                                                    path,
                                                    ANJ_ARRAY_SIZE(path),
                                                    &already_process,
                                                    &buff[out_len * 2],
                                                    &out_len,
                                                    buff_size,
                                                    &format,
                                                    true),
                          _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED);
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, buff_size);

    format = _ANJ_COAP_FORMAT_NOT_DEFINED;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      path,
                                                      ANJ_ARRAY_SIZE(path),
                                                      &already_process,
                                                      &(buff[out_len * 3]),
                                                      &out_len,
                                                      buff_size,
                                                      &format,
                                                      true));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, 5);

    VERIFY_PAYLOAD("\x84"          /* array(4) */
                   "\xA2"          /* map(2) */
                   "\x00"          /* unsigned(0) => SenML Name */
                   "\x67/11/2/0"   /* text(7) */
                   "\x02"          /* unsigned(2) => SenML Value*/
                   "\x03"          /* Value */
                   "\xA2"          /* map(2) */
                   "\x00"          /* unsigned(0) => SenML Name */
                   "\x67/12/2/0"   /* text(7) */
                   "\x02"          /* unsigned(2) => SenML Value*/
                   "\x03"          /* Value */
                   "\xA2"          /* map(2) */
                   "\x00"          /* unsigned(0) => SenML Name */
                   "\x69/12/2/2/1" /* text(9) */
                   "\x02"          /* unsigned(2) => SenML Value*/
                   "\x06"          /* Value */
                   "\xA2"          /* map(2) */
                   "\x00"          /* unsigned(0) => SenML Name */
                   "\x69/12/2/2/2" /* text(9) */
                   "\x02"          /* unsigned(2) => SenML Value*/
                   "\x07",         /* Value */
                   buff, buff_size * 3 + 5);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_composite_lack_of_one_path) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_SENML_CBOR;
    size_t already_process = 0;
    const anj_uri_path_t *path[2] = { &ANJ_MAKE_RESOURCE_PATH(11, 2, 0),
                                      &ANJ_MAKE_INSTANCE_PATH(22, 37) };
    // Paths that point to object that doesn't exist in DM should not be passed
    // by observe module to message callback
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_build_msg(&anj,
                                                    path,
                                                    ANJ_ARRAY_SIZE(path),
                                                    &already_process,
                                                    buff,
                                                    &out_len,
                                                    buff_len,
                                                    &format,
                                                    true),
                          ANJ_COAP_CODE_NOT_FOUND);
}

ANJ_UNIT_TEST(dm_observe_handlers,
              build_msg_composite_one_path_points_to_unreadable_resource) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_SENML_CBOR;
    size_t already_process = 0;
    const anj_uri_path_t *path[2] = { &ANJ_MAKE_RESOURCE_PATH(11, 2, 0),
                                      &ANJ_MAKE_RESOURCE_PATH(11, 2, 1) };

    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_observe_build_msg(&anj,
                                                    path,
                                                    ANJ_ARRAY_SIZE(path),
                                                    &already_process,
                                                    buff,
                                                    &out_len,
                                                    buff_len,
                                                    &format,
                                                    true),
                          ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
}

ANJ_UNIT_TEST(dm_observe_handlers,
              build_msg_composite_path_to_object_without_instances) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_SENML_CBOR;
    size_t already_process = 0;
    const anj_uri_path_t *path[1] = { &ANJ_MAKE_OBJECT_PATH(13) };
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      path,
                                                      ANJ_ARRAY_SIZE(path),
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      true));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, 1);
    ANJ_UNIT_ASSERT_EQUAL(buff[0], 0x80);
}

ANJ_UNIT_TEST(
        dm_observe_handlers,
        build_msg_composite_path_to_object_instance_without_readable_resources) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_SENML_CBOR;
    size_t already_process = 0;
    const anj_uri_path_t *path[1] = { &ANJ_MAKE_INSTANCE_PATH(11, 1) };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      path,
                                                      ANJ_ARRAY_SIZE(path),
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      true));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, 1);
    ANJ_UNIT_ASSERT_EQUAL(buff[0], 0x80);
}

ANJ_UNIT_TEST(
        dm_observe_handlers,
        build_msg_composite_path_to_object_instance_without_readable_resources_and_path_with_resources) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_SENML_CBOR;
    size_t already_process = 0;
    const anj_uri_path_t *path[2] = { &ANJ_MAKE_INSTANCE_PATH(11, 1),
                                      &ANJ_MAKE_RESOURCE_PATH(11, 2, 0) };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      path,
                                                      ANJ_ARRAY_SIZE(path),
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      true));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
    VERIFY_PAYLOAD("\x81"        /* array(4) */
                   "\xA2"        /* map(2) */
                   "\x00"        /* unsigned(0) => SenML Name */
                   "\x67/11/2/0" /* text(7) */
                   "\x02"        /* unsigned(2) => SenML Value*/
                   "\x03",       /* Value */
                   buff, out_len);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_composite_no_paths) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_SENML_CBOR;
    size_t already_process = 0;
    const anj_uri_path_t *path[0];

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_observe_build_msg(&anj,
                                                      path,
                                                      ANJ_ARRAY_SIZE(path),
                                                      &already_process,
                                                      buff,
                                                      &out_len,
                                                      buff_len,
                                                      &format,
                                                      true));
    ANJ_UNIT_ASSERT_EQUAL(format, _ANJ_COAP_FORMAT_SENML_CBOR);
    ANJ_UNIT_ASSERT_EQUAL(out_len, 1);
    ANJ_UNIT_ASSERT_EQUAL(buff[0], 0x80);
}

ANJ_UNIT_TEST(dm_observe_handlers, build_msg_wrong_format) {
    SET_UP();
    size_t out_len;
    uint16_t format = _ANJ_COAP_FORMAT_PLAINTEXT;
    size_t already_process = 0;
    const anj_uri_path_t *path[2] = { &ANJ_MAKE_RESOURCE_PATH(11, 2, 0),
                                      &ANJ_MAKE_INSTANCE_PATH(12, 2) };
    ANJ_UNIT_ASSERT_EQUAL((_anj_dm_observe_build_msg(&anj,
                                                     path,
                                                     ANJ_ARRAY_SIZE(path),
                                                     &already_process,
                                                     buff,
                                                     &out_len,
                                                     buff_len,
                                                     &format,
                                                     true)),
                          ANJ_COAP_CODE_BAD_REQUEST);
}
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
#endif
