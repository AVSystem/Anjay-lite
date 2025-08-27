/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include "../../../src/anj/coap/coap.h"
#include "../../../src/anj/io/cbor_encoder.h"
#include "../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_SENML_CBOR

typedef struct {
    _anj_io_out_ctx_t ctx;
    _anj_op_t op_type;
    char buf[500];
    size_t buffer_length;
    size_t out_length;
} senml_cbor_test_env_t;

static void senml_cbor_test_setup(senml_cbor_test_env_t *env,
                                  anj_uri_path_t *base_path,
                                  size_t items_count,
                                  _anj_op_t op_type) {
    env->buffer_length = sizeof(env->buf);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_init(&env->ctx, op_type, base_path,
                                                 items_count,
                                                 _ANJ_COAP_FORMAT_SENML_CBOR));
}

#    define VERIFY_BYTES(Env, Data)                                  \
        do {                                                         \
            ANJ_UNIT_ASSERT_EQUAL_BYTES(Env.buf, Data);              \
            ANJ_UNIT_ASSERT_EQUAL(Env.out_length, sizeof(Data) - 1); \
        } while (0)

ANJ_UNIT_TEST(senml_cbor_encoder, empty_read) {
    senml_cbor_test_env_t env = { 0 };
    anj_uri_path_t base_path = ANJ_MAKE_INSTANCE_PATH(3, 3);
    senml_cbor_test_setup(&env, &base_path, 0, ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x80");
}

ANJ_UNIT_TEST(senml_cbor_encoder, single_send_record_with_all_fields) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry = {
        .timestamp = 100000.0,
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA3"
                      "\x00\x66\x2F\x33\x2F\x33\x2F\x33" // name
                      "\x22\xFA\x47\xC3\x50\x00"         // base time
                      "\x02\x18\x19");
}

ANJ_UNIT_TEST(senml_cbor_encoder, single_read_record_with_all_fields) {
    senml_cbor_test_env_t env = { 0 };

    anj_uri_path_t base_path = ANJ_MAKE_INSTANCE_PATH(3, 3);
    senml_cbor_test_setup(&env, &base_path, 1, ANJ_OP_DM_READ);

    anj_io_out_entry_t entry = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA3"
                      "\x21\x64\x2F\x33\x2F\x33" // base name
                      "\x00\x62\x2F\x33"         // name
                      "\x02\x18\x19");
}

ANJ_UNIT_TEST(senml_cbor_encoder, largest_possible_size_of_single_msg) {
    senml_cbor_test_env_t env = { 0 };

    anj_uri_path_t base_path = ANJ_MAKE_INSTANCE_PATH(65534, 65534);
    env.buffer_length = sizeof(env.buf);
    env.ctx.format = _ANJ_COAP_FORMAT_SENML_CBOR;
    // call _anj_senml_cbor_encoder_init directly to allow to set basename and
    // timestamp in one message
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_senml_cbor_encoder_init(&env.ctx, &base_path, 65534, true));

    anj_io_out_entry_t entry = {
        .timestamp = 1.0e+300,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(65534, 65534, 65534, 65534),
        .type = ANJ_DATA_TYPE_OBJLNK,
        .value.objlnk.oid = 65534,
        .value.objlnk.iid = 65534
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(
            env,
            "\x99\xFF\xFE\xA4"
            "\x21\x6C\x2F\x36\x35\x35\x33\x34\x2F\x36\x35\x35\x33\x34" // basename
            "\x00\x6C\x2F\x36\x35\x35\x33\x34\x2F\x36\x35\x35\x33\x34" // name
            "\x22\xFB\x7E\x37\xE4\x3C\x88\x00\x75\x9C" // base time
            "\x63"
            "vlo" // objlink
            "\x6B\x36\x35\x35\x33\x34\x3A\x36\x35\x35\x33\x34");
    ANJ_UNIT_ASSERT_EQUAL(env.out_length,
                          _ANJ_IO_SENML_CBOR_SIMPLE_RECORD_MAX_LENGTH - 1);
}

ANJ_UNIT_TEST(senml_cbor_encoder, int) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_NON_CON_NOTIFY);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(77, 77, 77, 77),
        .type = ANJ_DATA_TYPE_INT,
        .value.int_value = -1000
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x6C\x2F\x37\x37\x2F\x37\x37\x2F\x37\x37\x2F\x37\x37"
                      "\x02\x39\x03\xE7");
}

ANJ_UNIT_TEST(senml_cbor_encoder, uint) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_NON_CON_NOTIFY);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(77, 77, 77, 77),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = UINT32_MAX
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x6C\x2F\x37\x37\x2F\x37\x37\x2F\x37\x37\x2F\x37\x37"
                      "\x02\x1A\xFF\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(senml_cbor_encoder, time) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_NON_CON_NOTIFY);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(77, 77, 77, 77),
        .type = ANJ_DATA_TYPE_TIME,
        .value.time_value = 1000000
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x6C\x2F\x37\x37\x2F\x37\x37\x2F\x37\x37\x2F\x37\x37"
                      "\x02\xC1\x1A\x00\x0F\x42\x40");
}

ANJ_UNIT_TEST(senml_cbor_encoder, bool) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_NON_CON_NOTIFY);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_PATH(7, 7, 7),
        .type = ANJ_DATA_TYPE_BOOL,
        .value.bool_value = true
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x66\x2F\x37\x2F\x37\x2F\x37"
                      "\x04\xF5");
}

ANJ_UNIT_TEST(senml_cbor_encoder, float) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_NON_CON_NOTIFY);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_PATH(7, 7, 7),
        .type = ANJ_DATA_TYPE_DOUBLE,
        .value.double_value = 100000.0
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x66\x2F\x37\x2F\x37\x2F\x37"
                      "\x02\xFA\x47\xC3\x50\x00");
}

ANJ_UNIT_TEST(senml_cbor_encoder, double) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_NON_CON_NOTIFY);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_PATH(7, 7, 7),
        .type = ANJ_DATA_TYPE_DOUBLE,
        .value.double_value = -4.1
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x66\x2F\x37\x2F\x37\x2F\x37"
                      "\x02\xFB\xC0\x10\x66\x66\x66\x66\x66\x66");
}

ANJ_UNIT_TEST(senml_cbor_encoder, string) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_NON_CON_NOTIFY);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_PATH(7, 7, 7),
        .type = ANJ_DATA_TYPE_STRING,
        .value.bytes_or_string.data = "DDDDDDDDDD"
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x66\x2F\x37\x2F\x37\x2F\x37"
                      "\x03\x6A"
                      "DDDDDDDDDD");
}

ANJ_UNIT_TEST(senml_cbor_encoder, bytes) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_NON_CON_NOTIFY);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_PATH(7, 7, 7),
        .type = ANJ_DATA_TYPE_BYTES,
        .value.bytes_or_string.data = "DDDDDDDDDD",
        .value.bytes_or_string.chunk_length = 10
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x66\x2F\x37\x2F\x37\x2F\x37"
                      "\x08\x4A"
                      "DDDDDDDDDD");
}

#    ifdef ANJ_WITH_EXTERNAL_DATA
static bool closed;
static bool opened;

static char *ptr_for_callback = NULL;
static size_t ext_data_size = 0;
static int external_data_handler(void *buffer,
                                 size_t *inout_size,
                                 size_t offset,
                                 void *user_args) {
    (void) user_args;
    ANJ_UNIT_ASSERT_TRUE(opened);
    assert(&ptr_for_callback);
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
    ANJ_UNIT_ASSERT_FALSE(opened);
    opened = true;

    return 0;
}

static void external_data_close(void *user_args) {
    (void) user_args;
    ANJ_UNIT_ASSERT_FALSE(closed);
    closed = true;
}

ANJ_UNIT_TEST(senml_cbor_encoder, ext_string) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_PATH(7, 7, 7),
        .type = ANJ_DATA_TYPE_EXTERNAL_STRING,
        .value.external_data.user_args = NULL,
        .value.external_data.get_external_data = external_data_handler,
        .value.external_data.open_external_data = external_data_open,
        .value.external_data.close_external_data = external_data_close
    };
    ext_data_size = 10;
    ptr_for_callback = "DDDDDDDDDD";
    opened = false;
    closed = false;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x66\x2F\x37\x2F\x37\x2F\x37"
                      "\x03"
                      "\x7F"
                      "\x6A"
                      "DDDDDDDDDD"
                      "\xFF");
    ANJ_UNIT_ASSERT_TRUE(closed);
}

ANJ_UNIT_TEST(senml_cbor_encoder, ext_bytes) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_PATH(7, 7, 7),
        .type = ANJ_DATA_TYPE_EXTERNAL_BYTES,
        .value.external_data.user_args = NULL,
        .value.external_data.get_external_data = external_data_handler,
        .value.external_data.open_external_data = external_data_open,
        .value.external_data.close_external_data = external_data_close
    };
    ext_data_size = 10;
    ptr_for_callback = "DDDDDDDDDD";
    opened = false;
    closed = false;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\x81\xA2"
                      "\x00\x66\x2F\x37\x2F\x37\x2F\x37"
                      "\x08"
                      "\x5F"
                      "\x4A"
                      "DDDDDDDDDD"
                      "\xFF");
    ANJ_UNIT_ASSERT_TRUE(closed);
}

ANJ_UNIT_TEST(senml_cbor_encoder, ext_string_2_records) {
    senml_cbor_test_env_t env = { 0 };

    senml_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_PATH(7, 7, 7),
        .type = ANJ_DATA_TYPE_EXTERNAL_STRING,
        .value.external_data.user_args = NULL,
        .value.external_data.get_external_data = external_data_handler,
        .value.external_data.open_external_data = external_data_open,
        .value.external_data.close_external_data = external_data_close
    };
    ext_data_size = 10;
    ptr_for_callback = "DDDDDDDDDD";
    anj_io_out_entry_t entry_2 = {
        .timestamp = NAN,
        .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 0),
        .type = ANJ_DATA_TYPE_INT,
        .value.int_value = 25
    };
    opened = false;
    closed = false;
    size_t len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &len));
    ANJ_UNIT_ASSERT_TRUE(closed);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[len], env.buffer_length, &env.out_length));
    env.out_length += len;
    VERIFY_BYTES(env, "\x82\xA2"
                      "\x00\x66\x2F\x37\x2F\x37\x2F\x37" // 7/7/7
                      "\x03"
                      "\x7F"
                      "\x6A"
                      "DDDDDDDDDD"
                      "\xFF"
                      "\xA2"                             // second record
                      "\x00\x66\x2F\x38\x2F\x38\x2F\x30" // 8/8/0
                      "\x02\x18\x19");
}
#    endif // ANJ_WITH_EXTERNAL_DATA

ANJ_UNIT_TEST(senml_cbor_encoder, complex_notify_msg) {
    senml_cbor_test_env_t env = { 0 };
    anj_io_out_entry_t entries[] = {
        {
            .timestamp = 65504.0,
            .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 0),
            .type = ANJ_DATA_TYPE_INT,
            .value.int_value = 25,
        },
        {
            .timestamp = 65504.0,
            .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 1),
            .type = ANJ_DATA_TYPE_UINT,
            .value.uint_value = 100,
        },
        {
            .timestamp = 65504.0,
            .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 2),
            .type = ANJ_DATA_TYPE_STRING,
            .value.bytes_or_string.data =
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
        },
        {
            .timestamp = 65504.0,
            .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1),
            .type = ANJ_DATA_TYPE_BYTES,
            .value.bytes_or_string.data =
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
            .value.bytes_or_string.chunk_length = 200
        },
        {
            .timestamp = 1.5,
            .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 25),
            .type = ANJ_DATA_TYPE_BOOL,
            .value.bool_value = false
        },
        {
            .timestamp = 1.5,
            .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 26),
            .type = ANJ_DATA_TYPE_OBJLNK,
            .value.objlnk.oid = 17,
            .value.objlnk.iid = 19,
        }
    };

    senml_cbor_test_setup(&env, NULL, ANJ_ARRAY_SIZE(entries),
                          ANJ_OP_INF_NON_CON_NOTIFY);

    for (size_t i = 0; i < ANJ_ARRAY_SIZE(entries); i++) {
        size_t record_len = 0;

        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_out_ctx_new_entry(&env.ctx, &entries[i]));
        int res = -1;
        while (res) {
            size_t temp_len = 0;
            res = _anj_io_out_ctx_get_payload(
                    &env.ctx, &env.buf[env.out_length + record_len], 50,
                    &temp_len);
            ANJ_UNIT_ASSERT_TRUE(res == 0 || res == ANJ_IO_NEED_NEXT_CALL);
            record_len += temp_len;
        }
        env.out_length += record_len;
    }

    VERIFY_BYTES(env, "\x86\xA3"
                      "\x00\x66\x2F\x38\x2F\x38\x2F\x30" // 8/8/0
                      "\x22\xFA\x47\x7F\xE0\x00"         // base time
                      "\x02\x18\x19"
                      "\xA2" // 8/8/1
                      "\x00\x66\x2F\x38\x2F\x38\x2F\x31"
                      "\x02\x18\x64"
                      "\xA2" // 8/8/2
                      "\x00\x66\x2F\x38\x2F\x38\x2F\x32"
                      "\x03\x78\x64"
                      "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                      "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                      "\xA2" // 1/1/1
                      "\x00\x66\x2F\x31\x2F\x31\x2F\x31"
                      "\x08\x58\xC8"
                      "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                      "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                      "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                      "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                      "\xA3" // 1/1/25
                      "\x00\x67\x2F\x31\x2F\x31\x2F\x32\x35"
                      "\x22\xFA\x3F\xC0\x00\x00"
                      "\x04\xF4"
                      "\xA2" // 1/1/26
                      "\x00\x67\x2F\x31\x2F\x31\x2F\x32\x36"
                      "\x63"
                      "vlo"
                      "\x65\x31\x37\x3A\x31\x39");
}

ANJ_UNIT_TEST(senml_cbor_encoder, complex_read_msg) {
    senml_cbor_test_env_t env = { 0 };
    anj_uri_path_t base_path = ANJ_MAKE_INSTANCE_PATH(8, 8);
    anj_io_out_entry_t entries[] = {
        {
            .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 0),
            .type = ANJ_DATA_TYPE_INT,
            .value.int_value = 25,
        },
        {
            .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 1),
            .type = ANJ_DATA_TYPE_UINT,
            .value.uint_value = 100,
        },
        {
            .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 2),
            .type = ANJ_DATA_TYPE_STRING,
            .value.bytes_or_string.data =
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
        },
        {
            .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 3),
            .type = ANJ_DATA_TYPE_BYTES,
            .value.bytes_or_string.data =
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
            .value.bytes_or_string.chunk_length = 200
        },
        {
            .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(8, 8, 4, 0),
            .type = ANJ_DATA_TYPE_BOOL,
            .value.bool_value = false
        },
        {
            .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(8, 8, 4, 1),
            .type = ANJ_DATA_TYPE_OBJLNK,
            .value.objlnk.oid = 17,
            .value.objlnk.iid = 19,
        }
    };

    for (size_t chunk_len = 50; chunk_len < 370; chunk_len += 10) {
        env.out_length = 0;
        senml_cbor_test_setup(&env, &base_path, ANJ_ARRAY_SIZE(entries),
                              ANJ_OP_DM_READ);

        for (size_t i = 0; i < ANJ_ARRAY_SIZE(entries); i++) {
            size_t record_len = 0;
            ANJ_UNIT_ASSERT_SUCCESS(
                    _anj_io_out_ctx_new_entry(&env.ctx, &entries[i]));
            int res = -1;
            while (res) {
                size_t temp_len = 0;
                res = _anj_io_out_ctx_get_payload(
                        &env.ctx, &env.buf[env.out_length + record_len],
                        chunk_len, &temp_len);
                ANJ_UNIT_ASSERT_TRUE(res == 0 || res == ANJ_IO_NEED_NEXT_CALL);
                record_len += temp_len;
            }
            env.out_length += record_len;
        }

        VERIFY_BYTES(env,
                     "\x86\xA3"
                     "\x21\x64\x2F\x38\x2F\x38"
                     "\x00\x62\x2F\x30" // 8/8/0
                     "\x02\x18\x19"
                     "\xA2" // 8/8/1
                     "\x00\x62\x2F\x31"
                     "\x02\x18\x64"
                     "\xA2" // 8/8/2
                     "\x00\x62\x2F\x32"
                     "\x03\x78\x64"
                     "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                     "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                     "\xA2" // 8/8/3
                     "\x00\x62\x2F\x33"
                     "\x08\x58\xC8"
                     "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                     "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                     "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                     "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
                     "\xA2" // 8/8/4/0
                     "\x00\x64\x2F\x34\x2F\x30"
                     "\x04\xF4"
                     "\xA2" // 8/8/4/1
                     "\x00\x64\x2F\x34\x2F\x31"
                     "\x63"
                     "vlo"
                     "\x65\x31\x37\x3A\x31\x39");
    }
}

#    ifdef ANJ_WITH_EXTERNAL_DATA
#        define DATA_HANDLER_ERROR_CODE -888
static int external_data_handler_with_error(void *buffer,
                                            size_t *inout_size,
                                            size_t offset,
                                            void *user_args) {
    (void) buffer;
    (void) inout_size;
    (void) offset;
    (void) user_args;
    ANJ_UNIT_ASSERT_TRUE(opened);
    return DATA_HANDLER_ERROR_CODE;
}
#    endif // ANJ_WITH_EXTERNAL_DATA

ANJ_UNIT_TEST(senml_cbor_encoder, read_error) {
    senml_cbor_test_env_t env = { 0 };

    anj_uri_path_t base_path = ANJ_MAKE_INSTANCE_PATH(3, 3);
    senml_cbor_test_setup(&env, &base_path, 1, ANJ_OP_DM_READ);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_PATH(1, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    ANJ_UNIT_ASSERT_FAILED(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 3, 1),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    ANJ_UNIT_ASSERT_FAILED(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));

    anj_io_out_entry_t entry_3 = {
        .path = ANJ_MAKE_INSTANCE_PATH(3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    ANJ_UNIT_ASSERT_FAILED(_anj_io_out_ctx_new_entry(&env.ctx, &entry_3));
#    ifdef ANJ_WITH_EXTERNAL_DATA
    anj_io_out_entry_t entry_4 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 4),
        .type = ANJ_DATA_TYPE_EXTERNAL_STRING,
        .value.external_data.user_args = NULL,
        .value.external_data.get_external_data =
                external_data_handler_with_error,
        .value.external_data.open_external_data = external_data_open,
        .value.external_data.close_external_data = external_data_close
    };
    opened = false;
    closed = false;

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_4));
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_out_ctx_get_payload(&env.ctx, env.buf,
                                                      env.buffer_length,
                                                      &env.out_length),
                          DATA_HANDLER_ERROR_CODE);
    ANJ_UNIT_ASSERT_TRUE(closed);
#    endif // ANJ_WITH_EXTERNAL_DATA
}

#endif // ANJ_WITH_SENML_CBOR
