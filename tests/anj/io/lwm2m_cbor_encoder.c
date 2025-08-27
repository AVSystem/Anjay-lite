/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include "../../../src/anj/coap/coap.h"
#include "../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_LWM2M_CBOR

typedef struct {
    _anj_io_out_ctx_t ctx;
    _anj_op_t op_type;
    char buf[500];
    size_t buffer_length;
    size_t out_length;
} lwm2m_cbor_test_env_t;

static void lwm2m_cbor_test_setup(lwm2m_cbor_test_env_t *env,
                                  anj_uri_path_t *base_path,
                                  size_t items_count,
                                  _anj_op_t op_type) {
    env->buffer_length = sizeof(env->buf);
    env->out_length = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_out_ctx_init(&env->ctx, op_type, base_path, items_count,
                                 _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
}

#    define VERIFY_BYTES(Env, Data)                                  \
        do {                                                         \
            ANJ_UNIT_ASSERT_EQUAL_BYTES(Env.buf, Data);              \
            ANJ_UNIT_ASSERT_EQUAL(Env.out_length, sizeof(Data) - 1); \
        } while (0)

ANJ_UNIT_TEST(lwm2m_cbor_encoder, read_empty) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_OBJECT_PATH(3), 0, ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    VERIFY_BYTES(env, "\xBF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, send_single_record) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    // {3: {3: {3: 25}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, read_single_resource_record) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_OBJECT_PATH(3), 1, ANJ_OP_DM_READ);
    anj_io_out_entry_t entry = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    // {3: {3: {3: 25}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF");

    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_INSTANCE_PATH(3, 3), 1,
                          ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    // {3: {3: {3: 25}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF");

    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_RESOURCE_PATH(3, 3, 3), 1,
                          ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    // {3: {3: {3: 25}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, read_single_resource_instance_record) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_OBJECT_PATH(3), 1, ANJ_OP_DM_READ);
    anj_io_out_entry_t entry = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    // {3: {3: {3: {3: 25}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF\xFF");

    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_INSTANCE_PATH(3, 3), 1,
                          ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    // {3: {3: {3: {3: 25}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF\xFF");

    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_RESOURCE_PATH(3, 3, 3), 1,
                          ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    // {3: {3: {3: {3: 25}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF\xFF");

    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 3, 3), 1,
                          ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    // {3: {3: {3: {3: 25}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, send_two_records_different_obj) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: 25}}, 1: {1: {1: 11}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF"
                      "\x01\xBF\x01\xBF\x01"
                      "\x0B"
                      "\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, send_two_records_different_inst) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: 25}, 1: {1: 11}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF"
                      "\x01\xBF\x01"
                      "\x0B"
                      "\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, send_two_records_different_res) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 1),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: 25, 1: 11}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\x01"
                      "\x0B"
                      "\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, send_two_resource_instances) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 3, 0),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 3, 1),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: {0: 25, 1: 11}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03\xBF\x00"
                      "\x18\x19"
                      "\x01"
                      "\x0B"
                      "\xFF\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, send_two_resource_instances_different_res) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 3, 0),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 1, 0),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: {0: 25}, 1: {0: 11}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03\xBF\x00"
                      "\x18\x19"
                      "\xFF\x01\xBF\x00"
                      "\x0B"
                      "\xFF\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, send_two_resource_instances_different_inst) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 3, 0),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 0, 0),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: {0: 25}}, 1: {0: {0: 11}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03\xBF\x00"
                      "\x18\x19"
                      "\xFF\xFF\x01\xBF\x00\xBF\x00"
                      "\x0B"
                      "\xFF\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, send_two_resource_instances_different_obj) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 3, 0),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 0, 0, 0),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: {0: 25}}}, 1: {0: {0: {0: 11}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03\xBF\x00"
                      "\x18\x19"
                      "\xFF\xFF\xFF\x01\xBF\x00\xBF\x00\xBF\x00"
                      "\x0B"
                      "\xFF\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder,
              send_two_records_different_level_different_res) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 3, 1, 1),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: 25, 1: {1: 11}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\x01\xBF\x01"
                      "\x0B"
                      "\xFF\xFF\xFF\xFF");

    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);
    out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {1: {1: 11}, 3: 25}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x01\xBF\x01"
                      "\x0B\xFF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder,
              send_two_records_different_level_different_inst) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 1, 1, 1),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: 25}, 1: {1: {1: 11}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\x01\xBF\x01\xBF\x01"
                      "\x0B"
                      "\xFF\xFF\xFF\xFF");

    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);
    out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {1: {1: {1: 11}}, 3: {3: 25}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x01\xBF\x01\xBF\x01"
                      "\x0B\xFF\xFF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder,
              send_two_records_different_level_different_obj) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 1, 1, 1),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {3: {3: {3: 25}}, 1: {1: {1: {1: 11}}}}
    VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\x01\xBF\x01\xBF\x01\xBF\x01"
                      "\x0B"
                      "\xFF\xFF\xFF\xFF");

    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);
    out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {1: {1: {1: {1: 11}}}, 3: {3: {3: 25}}}
    VERIFY_BYTES(env, "\xBF\x01\xBF\x01\xBF\x01\xBF\x01"
                      "\x0B\xFF\xFF\xFF\x03\xBF\x03\xBF\x03"
                      "\x18\x19"
                      "\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, send_two_records_same_path) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };
    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };

    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, biggest_possible_record) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(65534, 65534, 65534, 65534),
        .type = ANJ_DATA_TYPE_OBJLNK,
        .value.objlnk.iid = 65534,
        .value.objlnk.oid = 65534
    };

    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &env.out_length));
    // {65534: {65534: {65534: {65534: "65534:65534"}}}}
    VERIFY_BYTES(env, "\xBF\x19\xFF\xFE\xBF\x19\xFF\xFE\xBF\x19\xFF\xFE\xBF"
                      "\x19\xFF\xFE"
                      "\x6B\x36\x35\x35\x33\x34\x3A\x36\x35\x35\x33\x34"
                      "\xFF\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, biggest_possible_second_record) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(65533, 65533, 65533, 65533),
        .type = ANJ_DATA_TYPE_OBJLNK,
        .value.objlnk.iid = 65534,
        .value.objlnk.oid = 65534
    };
    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_INSTANCE_PATH(65534, 65534, 65534, 65534),
        .type = ANJ_DATA_TYPE_OBJLNK,
        .value.objlnk.iid = 65534,
        .value.objlnk.oid = 65534
    };

    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, &env.buf[env.out_length],
            env.buffer_length - env.out_length, &out_len));
    env.out_length += out_len;
    // {65533: {65533: {65533: {65533: "65534:65534"}}}, 65534: {65534: {65534:
    // {65534: "65534:65534"}}}}
    VERIFY_BYTES(env,
                 "\xBF\x19\xFF\xFD\xBF\x19\xFF\xFD"
                 "\xBF\x19\xFF\xFD\xBF\x19\xFF\xFD"
                 "\x6B\x36\x35\x35\x33\x34\x3A\x36\x35\x35\x33\x34"
                 "\xFF\xFF\xFF"
                 "\x19\xFF\xFE\xBF\x19\xFF\xFE\xBF\x19\xFF\xFE\xBF\x19\xFF\xFE"
                 "\x6B\x36\x35\x35\x33\x34\x3A\x36\x35\x35\x33\x34"
                 "\xFF\xFF\xFF\xFF");
}

ANJ_UNIT_TEST(lwm2m_cbor_encoder, single_record_chunk_read) {
    lwm2m_cbor_test_env_t env = { 0 };

    anj_io_out_entry_t entry = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_DOUBLE,
        .value.double_value = 1.1
    };

    for (size_t chunk_len = 1; chunk_len < 18; chunk_len++) {
        size_t out_len = 0;
        lwm2m_cbor_test_setup(&env, &ANJ_MAKE_OBJECT_PATH(3), 1,
                              ANJ_OP_DM_READ);
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry));
        int res = -1;
        while (res) {
            res = _anj_io_out_ctx_get_payload(
                    &env.ctx, &env.buf[env.out_length], chunk_len, &out_len);
            ANJ_UNIT_ASSERT_TRUE(res == 0 || res == ANJ_IO_NEED_NEXT_CALL);
            env.out_length += out_len;
        }
        // {3: {3: {3: 1.1}}}
        VERIFY_BYTES(env, "\xBF\x03\xBF\x03\xBF\x03"
                          "\xFB\x3F\xF1\x99\x99\x99\x99\x99\x9A"
                          "\xFF\xFF\xFF");
    }
}

#    ifdef ANJ_WITH_EXTERNAL_DATA
static bool opened;
static bool closed;

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
#    endif // ANJ_WITH_EXTERNAL_DATA

static anj_io_out_entry_t entries[] = {
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
        .type = ANJ_DATA_TYPE_TIME,
        .value.time_value = 3,
    },
    {
        .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 3),
        .type = ANJ_DATA_TYPE_STRING,
        .value.bytes_or_string.data =
                "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
    },
    {
        .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 4),
        .type = ANJ_DATA_TYPE_BYTES,
        .value.bytes_or_string.data =
                "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
        .value.bytes_or_string.chunk_length = 50
    },
    {
        .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 5),
        .type = ANJ_DATA_TYPE_BOOL,
        .value.bool_value = false
    },
    {
        .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 6),
        .type = ANJ_DATA_TYPE_OBJLNK,
        .value.objlnk.oid = 17,
        .value.objlnk.iid = 18,
    },
#    ifdef ANJ_WITH_EXTERNAL_DATA
    {
        .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 8),
        .type = ANJ_DATA_TYPE_EXTERNAL_STRING,
        .value.external_data.get_external_data = external_data_handler,
        .value.external_data.open_external_data = external_data_open,
        .value.external_data.close_external_data = external_data_close
    },
#    endif // ANJ_WITH_EXTERNAL_DATA
    {
        .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 7),
        .type = ANJ_DATA_TYPE_DOUBLE,
        .value.double_value = 1.1
    }
};

static char ext_data[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// {8: {8: {
// 0: 25,
// 1: 100,
// 2: 1(3),
// 3: "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
// 4:
// h'44444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444',
// 5: false, 6: "17:18",
// 8: "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEE",  7: 1.1,
// }}}
static char encoded_entries[] =
        "\xBF\x08\xBF\x08\xBF\x00"
        "\x18\x19"
        "\x01\x18\x64"
        "\x02\xC1\x03"
        "\x03\x78\x32"
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        "\x04\x58\x32"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "\x05\xF4"
        "\x06\x65\x31\x37\x3A\x31\x38"
#    ifdef ANJ_WITH_EXTERNAL_DATA
        "\x08"
        "\x7F"
        "\x77"
        "ABCDEFGHIJKLMNOPQRSTUVW"
        "\x63"
        "XYZ"
        "\xFF"
#    endif // ANJ_WITH_EXTERNAL_DATA
        "\x07\xFB\x3F\xF1\x99\x99\x99\x99\x99\x9A"
        "\xFF\xFF\xFF";

ANJ_UNIT_TEST(lwm2m_cbor_encoder, all_data_types_notify_msg) {
    lwm2m_cbor_test_env_t env = { 0 };
#    ifdef ANJ_WITH_EXTERNAL_DATA
    ext_data_size = sizeof(ext_data) - 1;
    ptr_for_callback = ext_data;
    opened = false;
    closed = false;
#    endif // ANJ_WITH_EXTERNAL_DATA
    lwm2m_cbor_test_setup(&env, NULL, ANJ_ARRAY_SIZE(entries),
                          ANJ_OP_INF_NON_CON_NOTIFY);

    for (size_t i = 0; i < ANJ_ARRAY_SIZE(entries); i++) {
        size_t out_len = 0;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_out_ctx_new_entry(&env.ctx, &entries[i]));
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
                &env.ctx, &env.buf[env.out_length],
                env.buffer_length - env.out_length, &out_len));
        env.out_length += out_len;
    }

    VERIFY_BYTES(env, encoded_entries);
#    ifdef ANJ_WITH_EXTERNAL_DATA
    ANJ_UNIT_ASSERT_TRUE(closed);
#    endif // ANJ_WITH_EXTERNAL_DATA
}
static char encoded_entries_chunks[] =
        "\xBF\x08\xBF\x08\xBF\x00"
        "\x18\x19"
        "\x01\x18\x64"
        "\x02\xC1\x03"
        "\x03\x78" // 1 chunk
        "\x32"
        "XXXXXXXXXXXXXXX"  // 2 chunk
        "XXXXXXXXXXXXXXXX" // 3 chunk
        "XXXXXXXXXXXXXXXX" // 4 chunk
        "XXX"
        "\x04\x58\x32"
        "DDDDDDDDDD"       // 5 chunk
        "DDDDDDDDDDDDDDDD" // 6 chunk
        "DDDDDDDDDDDDDDDD" // 7 chunk
        "DDDDDDDD"
        "\x05\xF4"
        "\x06\x65\x31\x37\x3A\x31" // 8 chunk
        "\x38"
#    ifdef ANJ_WITH_EXTERNAL_DATA
        "\x08"
        "\x7F"
        "\x6B"
        "ABCDEFGHIJK"
        "\x60" // 9 chunk
        "\x6E"
        "LMNOPQRSTUVWXY"
        "\x60" // 10 chunk
        "\x61"
        "Z"
        "\xFF"
#    endif // ANJ_WITH_EXTERNAL_DATA
        "\x07\xFB\x3F\xF1\x99\x99\x99\x99\x99\x9A"
        "\xFF"      // 11 chunk
        "\xFF\xFF"; // 12 chunk

ANJ_UNIT_TEST(lwm2m_cbor_encoder, all_data_types_chunk_read) {
    lwm2m_cbor_test_env_t env = { 0 };
    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_INSTANCE_PATH(8, 8),
                          ANJ_ARRAY_SIZE(entries), ANJ_OP_DM_READ);
    size_t buff_len = 16;
#    ifdef ANJ_WITH_EXTERNAL_DATA
    ext_data_size = sizeof(ext_data) - 1;
    ptr_for_callback = ext_data;
    opened = false;
    closed = false;
#    endif // ANJ_WITH_EXTERNAL_DATA
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(entries); i++) {
        size_t out_len = 0;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_out_ctx_new_entry(&env.ctx, &entries[i]));
        int res = -1;
        while (res) {
            res = _anj_io_out_ctx_get_payload(
                    &env.ctx, &env.buf[env.out_length], buff_len, &out_len);
            buff_len -= out_len;
            if (buff_len == 0) {
                buff_len = 16;
            }
            ANJ_UNIT_ASSERT_TRUE(res == 0 || res == ANJ_IO_NEED_NEXT_CALL);
            env.out_length += out_len;
        }
    }
    VERIFY_BYTES(env, encoded_entries_chunks);
#    ifdef ANJ_WITH_EXTERNAL_DATA
    ANJ_UNIT_ASSERT_TRUE(closed);
#    endif // ANJ_WITH_EXTERNAL_DATA
}
#    ifdef ANJ_WITH_EXTERNAL_DATA
static bool opened2;
static int external_data_open2(void *user_args) {
    (void) user_args;
    ANJ_UNIT_ASSERT_FALSE(opened2);
    opened2 = true;

    return 0;
}

static bool closed2;
static void external_data_close2(void *user_args) {
    (void) user_args;
    ANJ_UNIT_ASSERT_FALSE(closed2);
    closed2 = true;
}

static anj_io_out_entry_t entries_extended[] = {
    {
        .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 7),
        .type = ANJ_DATA_TYPE_EXTERNAL_STRING,
        .value.external_data.get_external_data = external_data_handler,
        .value.external_data.open_external_data = external_data_open,
        .value.external_data.close_external_data = external_data_close
    },
    {
        .path = ANJ_MAKE_RESOURCE_PATH(8, 8, 8),
        .type = ANJ_DATA_TYPE_EXTERNAL_STRING,
        .value.external_data.get_external_data = external_data_handler,
        .value.external_data.open_external_data = external_data_open2,
        .value.external_data.close_external_data = external_data_close2
    }
};

static char encoded_ext_entries[] = "\xBF\x08\xBF\x08\xBF\x07"
                                    "\x7F"
                                    "\x77"
                                    "ABCDEFGHIJKLMNOPQRSTUVW"
                                    "\x63"
                                    "X"
                                    "\xC4\x87"
                                    "\xFF"
                                    "\x08"
                                    "\x7F"
                                    "\x77"
                                    "ABCDEFGHIJKLMNOPQRSTUVW"
                                    "\x63"
                                    "X"
                                    "\xC4\x87"
                                    "\xFF"
                                    "\xFF\xFF\xFF";

ANJ_UNIT_TEST(lwm2m_cbor_encoder, extended_type_at_the_end) {
    lwm2m_cbor_test_env_t env = { 0 };
    char ext_data[] = "ABCDEFGHIJKLMNOPQRSTUVWXć";
    ptr_for_callback = ext_data;
    lwm2m_cbor_test_setup(&env, NULL, ANJ_ARRAY_SIZE(entries_extended),
                          ANJ_OP_INF_NON_CON_NOTIFY);
    opened = false;
    opened2 = false;
    closed = false;
    closed2 = false;

    for (size_t i = 0; i < ANJ_ARRAY_SIZE(entries_extended); i++) {
        size_t out_len = 0;
        ext_data_size = sizeof(ext_data) - 1;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_out_ctx_new_entry(&env.ctx, &entries_extended[i]));
        if (i == 0) {
            ANJ_UNIT_ASSERT_TRUE(opened);
        } else if (i == 1) {
            ANJ_UNIT_ASSERT_TRUE(opened2);
        }
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
                &env.ctx, &env.buf[env.out_length],
                env.buffer_length - env.out_length, &out_len));
        env.out_length += out_len;
        if (i == 0) {
            ANJ_UNIT_ASSERT_TRUE(closed);
        } else if (i == 1) {
            ANJ_UNIT_ASSERT_TRUE(closed2);
        }
    }
    VERIFY_BYTES(env, encoded_ext_entries);
}
#    endif // ANJ_WITH_EXTERNAL_DATA

ANJ_UNIT_TEST(lwm2m_cbor_encoder, errors) {
    lwm2m_cbor_test_env_t env = { 0 };

    anj_io_out_entry_t entry_1 = {
        .path = ANJ_MAKE_RESOURCE_PATH(3, 3, 3),
        .type = ANJ_DATA_TYPE_UINT,
        .value.uint_value = 25
    };
    anj_io_out_entry_t entry_2 = {
        .path = ANJ_MAKE_RESOURCE_PATH(1, 1, 1),
        .type = ANJ_DATA_TYPE_UINT,
        .value.int_value = 11
    };
    // one entry allowed
    lwm2m_cbor_test_setup(&env, NULL, 1, ANJ_OP_INF_CON_SEND);
    size_t out_len = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_get_payload(
            &env.ctx, env.buf, env.buffer_length, &out_len));
    env.out_length += out_len;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2),
                          _ANJ_IO_ERR_LOGIC);

    // _anj_io_out_ctx_get_payload not called
    lwm2m_cbor_test_setup(&env, NULL, 2, ANJ_OP_INF_CON_SEND);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1));
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_out_ctx_new_entry(&env.ctx, &entry_2),
                          _ANJ_IO_ERR_LOGIC);

    // path outside of the base path
    lwm2m_cbor_test_setup(&env, &ANJ_MAKE_INSTANCE_PATH(8, 8), 1,
                          ANJ_OP_DM_READ);
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_out_ctx_new_entry(&env.ctx, &entry_1),
                          _ANJ_IO_ERR_INPUT_ARG);
}

#endif // ANJ_WITH_LWM2M_CBOR
