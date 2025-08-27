/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/fw_update.h>
#include <anj/utils.h>

#include "../../../src/anj/dm/dm_core.h"
#include "../../../src/anj/dm/dm_io.h"
#include "../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_DEFAULT_FOTA_OBJ

#    define EXAMPLE_URI "coap://eu.iot.avsystem.cloud:5663"

#    define BEGIN_READ                                   \
        do {                                             \
        ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin( \
                &anj, ANJ_OP_DM_READ, false, &ANJ_MAKE_OBJECT_PATH(5)))

#    define END_READ                                          \
        ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj)); \
        }                                                     \
        while (false)

#    define SUCCESS 1
#    define FAIL 0

#    define PERFORM_RESOURCE_READ(Rid, result)                          \
        do {                                                            \
            if (result) {                                               \
                ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_resource_value(     \
                        &anj, &ANJ_MAKE_RESOURCE_PATH(5, 0, Rid), &val, \
                        &out_type, NULL));                              \
            } else {                                                    \
                ANJ_UNIT_ASSERT_FAILED(_anj_dm_get_resource_value(      \
                        &anj, &ANJ_MAKE_RESOURCE_PATH(5, 0, Rid), &val, \
                        &out_type, NULL));                              \
            }                                                           \
        } while (false)

#    define PERFORM_RESOURCE_INSTANCE_READ(Rid, Riid, result)              \
        do {                                                               \
            if (result) {                                                  \
                ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_get_resource_value(        \
                        &anj,                                              \
                        &ANJ_MAKE_RESOURCE_INSTANCE_PATH(5, 0, Rid, Riid), \
                        &val, &out_type, NULL));                           \
            } else {                                                       \
                ANJ_UNIT_ASSERT_FAILED(_anj_dm_get_resource_value(         \
                        &anj,                                              \
                        &ANJ_MAKE_RESOURCE_INSTANCE_PATH(5, 0, Rid, Riid), \
                        &val, &out_type, NULL));                           \
            }                                                              \
        } while (false)

typedef struct {
    char order[10];
    bool fail;
} arg_t;

#    define INIT_ENV_DM(handlers)                                \
        arg_t user_arg = { 0 };                                  \
        anj_t anj = { 0 };                                       \
        _anj_dm_initialize(&anj);                                \
        anj_dm_fw_update_entity_ctx_t fu_ctx;                    \
        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_fw_update_object_install( \
                &anj, &fu_ctx, &(handlers), &user_arg));         \
        ANJ_UNIT_ASSERT_EQUAL(anj.dm.objs_count, 1);             \
        anj_res_value_t val;                                     \
        anj_data_type_t out_type

static const char pkg_name[] = "dm_test_name";
static const char pkg_ver[] = "dm_test_ver";
static uint8_t package_buffer[512];
static size_t package_buffer_offset;
static anj_dm_fw_update_result_t result_to_return;
static char expected_uri[256];

static anj_dm_fw_update_result_t
user_package_write_start_handler(void *user_ptr) {
    arg_t *arg = (arg_t *) user_ptr;
    strcat(arg->order, "0");
    return result_to_return;
}

static anj_dm_fw_update_result_t
user_package_write_handler(void *user_ptr, const void *data, size_t data_size) {
    arg_t *arg = (arg_t *) user_ptr;
    strcat(arg->order, "1");
    memcpy(&package_buffer[package_buffer_offset], data, data_size);
    package_buffer_offset += data_size;
    return result_to_return;
}

static anj_dm_fw_update_result_t
user_package_write_finish_handler(void *user_ptr) {
    arg_t *arg = (arg_t *) user_ptr;
    strcat(arg->order, "2");
    return result_to_return;
}

static anj_dm_fw_update_result_t user_uri_write_handler(void *user_ptr,
                                                        const char *uri) {
    arg_t *arg = (arg_t *) user_ptr;
    strcat(arg->order, "3");
    ANJ_UNIT_ASSERT_EQUAL_STRING(uri, expected_uri);
    return result_to_return;
}

static int user_update_start_handler(void *user_ptr) {
    arg_t *arg = (arg_t *) user_ptr;
    strcat(arg->order, "4");
    if (arg->fail) {
        return 1;
    } else {
        return 0;
    }
}

static const char *user_get_name(void *user_ptr) {
    arg_t *arg = (arg_t *) user_ptr;
    strcat(arg->order, "5");
    if (arg->fail) {
        return NULL;
    } else {
        return pkg_name;
    }
}

static const char *user_get_ver(void *user_ptr) {
    arg_t *arg = (arg_t *) user_ptr;
    strcat(arg->order, "6");
    if (arg->fail) {
        return NULL;
    } else {
        return pkg_ver;
    }
}

static void user_reset_handler(void *user_ptr) {
    arg_t *arg = (arg_t *) user_ptr;
    strcat(arg->order, "7");
}

static anj_dm_fw_update_handlers_t handlers = {
    .package_write_start_handler = &user_package_write_start_handler,
    .package_write_handler = &user_package_write_handler,
    .package_write_finish_handler = &user_package_write_finish_handler,
    .uri_write_handler = &user_uri_write_handler,
    .update_start_handler = &user_update_start_handler,
    .get_name = &user_get_name,
    .get_version = &user_get_ver,
    .reset_handler = &user_reset_handler
};

ANJ_UNIT_TEST(dm_fw_update, reading_resources) {
    INIT_ENV_DM(handlers);

    BEGIN_READ;
    // Package
    PERFORM_RESOURCE_READ(0, FAIL);
    // URI
    PERFORM_RESOURCE_READ(1, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, "");
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_STRING);
    // update
    PERFORM_RESOURCE_READ(2, FAIL);
    // state
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_INT);
    // no resource
    PERFORM_RESOURCE_READ(4, FAIL);
    // result
    PERFORM_RESOURCE_READ(5, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_RESULT_INITIAL);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_INT);
    // pkg name
    PERFORM_RESOURCE_READ(6, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, pkg_name);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_STRING);
    // pkg version
    PERFORM_RESOURCE_READ(7, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, pkg_ver);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_STRING);
    // protocol support
    PERFORM_RESOURCE_INSTANCE_READ(8, 0, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_INT);
    PERFORM_RESOURCE_INSTANCE_READ(8, 1, FAIL);
    PERFORM_RESOURCE_INSTANCE_READ(8, 2, FAIL);
    PERFORM_RESOURCE_INSTANCE_READ(8, 3, FAIL);
    PERFORM_RESOURCE_INSTANCE_READ(8, 4, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, 4);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_INT);
    PERFORM_RESOURCE_INSTANCE_READ(8, 5, FAIL);
    PERFORM_RESOURCE_READ(9, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_INT);
    END_READ;

    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "56");
}

static anj_dm_fw_update_handlers_t handlers_simple = {
    .package_write_start_handler = &user_package_write_start_handler,
    .package_write_handler = &user_package_write_handler,
    .package_write_finish_handler = &user_package_write_finish_handler,
    .uri_write_handler = &user_uri_write_handler,
    .update_start_handler = &user_update_start_handler,
    .reset_handler = &user_reset_handler
};

ANJ_UNIT_TEST(dm_fw_update, simple_handlers) {
    INIT_ENV_DM(handlers_simple);

    BEGIN_READ;
    PERFORM_RESOURCE_READ(6, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, NULL);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_STRING);

    PERFORM_RESOURCE_READ(7, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, NULL);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_STRING);
    END_READ;
}

ANJ_UNIT_TEST(dm_fw_update, null_pkg_metadata) {
    INIT_ENV_DM(handlers_simple);

    user_arg.fail = true;
    BEGIN_READ;
    PERFORM_RESOURCE_READ(6, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, NULL);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_STRING);

    PERFORM_RESOURCE_READ(7, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, NULL);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_STRING);
    END_READ;
}

#    if defined(ANJ_FOTA_WITH_PUSH_METHOD) \
            && !defined(ANJ_FOTA_WITH_PULL_METHOD)
static anj_dm_fw_update_handlers_t handlers_simple_push = {
    .package_write_start_handler = &user_package_write_start_handler,
    .package_write_handler = &user_package_write_handler,
    .package_write_finish_handler = &user_package_write_finish_handler,
    .update_start_handler = &user_update_start_handler,
    .reset_handler = &user_reset_handler
};

ANJ_UNIT_TEST(dm_fw_update, simple_handlers_push_only) {
    INIT_ENV_DM(handlers_simple_push);

    // reset the testing buffer
    memset(package_buffer, '\0', sizeof(package_buffer));
    package_buffer_offset = 0;

    // write partial data
    uint8_t data[256] = { 1 };
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_BYTES,
        .value.bytes_or_string.data = data,
        .value.bytes_or_string.chunk_length = 250,
        .value.bytes_or_string.full_length_hint = 256,
        .path = ANJ_MAKE_RESOURCE_PATH(5, 0, 0)
    };
    result_to_return = ANJ_DM_FW_UPDATE_RESULT_SUCCESS;

    // can't set the result in idle state
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_fw_update_object_set_download_result(&anj, &fu_ctx, 0));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    // write start and write
    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "01");

    // check if state == idle
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
    END_READ;

    // write the rest
    record.value.bytes_or_string.chunk_length = 6;
    record.value.bytes_or_string.offset = 250;
    err_to_return = ANJ_DM_FW_UPDATE_RESULT_SUCCESS;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    // write start, 2 writes and write finish
    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "0112");

    // check if state == DOWNLOADED and result applied
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_DOWNLOADED);
    PERFORM_RESOURCE_READ(5, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_RESULT_SUCCESS);

    PERFORM_RESOURCE_READ(9, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_INT);
    END_READ;

    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "56");
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(package_buffer, data, 256);
}
#    endif // defined(ANJ_FOTA_WITH_PUSH_METHOD) &&
           // !defined(ANJ_FOTA_WITH_PULL_METHOD)

#    if !defined(ANJ_FOTA_WITH_PUSH_METHOD) \
            && defined(ANJ_FOTA_WITH_PULL_METHOD)
static anj_dm_fw_update_handlers_t handlers_simple_pull = {
    .uri_write_handler = &user_uri_write_handler,
    .update_start_handler = &user_update_start_handler,
    .reset_handler = &user_reset_handler
};

ANJ_UNIT_TEST(dm_fw_update, simple_handlers_pull_only) {
    INIT_ENV_DM(handlers_simple_pull);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_STRING,
        .value.bytes_or_string.data = EXAMPLE_URI,
        .value.bytes_or_string.chunk_length = strlen(EXAMPLE_URI),
        .value.bytes_or_string.full_length_hint = strlen(EXAMPLE_URI),
        .path = ANJ_MAKE_RESOURCE_PATH(5, 0, 1)
    };
    strcpy(expected_uri, EXAMPLE_URI);
    result_to_return = ANJ_DM_FW_UPDATE_RESULT_SUCCESS;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 1)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    // check if state == downloading
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_DOWNLOADING);

    // check if uri applied to the resource
    PERFORM_RESOURCE_READ(1, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, EXAMPLE_URI);

    PERFORM_RESOURCE_READ(9, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_type, ANJ_DATA_TYPE_INT);
    END_READ;

    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "56");
}
#    endif // !defined(ANJ_FOTA_WITH_PUSH_METHOD) &&
           // defined(ANJ_FOTA_WITH_PULL_METHOD)

ANJ_UNIT_TEST(dm_fw_update, write_uri) {
    INIT_ENV_DM(handlers);

    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_STRING,
        .value.bytes_or_string.data = EXAMPLE_URI,
        .value.bytes_or_string.chunk_length = strlen(EXAMPLE_URI),
        .value.bytes_or_string.full_length_hint = strlen(EXAMPLE_URI),
        .path = ANJ_MAKE_RESOURCE_PATH(5, 0, 1)
    };
    strcpy(expected_uri, EXAMPLE_URI);
    result_to_return = ANJ_DM_FW_UPDATE_RESULT_INITIAL;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 1)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    BEGIN_READ;
    // check if state == downloading
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_DOWNLOADING);
    // check if uri applied to the resource
    PERFORM_RESOURCE_READ(1, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, EXAMPLE_URI);
    END_READ;

    // cancel by empty write
    record.value.bytes_or_string.chunk_length = 0;
    record.value.bytes_or_string.full_length_hint = 0;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 1)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

    // check if state == idle
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
    // check if uri applied to the resource
    PERFORM_RESOURCE_READ(1, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, "");
    END_READ;

    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "37");

    // check if a wrong URI works properly
    record.value.bytes_or_string.data = "wrong::uri";
    record.value.bytes_or_string.chunk_length = strlen("wrong::uri") + 1;
    record.value.bytes_or_string.full_length_hint = strlen("wrong::uri") + 1;
    strcpy(expected_uri, "wrong::uri");
    result_to_return = ANJ_DM_FW_UPDATE_RESULT_INVALID_URI;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 1)));
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_write_entry(&anj, &record));
    int ret = _anj_dm_operation_end(&anj);
    ANJ_UNIT_ASSERT_EQUAL(ret, ANJ_DM_ERR_BAD_REQUEST);

    // check if state == idle
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
    // check if uri applied to the resource
    PERFORM_RESOURCE_READ(1, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL_STRING(val.bytes_or_string.data, "wrong::uri");
    // check result applied
    PERFORM_RESOURCE_READ(5, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_RESULT_INVALID_URI);
    END_READ;
}

ANJ_UNIT_TEST(dm_fw_update, write_package_success) {
    INIT_ENV_DM(handlers);

    // reset the testing buffer
    memset(package_buffer, '\0', sizeof(package_buffer));
    package_buffer_offset = 0;

    // write partial data
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = 1;
    }
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_BYTES,
        .value.bytes_or_string.data = data,
        .value.bytes_or_string.chunk_length = 250,
        .value.bytes_or_string.full_length_hint = 256,
        .path = ANJ_MAKE_RESOURCE_PATH(5, 0, 0)
    };
    result_to_return = ANJ_DM_FW_UPDATE_RESULT_SUCCESS;

    // can't set the result in idle state
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_fw_update_object_set_download_result(&anj, &fu_ctx, 0));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    // write start and write
    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "01");

    // check if state == IDLE
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
    END_READ;

    // write the rest
    record.value.bytes_or_string.chunk_length = 6;
    record.value.bytes_or_string.offset = 250;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    // write start, 2 writes and write finish
    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "0112");

    // check if state == DOWNLOADED and result applied
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_DOWNLOADED);
    PERFORM_RESOURCE_READ(5, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_RESULT_INITIAL);
    END_READ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(package_buffer, data, 256);
}

ANJ_UNIT_TEST(dm_fw_update, write_package_failed) {
    INIT_ENV_DM(handlers);

    // reset the testing buffer
    memset(package_buffer, '\0', sizeof(package_buffer));
    package_buffer_offset = 0;

    // write partial data
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = 1;
    }
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_BYTES,
        .value.bytes_or_string.data = data,
        .value.bytes_or_string.chunk_length = 250,
        .value.bytes_or_string.full_length_hint = 256,
        .path = ANJ_MAKE_RESOURCE_PATH(5, 0, 0)
    };
    result_to_return = ANJ_DM_FW_UPDATE_RESULT_SUCCESS;

    // can't set the result in idle state
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_fw_update_object_set_download_result(&anj, &fu_ctx, 0));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    // write start and write
    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "01");

    // check if state == idle
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
    END_READ;

    // write the rest
    record.value.bytes_or_string.chunk_length = 6;
    record.value.bytes_or_string.offset = 250;
    result_to_return = ANJ_DM_FW_UPDATE_RESULT_FAILED;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 0)));
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_write_entry(&anj, &record));
    int ret = _anj_dm_operation_end(&anj);
    ANJ_UNIT_ASSERT_EQUAL(ret, ANJ_DM_ERR_INTERNAL);
    // write start, write, write finish
    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "0117");

    // check if state == IDLE and result applied
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
    PERFORM_RESOURCE_READ(5, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_RESULT_FAILED);
    END_READ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(package_buffer, data, 256);
}

ANJ_UNIT_TEST(dm_fw_update, write_package_failed_integrity) {
    INIT_ENV_DM(handlers);

    // reset the testing buffer
    memset(package_buffer, '\0', sizeof(package_buffer));
    package_buffer_offset = 0;

    // write partial data
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = 1;
    }
    anj_io_out_entry_t record = {
        .type = ANJ_DATA_TYPE_BYTES,
        .value.bytes_or_string.data = data,
        .value.bytes_or_string.chunk_length = 250,
        .value.bytes_or_string.full_length_hint = 256,
        .path = ANJ_MAKE_RESOURCE_PATH(5, 0, 0)
    };
    result_to_return = ANJ_DM_FW_UPDATE_RESULT_SUCCESS;

    // can't set the result in idle state
    ANJ_UNIT_ASSERT_FAILED(
            anj_dm_fw_update_object_set_download_result(&anj, &fu_ctx, 0));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 0)));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));
    // write start and write
    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "01");

    // check if state == idle
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
    END_READ;

    // write the rest
    record.value.bytes_or_string.chunk_length = 6;
    record.value.bytes_or_string.offset = 250;
    result_to_return = ANJ_DM_FW_UPDATE_RESULT_INTEGRITY_FAILURE;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_dm_operation_begin(&anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                                    &ANJ_MAKE_RESOURCE_PATH(5, 0, 0)));
    ANJ_UNIT_ASSERT_FAILED(_anj_dm_write_entry(&anj, &record));
    int ret = _anj_dm_operation_end(&anj);
    ANJ_UNIT_ASSERT_EQUAL(ret, ANJ_DM_ERR_INTERNAL);
    // write start, write, write finish and reset
    ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "0117");

    // check if state == IDLE and result applied
    BEGIN_READ;
    PERFORM_RESOURCE_READ(3, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
    PERFORM_RESOURCE_READ(5, SUCCESS);
    ANJ_UNIT_ASSERT_EQUAL(val.int_value,
                          ANJ_DM_FW_UPDATE_RESULT_INTEGRITY_FAILURE);
    END_READ;

    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(package_buffer, data, 256);
}

ANJ_UNIT_TEST(dm_fw_update, execute) {
    for (int i = 0; i < 2; i++) {
        INIT_ENV_DM(handlers);

        // start with different update result value, which may simulate second
        // fota update
        if (i == 1) {
            fu_ctx.repr.result = ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
        }

        anj_io_out_entry_t record = {
            .type = ANJ_DATA_TYPE_STRING,
            .value.bytes_or_string.data = EXAMPLE_URI,
            .value.bytes_or_string.chunk_length = strlen(EXAMPLE_URI),
            .value.bytes_or_string.full_length_hint = strlen(EXAMPLE_URI),
            .path = ANJ_MAKE_RESOURCE_PATH(5, 0, 1)
        };
        strcpy(expected_uri, EXAMPLE_URI);
        result_to_return = ANJ_DM_FW_UPDATE_RESULT_INITIAL;

        ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_begin(
                &anj, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, false,
                &ANJ_MAKE_RESOURCE_PATH(5, 0, 1)));
        ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_write_entry(&anj, &record));
        ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

        ANJ_UNIT_ASSERT_SUCCESS(anj_dm_fw_update_object_set_download_result(
                &anj, &fu_ctx, ANJ_DM_FW_UPDATE_RESULT_SUCCESS));

        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_dm_operation_begin(&anj, ANJ_OP_DM_EXECUTE, false,
                                        &ANJ_MAKE_RESOURCE_PATH(5, 0, 2)));
        ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_execute(&anj, NULL, 0));
        ANJ_UNIT_ASSERT_SUCCESS(_anj_dm_operation_end(&anj));

        // check if state == UPDATING
        BEGIN_READ;
        PERFORM_RESOURCE_READ(3, SUCCESS);
        ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_UPDATING);
        PERFORM_RESOURCE_READ(5, SUCCESS);
        ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_RESULT_INITIAL);
        END_READ;

        anj_dm_fw_update_object_set_update_result(
                &anj, &fu_ctx, ANJ_DM_FW_UPDATE_RESULT_SUCCESS);

        // check if state == IDLE and result did not apply
        BEGIN_READ;
        PERFORM_RESOURCE_READ(3, SUCCESS);
        ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_STATE_IDLE);
        PERFORM_RESOURCE_READ(5, SUCCESS);
        ANJ_UNIT_ASSERT_EQUAL(val.int_value, ANJ_DM_FW_UPDATE_RESULT_SUCCESS);
        END_READ;

        ANJ_UNIT_ASSERT_EQUAL_STRING(user_arg.order, "34");
    }
}

#endif // ANJ_WITH_DEFAULT_FOTA_OBJ
