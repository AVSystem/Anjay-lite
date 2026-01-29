/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stddef.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/device_object.h>
#include <anj/utils.h>

#include "../../../../src/anj/dm/dm_io.h"
#include "../../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#define DM_INITIALIZE_BASIC(Anj)                             \
    anj_dm_device_obj_t device_obj;                          \
    anj_t Anj = { 0 };                                       \
    _anj_dm_initialize(&Anj);                                \
    anj_dm_obj_t obj_1 = {                                   \
        .oid = 1                                             \
    };                                                       \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&(Anj), &obj_1)); \
    anj_dm_obj_t obj_2 = {                                   \
        .oid = 2,                                            \
        .version = "2.2"                                     \
    };                                                       \
    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_add_obj(&(Anj), &obj_2));

#define VERIFY_INT_ENTRY(Out, Path, Value)                         \
    ANJ_UNIT_ASSERT_TRUE(anj_uri_path_equal(&(Out).path, (Path))); \
    ANJ_UNIT_ASSERT_EQUAL((Out).value.int_value, (Value));         \
    ANJ_UNIT_ASSERT_EQUAL((Out).type, ANJ_DATA_TYPE_INT);

#define VERIFY_STR_ENTRY(Out, Path, Value)                                   \
    ANJ_UNIT_ASSERT_TRUE(anj_uri_path_equal(&(Out).path, (Path)));           \
    ANJ_UNIT_ASSERT_EQUAL_STRING((Out).value.bytes_or_string.data, (Value)); \
    ANJ_UNIT_ASSERT_EQUAL((Out).type, ANJ_DATA_TYPE_STRING);

#define CHECK_AND_VERIFY_STRING_RESOURCE(Anj, Path, Value, Record, Count)     \
    ANJ_UNIT_ASSERT_SUCCESS(                                                  \
            _anj_dm_operation_begin(&(Anj), ANJ_OP_DM_READ, false, &(Path))); \
    _anj_dm_get_readable_res_count(&(Anj), &(Count));                         \
    ANJ_UNIT_ASSERT_EQUAL(1, (Count));                                        \
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&(Anj), &(Record)),          \
                          _ANJ_DM_LAST_RECORD);                               \
    VERIFY_STR_ENTRY((Record), &(Path), (Value));                             \
    _anj_dm_operation_end(&(Anj), ANJ_DM_TRANSACTION_SUCCESS);

#define MANUFACTURER_STR "manufacturer"
#define MODEL_NUMBER_STR "model_number"
#define SERIAL_NUMBER_STR "serial_number"
#define FIRMWARE_VERSION_STR "firmware_version"
#define SOFTWARE_VERSION_STR "software_version"

static int g_reboot_execute_counter;

static void reboot_cb(void *arg, anj_t *anj) {
    (void) anj;
    (void) arg;
    g_reboot_execute_counter++;
}

ANJ_UNIT_TEST(dm_device_object, add_remove_objects) {
    DM_INITIALIZE_BASIC(anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 2);

    anj_dm_device_object_init_t dev_obj_init = {
        .manufacturer = MANUFACTURER_STR,
        .model_number = MODEL_NUMBER_STR,
        .serial_number = SERIAL_NUMBER_STR,
        .firmware_version = FIRMWARE_VERSION_STR,
        .software_version = SOFTWARE_VERSION_STR,
        .reboot_cb = reboot_cb
    };

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_device_obj_install(&anj, &device_obj, &dev_obj_init));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 3);

    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_device_obj_install(&anj, &device_obj, &dev_obj_init));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 3);

    ANJ_UNIT_ASSERT_SUCCESS(anj_dm_remove_obj(&anj, 3));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 2);
}

ANJ_UNIT_TEST(dm_device_object, resources_execute) {
    DM_INITIALIZE_BASIC(anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 2);

    anj_dm_device_object_init_t dev_obj_init = {
        .manufacturer = MANUFACTURER_STR,
        .model_number = MODEL_NUMBER_STR,
        .serial_number = SERIAL_NUMBER_STR,
        .firmware_version = FIRMWARE_VERSION_STR,
        .software_version = SOFTWARE_VERSION_STR,
        .reboot_cb = reboot_cb
    };

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_device_obj_install(&anj, &device_obj, &dev_obj_init));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 3);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(3, 0, 4)));
    ANJ_UNIT_ASSERT_EQUAL(g_reboot_execute_counter, 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_execute(&anj, NULL, 0));
    ANJ_UNIT_ASSERT_EQUAL(g_reboot_execute_counter, 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_execute(&anj, NULL, 0));
    ANJ_UNIT_ASSERT_EQUAL(g_reboot_execute_counter, 2);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);

    ANJ_UNIT_ASSERT_FAILED(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(3, 0, 0)));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1)));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(3, 0, 2)));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3)));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(3, 0, 11)));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_FAILURE);
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(3, 0, 16)));
}

ANJ_UNIT_TEST(dm_device_object, execute_on_missing_resource) {
    DM_INITIALIZE_BASIC(anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 2);

    anj_dm_device_object_init_t dev_obj_init = {
        .manufacturer = MANUFACTURER_STR,
        .model_number = MODEL_NUMBER_STR,
        .serial_number = SERIAL_NUMBER_STR,
        .firmware_version = FIRMWARE_VERSION_STR,
        .reboot_cb = NULL
    };

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_device_obj_install(&anj, &device_obj, &dev_obj_init));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 3);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
            &anj, ANJ_OP_DM_EXECUTE, false, &ANJ_MAKE_RESOURCE_PATH(3, 0, 4)));
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_execute(&anj, NULL, 0));
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
}

ANJ_UNIT_TEST(dm_device_object, resources_read) {
    DM_INITIALIZE_BASIC(anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 2);

    anj_dm_device_object_init_t dev_obj_init = {
        .manufacturer = MANUFACTURER_STR,
        .model_number = MODEL_NUMBER_STR,
        .serial_number = SERIAL_NUMBER_STR,
        .firmware_version = FIRMWARE_VERSION_STR,
        .software_version = SOFTWARE_VERSION_STR,
        .reboot_cb = NULL
    };

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_device_obj_install(&anj, &device_obj, &dev_obj_init));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 3);

    size_t out_res_count = 0;
    anj_io_out_entry_t out_record = { 0 };

    CHECK_AND_VERIFY_STRING_RESOURCE(anj,
                                     ANJ_MAKE_RESOURCE_PATH(3, 0, 0),
                                     MANUFACTURER_STR,
                                     out_record,
                                     out_res_count);
    CHECK_AND_VERIFY_STRING_RESOURCE(anj,
                                     ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
                                     MODEL_NUMBER_STR,
                                     out_record,
                                     out_res_count);
    CHECK_AND_VERIFY_STRING_RESOURCE(anj,
                                     ANJ_MAKE_RESOURCE_PATH(3, 0, 2),
                                     SERIAL_NUMBER_STR,
                                     out_record,
                                     out_res_count);
    CHECK_AND_VERIFY_STRING_RESOURCE(anj,
                                     ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
                                     FIRMWARE_VERSION_STR,
                                     out_record,
                                     out_res_count);
    CHECK_AND_VERIFY_STRING_RESOURCE(anj,
                                     ANJ_MAKE_RESOURCE_PATH(3, 0, 16),
                                     ANJ_SUPPORTED_BINDING_MODES,
                                     out_record,
                                     out_res_count);
    CHECK_AND_VERIFY_STRING_RESOURCE(anj,
                                     ANJ_MAKE_RESOURCE_PATH(3, 0, 19),
                                     SOFTWARE_VERSION_STR,
                                     out_record,
                                     out_res_count);
}

ANJ_UNIT_TEST(dm_device_object, err_codes) {
    DM_INITIALIZE_BASIC(anj);
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 2);

    anj_dm_device_object_init_t dev_obj_init = {
        .manufacturer = MANUFACTURER_STR,
        .model_number = MODEL_NUMBER_STR,
        .serial_number = SERIAL_NUMBER_STR,
        .firmware_version = FIRMWARE_VERSION_STR,
        .reboot_cb = NULL
    };

    ANJ_UNIT_ASSERT_SUCCESS(
            anj_dm_device_obj_install(&anj, &device_obj, &dev_obj_init));
    ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 3);

    size_t out_res_count = 0;
    anj_io_out_entry_t out_record = { 0 };
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(3, 0, 11);

    // object initialized - no error code
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_READ, false, &path));
    _anj_dm_get_readable_res_count(&anj, &out_res_count);
    ANJ_UNIT_ASSERT_EQUAL(out_res_count, 1);
    ANJ_UNIT_ASSERT_EQUAL(_anj_dm_get_read_entry(&anj, &out_record),
                          _ANJ_DM_LAST_RECORD);
    VERIFY_INT_ENTRY(out_record, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 11, 0),
                     0);
    _anj_dm_operation_end(&anj, ANJ_DM_TRANSACTION_SUCCESS);
}
