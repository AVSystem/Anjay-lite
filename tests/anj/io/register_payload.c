/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <string.h>

#include <anj/utils.h>

#include "../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#define VERIFY_PAYLOAD(Payload, Buff, Len)                     \
    do {                                                       \
        ANJ_UNIT_ASSERT_EQUAL(Len, strlen(Buff));              \
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(Payload, Buff, Len); \
    } while (0)

ANJ_UNIT_TEST(register_payload, only_objects) {
    _anj_io_register_ctx_t ctx;
    char out_buff[100] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    _anj_io_register_ctx_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(1), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(2), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(3), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(4), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(5), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    VERIFY_PAYLOAD("</1>,</2>,</3>,</4>,</5>", out_buff, msg_len);
}

ANJ_UNIT_TEST(register_payload, objects_with_version) {
    _anj_io_register_ctx_t ctx;
    char out_buff[100] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    _anj_io_register_ctx_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(1), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(2), "1.2"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(3), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(4), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(5), "2.3"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</1>,</2>;ver=1.2,</3>,</4>,</5>;ver=2.3", out_buff,
                   msg_len);
}

ANJ_UNIT_TEST(register_payload, objects_with_instances) {
    _anj_io_register_ctx_t ctx;
    char out_buff[100] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    _anj_io_register_ctx_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 1), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(2), "1.2"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(2, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 1), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 2), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 3), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(4), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(5), "2.3"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</1/0>,</1/1>,</2>;ver=1.2,</2/0>,</3/0>,</3/1>,</3/2>,</3/"
                   "3>,</4>,</5>;ver=2.3",
                   out_buff, msg_len);
}

ANJ_UNIT_TEST(register_payload, instances_without_version) {
    _anj_io_register_ctx_t ctx;
    char out_buff[100] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    _anj_io_register_ctx_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 1), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(2, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 1), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 2), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 3), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</1/0>,</1/1>,</2/0>,</3/0>,</3/1>,</3/2>,</3/3>,</4>,</5>",
                   out_buff, msg_len);
}

ANJ_UNIT_TEST(register_payload, instances_with_version) {
    _anj_io_register_ctx_t ctx;
    char out_buff[100] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    _anj_io_register_ctx_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(1), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 3), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(4), "1.1"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(5, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</1>,</3/3>,</4>;ver=1.1,</5/0>", out_buff, msg_len);
}

ANJ_UNIT_TEST(register_payload, big_numbers) {
    _anj_io_register_ctx_t ctx;
    char out_buff[100] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    _anj_io_register_ctx_init(&ctx);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 1), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(22), "1.2"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(22, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(333, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(333, 1), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(333, 2), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(333, 3333), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(4444), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(55555), "2.3"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</1/0>,</1/1>,</22>;ver=1.2,</22/0>,</333/0>,</333/1>,</"
                   "333/2>,</333/3333>,</4444>,</55555>;ver=2.3",
                   out_buff, msg_len);
}

ANJ_UNIT_TEST(register_payload, errors) {
    _anj_io_register_ctx_t ctx;
    char out_buff[100] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;

    _anj_io_register_ctx_init(&ctx);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(2), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_FAILED(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(1), NULL));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(2, 0), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(2, 2), NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_FAILED(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(2, 1), NULL));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(22), "1.2"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_FAILED(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(23), "12"));
    ANJ_UNIT_ASSERT_FAILED(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(23), ".12"));
    ANJ_UNIT_ASSERT_FAILED(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(23), "12."));
    ANJ_UNIT_ASSERT_FAILED(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(23), "a.2"));
    ANJ_UNIT_ASSERT_FAILED(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(23), "2.b"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(23), "1.2"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_get_payload(
            &ctx, &out_buff[msg_len], 100, &copied_bytes));
    msg_len += copied_bytes;

    VERIFY_PAYLOAD("</2>,</2/0>,</2/2>,</22>;ver=1.2,</23>;ver=1.2", out_buff,
                   msg_len);
}

ANJ_UNIT_TEST(register_payload, block_transfer) {
    for (size_t i = 5; i < 25; i++) {
        _anj_io_register_ctx_t ctx;
        char out_buff[50] = { 0 };
        _anj_io_register_ctx_init(&ctx);
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_register_ctx_new_entry(
                &ctx, &ANJ_MAKE_OBJECT_PATH(65222), "9.9"));

        int res = -1;
        size_t copied_bytes = 0;
        size_t msg_len = 0;
        while (res) {
            res = _anj_io_register_ctx_get_payload(&ctx, &out_buff[msg_len], i,
                                                   &copied_bytes);
            msg_len += copied_bytes;
            ANJ_UNIT_ASSERT_TRUE(res == 0 || res == ANJ_IO_NEED_NEXT_CALL);
        }
        VERIFY_PAYLOAD("</65222>;ver=9.9", out_buff, msg_len);
    }
}
