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
#include <anj/dm/server_object.h>
#include <anj/utils.h>

#include "../../../src/anj/dm/dm_io.h"
#include "../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_DEFAULT_SERVER_OBJ

enum server_resources {
    RID_SSID = 0,
    RID_LIFETIME = 1,
    RID_DEFAULT_MIN_PERIOD = 2,
    RID_DEFAULT_MAX_PERIOD = 3,
    RID_DISABLE = 4,
    RID_DISABLE_TIMEOUT = 5,
    RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE = 6,
    RID_BINDING = 7,
    RID_REGISTRATION_UPDATE_TRIGGER = 8,
    RID_BOOTSTRAP_REQUEST_TRIGGER = 9,
    RID_BOOTSTRAP_ON_REGISTRATION_FAILURE = 16,
    RID_COMMUNICATION_RETRY_COUNT = 17,
    RID_COMMUNICATION_RETRY_TIMER = 18,
    RID_COMMUNICATION_SEQUENCE_DELAY_TIMER = 19,
    RID_COMMUNICATION_SEQUENCE_RETRY_COUNT = 20,
    RID_MUTE_SEND = 23,
    RID_DEFAULT_NOTIFICATION_MODE = 26
};

#    define RESOURCE_CHECK_INT(Iid, Rid, Expected_val)                   \
        do {                                                             \
            anj_res_value_t val;                                         \
            anj_uri_path_t path =                                        \
                    ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, Iid, Rid); \
            ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(             \
                    &anj, ANJ_OP_DM_READ, false, &path));                \
            ANJ_UNIT_ASSERT_SUCCESS(anj_dm_res_read(&anj, &path, &val)); \
            ANJ_UNIT_ASSERT_EQUAL(val.int_value, Expected_val);          \
            ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));        \
        } while (false)

#    define RESOURCE_CHECK_BOOL(Iid, Rid, Expected_val)                  \
        do {                                                             \
            anj_res_value_t val;                                         \
            anj_uri_path_t path =                                        \
                    ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, Iid, Rid); \
            ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(             \
                    &anj, ANJ_OP_DM_READ, false, &path));                \
            ANJ_UNIT_ASSERT_SUCCESS(anj_dm_res_read(&anj, &path, &val)); \
            ANJ_UNIT_ASSERT_EQUAL(val.bool_value, Expected_val);         \
            ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));        \
        } while (false)

#    define RESOURCE_CHECK_STR(Iid, Rid, Expected_val)                   \
        do {                                                             \
            anj_res_value_t val;                                         \
            anj_uri_path_t path =                                        \
                    ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, Iid, Rid); \
            ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(             \
                    &anj, ANJ_OP_DM_READ, false, &path));                \
            ANJ_UNIT_ASSERT_SUCCESS(anj_dm_res_read(&anj, &path, &val)); \
            ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data,       \
                                         Expected_val);                  \
            ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));        \
        } while (false)

#    define INIT_ENV()                     \
        anj_dm_server_obj_t server_object; \
        anj_t anj = { 0 };                 \
        _anj_dm_initialize(&anj);          \
        anj_dm_server_obj_init(&server_object);

ANJ_UNIT_TEST(dm_server_object, check_resources_values) {
    INIT_ENV();

    anj_dm_server_instance_init_t inst_1 = {
        .ssid = 1,
        .lifetime = 2,
        .default_min_period = 3,
        .default_max_period = 4,
        .binding = "U",
        .bootstrap_on_registration_failure = &(const bool) { false },
        .mute_send = false,
        .notification_storing = false,
        .iid = &(anj_iid_t) { 0 },
        .comm_retry_res = &(anj_communication_retry_res_t) {
            .retry_count = 1,
            .retry_timer = 2,
            .seq_delay_timer = 3,
            .seq_retry_count = 4
        },
        .default_notification_mode = 1,
        .disable_timeout = 5,
    };

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_server_obj_add_instance(&server_object, &inst_1));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &server_object));

    RESOURCE_CHECK_INT(0, RID_SSID, 1);
    RESOURCE_CHECK_INT(0, RID_LIFETIME, 2);
    RESOURCE_CHECK_INT(0, RID_DEFAULT_MIN_PERIOD, 3);
    RESOURCE_CHECK_INT(0, RID_DEFAULT_MAX_PERIOD, 4);
    RESOURCE_CHECK_STR(0, RID_BINDING, "U");
    RESOURCE_CHECK_BOOL(0, RID_BOOTSTRAP_ON_REGISTRATION_FAILURE, false);
    RESOURCE_CHECK_BOOL(0, RID_MUTE_SEND, false);
    RESOURCE_CHECK_BOOL(0, RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE,
                        false);
    RESOURCE_CHECK_INT(0, RID_COMMUNICATION_RETRY_COUNT, 1);
    RESOURCE_CHECK_INT(0, RID_COMMUNICATION_RETRY_TIMER, 2);
    RESOURCE_CHECK_INT(0, RID_COMMUNICATION_SEQUENCE_DELAY_TIMER, 3);
    RESOURCE_CHECK_INT(0, RID_COMMUNICATION_SEQUENCE_RETRY_COUNT, 4);
    RESOURCE_CHECK_INT(0, RID_DEFAULT_NOTIFICATION_MODE, 1);
    RESOURCE_CHECK_INT(0, RID_DISABLE_TIMEOUT, 5);
}

ANJ_UNIT_TEST(dm_server_object, custom_iid) {
    INIT_ENV();

    anj_dm_server_instance_init_t inst_1 = {
        .ssid = 1,
        .lifetime = 2,
        .binding = "U",
        .iid = &(anj_iid_t) { 20 }
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_server_obj_add_instance(&server_object, &inst_1));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &server_object));

    RESOURCE_CHECK_INT(20, RID_SSID, 1);
    ANJ_UNIT_ASSERT_EQUAL(server_object.inst.iid, 20);
}

ANJ_UNIT_TEST(dm_server_object, write_replace) {
    INIT_ENV();

    anj_dm_server_instance_init_t inst_1 = {
        .ssid = 1,
        .lifetime = 2,
        .default_min_period = 3,
        .default_max_period = 4,
        .binding = "U",
        .bootstrap_on_registration_failure = &(const bool) { false },
        .mute_send = false,
        .notification_storing = false
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_server_obj_add_instance(&server_object, &inst_1));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &server_object));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_WRITE_REPLACE, true,
            &ANJ_MAKE_INSTANCE_PATH(ANJ_OBJ_ID_SERVER, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 4,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 0, RID_SSID)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_write_entry(&anj,
                                &(anj_io_out_entry_t) {
                                    .type = ANJ_DATA_TYPE_INT,
                                    .value.int_value = 77,
                                    .path = ANJ_MAKE_RESOURCE_PATH(
                                            ANJ_OBJ_ID_SERVER, 0, RID_LIFETIME)
                                }));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_write_entry(&anj,
                                &(anj_io_out_entry_t) {
                                    .type = ANJ_DATA_TYPE_STRING,
                                    .value.bytes_or_string.data = "T",
                                    .value.bytes_or_string.chunk_length = 1,
                                    .path = ANJ_MAKE_RESOURCE_PATH(
                                            ANJ_OBJ_ID_SERVER, 0, RID_BINDING)
                                }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 8,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 0,
                                               RID_DISABLE_TIMEOUT)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_UINT,
                .value.int_value = 9,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 0,
                                               RID_COMMUNICATION_RETRY_COUNT)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_UINT,
                .value.int_value = 10,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 0,
                                               RID_COMMUNICATION_RETRY_TIMER)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_UINT,
                .value.int_value = 11,
                .path = ANJ_MAKE_RESOURCE_PATH(
                        ANJ_OBJ_ID_SERVER, 0,
                        RID_COMMUNICATION_SEQUENCE_DELAY_TIMER)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_UINT,
                .value.int_value = 12,
                .path = ANJ_MAKE_RESOURCE_PATH(
                        ANJ_OBJ_ID_SERVER, 0,
                        RID_COMMUNICATION_SEQUENCE_RETRY_COUNT)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 1,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 0,
                                               RID_DEFAULT_NOTIFICATION_MODE)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    RESOURCE_CHECK_INT(0, RID_SSID, 4);
    RESOURCE_CHECK_INT(0, RID_LIFETIME, 77);
    RESOURCE_CHECK_INT(0, RID_DISABLE_TIMEOUT, 8);
    RESOURCE_CHECK_INT(0, RID_COMMUNICATION_RETRY_COUNT, 9);
    RESOURCE_CHECK_INT(0, RID_COMMUNICATION_RETRY_TIMER, 10);
    RESOURCE_CHECK_INT(0, RID_COMMUNICATION_SEQUENCE_DELAY_TIMER, 11);
    RESOURCE_CHECK_INT(0, RID_COMMUNICATION_SEQUENCE_RETRY_COUNT, 12);
    RESOURCE_CHECK_INT(0, RID_DEFAULT_NOTIFICATION_MODE, 1);
    RESOURCE_CHECK_INT(0, RID_DEFAULT_MIN_PERIOD, 0);
    RESOURCE_CHECK_INT(0, RID_DEFAULT_MAX_PERIOD, 0);
    RESOURCE_CHECK_STR(0, RID_BINDING, "T");
    RESOURCE_CHECK_BOOL(0, RID_BOOTSTRAP_ON_REGISTRATION_FAILURE, true);
    RESOURCE_CHECK_BOOL(0, RID_MUTE_SEND, false);
    RESOURCE_CHECK_BOOL(0, RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE,
                        false);
}

ANJ_UNIT_TEST(dm_server_object, server_create_instance_minimal) {
    INIT_ENV();

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &server_object));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, true,
                                    &ANJ_MAKE_OBJECT_PATH(ANJ_OBJ_ID_SERVER)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, 20));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 7,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 20, RID_SSID)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_write_entry(&anj,
                                &(anj_io_out_entry_t) {
                                    .type = ANJ_DATA_TYPE_INT,
                                    .value.int_value = 8,
                                    .path = ANJ_MAKE_RESOURCE_PATH(
                                            ANJ_OBJ_ID_SERVER, 20, RID_LIFETIME)
                                }));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_write_entry(&anj,
                                &(anj_io_out_entry_t) {
                                    .type = ANJ_DATA_TYPE_STRING,
                                    .value.bytes_or_string.data = "U",
                                    .value.bytes_or_string.chunk_length = 1,
                                    .path = ANJ_MAKE_RESOURCE_PATH(
                                            ANJ_OBJ_ID_SERVER, 20, RID_BINDING)
                                }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    RESOURCE_CHECK_INT(20, RID_SSID, 7);
    RESOURCE_CHECK_INT(20, RID_LIFETIME, 8);
    RESOURCE_CHECK_INT(20, RID_DEFAULT_MIN_PERIOD, 0);
    RESOURCE_CHECK_INT(20, RID_DEFAULT_MAX_PERIOD, 0);
    RESOURCE_CHECK_STR(20, RID_BINDING, "U");
    RESOURCE_CHECK_BOOL(20, RID_BOOTSTRAP_ON_REGISTRATION_FAILURE, true);
    RESOURCE_CHECK_BOOL(20, RID_MUTE_SEND, false);
    RESOURCE_CHECK_BOOL(20, RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE,
                        false);
    anj_communication_retry_res_t default_comm_retry_res =
            ANJ_COMMUNICATION_RETRY_RES_DEFAULT;
    RESOURCE_CHECK_INT(20, RID_COMMUNICATION_RETRY_COUNT,
                       default_comm_retry_res.retry_count);
    RESOURCE_CHECK_INT(20, RID_COMMUNICATION_RETRY_TIMER,
                       default_comm_retry_res.retry_timer);
    RESOURCE_CHECK_INT(20,
                       RID_COMMUNICATION_SEQUENCE_DELAY_TIMER,
                       default_comm_retry_res.seq_delay_timer);
    RESOURCE_CHECK_INT(20,
                       RID_COMMUNICATION_SEQUENCE_RETRY_COUNT,
                       default_comm_retry_res.seq_retry_count);
    RESOURCE_CHECK_INT(20, RID_DEFAULT_NOTIFICATION_MODE, 0);
    RESOURCE_CHECK_INT(20, RID_DISABLE_TIMEOUT,
                       ANJ_DISABLE_TIMEOUT_DEFAULT_VALUE);
}

ANJ_UNIT_TEST(dm_server_object, server_create_instance) {
    INIT_ENV();

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &server_object));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, true,
                                    &ANJ_MAKE_OBJECT_PATH(ANJ_OBJ_ID_SERVER)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, 22));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 17,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 22, RID_SSID)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_write_entry(&anj,
                                &(anj_io_out_entry_t) {
                                    .type = ANJ_DATA_TYPE_INT,
                                    .value.int_value = 18,
                                    .path = ANJ_MAKE_RESOURCE_PATH(
                                            ANJ_OBJ_ID_SERVER, 22, RID_LIFETIME)
                                }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 19,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 22,
                                               RID_DEFAULT_MIN_PERIOD)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_INT,
                .value.int_value = 20,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 22,
                                               RID_DEFAULT_MAX_PERIOD)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_write_entry(&anj,
                                &(anj_io_out_entry_t) {
                                    .type = ANJ_DATA_TYPE_STRING,
                                    .value.bytes_or_string.data = "T",
                                    .value.bytes_or_string.chunk_length = 1,
                                    .path = ANJ_MAKE_RESOURCE_PATH(
                                            ANJ_OBJ_ID_SERVER, 22, RID_BINDING)
                                }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_BOOL,
                .value.bool_value = true,
                .path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER, 22,
                                               RID_MUTE_SEND)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_BOOL,
                .value.bool_value = true,
                .path = ANJ_MAKE_RESOURCE_PATH(
                        ANJ_OBJ_ID_SERVER, 22,
                        RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(
            &anj,
            &(anj_io_out_entry_t) {
                .type = ANJ_DATA_TYPE_BOOL,
                .value.bool_value = false,
                .path = ANJ_MAKE_RESOURCE_PATH(
                        ANJ_OBJ_ID_SERVER, 22,
                        RID_BOOTSTRAP_ON_REGISTRATION_FAILURE)
            }));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    RESOURCE_CHECK_INT(22, RID_SSID, 17);
    RESOURCE_CHECK_INT(22, RID_LIFETIME, 18);
    RESOURCE_CHECK_INT(22, RID_DEFAULT_MIN_PERIOD, 19);
    RESOURCE_CHECK_INT(22, RID_DEFAULT_MAX_PERIOD, 20);
    RESOURCE_CHECK_STR(22, RID_BINDING, "T");
    RESOURCE_CHECK_BOOL(22, RID_BOOTSTRAP_ON_REGISTRATION_FAILURE, false);
    RESOURCE_CHECK_BOOL(22, RID_MUTE_SEND, true);
    RESOURCE_CHECK_BOOL(22, RID_NOTIFICATION_STORING_WHEN_DISABLED_OR_OFFLINE,
                        true);
}

ANJ_UNIT_TEST(dm_server_object, server_create_error) {
    INIT_ENV();

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &server_object));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_CREATE, true,
                                    &ANJ_MAKE_OBJECT_PATH(ANJ_OBJ_ID_SERVER)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_create_object_instance(&anj, 20));
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_operation_end(&anj));
    ANJ_UNIT_ASSERT_EQUAL(server_object.obj.insts[0].iid, ANJ_ID_INVALID);
}

ANJ_UNIT_TEST(dm_server_object, server_delete_instance) {
    INIT_ENV();

    anj_dm_server_instance_init_t inst_1 = {
        .ssid = 1,
        .lifetime = 2,
        .default_min_period = 3,
        .default_max_period = 4,
        .binding = "U",
        .bootstrap_on_registration_failure = &(const bool) { false },
        .mute_send = false,
        .notification_storing = false
    };

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_server_obj_add_instance(&server_object, &inst_1));
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_server_obj_install(&anj, &server_object));

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_DELETE, false,
            &ANJ_MAKE_INSTANCE_PATH(ANJ_OBJ_ID_SERVER, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    ANJ_UNIT_ASSERT_EQUAL(server_object.obj.insts[0].iid, ANJ_ID_INVALID);
}

ANJ_UNIT_TEST(dm_server_object, errors) {
    INIT_ENV();

    anj_dm_server_instance_init_t inst_1 = {
        .ssid = 1,
        .lifetime = 2,
        .default_min_period = 3,
        .default_max_period = 4,
        .binding = "UU",
        .bootstrap_on_registration_failure = &(const bool) { false },
        .mute_send = false,
        .notification_storing = false
    };
    // UU duplication
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_server_obj_add_instance(&server_object, &inst_1));
    anj_dm_server_instance_init_t inst_3 = {
        .ssid = 2,
        .lifetime = 1,
        .default_min_period = 3,
        .default_max_period = 4,
        .binding = "B",
        .bootstrap_on_registration_failure = &(const bool) { false },
        .mute_send = false,
        .notification_storing = false
    };
    // incorrect binding
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_server_obj_add_instance(&server_object, &inst_3));
    anj_dm_server_instance_init_t inst_4 = {
        .ssid = 2,
        .lifetime = 1,
        .default_min_period = 3,
        .default_max_period = 4,
        .binding = "",
        .bootstrap_on_registration_failure = &(const bool) { false },
        .mute_send = false,
        .notification_storing = false
    };
    // empty binding
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_server_obj_add_instance(&server_object, &inst_4));
    anj_dm_server_instance_init_t inst_6 = {
        .ssid = 2,
        .lifetime = 1,
        .default_min_period = 3,
        .default_max_period = 4,
        .binding = "U",
        .bootstrap_on_registration_failure = &(const bool) { false },
        .mute_send = false,
        .notification_storing = false,
        .default_notification_mode = 2
    };
    // default_notification_mode
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_server_obj_add_instance(&server_object, &inst_6));
    anj_dm_server_instance_init_t inst_7 = {
        .ssid = 3,
        .lifetime = 1,
        .default_min_period = 3,
        .default_max_period = 4,
        .binding = "U",
        .bootstrap_on_registration_failure = &(const bool) { false },
        .mute_send = false,
        .notification_storing = false
    };
    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_server_obj_add_instance(&server_object, &inst_7));
    anj_dm_server_instance_init_t inst_8 = {
        .ssid = 4,
        .lifetime = 1,
        .default_min_period = 3,
        .default_max_period = 4,
        .binding = "U",
        .bootstrap_on_registration_failure = &(const bool) { false },
        .mute_send = false,
        .notification_storing = false
    };
    // max instances reached
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_server_obj_add_instance(&server_object, &inst_8));
}

#endif // ANJ_WITH_DEFAULT_SERVER_OBJ
