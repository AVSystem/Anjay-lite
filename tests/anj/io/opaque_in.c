/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#define ANJ_UNIT_ENABLE_SHORT_ASSERTS

#include <stdbool.h>
#include <stddef.h>

#include <anj/defs.h>
#include <anj/utils.h>

#include "../../../src/anj/coap/coap.h"
#include "../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#define TEST_ENV(Data, Path, PayloadFinished)                                \
    _anj_io_in_ctx_t ctx;                                                    \
    ASSERT_OK(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,      \
                                  &(Path), _ANJ_COAP_FORMAT_OPAQUE_STREAM)); \
    const anj_res_value_t *value = NULL;                                     \
    const anj_uri_path_t *path = NULL;                                       \
    ASSERT_OK(_anj_io_in_ctx_feed_payload(&ctx, Data, sizeof(Data) - 1,      \
                                          PayloadFinished))

static const anj_uri_path_t TEST_INSTANCE_PATH =
        _ANJ_URI_PATH_INITIALIZER(3, 4, ANJ_ID_INVALID, ANJ_ID_INVALID, 2);

#define MAKE_TEST_RESOURCE_PATH(Rid)                            \
    (ANJ_MAKE_RESOURCE_PATH(TEST_INSTANCE_PATH.ids[ANJ_ID_OID], \
                            TEST_INSTANCE_PATH.ids[ANJ_ID_IID], (Rid)))

ANJ_UNIT_TEST(opaque_in, disambiguation) {
    char TEST_DATA[] = "Hello, world!";
    TEST_ENV(TEST_DATA, MAKE_TEST_RESOURCE_PATH(5), true);
    anj_data_type_t type_bitmask = ANJ_DATA_TYPE_ANY;
    ASSERT_OK(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path));
    ASSERT_EQ(type_bitmask, ANJ_DATA_TYPE_BYTES);
    ASSERT_NOT_NULL(value);
    ASSERT_NOT_NULL(path);
    ASSERT_TRUE(anj_uri_path_equal(path, &ANJ_MAKE_RESOURCE_PATH(3, 4, 5)));
    ASSERT_EQ(value->bytes_or_string.chunk_length, sizeof(TEST_DATA) - 1);
    ASSERT_EQ_BYTES(value->bytes_or_string.data, "Hello, world!");
    ASSERT_EQ(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path),
              _ANJ_IO_EOF);
    ASSERT_EQ(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path),
              _ANJ_IO_ERR_LOGIC);
}

ANJ_UNIT_TEST(opaque_in, bytes) {
    char TEST_DATA[] = "Hello, world!";
    TEST_ENV(TEST_DATA, MAKE_TEST_RESOURCE_PATH(5), true);
    anj_data_type_t type_bitmask = ANJ_DATA_TYPE_BYTES;
    ASSERT_OK(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path));
    ASSERT_EQ(type_bitmask, ANJ_DATA_TYPE_BYTES);
    ASSERT_NOT_NULL(value);
    ASSERT_NOT_NULL(path);
    ASSERT_TRUE(anj_uri_path_equal(path, &ANJ_MAKE_RESOURCE_PATH(3, 4, 5)));
    ASSERT_EQ(value->bytes_or_string.chunk_length, sizeof(TEST_DATA) - 1);
    ASSERT_EQ_BYTES(value->bytes_or_string.data, "Hello, world!");
    ASSERT_EQ(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path),
              _ANJ_IO_EOF);
    ASSERT_EQ(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path),
              _ANJ_IO_ERR_LOGIC);
}

ANJ_UNIT_TEST(opaque_in, bytes_in_parts) {
    char TEST_DATA_1[] = "Hello";
    char TEST_DATA_2[] = ", world!";
    TEST_ENV(TEST_DATA_1, MAKE_TEST_RESOURCE_PATH(5), false);
    anj_data_type_t type_bitmask = ANJ_DATA_TYPE_BYTES;
    ASSERT_OK(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path));
    ASSERT_EQ(type_bitmask, ANJ_DATA_TYPE_BYTES);
    ASSERT_TRUE(anj_uri_path_equal(path, &ANJ_MAKE_RESOURCE_PATH(3, 4, 5)));
    ASSERT_EQ(value->bytes_or_string.chunk_length, sizeof(TEST_DATA_1) - 1);
    ASSERT_EQ_BYTES(value->bytes_or_string.data, "Hello");
    ASSERT_EQ(value->bytes_or_string.full_length_hint, 0);
    ASSERT_EQ(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path),
              _ANJ_IO_WANT_NEXT_PAYLOAD);
    ASSERT_OK(_anj_io_in_ctx_feed_payload(&ctx, TEST_DATA_2,
                                          sizeof(TEST_DATA_2) - 1, true));
    ASSERT_OK(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path));
    ASSERT_EQ(type_bitmask, ANJ_DATA_TYPE_BYTES);
    ASSERT_TRUE(anj_uri_path_equal(path, &ANJ_MAKE_RESOURCE_PATH(3, 4, 5)));
    ASSERT_EQ(value->bytes_or_string.chunk_length, sizeof(TEST_DATA_2) - 1);
    ASSERT_EQ_BYTES(value->bytes_or_string.data, ", world!");
    ASSERT_EQ(value->bytes_or_string.full_length_hint,
              sizeof(TEST_DATA_1) + sizeof(TEST_DATA_2) - 2);
}

ANJ_UNIT_TEST(opaque_in, unsupported_data_types) {
    const anj_data_type_t DATA_TYPES[] = {
        ANJ_DATA_TYPE_NULL,   ANJ_DATA_TYPE_STRING, ANJ_DATA_TYPE_INT,
        ANJ_DATA_TYPE_DOUBLE, ANJ_DATA_TYPE_BOOL,   ANJ_DATA_TYPE_OBJLNK,
        ANJ_DATA_TYPE_UINT,   ANJ_DATA_TYPE_TIME
    };
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(DATA_TYPES); ++i) {
        char TEST_DATA[] = "Hello, world!";
        TEST_ENV(TEST_DATA, MAKE_TEST_RESOURCE_PATH(5), true);
        anj_data_type_t type_bitmask = DATA_TYPES[i];
        ASSERT_EQ(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path),
                  _ANJ_IO_ERR_FORMAT);
        ASSERT_EQ(type_bitmask, ANJ_DATA_TYPE_NULL);
        ASSERT_NULL(value);
        ASSERT_NOT_NULL(path);
        ASSERT_TRUE(anj_uri_path_equal(path, &ANJ_MAKE_RESOURCE_PATH(3, 4, 5)));
    }
}

ANJ_UNIT_TEST(opaque_in, bytes_no_data_with_payload_finished) {
    TEST_ENV("", MAKE_TEST_RESOURCE_PATH(5), true);
    anj_data_type_t type_bitmask = ANJ_DATA_TYPE_BYTES;
    ASSERT_OK(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path));
    ASSERT_TRUE(anj_uri_path_equal(path, &ANJ_MAKE_RESOURCE_PATH(3, 4, 5)));
    ASSERT_EQ(value->bytes_or_string.chunk_length, 0);
    ASSERT_EQ(value->bytes_or_string.offset, 0);
    ASSERT_EQ(value->bytes_or_string.full_length_hint, 0);
    ASSERT_NULL(value->bytes_or_string.data);
    ASSERT_EQ(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path),
              _ANJ_IO_EOF);
    ASSERT_EQ(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path),
              _ANJ_IO_ERR_LOGIC);
    ASSERT_EQ(_anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value, &path),
              _ANJ_IO_ERR_LOGIC);
}
