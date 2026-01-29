/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdint.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include "../../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#define VERIFY_PAYLOAD(Payload, Buff, Len)                     \
    do {                                                       \
        ANJ_UNIT_ASSERT_EQUAL(Len, strlen(Buff));              \
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(Payload, Buff, Len); \
    } while (0)

ANJ_UNIT_TEST(bootstrap_discover_payload, object_0_call) {
    _anj_io_bootstrap_discover_ctx_t ctx;
    char out_buff[200] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;
    uint16_t ssid;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_init(
            &ctx, &ANJ_MAKE_OBJECT_PATH(0)));
    ssid = 101;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 0), NULL, &ssid,
            "coaps://server_1.example.com"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ssid = 102;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 2), NULL, &ssid,
            "coaps://server_2.example.com"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
#ifdef ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.2,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</0/1>,"
                   "</0/2>;ssid=102;uri=\"coaps://server_2.example.com\"",
                   out_buff, msg_len);
#else  // ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.1,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</0/1>,"
                   "</0/2>;ssid=102;uri=\"coaps://server_2.example.com\"",
                   out_buff, msg_len);
#endif // ANJ_WITH_LWM2M12
}

ANJ_UNIT_TEST(bootstrap_discover_payload, object_root_call) {
    _anj_io_bootstrap_discover_ctx_t ctx;
    char out_buff[200] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;
    uint16_t ssid;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_bootstrap_discover_ctx_init(&ctx, &ANJ_MAKE_ROOT_PATH()));
    ssid = 101;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 0), NULL, &ssid,
            "coaps://server_1.example.com"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ssid = 102;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 2), NULL, &ssid,
            "coaps://server_2.example.com"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;

#ifdef ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.2,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</0/1>,"
                   "</0/2>;ssid=102;uri=\"coaps://server_2.example.com\"",
                   out_buff, msg_len);
#else  // ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.1,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</0/1>,"
                   "</0/2>;ssid=102;uri=\"coaps://server_2.example.com\"",
                   out_buff, msg_len);
#endif // ANJ_WITH_LWM2M12
}

ANJ_UNIT_TEST(bootstrap_discover_payload, object_root_call_empty_server_obj) {
    _anj_io_bootstrap_discover_ctx_t ctx;
    char out_buff[200] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;
    uint16_t ssid;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_bootstrap_discover_ctx_init(&ctx, &ANJ_MAKE_ROOT_PATH()));
    ssid = 101;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 0), NULL, &ssid,
            "coaps://server_1.example.com"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;

#ifdef ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.2,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</1>",
                   out_buff, msg_len);
#else  // ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.1,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</1>",
                   out_buff, msg_len);
#endif // ANJ_WITH_LWM2M12
}

ANJ_UNIT_TEST(bootstrap_discover_payload, more_object_call) {
    _anj_io_bootstrap_discover_ctx_t ctx;
    char out_buff[200] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;
    uint16_t ssid;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_bootstrap_discover_ctx_init(&ctx, &ANJ_MAKE_ROOT_PATH()));
    ssid = 101;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 0), NULL, &ssid,
            "coaps://server_1.example.com"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 0), NULL, &ssid, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(4), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(5), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;

#ifdef ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.2,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</0/1>,</1/0>;ssid=101,"
                   "</3/0>,</4>,</5>",
                   out_buff, msg_len);
#else  // ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.1,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</0/1>,</1/0>;ssid=101,"
                   "</3/0>,</4>,</5>",
                   out_buff, msg_len);
#endif // ANJ_WITH_LWM2M12
}

ANJ_UNIT_TEST(bootstrap_discover_payload, oscore) {
    _anj_io_bootstrap_discover_ctx_t ctx;
    char out_buff[200] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;
    anj_uri_path_t base_path = ANJ_MAKE_ROOT_PATH();
    uint16_t ssid;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_bootstrap_discover_ctx_init(&ctx, &base_path));
    ssid = 101;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 0), NULL, &ssid,
            "coaps://server_1.example.com"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ssid = 102;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 2), NULL, &ssid,
            "coap://server_1.example.com"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ssid = 101;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(21, 0), NULL, &ssid, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ssid = 102;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(21, 1), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(21, 2), NULL, &ssid, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
#ifdef ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.2,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</0/1>,"
                   "</0/2>;ssid=102;uri=\"coap://server_1.example.com\",</21/"
                   "0>;ssid=101,</21/1>,</21/2>;ssid=102",
                   out_buff, msg_len);
#else  // ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.1,</0/0>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</0/1>,"
                   "</0/2>;ssid=102;uri=\"coap://server_1.example.com\",</21/"
                   "0>;ssid=101,</21/1>,</21/2>;ssid=102",
                   out_buff, msg_len);
#endif // ANJ_WITH_LWM2M12
}

ANJ_UNIT_TEST(bootstrap_discover_payload, version) {
    _anj_io_bootstrap_discover_ctx_t ctx;
    char out_buff[200] = { 0 };
    size_t copied_bytes = 0;
    size_t msg_len = 0;
    anj_uri_path_t base_path = ANJ_MAKE_ROOT_PATH();
    uint16_t ssid;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_bootstrap_discover_ctx_init(&ctx, &base_path));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 0), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ssid = 101;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 1), NULL, &ssid,
            "coaps://server_1.example.com"));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 0), NULL, &ssid, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(4, 0), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(5), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(55), "1.9", NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(55, 0), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, &out_buff[msg_len], 200, &copied_bytes));
    msg_len += copied_bytes;

#ifdef ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.2,</0/0>,</0/1>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</1/0>;ssid=101,</3/0>,</4/"
                   "0>,</5>,</55>;ver=1.9,</55/0>",
                   out_buff, msg_len);
#else  // ANJ_WITH_LWM2M12
    VERIFY_PAYLOAD("</>;lwm2m=1.1,</0/0>,</0/1>;ssid=101;uri=\"coaps://"
                   "server_1.example.com\",</1/0>;ssid=101,</3/0>,</4/"
                   "0>,</5>,</55>;ver=1.9,</55/0>",
                   out_buff, msg_len);
#endif // ANJ_WITH_LWM2M12
}

ANJ_UNIT_TEST(bootstrap_discover_payload, errors) {
    _anj_io_bootstrap_discover_ctx_t ctx;
    anj_uri_path_t base_path = ANJ_MAKE_OBJECT_PATH(1);
    uint16_t ssid;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_bootstrap_discover_ctx_init(&ctx, &base_path));

    ANJ_UNIT_ASSERT_FAILED(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(0), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_FAILED(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_OBJECT_PATH(1), ".", NULL, NULL));
    size_t copied_bytes = 0;
    uint8_t out_buff[200] = { 0 };
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_get_payload(
            &ctx, out_buff, 200, &copied_bytes));

    ANJ_UNIT_ASSERT_FAILED(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 0), NULL, NULL, NULL));
    ANJ_UNIT_ASSERT_FAILED(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 0), NULL, NULL, NULL));
    ssid = 101;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
            &ctx, &ANJ_MAKE_INSTANCE_PATH(1, 0), NULL, &ssid, NULL));
}

ANJ_UNIT_TEST(bootstrap_discover_payload, block_transfer) {
    for (size_t i = 5; i < 75; i++) {
        _anj_io_bootstrap_discover_ctx_t ctx;
        char out_buff[200] = { 0 };
        uint16_t ssid = 65534;
        const char *uri = "coaps://server_1.example.com";
        _anj_io_bootstrap_discover_ctx_init(&ctx, &ANJ_MAKE_ROOT_PATH());
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_bootstrap_discover_ctx_new_entry(
                &ctx, &ANJ_MAKE_INSTANCE_PATH(0, 65534), NULL, &ssid, uri));

        int res = -1;
        size_t copied_bytes = 0;
        size_t msg_len = 0;
        while (res) {
            res = _anj_io_bootstrap_discover_ctx_get_payload(
                    &ctx, &out_buff[msg_len], i, &copied_bytes);
            msg_len += copied_bytes;
            ANJ_UNIT_ASSERT_TRUE(res == 0 || res == ANJ_IO_NEED_NEXT_CALL);
        }
#ifdef ANJ_WITH_LWM2M12
        VERIFY_PAYLOAD("</>;lwm2m=1.2,</0/65534>;ssid=65534;uri=\"coaps://"
                       "server_1.example.com\"",
                       out_buff, msg_len);
#else  // ANJ_WITH_LWM2M12
        VERIFY_PAYLOAD("</>;lwm2m=1.1,</0/65534>;ssid=65534;uri=\"coaps://"
                       "server_1.example.com\"",
                       out_buff, msg_len);
#endif // ANJ_WITH_LWM2M12
    }
}
