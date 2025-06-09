/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>

#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/security_object.h>
#include <anj/utils.h>

#include "../../../src/anj/dm/dm_io.h"
#include "../../../src/anj/io/io.h"

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

ANJ_UNIT_TEST(dm_security_object, check_resources_values) {
    INIT_ENV();

    anj_dm_security_instance_init_t inst_1 = {
        .server_uri = "coap://server.com:5683",
        .bootstrap_server = true,
        .security_mode = 1,
        .public_key_or_identity = PUBLIC_KEY_OR_IDENTITY_1,
        .public_key_or_identity_size = sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1,
        .server_public_key = SERVER_PUBLIC_KEY_1,
        .server_public_key_size = sizeof(SERVER_PUBLIC_KEY_1) - 1,
        .secret_key = SECRET_KEY_1,
        .secret_key_size = sizeof(SECRET_KEY_1) - 1
    };
    anj_dm_security_instance_init_t inst_2 = {
        .server_uri = "coaps://server.com:5684",
        .bootstrap_server = false,
        .security_mode = 2,
        .public_key_or_identity = PUBLIC_KEY_OR_IDENTITY_2,
        .public_key_or_identity_size = sizeof(PUBLIC_KEY_OR_IDENTITY_2) - 1,
        .server_public_key = SERVER_PUBLIC_KEY_2,
        .server_public_key_size = sizeof(SERVER_PUBLIC_KEY_2) - 1,
        .secret_key = SECRET_KEY_2,
        .secret_key_size = sizeof(SECRET_KEY_2) - 1,
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
    RESOURCE_CHECK_BYTES(0,
                         sec_obj.security_instances[0].public_key_or_identity,
                         PUBLIC_KEY_OR_IDENTITY_1,
                         sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1);
    RESOURCE_CHECK_BYTES(0, sec_obj.security_instances[0].server_public_key,
                         SERVER_PUBLIC_KEY_1, sizeof(SERVER_PUBLIC_KEY_1) - 1);
    RESOURCE_CHECK_BYTES(0, sec_obj.security_instances[0].secret_key,
                         SECRET_KEY_1, sizeof(SECRET_KEY_1) - 1);
    RESOURCE_CHECK_INT(0, sec_obj.security_instances[0].ssid,
                       _ANJ_SSID_BOOTSTRAP);
    RESOURCE_CHECK_STRING(1, sec_obj.security_instances[1].server_uri,
                          "coaps://server.com:5684");
    RESOURCE_CHECK_BOOL(1, sec_obj.security_instances[1].bootstrap_server,
                        false);
    RESOURCE_CHECK_INT(1, sec_obj.security_instances[1].security_mode, 2);
    RESOURCE_CHECK_BYTES(1,
                         sec_obj.security_instances[1].public_key_or_identity,
                         PUBLIC_KEY_OR_IDENTITY_2,
                         sizeof(PUBLIC_KEY_OR_IDENTITY_2) - 1);
    RESOURCE_CHECK_BYTES(1, sec_obj.security_instances[1].server_public_key,
                         SERVER_PUBLIC_KEY_2, sizeof(SERVER_PUBLIC_KEY_2) - 1);
    RESOURCE_CHECK_BYTES(1, sec_obj.security_instances[1].secret_key,
                         SECRET_KEY_2, sizeof(SECRET_KEY_2) - 1);
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
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    RESOURCE_CHECK_STRING(0, sec_obj.security_instances[0].server_uri,
                          "coap://server.com:5683");
    RESOURCE_CHECK_INT(0, sec_obj.security_instances[0].ssid, 1);
    RESOURCE_CHECK_STRING(20, sec_obj.security_instances[1].server_uri,
                          "coap://test.com:5684");
    RESOURCE_CHECK_BOOL(20, sec_obj.security_instances[1].bootstrap_server,
                        false);
    RESOURCE_CHECK_INT(20, sec_obj.security_instances[1].security_mode, 0);
    RESOURCE_CHECK_BYTES(
            20, sec_obj.security_instances[1].public_key_or_identity, "", 0);
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
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    RESOURCE_CHECK_STRING(0, sec_obj.security_instances[0].server_uri,
                          "coap://server.com:5683");
    RESOURCE_CHECK_INT(0, sec_obj.security_instances[0].ssid, 1);
    RESOURCE_CHECK_STRING(20, sec_obj.security_instances[1].server_uri,
                          "coap://test.com:5683");
    RESOURCE_CHECK_BOOL(20, sec_obj.security_instances[1].bootstrap_server,
                        true);
    RESOURCE_CHECK_INT(20, sec_obj.security_instances[1].security_mode, 1);
    RESOURCE_CHECK_BYTES(20,
                         sec_obj.security_instances[1].public_key_or_identity,
                         PUBLIC_KEY_OR_IDENTITY_1,
                         sizeof(PUBLIC_KEY_OR_IDENTITY_1) - 1);
    RESOURCE_CHECK_BYTES(20, sec_obj.security_instances[1].server_public_key,
                         SERVER_PUBLIC_KEY_1, sizeof(SERVER_PUBLIC_KEY_1) - 1);
    RESOURCE_CHECK_BYTES(20, sec_obj.security_instances[1].secret_key,
                         SECRET_KEY_1, sizeof(SECRET_KEY_1) - 1);
    RESOURCE_CHECK_INT(20, sec_obj.security_instances[1].ssid, 7);
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
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    ANJ_UNIT_ASSERT_EQUAL(sec_obj.obj.insts[1].iid, ANJ_ID_INVALID);

    RESOURCE_CHECK_STRING(1, sec_obj.security_instances[1].server_uri,
                          "coaps://server.com:5684");
    RESOURCE_CHECK_INT(1, sec_obj.security_instances[1].ssid, 2);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_DELETE, true,
            &ANJ_MAKE_INSTANCE_PATH(ANJ_OBJ_ID_SECURITY, 1)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

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

    anj_dm_security_instance_init_t inst_3 = {
        .server_uri = "coap://server.com:5683",
        .ssid = 2,
    };

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
    int ret = _anj_dm_operation_end(&anj);
    ANJ_UNIT_ASSERT_EQUAL(ret, ANJ_DM_ERR_BAD_REQUEST);
}

#endif // ANJ_WITH_DEFAULT_SECURITY_OBJ
