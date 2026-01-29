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
#include <stdio.h>
#include <string.h>

#include <anj/defs.h>
#include <anj/utils.h>

#include "../../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#define VERIFY_PAYLOAD(Payload, Buff, Len)                     \
    do {                                                       \
        ANJ_UNIT_ASSERT_EQUAL(Len, strlen(Buff));              \
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(Payload, Buff, Len); \
    } while (0)

ANJ_UNIT_TEST(discover_payload, first_example_from_specification) {
    _anj_io_discover_ctx_t ctx;
    char out_buff[300] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    anj_uri_path_t base_path = ANJ_MAKE_OBJECT_PATH(3);
    _anj_attr_notification_t obj_attr = { 0 };
    obj_attr.has_min_period = true;
    obj_attr.min_period = 10;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_init(&ctx, &base_path, NULL));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &base_path, &obj_attr, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    _anj_attr_notification_t obj_inst_attr = { 0 };
    obj_inst_attr.has_max_period = true;
    obj_inst_attr.max_period = 60;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0), &obj_inst_attr, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 2), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 4), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    uint16_t dim = 2;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 6), NULL, NULL, &dim));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    _anj_attr_notification_t res_attr = { 0 };
    res_attr.has_greater_than = true;
    res_attr.has_less_than = true;
    res_attr.greater_than = 50;
    res_attr.less_than = 42.2;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 7), &res_attr, NULL, &dim));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 8), NULL, NULL, &dim));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 11), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 16), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</3>;pmin=10,</3/0>;pmax=60,</3/0/1>,</3/0/2>,</3/0/3>,</3/"
                   "0/4>,</3/0/6>;dim=2,</3/0/7>;dim=2;gt=50;lt=42.2,</3/0/"
                   "8>;dim=2,</3/0/11>,</3/0/16>",
                   out_buff, msg_len);
}

ANJ_UNIT_TEST(discover_payload, second_example_from_specification) {
    _anj_io_discover_ctx_t ctx;
    char out_buff[300] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    anj_uri_path_t base_path = ANJ_MAKE_OBJECT_PATH(1);
    uint32_t depth = 1;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_discover_ctx_init(&ctx, &base_path, &depth));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_discover_ctx_new_entry(&ctx, &base_path, NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 0), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    _anj_attr_notification_t obj_inst_attr = { 0 };
    obj_inst_attr.has_max_period = true;
    obj_inst_attr.max_period = 300;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 4), &obj_inst_attr, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</1>,</1/0>,</1/4>;pmax=300", out_buff, msg_len);
}

ANJ_UNIT_TEST(discover_payload, third_example_from_specification) {
    _anj_io_discover_ctx_t ctx;
    char out_buff[300] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    anj_uri_path_t base_path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    uint32_t depth = 3;
    _anj_attr_notification_t obj_inst_attr = { 0 };
    obj_inst_attr.has_min_period = true;
    obj_inst_attr.min_period = 10;
    obj_inst_attr.has_max_period = true;
    obj_inst_attr.max_period = 60;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_discover_ctx_init(&ctx, &base_path, &depth));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &base_path, &obj_inst_attr, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 2), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 4), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    uint16_t dim = 2;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 6), NULL, NULL, &dim));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 0), NULL, NULL,
            NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 3), NULL, NULL,
            NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    _anj_attr_notification_t res_attr = { 0 };
    res_attr.has_greater_than = true;
    res_attr.has_less_than = true;
    res_attr.greater_than = 50;
    res_attr.less_than = 42.2;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 7), &res_attr, NULL, &dim));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 7, 0), NULL, NULL,
            NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    _anj_attr_notification_t res_inst_attr = { 0 };
    res_inst_attr.has_less_than = true;
    res_inst_attr.less_than = 45.0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 7, 1), &res_inst_attr,
            NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 8), NULL, NULL, &dim));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 8, 1), NULL, NULL,
            NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 8, 2), NULL, NULL,
            NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 11), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 16), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</3/0>;pmin=10;pmax=60,</3/0/1>,</3/0/2>,</3/0/3>,</3/0/"
                   "4>,</3/0/6>;dim=2,</3/0/6/0>,</3/0/6/3>,</3/0/"
                   "7>;dim=2;gt=50;lt=42.2,</3/0/7/0>,</3/0/7/1>;lt=45,</3/"
                   "0/8>;dim=2,</3/0/8/1>,</3/0/8/2>,</3/0/11>,</3/0/16>",
                   out_buff, msg_len);
}

ANJ_UNIT_TEST(discover_payload, fourth_example_from_specification) {
    _anj_io_discover_ctx_t ctx;
    char out_buff[300] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    anj_uri_path_t base_path = ANJ_MAKE_INSTANCE_PATH(3, 0);
    uint32_t depth = 0;
    _anj_attr_notification_t attributes = { 0 };
    attributes.has_max_period = true;
    attributes.has_min_period = true;
    attributes.max_period = 60;
    attributes.min_period = 10;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_discover_ctx_init(&ctx, &base_path, &depth));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &base_path, &attributes, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    VERIFY_PAYLOAD("</3/0>;pmin=10;pmax=60", out_buff, msg_len);
}

ANJ_UNIT_TEST(discover_payload, fifth_example_from_specification) {
    _anj_io_discover_ctx_t ctx;
    char out_buff[300] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    anj_uri_path_t base_path = ANJ_MAKE_RESOURCE_PATH(3, 0, 7);
    _anj_attr_notification_t attributes = { 0 };
    attributes.has_max_period = true;
    attributes.has_min_period = true;
    attributes.max_period = 60;
    attributes.min_period = 10;
    attributes.has_greater_than = true;
    attributes.has_less_than = true;
    attributes.greater_than = 50;
    attributes.less_than = 42e20;
    uint16_t dim = 2;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_init(&ctx, &base_path, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &base_path, &attributes, NULL, &dim));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 7, 0), NULL, NULL,
            NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    _anj_attr_notification_t res_inst_attr = { 0 };
    res_inst_attr.has_less_than = true;
    res_inst_attr.less_than = 45.0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 7, 1), &res_inst_attr,
            NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</3/0/7>;dim=2;pmin=10;pmax=60;gt=50;lt=4.2e+21,</3/0/7/"
                   "0>,</3/0/7/1>;lt=45",
                   out_buff, msg_len);
}

ANJ_UNIT_TEST(discover_payload, block_transfer) {
    for (size_t i = 5; i < 75; i++) {
        _anj_io_discover_ctx_t ctx;
        char out_buff[300] = { 0 };
        size_t copied_bytes = 0;
        size_t msg_len = 0;

        anj_uri_path_t base_path = ANJ_MAKE_RESOURCE_PATH(3, 0, 7);
        _anj_attr_notification_t attributes = { 0 };
        attributes.has_max_period = true;
        attributes.has_min_period = true;
        attributes.max_period = 60;
        attributes.min_period = 10;
        attributes.has_greater_than = true;
        attributes.has_less_than = true;
        attributes.greater_than = 50;
        attributes.less_than = 42.2;
        uint16_t dim = 2;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_discover_ctx_init(&ctx, &base_path, NULL));
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
                &ctx, &base_path, &attributes, NULL, &dim));

        int res = -1;
        while (res) {
            res = _anj_io_discover_ctx_get_payload(&ctx, &out_buff[msg_len], i,
                                                   &copied_bytes);
            msg_len += copied_bytes;
            ANJ_UNIT_ASSERT_TRUE(res == 0 || res == ANJ_IO_NEED_NEXT_CALL);
        }
        res = -1;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
                &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 7, 0), NULL, NULL,
                NULL));
        while (res) {
            res = _anj_io_discover_ctx_get_payload(&ctx, &out_buff[msg_len], i,
                                                   &copied_bytes);
            msg_len += copied_bytes;
            ANJ_UNIT_ASSERT_TRUE(res == 0 || res == ANJ_IO_NEED_NEXT_CALL);
        }
        res = -1;
        _anj_attr_notification_t res_inst_attr = { 0 };
        res_inst_attr.has_less_than = true;
        res_inst_attr.less_than = 45.0;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
                &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 7, 1),
                &res_inst_attr, NULL, NULL));
        while (res) {
            res = _anj_io_discover_ctx_get_payload(&ctx, &out_buff[msg_len], i,
                                                   &copied_bytes);
            msg_len += copied_bytes;
            ANJ_UNIT_ASSERT_TRUE(res == 0 || res == ANJ_IO_NEED_NEXT_CALL);
        }

        VERIFY_PAYLOAD("</3/0/7>;dim=2;pmin=10;pmax=60;gt=50;lt=42.2,</3/0/7/"
                       "0>,</3/0/7/1>;lt=45",
                       out_buff, msg_len);
    }
}

ANJ_UNIT_TEST(discover_payload, errors) {
    _anj_io_discover_ctx_t ctx;
    char out_buff[300] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    // root path
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_discover_ctx_init(&ctx, &ANJ_MAKE_ROOT_PATH(),
                                                    NULL),
                          _ANJ_IO_ERR_INPUT_ARG);
    // resource instance path
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_io_discover_ctx_init(
                    &ctx, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 3, 3), NULL),
            _ANJ_IO_ERR_INPUT_ARG);
    uint32_t depth = 4;
    // depth greater than 3
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_io_discover_ctx_init(&ctx, &ANJ_MAKE_OBJECT_PATH(3), &depth),
            _ANJ_IO_ERR_INPUT_ARG);
    depth = 3;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_discover_ctx_init(&ctx, &ANJ_MAKE_OBJECT_PATH(3), &depth));
    // given path is outside the base_path
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_io_discover_ctx_new_entry(&ctx, &ANJ_MAKE_INSTANCE_PATH(2, 1),
                                           NULL, NULL, NULL),
            _ANJ_IO_ERR_INPUT_ARG);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    // ascending order of path is not respected
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_io_discover_ctx_new_entry(&ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0),
                                           NULL, NULL, NULL),
            _ANJ_IO_ERR_INPUT_ARG);
    // dim is given for path that is not Resource
    uint16_t dim = 0;
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_io_discover_ctx_new_entry(&ctx, &ANJ_MAKE_INSTANCE_PATH(3, 2),
                                           NULL, NULL, &dim),
            _ANJ_IO_ERR_INPUT_ARG);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 2), NULL, NULL, NULL));
    // internal buffer not empty
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_io_discover_ctx_new_entry(&ctx, &ANJ_MAKE_INSTANCE_PATH(3, 3),
                                           NULL, NULL, NULL),
            _ANJ_IO_ERR_LOGIC);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    dim = 1;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 2, 2), NULL, NULL, &dim));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    // expected resource instance
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_discover_ctx_new_entry(
                                  &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 2, 3), NULL,
                                  NULL, NULL),
                          _ANJ_IO_ERR_LOGIC);
    // no more data in internal buffer
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_discover_ctx_get_payload(
                                  &ctx, &out_buff[msg_len], 300, &copied_bytes),
                          _ANJ_IO_ERR_LOGIC);

    VERIFY_PAYLOAD("</3/1>,</3/2>,</3/2/2>;dim=1", out_buff, msg_len);
}

ANJ_UNIT_TEST(discover_payload, depth_warning) {
    _anj_io_discover_ctx_t ctx;
    char out_buff[300] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    uint32_t depth = 1;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_init(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 1), &depth));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    uint16_t dim = 1;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), NULL, NULL, &dim));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_discover_ctx_new_entry(
                                  &ctx,
                                  &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 1, 0),
                                  NULL, NULL, NULL),
                          _ANJ_IO_WARNING_DEPTH);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 2), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 300, &copied_bytes));
    msg_len += copied_bytes;
    VERIFY_PAYLOAD("</3/1>,</3/1/1>;dim=1,</3/1/2>", out_buff, msg_len);
}
