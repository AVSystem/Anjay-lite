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

#ifdef ANJ_WITH_CERTIFICATES
#    include <anj/compat/net/anj_net_api.h>
#endif // ANJ_WITH_CERTIFICATES

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

#    define RESOURCE_CHECK_INT(SecInstElement, ExpectedValue) \
        ANJ_UNIT_ASSERT_EQUAL(SecInstElement, ExpectedValue);

#    define RESOURCE_CHECK_BYTES(SecInstElement, ExpectedValue,          \
                                 ExpectedValueLen)                       \
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(SecInstElement, ExpectedValue, \
                                          ExpectedValueLen);

#    define RESOURCE_CHECK_STRING(SecInstElement, ExpectedValue) \
        RESOURCE_CHECK_BYTES(SecInstElement, ExpectedValue,      \
                             sizeof(ExpectedValue) - 1)

#    define RESOURCE_CHECK_BOOL(SecInstElement, ExpectedValue) \
        RESOURCE_CHECK_INT(SecInstElement, ExpectedValue)

#    define INIT_ENV()                      \
        anj_t anj = { 0 };                  \
        anj_dm_security_obj_t sec_obj;      \
        _anj_dm_initialize(&anj);           \
        anj_dm_security_obj_init(&sec_obj); \
        g_mock_identity_counter = 0;

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

static int g_mock_identity_counter;
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
static char g_mock_identity[12];
static char g_mock_buffer[12][256];
static size_t g_mock_buffer_len[12];
static int g_mock_create_fail_counter = -1;
// ANJ_WITH_CRYPTO_STORAGE_DEFAULT is not set so we have to provide custom
// integration layer

int anj_crypto_storage_init(void **out_crypto_ctx) {
    *out_crypto_ctx = NULL;
    return 0;
}
void anj_crypto_storage_deinit(void *out_crypto_ctx) {
    (void) out_crypto_ctx;
}
int anj_crypto_storage_create_record(void *crypto_ctx,
                                     anj_crypto_security_info_t *out_info,
                                     const void *data,
                                     size_t data_size) {
    (void) crypto_ctx;
    g_mock_buffer_len[g_mock_identity_counter] = data_size;
    memcpy(g_mock_buffer[g_mock_identity_counter], data, data_size);
    out_info->info.external.identity =
            &g_mock_identity[g_mock_identity_counter++];
    if (g_mock_create_fail_counter == g_mock_identity_counter) {
        return -1;
    }
    return 0;
}

int anj_crypto_storage_delete_record(void *crypto_ctx,
                                     const anj_crypto_security_info_t *info) {
    (void) crypto_ctx;
    (void) info;
    return 0;
}

static char identity_info[12][11] = { "1_identity",  "2_identity",
                                      "3_identity",  "4_identity",
                                      "5_identity",  "6_identity",
                                      "7_identity",  "8_identity",
                                      "9_identity",  "10_identity",
                                      "11_identity", "12_identity" };
static const char *identity_info_ptrs[12];
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

#    ifdef ANJ_WITH_CERTIFICATES
    anj_net_certificate_usage_t cert_usage = 1;
#    endif // ANJ_WITH_CERTIFICATES
    anj_dm_security_instance_init_t inst_1 = {
        .server_uri = "coap://server.com:5683",
        .bootstrap_server = true,
#    ifdef ANJ_WITH_CERTIFICATES
        .server_name_indication = "qwerty",
        .certificate_usage = &cert_usage,
#    endif // ANJ_WITH_CERTIFICATES
        .security_mode = 3,
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
        .security_mode = 0,
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

    RESOURCE_CHECK_STRING(sec_obj.security_instances[0].server_uri,
                          "coap://server.com:5683");
    RESOURCE_CHECK_BOOL(sec_obj.security_instances[0].bootstrap_server, true);
    RESOURCE_CHECK_INT(sec_obj.security_instances[0].security_mode, 3);
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[0].public_key_or_identity.source,
            ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[0].server_public_key.source,
            ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[0].secret_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_TRUE(sec_obj.security_instances[0]
                                 .public_key_or_identity.info.external.identity
                         == &g_mock_identity[0]);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[0].secret_key.info.external.identity
            == &g_mock_identity[1]);
    ANJ_UNIT_ASSERT_TRUE(sec_obj.security_instances[0]
                                 .server_public_key.info.external.identity
                         == &g_mock_identity[2]);
#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    RESOURCE_CHECK_BYTES(
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
    RESOURCE_CHECK_BYTES(sec_obj.security_instances[0].server_public_key_buff,
                         SERVER_PUBLIC_KEY_1,
                         sizeof(SERVER_PUBLIC_KEY_1) - 1);
    RESOURCE_CHECK_BYTES(sec_obj.security_instances[0].secret_key_buff,
                         SECRET_KEY_1,
                         sizeof(SECRET_KEY_1) - 1);
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

    RESOURCE_CHECK_INT(sec_obj.security_instances[0].ssid, _ANJ_SSID_BOOTSTRAP);
#    ifdef ANJ_WITH_CERTIFICATES
    RESOURCE_CHECK_STRING(sec_obj.security_instances[0].server_name_indication,
                          "qwerty");
    RESOURCE_CHECK_INT(sec_obj.security_instances[0].certificate_usage, 1);
#    endif // ANJ_WITH_CERTIFICATES
    RESOURCE_CHECK_STRING(sec_obj.security_instances[1].server_uri,
                          "coaps://server.com:5684");
    RESOURCE_CHECK_BOOL(sec_obj.security_instances[1].bootstrap_server, false);
    RESOURCE_CHECK_INT(sec_obj.security_instances[1].security_mode, 0);

#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[1].public_key_or_identity.source,
            ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[1].server_public_key.source,
            ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[1].secret_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_TRUE(sec_obj.security_instances[1]
                                 .public_key_or_identity.info.external.identity
                         == &g_mock_identity[3]);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[1].secret_key.info.external.identity
            == &g_mock_identity[4]);
    ANJ_UNIT_ASSERT_TRUE(sec_obj.security_instances[1]
                                 .server_public_key.info.external.identity
                         == &g_mock_identity[5]);
#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    RESOURCE_CHECK_BYTES(
            sec_obj.security_instances[1].public_key_or_identity_buff,
            PUBLIC_KEY_OR_IDENTITY_2,
            sizeof(PUBLIC_KEY_OR_IDENTITY_2) - 1);
    RESOURCE_CHECK_BYTES(sec_obj.security_instances[1].server_public_key_buff,
                         SERVER_PUBLIC_KEY_2,
                         sizeof(SERVER_PUBLIC_KEY_2) - 1);
    RESOURCE_CHECK_BYTES(sec_obj.security_instances[1].secret_key_buff,
                         SECRET_KEY_2,
                         sizeof(SECRET_KEY_2) - 1);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[1].server_public_key.source,
            ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[1]
                                  .server_public_key.info.buffer.data_size,
                          sizeof(SERVER_PUBLIC_KEY_2) - 1);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[1].server_public_key.info.buffer.data
            == sec_obj.security_instances[1].server_public_key_buff);
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

    RESOURCE_CHECK_INT(sec_obj.security_instances[1].ssid, 2);
#    ifdef ANJ_WITH_CERTIFICATES
    RESOURCE_CHECK_STRING(sec_obj.security_instances[1].server_name_indication,
                          "");
    // default value should be set
    RESOURCE_CHECK_INT(sec_obj.security_instances[1].certificate_usage, 3);
#    endif // ANJ_WITH_CERTIFICATES
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
            &anj, _ANJ_OP_DM_CREATE, true,
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
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_bootstrap_operation_validate(&anj));
    _anj_dm_bootstrap_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    RESOURCE_CHECK_STRING(sec_obj.security_instances[0].server_uri,
                          "coap://server.com:5683");
    RESOURCE_CHECK_INT(sec_obj.security_instances[0].ssid, 1);
    RESOURCE_CHECK_STRING(sec_obj.security_instances[1].server_uri,
                          "coap://test.com:5684");
    RESOURCE_CHECK_BOOL(sec_obj.security_instances[1].bootstrap_server, false);
    RESOURCE_CHECK_INT(sec_obj.security_instances[1].security_mode, 0);
    RESOURCE_CHECK_INT(sec_obj.security_instances[1].ssid, 7);
}
#    include <stdio.h>

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
            &anj, _ANJ_OP_DM_CREATE, true,
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
                .value.int_value = 3,
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
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_bootstrap_operation_validate(&anj));
    _anj_dm_bootstrap_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    RESOURCE_CHECK_STRING(sec_obj.security_instances[0].server_uri,
                          "coap://server.com:5683");
    RESOURCE_CHECK_INT(sec_obj.security_instances[0].ssid, 1);
    RESOURCE_CHECK_STRING(sec_obj.security_instances[1].server_uri,
                          "coap://test.com:5683");
    RESOURCE_CHECK_BOOL(sec_obj.security_instances[1].bootstrap_server, true);
    RESOURCE_CHECK_INT(sec_obj.security_instances[1].security_mode, 3);
    RESOURCE_CHECK_INT(sec_obj.security_instances[1].ssid, 7);
    RESOURCE_CHECK_INT(sec_obj.security_instances[1].client_hold_off_time, 17);

    // before anj_dm_security_obj_offload_keys_and_certs() is
    // called, sources should be set to buffer
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[1].public_key_or_identity.source,
            ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[1].server_public_key.source,
            ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[1].secret_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_BUFFER);
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    // move data to external storage to check if it works correctly.
    ANJ_UNIT_ASSERT_SUCCESS(
            anj.security_credential_handlers->offload_keys_and_certs(&anj));
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
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[1].secret_key.info.external.identity
            == &g_mock_identity[1]);
    ANJ_UNIT_ASSERT_TRUE(sec_obj.security_instances[1]
                                 .server_public_key.info.external.identity
                         == &g_mock_identity[2]);
    // check provided data and data size
    ANJ_UNIT_ASSERT_EQUAL(g_mock_buffer_len[0],
                          sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(g_mock_buffer_len[1], sizeof(SECRET_KEY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(g_mock_buffer_len[2],
                          sizeof(SERVER_PUBLIC_KEY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(g_mock_buffer[0],
                                      PUBLIC_KEY_OR_IDENTITY_1,
                                      sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(g_mock_buffer[1], SECRET_KEY_1,
                                      sizeof(SECRET_KEY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(g_mock_buffer[2],
                                      SERVER_PUBLIC_KEY_1,
                                      sizeof(SERVER_PUBLIC_KEY_1) - 1);
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
}

#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
// This test checks if anj_dm_security_obj_offload_keys_and_certs() correctly
// handles errors from crypto storage layer. In case of error, keys and certs
// should remain in their original place (buffers) and sources should not be
// changed to external.
ANJ_UNIT_TEST(dm_security_object, move_credentials_to_hsm_fails) {
    INIT_ENV();
    // fail during creation of second record
    g_mock_create_fail_counter = 2;

    anj_dm_security_instance_init_t inst = {
        .server_uri = "coap://server.com:5683",
        .bootstrap_server = true,
        .security_mode = 3,
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

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_security_obj_add_instance(&sec_obj, &inst));
    // first move is called during installation, it should fail
    ANJ_UNIT_ASSERT_FAILED(anj_dm_security_obj_install(&anj, &sec_obj));
    // check that sources were not changed
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[0].public_key_or_identity.source,
            ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[0].server_public_key.source,
            ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[0].secret_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    // check that data was not changed: data size and pointers to buffers
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[0]
                                  .public_key_or_identity.info.buffer.data_size,
                          sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[0]
                                  .server_public_key.info.buffer.data_size,
                          sizeof(SERVER_PUBLIC_KEY_1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[0].secret_key.info.buffer.data_size,
            sizeof(SECRET_KEY_1) - 1);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[0]
                    .public_key_or_identity.info.buffer.data
            == sec_obj.security_instances[0].public_key_or_identity_buff);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[0].server_public_key.info.buffer.data
            == sec_obj.security_instances[0].server_public_key_buff);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[0].secret_key.info.buffer.data
            == sec_obj.security_instances[0].secret_key_buff);

    // reset mock storage state
    g_mock_identity_counter = 0;
    // restore normal behavior of mock storage
    g_mock_create_fail_counter = -1;

    // now we successfully move credentials to external storage
    ANJ_UNIT_ASSERT_SUCCESS(
            anj.security_credential_handlers->offload_keys_and_certs(&anj));
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[0].public_key_or_identity.source,
            ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(
            sec_obj.security_instances[0].server_public_key.source,
            ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.security_instances[0].secret_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_TRUE(sec_obj.security_instances[0]
                                 .public_key_or_identity.info.external.identity
                         == &g_mock_identity[0]);
    ANJ_UNIT_ASSERT_TRUE(
            sec_obj.security_instances[0].secret_key.info.external.identity
            == &g_mock_identity[1]);
    ANJ_UNIT_ASSERT_TRUE(sec_obj.security_instances[0]
                                 .server_public_key.info.external.identity
                         == &g_mock_identity[2]);
}
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

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
            &anj, _ANJ_OP_DM_DELETE, true,
            &ANJ_MAKE_INSTANCE_PATH(ANJ_OBJ_ID_SECURITY, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_bootstrap_operation_validate(&anj));
    _anj_dm_bootstrap_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.obj.insts[1].iid, ANJ_ID_INVALID);

    RESOURCE_CHECK_STRING(sec_obj.security_instances[1].server_uri,
                          "coaps://server.com:5684");
    RESOURCE_CHECK_INT(sec_obj.security_instances[1].ssid, 2);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, _ANJ_OP_DM_DELETE, true,
            &ANJ_MAKE_INSTANCE_PATH(ANJ_OBJ_ID_SECURITY, 1)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_bootstrap_operation_validate(&anj));
    _anj_dm_bootstrap_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

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
            &anj, _ANJ_OP_DM_WRITE_PARTIAL_UPDATE, true,
            &ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 0, 2)));
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 5,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY, 0,
                                               RID_SECURITY_MODE)
            }));
    _anj_dm_bootstrap_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
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

    anj_net_psk_info_t psk_info;
    ANJ_UNIT_ASSERT_SUCCESS(anj_security_get_psk_info(&anj, true, &psk_info));
    ANJ_UNIT_ASSERT_EQUAL(psk_info.identity.tag,
                          ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY);
    ANJ_UNIT_ASSERT_EQUAL(psk_info.key.tag, ANJ_CRYPTO_SECURITY_TAG_PSK_KEY);
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_TRUE(psk_info.identity.info.external.identity
                         == &g_mock_identity[2]);
    ANJ_UNIT_ASSERT_TRUE(psk_info.key.info.external.identity
                         == &g_mock_identity[3]);
    ANJ_UNIT_ASSERT_EQUAL(psk_info.identity.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(psk_info.key.source, ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);

#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(psk_info.identity.info.buffer.data,
                                      "public", strlen("public"));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(psk_info.key.info.buffer.data, "secret",
                                      strlen("secret"));
    ANJ_UNIT_ASSERT_EQUAL(psk_info.identity.source,
                          ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(psk_info.key.source, ANJ_CRYPTO_DATA_SOURCE_BUFFER);
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

    ANJ_UNIT_ASSERT_SUCCESS(anj_security_get_psk_info(&anj, false, &psk_info));
    ANJ_UNIT_ASSERT_EQUAL(psk_info.identity.tag,
                          ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY);
    ANJ_UNIT_ASSERT_EQUAL(psk_info.key.tag, ANJ_CRYPTO_SECURITY_TAG_PSK_KEY);
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_TRUE(psk_info.identity.info.external.identity
                         == &g_mock_identity[0]);
    ANJ_UNIT_ASSERT_TRUE(psk_info.key.info.external.identity
                         == &g_mock_identity[1]);
    ANJ_UNIT_ASSERT_EQUAL(psk_info.identity.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(psk_info.key.source, ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(psk_info.identity.info.buffer.data, "ddd",
                                      strlen("ddd"));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(psk_info.key.info.buffer.data, "eee",
                                      strlen("eee"));
    ANJ_UNIT_ASSERT_EQUAL(psk_info.identity.source,
                          ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(psk_info.key.source, ANJ_CRYPTO_DATA_SOURCE_BUFFER);
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
}

#    ifdef ANJ_WITH_CERTIFICATES
ANJ_UNIT_TEST(dm_security_object, get_cert_check) {
    INIT_ENV();
    anj_net_certificate_usage_t certificate_usage = 1;
    anj_dm_security_instance_init_t inst_1 = {
        .ssid = 1,
        .bootstrap_server = false,
        .server_uri = "coaps://dddd:777",
        .security_mode = ANJ_DM_SECURITY_CERTIFICATE,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "ddd",
            .info.buffer.data_size = strlen("ddd")
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "eeee",
            .info.buffer.data_size = strlen("eeee")
        },
        .server_public_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "ww",
            .info.buffer.data_size = strlen("ww")
        },
        .server_name_indication = "aa",
        .certificate_usage = &certificate_usage,
    };
    anj_dm_security_instance_init_t inst_2 = {
        .ssid = 2,
        .bootstrap_server = true,
        .server_uri = "coaps://dddd:777",
        .security_mode = ANJ_DM_SECURITY_CERTIFICATE,
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

    anj_net_certificate_info_t cert_info;
    ANJ_UNIT_ASSERT_SUCCESS(anj_security_get_cert_info(&anj, true, &cert_info));
    ANJ_UNIT_ASSERT_EQUAL(cert_info.client_cert.tag,
                          ANJ_CRYPTO_SECURITY_TAG_CERTIFICATE_CHAIN);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.private_key.tag,
                          ANJ_CRYPTO_SECURITY_TAG_PRIVATE_KEY);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.server_cert.tag,
                          ANJ_CRYPTO_SECURITY_TAG_CERTIFICATE_CHAIN);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(cert_info.sni, "", 0);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.certificate_usage, 3);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.server_cert.source,
                          ANJ_CRYPTO_DATA_SOURCE_EMPTY);
#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_TRUE(cert_info.client_cert.info.external.identity
                         == &g_mock_identity[3]);
    ANJ_UNIT_ASSERT_TRUE(cert_info.private_key.info.external.identity
                         == &g_mock_identity[4]);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.client_cert.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.private_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
#        else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(cert_info.client_cert.info.buffer.data,
                                      "public", strlen("public"));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(cert_info.private_key.info.buffer.data,
                                      "secret", strlen("secret"));
    ANJ_UNIT_ASSERT_TRUE(cert_info.server_cert.info.buffer.data == NULL);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.client_cert.source,
                          ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.private_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_BUFFER);
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_security_get_cert_info(&anj, false, &cert_info));
    ANJ_UNIT_ASSERT_EQUAL(cert_info.client_cert.tag,
                          ANJ_CRYPTO_SECURITY_TAG_CERTIFICATE_CHAIN);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.private_key.tag,
                          ANJ_CRYPTO_SECURITY_TAG_PRIVATE_KEY);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.server_cert.tag,
                          ANJ_CRYPTO_SECURITY_TAG_CERTIFICATE_CHAIN);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(cert_info.sni, "aa", strlen("aa"));
    ANJ_UNIT_ASSERT_EQUAL(cert_info.certificate_usage, 1);

#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_TRUE(cert_info.client_cert.info.external.identity
                         == &g_mock_identity[0]);
    ANJ_UNIT_ASSERT_TRUE(cert_info.private_key.info.external.identity
                         == &g_mock_identity[1]);
    ANJ_UNIT_ASSERT_TRUE(cert_info.server_cert.info.external.identity
                         == &g_mock_identity[2]);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.client_cert.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.private_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.server_cert.source,
                          ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
#        else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(cert_info.client_cert.info.buffer.data,
                                      "ddd", strlen("ddd"));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(cert_info.private_key.info.buffer.data,
                                      "eeee", strlen("eeee"));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(cert_info.server_cert.info.buffer.data,
                                      "ww", strlen("ww"));
    ANJ_UNIT_ASSERT_EQUAL(cert_info.client_cert.source,
                          ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.private_key.source,
                          ANJ_CRYPTO_DATA_SOURCE_BUFFER);
    ANJ_UNIT_ASSERT_EQUAL(cert_info.server_cert.source,
                          ANJ_CRYPTO_DATA_SOURCE_BUFFER);
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
}
#    endif // ANJ_WITH_CERTIFICATES

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
        g_membuf_write_offset = 0;             \
        g_mock_identity_counter = 0;

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

#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
void clear_internal_buffers(anj_dm_security_obj_t *sec_obj) {
    for (size_t i = 0; i < ANJ_DM_SECURITY_OBJ_INSTANCES; i++) {
        memset(sec_obj->security_instances[i].public_key_or_identity_buff, 0,
               sizeof(sec_obj->security_instances[i]
                              .public_key_or_identity_buff));
        memset(sec_obj->security_instances[i].secret_key_buff, 0,
               sizeof(sec_obj->security_instances[i].secret_key_buff));
        memset(sec_obj->security_instances[i].server_public_key_buff, 0,
               sizeof(sec_obj->security_instances[i].server_public_key_buff));
    }
}
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

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

#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    // during creds offloading, tag is set, we have to do it manually here
    anj_dm_security_instance_t *inst = &sec_obj_2.security_instances[0];
    inst->public_key_or_identity.tag = ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY;
    inst->secret_key.tag = ANJ_CRYPTO_SECURITY_TAG_PSK_KEY;
    // also buffer size is still set but it doesn't matter because source is
    // external
    inst = &sec_obj.security_instances[0];
    inst->public_key_or_identity.info.buffer.data_size = 0;
    inst->secret_key.info.buffer.data_size = 0;
    clear_internal_buffers(&sec_obj);
    clear_internal_buffers(&sec_obj_2);
#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    // before check handle crypto data pointers - check it and reset
    anj_dm_security_instance_t *inst = &sec_obj_2.security_instances[0];
    ANJ_UNIT_ASSERT_TRUE(inst->public_key_or_identity.info.buffer.data
                         == inst->public_key_or_identity_buff);
    ANJ_UNIT_ASSERT_TRUE(inst->secret_key.info.buffer.data
                         == inst->secret_key_buff);
    CLEAR_CRYPTO_BUFFER_POINTERS(inst);
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

    compare_objects_after_persistence(&sec_obj, &sec_obj_2);
}

#    ifdef ANJ_WITH_CERTIFICATES
ANJ_UNIT_TEST(dm_security_object, persistence_cert_instance) {
    INIT_ENV_PERSISTENCE(anj, sec_obj);
    INIT_ENV_PERSISTENCE(anj_2, sec_obj_2);

    anj_net_certificate_usage_t certificate_usage = 2;
    anj_dm_security_instance_init_t inst_1 = {
        .ssid = 1,
        .bootstrap_server = false,
        .server_uri = "coaps://dddd:777",
        .security_mode = ANJ_DM_SECURITY_CERTIFICATE,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "public",
            .info.buffer.data_size = strlen("public")
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "secret",
            .info.buffer.data_size = strlen("secret")
        },
        .server_public_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = "server_public_key",
            .info.buffer.data_size = strlen("server_public_key")
        },
        .server_name_indication = "DDD",
        .certificate_usage = &certificate_usage
    };
    persistence_store(&anj, &sec_obj, &inst_1);
    persistence_restore(&anj_2, &sec_obj_2);

#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    // during creds offloading, tag is set, we have to do it manually here
    anj_dm_security_instance_t *inst = &sec_obj_2.security_instances[0];
    inst->public_key_or_identity.tag =
            ANJ_CRYPTO_SECURITY_TAG_CERTIFICATE_CHAIN;
    inst->server_public_key.tag = ANJ_CRYPTO_SECURITY_TAG_CERTIFICATE_CHAIN;
    inst->secret_key.tag = ANJ_CRYPTO_SECURITY_TAG_PRIVATE_KEY;
    // also buffer size is still set but it doesn't matter because source is
    // external
    inst = &sec_obj.security_instances[0];
    inst->public_key_or_identity.info.buffer.data_size = 0;
    inst->secret_key.info.buffer.data_size = 0;
    inst->server_public_key.info.buffer.data_size = 0;
    clear_internal_buffers(&sec_obj);
    clear_internal_buffers(&sec_obj_2);
#        else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    // before check handle crypto data pointers - check it and reset
    anj_dm_security_instance_t *inst = &sec_obj_2.security_instances[0];
    ANJ_UNIT_ASSERT_TRUE(inst->public_key_or_identity.info.buffer.data
                         == inst->public_key_or_identity_buff);
    ANJ_UNIT_ASSERT_TRUE(inst->secret_key.info.buffer.data
                         == inst->secret_key_buff);
    CLEAR_CRYPTO_BUFFER_POINTERS(inst);
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

    compare_objects_after_persistence(&sec_obj, &sec_obj_2);
}
#    endif // ANJ_WITH_CERTIFICATES

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

#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    // during creds offloading, tag is set, we have to do it manually here
    for (size_t i = 0; i < ANJ_DM_SECURITY_OBJ_INSTANCES; i++) {
        anj_dm_security_instance_t *inst = &sec_obj_2.security_instances[i];
        inst->public_key_or_identity.tag = ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY;
        inst->secret_key.tag = ANJ_CRYPTO_SECURITY_TAG_PSK_KEY;
        // also buffer size is still set but it doesn't matter because source is
        // external
        inst = &sec_obj.security_instances[i];
        inst->public_key_or_identity.info.buffer.data_size = 0;
        inst->secret_key.info.buffer.data_size = 0;
    }
    clear_internal_buffers(&sec_obj);
    clear_internal_buffers(&sec_obj_2);
#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    anj_dm_security_instance_t *inst = &sec_obj_2.security_instances[0];
    CLEAR_CRYPTO_BUFFER_POINTERS(inst);
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

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
