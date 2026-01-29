/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/crypto.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/security_object.h>
#include <anj/persistence.h>
#include <anj/utils.h>

#include "../../../../src/anj/dm/dm_io.h"
#include "../../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_DEFAULT_SECURITY_OBJ

enum security_resources {
    RID_SERVER_URI = 0,
    RID_BOOTSTRAP_SERVER = 1,
    RID_SECURITY_MODE = 2,
    RID_PUBLIC_KEY_OR_IDENTITY = 3,
    RID_SERVER_PUBLIC_KEY = 4,
    RID_SECRET_KEY = 5,
    RID_SSID = 10,
    RID_CLIENT_HOLD_OFF_TIME = 11,
};

#    define RESOURCE_CHECK_INT(Iid, SecInstElement, ExpectedValue) \
        ANJ_UNIT_ASSERT_EQUAL(SecInstElement, ExpectedValue);

#    define RESOURCE_CHECK_BYTES(Iid, SecInstElement, ExpectedValue,     \
                                 ExpectedValueLen)                       \
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(SecInstElement, ExpectedValue, \
                                          ExpectedValueLen);

#    define RESOURCE_CHECK_STRING(Iid, SecInstElement, ExpectedValue) \
        RESOURCE_CHECK_BYTES(Iid, SecInstElement, ExpectedValue,      \
                             sizeof(ExpectedValue) - 1)

#    define RESOURCE_CHECK_BOOL(Iid, SecInstElement, ExpectedValue) \
        RESOURCE_CHECK_INT(Iid, SecInstElement, ExpectedValue)

#    define INIT_ENV()                 \
        anj_t anj = { 0 };             \
        anj_dm_security_obj_t sec_obj; \
        _anj_dm_initialize(&anj);      \
        anj_dm_security_obj_init(&sec_obj);

#    define PUBLIC_KEY_OR_IDENTITY_1 "public_key"
#    define SERVER_PUBLIC_KEY_1 \
        "server"                \
        "\x00\x01"              \
        "key"
#    define SECRET_KEY_1 "\x55\x66\x77\x88"

#    define PUBLIC_KEY_OR_IDENTITY_2 "advanced_public_key"
#    define SERVER_PUBLIC_KEY_2 \
        "server"                \
        "\x00\x02\x03"          \
        "key"
#    define SECRET_KEY_2 "\x99\x88\x77\x66\x55"

static char g_mock_identity[3];
static char g_mock_buffer[3][256];
static size_t g_mock_buffer_len[3];
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
static int g_mock_identity_counter;
// ANJ_WITH_CRYPTO_STORAGE_DEFAULT is not set so we have to provide custom
// integration layer

int anj_crypto_storage_init(void **out_crypto_ctx) {
    *out_crypto_ctx = NULL;
    return 0;
}
void anj_crypto_storage_deinit(void *out_crypto_ctx) {
    (void) out_crypto_ctx;
}
int anj_crypto_storage_create_new_record(void *crypto_ctx,
                                         anj_crypto_security_info_t *out_info) {
    (void) crypto_ctx;
    out_info->info.external.identity =
            &g_mock_identity[g_mock_identity_counter++];
    return 0;
}
int anj_crypto_storage_store_data(void *crypto_ctx,
                                  const anj_crypto_security_info_t *info,
                                  const void *data,
                                  size_t data_size,
                                  bool last_chunk) {
    (void) crypto_ctx;
    (void) info;
    (void) data;
    (void) data_size;
    ANJ_UNIT_ASSERT_TRUE(last_chunk);
    g_mock_buffer_len[g_mock_identity_counter - 1] = data_size;
    memcpy(g_mock_buffer[g_mock_identity_counter - 1], data, data_size);
    return 0;
}
int anj_crypto_storage_delete_record(void *crypto_ctx,
                                     const anj_crypto_security_info_t *info) {
    (void) crypto_ctx;
    (void) info;
    return 0;
}

static char identity_info[3][11] = { "1_identity", "2_identity", "3_identity" };
static const char *identity_info_ptrs[3];
static int given_identity_read_index;
static int given_identity_write_index;
int anj_crypto_storage_get_persistence_info(
        void *crypto_ctx,
        const anj_crypto_security_info_external_t *info,
        void *out_data,
        size_t *out_data_size) {
    (void) crypto_ctx;
    (void) info;
    (void) out_data;
    (void) out_data_size;
    // for first call return 1 byte, 2 for second..
    given_identity_read_index++;
    *out_data_size = given_identity_read_index;
    memcpy(out_data, identity_info[given_identity_read_index - 1],
           given_identity_read_index);
    identity_info_ptrs[given_identity_read_index - 1] = info->identity;
    return 0;
}
int anj_crypto_storage_resolve_persistence_info(
        void *crypto_ctx,
        const void *data,
        size_t data_size,
        anj_crypto_security_info_external_t *out_info) {
    (void) crypto_ctx;
    (void) data;
    (void) data_size;
    given_identity_write_index++;
    // check if given persistence info is equal to previously returned info
    ANJ_UNIT_ASSERT_EQUAL(given_identity_write_index, data_size);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
            identity_info[given_identity_write_index - 1], data, data_size);
    // return identity pointer
    out_info->identity = identity_info_ptrs[given_identity_write_index - 1];
    return 0;
}
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

ANJ_UNIT_TEST(dm_security_object, check_resources_values) {
    INIT_ENV();

    anj_dm_security_instance_init_t inst_1 = {
        .server_uri = "coap://server.com:5683",
        .bootstrap_server = true,
        .security_mode = 1,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = PUBLIC_KEY_OR_IDENTITY_1,
            .info.buffer.data_size = sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1
        },
        .server_public_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = SERVER_PUBLIC_KEY_1,
            .info.buffer.data_size = sizeof(SERVER_PUBLIC_KEY_1) - 1
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = SECRET_KEY_1,
            .info.buffer.data_size = sizeof(SECRET_KEY_1) - 1
        }
    };
    anj_dm_security_instance_init_t inst_2 = {
        .server_uri = "coaps://server.com:5684",
        .bootstrap_server = false,
        .security_mode = 2,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = PUBLIC_KEY_OR_IDENTITY_2,
            .info.buffer.data_size = sizeof(PUBLIC_KEY_OR_IDENTITY_2) - 1
        },
        .server_public_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = SERVER_PUBLIC_KEY_2,
            .info.buffer.data_size = sizeof(SERVER_PUBLIC_KEY_2) - 1
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = SECRET_KEY_2,
            .info.buffer.data_size = sizeof(SECRET_KEY_2) - 1
        },
        .ssid = 2,
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_1));
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_2));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));

    RESOURCE_CHECK_STRING(0, sec_obj.security_instances[0].server_uri,
                          "coap://server.com:5683");
    RESOURCE_CHECK_BOOL(0, sec_obj.security_instances[0].bootstrap_server,
                        true);
    RESOURCE_CHECK_INT(0, sec_obj.security_instances[0].security_mode, 1);
    RESOURCE_CHECK_BYTES(
            0,
            sec_obj.security_instances[0].public_key_or_identity_buff,
            PUBLIC_KEY_OR_IDENTITY_1,
            sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[0].public_key_or_identity.source,
            ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[0]
                                  .public_key_or_identity.info.buffer.data_size,
                          sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[0]
                    .public_key_or_identity.info.buffer.data
            == sec_obj.security_instances[0].public_key_or_identity_buff);
    RESOURCE_CHECK_BYTES(0,
                         sec_obj.security_instances[0].server_public_key_buff,
                         SERVER_PUBLIC_KEY_1, sizeof(SERVER_PUBLIC_KEY_1) - 1);
    RESOURCE_CHECK_BYTES(0, sec_obj.security_instances[0].secret_key_buff,
                         SECRET_KEY_1, sizeof(SECRET_KEY_1) - 1);
    RESOURCE_CHECK_INT(0, sec_obj.security_instances[0].ssid,
                       _ANJ_SSID_BOOTSTRAP);
    RESOURCE_CHECK_STRING(1, sec_obj.security_instances[1].server_uri,
                          "coaps://server.com:5684");
    RESOURCE_CHECK_BOOL(1, sec_obj.security_instances[1].bootstrap_server,
                        false);
    RESOURCE_CHECK_INT(1, sec_obj.security_instances[1].security_mode, 2);
    RESOURCE_CHECK_BYTES(
            1,
            sec_obj.security_instances[1].public_key_or_identity_buff,
            PUBLIC_KEY_OR_IDENTITY_2,
            sizeof(PUBLIC_KEY_OR_IDENTITY_2) - 1);
    RESOURCE_CHECK_BYTES(1,
                         sec_obj.security_instances[1].server_public_key_buff,
                         SERVER_PUBLIC_KEY_2, sizeof(SERVER_PUBLIC_KEY_2) - 1);
    RESOURCE_CHECK_BYTES(1, sec_obj.security_instances[1].secret_key_buff,
                         SECRET_KEY_2, sizeof(SECRET_KEY_2) - 1);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[1].server_public_key.source,
            ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[1]
                                  .server_public_key.info.buffer.data_size,
                          sizeof(SERVER_PUBLIC_KEY_2) - 1);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[1].server_public_key.info.buffer.data
            == sec_obj.security_instances[1].server_public_key_buff);
    RESOURCE_CHECK_INT(1, sec_obj.security_instances[1].ssid, 2);
}

ANJ_UNIT_TEST(dm_security_object, create_instance_minimal) {
    INIT_ENV();

    anj_dm_security_instance_init_t inst_1 = {
        .server_uri = "coap://server.com:5683",
        .ssid = 1,
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_1));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_CREATE, true,
            &ANJ_MAKE_OBJECT_PATH(ANJ_OBJ_ID_SECURITY)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, 20));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_STRING,
                .value.bytes_or_string.data = "coap://test.com:5684",
                .value.bytes_or_string.chunk_length =
                        sizeof("coap://test.com:5684") - 1,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 20,
                                               RID_SERVER_URI)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_write_entry(&anj,
                                &(anj_io_out_entry_t) {
                                    .type = ANJ_DATA_TYPE_INT,
                                    .value.int_value = 7,
                                    .path = ANJ_MAKE_RESOURCE_PATH(
                                            ANJ_OBJ_ID_SECURITY, 20, RID_SSID)
                                }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    RESOURCE_CHECK_STRING(0, sec_obj.security_instances[0].server_uri,
                          "coap://server.com:5683");
    RESOURCE_CHECK_INT(0, sec_obj.security_instances[0].ssid, 1);
    RESOURCE_CHECK_STRING(20, sec_obj.security_instances[1].server_uri,
                          "coap://test.com:5684");
    RESOURCE_CHECK_BOOL(20, sec_obj.security_instances[1].bootstrap_server,
                        false);
    RESOURCE_CHECK_INT(20, sec_obj.security_instances[1].security_mode, 0);
    RESOURCE_CHECK_INT(20, sec_obj.security_instances[1].ssid, 7);
}

ANJ_UNIT_TEST(dm_security_object, create_instance) {
    INIT_ENV();

    anj_dm_security_instance_init_t inst_1 = {
        .server_uri = "coap://server.com:5683",
        .ssid = 1,
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_1));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_CREATE, true,
            &ANJ_MAKE_OBJECT_PATH(ANJ_OBJ_ID_SECURITY)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, 20));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_STRING,
                .value.bytes_or_string.data = "coap://test.com:5683",
                .value.bytes_or_string.chunk_length =
                        sizeof("coap://test.com:5683") - 1,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 20,
                                               RID_SERVER_URI)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_BOOL,
                .value.bool_value = true,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 20,
                                               RID_BOOTSTRAP_SERVER)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 1,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 20,
                                               RID_SECURITY_MODE)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_BYTES,
                .value.bytes_or_string.data = PUBLIC_KEY_OR_IDENTITY_1,
                .value.bytes_or_string.chunk_length =
                        sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1,
                .value.bytes_or_string.full_length_hint =
                        sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 20,
                                               RID_PUBLIC_KEY_OR_IDENTITY)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_BYTES,
                .value.bytes_or_string.data = SERVER_PUBLIC_KEY_1,
                .value.bytes_or_string.chunk_length =
                        sizeof(SERVER_PUBLIC_KEY_1) - 1,
                .value.bytes_or_string.full_length_hint =
                        sizeof(SERVER_PUBLIC_KEY_1) - 1,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 20,
                                               RID_SERVER_PUBLIC_KEY)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_BYTES,
                .value.bytes_or_string.data = SECRET_KEY_1,
                .value.bytes_or_string.chunk_length = sizeof(SECRET_KEY_1) - 1,
                .value.bytes_or_string.full_length_hint =
                        sizeof(SECRET_KEY_1) - 1,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 20,
                                               RID_SECRET_KEY)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_write_entry(&anj,
                                &(anj_io_out_entry_t) {
                                    .type = ANJ_DATA_TYPE_INT,
                                    .value.int_value = 7,
                                    .path = ANJ_MAKE_RESOURCE_PATH(
                                            ANJ_OBJ_ID_SECURITY, 20, RID_SSID)
                                }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 17,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 20,
                                               RID_CLIENT_HOLD_OFF_TIME)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    RESOURCE_CHECK_STRING(0, sec_obj.security_instances[0].server_uri,
                          "coap://server.com:5683");
    RESOURCE_CHECK_INT(0, sec_obj.security_instances[0].ssid, 1);
    RESOURCE_CHECK_STRING(20, sec_obj.security_instances[1].server_uri,
                          "coap://test.com:5683");
    RESOURCE_CHECK_BOOL(20, sec_obj.security_instances[1].bootstrap_server,
                        true);
    RESOURCE_CHECK_INT(20, sec_obj.security_instances[1].security_mode, 1);
    RESOURCE_CHECK_INT(20, sec_obj.security_instances[1].ssid, 7);
    RESOURCE_CHECK_INT(20, sec_obj.security_instances[1].client_hold_off_time,
                       17);

#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    // check external resources
    // check source
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[1].public_key_or_identity.source,
            ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[1].server_public_key.source,
            ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[1].secret_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    // check identity pointers
    ANJ_UNIT_ASSERT_TRUE(sec_obj.security_instances[1]
                                 .public_key_or_identity.info.external.identity
                         == &g_mock_identity[0]);
    ANJ_UNIT_ASSERT_TRUE(sec_obj.security_instances[1]
                                 .server_public_key.info.external.identity
                         == &g_mock_identity[1]);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[1].secret_key.info.external.identity
            == &g_mock_identity[2]);
    // check provided data and data size
    ANJ_UNIT_ASSERT_EQUAL(g_mock_buffer_len[0],
                          sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(g_mock_buffer_len[1],
                          sizeof(SERVER_PUBLIC_KEY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(g_mock_buffer_len[2], sizeof(SECRET_KEY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(g_mock_buffer[0],
                                      PUBLIC_KEY_OR_IDENTITY_1,
                                      sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(g_mock_buffer[1],
                                      SERVER_PUBLIC_KEY_1,
                                      sizeof(SERVER_PUBLIC_KEY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(g_mock_buffer[2], SECRET_KEY_1,
                                      sizeof(SECRET_KEY_1) - 1);
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
}

ANJ_UNIT_TEST(dm_security_object, delete_instance) {
    INIT_ENV();

    anj_dm_security_instance_init_t inst_1 = {
        .server_uri = "coap://server.com:5683",
        .ssid = 1,
    };
    anj_dm_security_instance_init_t inst_2 = {
        .server_uri = "coaps://server.com:5684",
        .ssid = 2,
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_1));
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_2));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_DELETE, true,
            &ANJ_MAKE_INSTANCE_PATH(ANJ_OBJ_ID_SECURITY, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.obj.insts[1].iid, ANJ_ID_INVALID);

    RESOURCE_CHECK_STRING(1, sec_obj.security_instances[1].server_uri,
                          "coaps://server.com:5684");
    RESOURCE_CHECK_INT(1, sec_obj.security_instances[1].ssid, 2);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_DELETE, true,
            &ANJ_MAKE_INSTANCE_PATH(ANJ_OBJ_ID_SECURITY, 1)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_validate(&anj));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_EQUAL(sec_obj.inst[0].iid, ANJ_ID_INVALID);
}

ANJ_UNIT_TEST(dm_security_object, errors) {
    INIT_ENV();

    anj_dm_security_instance_init_t inst_1 = {
        .server_uri = "coap://server.com:5683",
        .ssid = 1,
    };
    anj_dm_security_instance_init_t inst_2 = {
        .server_uri = "coaps://server.com:5684",
        .ssid = 1,
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_1));
    // ssid duplication
    ANJ_UNIT_ASSERT_FAILED(anj_dm_security_obj_add_instance(&sec_obj, &inst_2));

    anj_dm_security_instance_init_t inst_4 = {
        .server_uri = "coap://test.com:5683",
        .ssid = 2,
        .security_mode = 5
    };
    // invalid security mode
    ANJ_UNIT_ASSERT_FAILED(anj_dm_security_obj_add_instance(&sec_obj, &inst_4));

    anj_dm_security_instance_init_t inst_5 = {
        .server_uri = "coap://test.com:5683",
        .ssid = 2,
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_5));

    anj_dm_security_instance_init_t inst_6 = {
        .server_uri = "coap://test.com:5684",
        .ssid = 3,
    };
    // max instances reached
    ANJ_UNIT_ASSERT_FAILED(anj_dm_security_obj_add_instance(&sec_obj, &inst_6));

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, true,
            &ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 0, 2)));
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 5,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 0,
                                               RID_SECURITY_MODE)
            }));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
}

ANJ_UNIT_TEST(dm_security_object, get_psk_check) {
    INIT_ENV();

    anj_dm_security_instance_init_t inst_1 = {
        .ssid = 1,
        .bootstrap_server = false,
        .server_uri = "coaps://dddd:777",
        .security_mode = ANJ_DM_SECURITY_PSK,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "ddd",
            .info.buffer.data_size = strlen("ddd")
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "eee",
            .info.buffer.data_size = strlen("eee")
        }
    };
    anj_dm_security_instance_init_t inst_2 = {
        .ssid = 2,
        .bootstrap_server = true,
        .server_uri = "coaps://dddd:777",
        .security_mode = ANJ_DM_SECURITY_PSK,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "public",
            .info.buffer.data_size = strlen("public")
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "secret",
            .info.buffer.data_size = strlen("secret")
        }
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_1));
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_2));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));

    anj_crypto_security_info_t psk_identity;
    anj_crypto_security_info_t psk_key;
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_get_psk(&anj, true, &psk_identity, &psk_key));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(psk_identity.info.buffer.data, "public",
                                      strlen("public"));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(psk_key.info.buffer.data, "secret",
                                      strlen("secret"));
    ANJ_UNIT_ASSERT_EQUAL(psk_identity.tag,
                          ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY);
    ANJ_UNIT_ASSERT_EQUAL(psk_key.tag, ANJ_CRYPTO_SECURITY_TAG_PSK_KEY);

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_get_psk(&anj, false, &psk_identity, &psk_key));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(psk_identity.info.buffer.data, "ddd",
                                      strlen("ddd"));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(psk_key.info.buffer.data, "eee",
                                      strlen("eee"));
    ANJ_UNIT_ASSERT_EQUAL(psk_identity.tag,
                          ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY);
    ANJ_UNIT_ASSERT_EQUAL(psk_key.tag, ANJ_CRYPTO_SECURITY_TAG_PSK_KEY);
}

///////////////////////////////////////////////////////////////////////
////////////////////////// PERSISTENCE TESTS //////////////////////////
///////////////////////////////////////////////////////////////////////

static uint8_t g_membuf_data[1024];
static size_t g_membuf_write_offset;
static size_t g_membuf_read_offset;
static bool g_membuf_read_error_enabled;
static int g_membuf_read_error_countdown;

static int mem_write_cb(void *ctx, const void *buf, size_t size) {
    (void) ctx;
    if (size == 0) {
        return 0;
    }
    if (g_membuf_write_offset + size > sizeof(g_membuf_data)) {
        return -1;
    }
    memcpy(g_membuf_data + g_membuf_write_offset, buf, size);
    g_membuf_write_offset += size;
    return 0;
}
static int mem_read_cb(void *ctx, void *buf, size_t size) {
    (void) ctx;
    if (size == 0) {
        return 0;
    }

    if (g_membuf_read_error_countdown) {
        g_membuf_read_error_countdown--;
    }
    if (g_membuf_read_error_enabled && g_membuf_read_error_countdown == 0) {
        return -1;
    }

    if (g_membuf_read_offset + size > g_membuf_write_offset) {
        return -1;
    }
    memcpy(buf, g_membuf_data + g_membuf_read_offset, size);
    g_membuf_read_offset += size;
    return 0;
}

#    define INIT_ENV_PERSISTENCE(Anj, Sec_obj) \
        anj_t Anj = { 0 };                     \
        anj_dm_security_obj_t Sec_obj;         \
        _anj_dm_initialize(&Anj);              \
        anj_dm_security_obj_init(&Sec_obj);    \
        g_membuf_read_offset = 0;              \
        g_membuf_write_offset = 0;

// we can't compare whole object, because it contains pointers to instances
// with different addresses
static void compare_objects_after_persistence(anj_dm_security_obj_t *obj1,
                                              anj_dm_security_obj_t *obj2) {
    ANJ_UNIT_ASSERT_EQUAL(g_membuf_read_offset, g_membuf_write_offset);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(&obj1->security_instances[0],
                                      &obj2->security_instances[0],
                                      sizeof(anj_dm_security_instance_t));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(&obj1->security_instances[1],
                                      &obj2->security_instances[1],
                                      sizeof(anj_dm_security_instance_t));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(&obj1->inst[0], &obj2->inst[0],
                                      sizeof(anj_dm_obj_inst_t));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(&obj1->inst[1], &obj2->inst[1],
                                      sizeof(anj_dm_obj_inst_t));
}

static void persistence_store(anj_t *anj,
                              anj_dm_security_obj_t *sec_obj,
                              anj_dm_security_instance_init_t *inst) {
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_add_instance(sec_obj, inst));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(anj, sec_obj));
    anj_persistence_context_t ctx =
            anj_persistence_store_context_create(mem_write_cb, NULL);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_store(anj, sec_obj, &ctx));
}

static void persistence_restore(anj_t *anj, anj_dm_security_obj_t *sec_obj) {
    anj_persistence_context_t ctx =
            anj_persistence_restore_context_create(mem_read_cb, NULL);
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_restore(anj, sec_obj, &ctx));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(anj, sec_obj));
}

ANJ_UNIT_TEST(dm_security_object, persistence_no_sec_basic) {
    INIT_ENV_PERSISTENCE(anj, sec_obj);
    // create second object and restore data here, in the end objects should be
    // identical object should be identical
    INIT_ENV_PERSISTENCE(anj_2, sec_obj_2);

    anj_dm_security_instance_init_t inst = {
        .ssid = 1,
        .bootstrap_server = false,
        .server_uri = "coap://dddd:777",
        .security_mode = ANJ_DM_SECURITY_NOSEC,
        .client_hold_off_time = 0,
    };
    persistence_store(&anj, &sec_obj, &inst);
    persistence_restore(&anj_2, &sec_obj_2);

    compare_objects_after_persistence(&sec_obj, &sec_obj_2);
}

#    define CLEAR_CRYPTO_BUFFER_POINTERS(Inst)                \
        Inst = &sec_obj_2.security_instances[0];              \
        Inst->public_key_or_identity.info.buffer.data = NULL; \
        Inst->secret_key.info.buffer.data = NULL;             \
        Inst->server_public_key.info.buffer.data = NULL;      \
        Inst = &sec_obj_2.security_instances[1];              \
        Inst->public_key_or_identity.info.buffer.data = NULL; \
        Inst->secret_key.info.buffer.data = NULL;             \
        Inst->server_public_key.info.buffer.data = NULL;      \
        Inst = &sec_obj.security_instances[0];                \
        Inst->public_key_or_identity.info.buffer.data = NULL; \
        Inst->secret_key.info.buffer.data = NULL;             \
        Inst->server_public_key.info.buffer.data = NULL;      \
        Inst = &sec_obj.security_instances[1];                \
        Inst->public_key_or_identity.info.buffer.data = NULL; \
        Inst->secret_key.info.buffer.data = NULL;             \
        Inst->server_public_key.info.buffer.data = NULL;

ANJ_UNIT_TEST(dm_security_object, persistence_psk_instance) {
    INIT_ENV_PERSISTENCE(anj, sec_obj);
    INIT_ENV_PERSISTENCE(anj_2, sec_obj_2);

    anj_dm_security_instance_init_t inst_1 = {
        .ssid = 1,
        .bootstrap_server = false,
        .server_uri = "coaps://dddd:777",
        .security_mode = ANJ_DM_SECURITY_PSK,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "public",
            .info.buffer.data_size = strlen("public")
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "secret",
            .info.buffer.data_size = strlen("secret")
        }
    };
    persistence_store(&anj, &sec_obj, &inst_1);
    persistence_restore(&anj_2, &sec_obj_2);

    // before check handle crypto data pointers - check it and reset
    anj_dm_security_instance_t *inst = &sec_obj_2.security_instances[0];
    ANJ_UNIT_ASSERT_TRUE(inst->public_key_or_identity.info.buffer.data
                         == inst->public_key_or_identity_buff);
    ANJ_UNIT_ASSERT_TRUE(inst->secret_key.info.buffer.data
                         == inst->secret_key_buff);
    CLEAR_CRYPTO_BUFFER_POINTERS(inst);

    compare_objects_after_persistence(&sec_obj, &sec_obj_2);
}

ANJ_UNIT_TEST(dm_security_object, persistence_psk_two_instances) {
    INIT_ENV_PERSISTENCE(anj, sec_obj);
    INIT_ENV_PERSISTENCE(anj_2, sec_obj_2);

    anj_dm_security_instance_init_t inst_1 = {
        .ssid = 1,
        .bootstrap_server = false,
        .server_uri = "coaps://dddd:777",
        .security_mode = ANJ_DM_SECURITY_PSK,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "public",
            .info.buffer.data_size = strlen("public")
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "secret",
            .info.buffer.data_size = strlen("secret")
        }
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_security_obj_add_instance(&sec_obj, &inst_1));
    anj_dm_security_instance_init_t inst_2 = {
        .ssid = 2,
        .bootstrap_server = true,
        .server_uri = "coaps://dddd:777",
        .security_mode = ANJ_DM_SECURITY_PSK,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "wwwww",
            .info.buffer.data_size = strlen("wwwww")
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "ddd",
            .info.buffer.data_size = strlen("ddd")
        }
    };
    persistence_store(&anj, &sec_obj, &inst_2);
    persistence_restore(&anj_2, &sec_obj_2);

    anj_dm_security_instance_t *inst = &sec_obj_2.security_instances[0];
    CLEAR_CRYPTO_BUFFER_POINTERS(inst);

    compare_objects_after_persistence(&sec_obj, &sec_obj_2);
}

#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
ANJ_UNIT_TEST(dm_security_object, persistence_external_psk_instance) {
    INIT_ENV_PERSISTENCE(anj, sec_obj);
    INIT_ENV_PERSISTENCE(anj_2, sec_obj_2);

    anj_dm_security_instance_init_t inst_1 = {
        .ssid = 1,
        .bootstrap_server = false,
        .server_uri = "coaps://dddd:777",
        .security_mode = ANJ_DM_SECURITY_PSK,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_EXTERNAL,
            .info.external.identity = "public"
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_EXTERNAL,
            .info.external.identity = "secret"
        }
    };
    persistence_store(&anj, &sec_obj, &inst_1);
    persistence_restore(&anj_2, &sec_obj_2);

    compare_objects_after_persistence(&sec_obj, &sec_obj_2);
}
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

ANJ_UNIT_TEST(dm_security_object, persistence_wrong_magic) {
    INIT_ENV_PERSISTENCE(anj, sec_obj);
    INIT_ENV_PERSISTENCE(anj_2, sec_obj_2);

    anj_dm_security_instance_init_t inst = {
        .ssid = 1,
        .bootstrap_server = false,
        .server_uri = "coap://dddd:777",
        .security_mode = ANJ_DM_SECURITY_NOSEC,
        .client_hold_off_time = 0,
    };
    persistence_store(&anj, &sec_obj, &inst);

    anj_persistence_context_t ctx_2 =
            anj_persistence_restore_context_create(mem_read_cb, NULL);
    // corrupt magic
    g_membuf_data[2] = 'X';
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_security_obj_restore(&anj_2, &sec_obj_2, &ctx_2));
}

ANJ_UNIT_TEST(dm_security_object, persistence_no_instance_error) {
    INIT_ENV_PERSISTENCE(anj, sec_obj);
    INIT_ENV_PERSISTENCE(anj_2, sec_obj_2);

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_install(&anj, &sec_obj));
    anj_persistence_context_t ctx =
            anj_persistence_store_context_create(mem_write_cb, NULL);
    ANJ_UNIT_ASSERT_FAILED(anj_dm_security_obj_store(&anj, &sec_obj, &ctx));
}

ANJ_UNIT_TEST(dm_security_object, persistence_restore_error) {
    INIT_ENV_PERSISTENCE(anj, sec_obj);
    INIT_ENV_PERSISTENCE(anj_2, sec_obj_2);

    anj_dm_security_instance_init_t inst = {
        .ssid = 1,
        .server_uri = "coap://dddd:777",
        .security_mode = ANJ_DM_SECURITY_NOSEC,
    };
    persistence_store(&anj, &sec_obj, &inst);

    anj_persistence_context_t ctx_2 =
            anj_persistence_restore_context_create(mem_read_cb, NULL);
    // simulate read error
    g_membuf_read_error_enabled = true;
    g_membuf_read_error_countdown = 10; // fail during instance_persistence()
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_security_obj_restore(&anj_2, &sec_obj_2, &ctx_2));
    ANJ_UNIT_ASSERT_EQUAL(sec_obj_2.inst[0].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj_2.inst[1].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj_2.security_instances[0].iid, ANJ_ID_INVALID);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj_2.security_instances[1].iid, ANJ_ID_INVALID);
    g_membuf_read_error_enabled = false;
}

#endif // ANJ_WITH_DEFAULT_SECURITY_OBJ
