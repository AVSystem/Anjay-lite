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
#include <anj/defs.h>
#include <anj/utils.h>

#include "../../../../src/anj/coap/coap.h"
#include "../../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_LWM2M_CBOR

static void anj_uri_path_t_compare(const anj_uri_path_t *a,
                                   const anj_uri_path_t *b) {
    ANJ_UNIT_ASSERT_EQUAL(a->uri_len, b->uri_len);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(a->ids, b->ids, sizeof(a->ids));
}

static void test_single_resource(void *buff, size_t buff_size) {
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &ANJ_MAKE_RESOURCE_PATH(13, 26, 1),
                                _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, buff, buff_size, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 42);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, single_resource) {
    // {[13, 26, 1]: 42}
    char DATA[] = { "\xA1\x83\x0D\x18\x1A\x01\x18\x2A" };
    test_single_resource(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, single_resource_indefinite) {
    // {[13, 26, 1]: 42}
    char DATA[] = { "\xBF\x9F\x0D\x18\x1A\x01\xFF\x18\x2A\xFF" };
    test_single_resource(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, single_resource_nested) {
    // {13: {26: {1: 42}}}
    char DATA[] = { "\xA1\x0D\xA1\x18\x1A\xA1\x01\x18\x2A" };
    test_single_resource(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, single_resource_nested_indefinite) {
    // {13: {26: {1: 42}}}
    char DATA[] = { "\xBF\x0D\xBF\x18\x1A\xBF\x01\x18\x2A\xFF\xFF\xFF" };
    test_single_resource(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, single_resource_nested_arrays) {
    // {[13]: {[26]: {[1]: 42}}}
    char DATA[] = { "\xA1\x81\x0D\xA1\x81\x18\x1A\xA1\x81\x01\x18\x2A" };
    test_single_resource(DATA, sizeof(DATA) - 1);
}

static void test_single_resource_instance(void *buff, size_t buff_size) {
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 2),
                                _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, buff, buff_size, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 2));

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, 4.5);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 2));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource_instance, max_possible_nesting) {
    // Uses decimal fraction
    // {[13]: {[26]: {[1]: {[2]: 4([-1, 45])}}}}
    char DATA[] = { "\xA1\x81\x0D\xA1\x81\x18\x1A\xA1\x81\x01"
                    "\xA1\x81\x02\xC4\x82\x20\x18\x2D" };
    test_single_resource_instance(DATA, sizeof(DATA) - 1);
}

static void test_two_resources(void *buff, size_t buff_size) {
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_INSTANCE_PATH(13, 26), _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, buff, buff_size, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 42);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 2));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 21);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 2));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, two_resources_1) {
    // {[13, 26]: {1: 42, 2: 21}}
    char DATA[] = { "\xA1\x82\x0D\x18\x1A\xA2\x01\x18\x2A\x02\x15" };
    test_two_resources(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, two_resources_2) {
    // {[13, 26, 1]: 42, [13, 26, 2]: 21}
    char DATA[] = {
        "\xA2\x83\x0D\x18\x1A\x01\x18\x2A\x83\x0D\x18\x1A\x02\x15"
    };
    test_two_resources(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, two_resources_3) {
    // {[13, 26]: {1: 42}, [13, 26, 2]: 21}
    char DATA[] = {
        "\xA2\x82\x0D\x18\x1A\xA1\x01\x18\x2A\x83\x0D\x18\x1A\x02\x15"
    };
    test_two_resources(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, two_resources_4) {
    // {[13, 26, 1]: 42, [13, 26]: {[2]: 21}}
    char DATA[] = {
        "\xA2\x83\x0D\x18\x1A\x01\x18\x2A\x82\x0D\x18\x1A\xA1\x81\x02\x15"
    };
    test_two_resources(DATA, sizeof(DATA) - 1);
}

#    define TEST_BYTES \
        "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF"

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, bytes) {
    // {[13, 26, 1]: h'00112233445566778899AABBCCDDEEFF'}
    char DATA[] = { "\xA1\x83\x0D\x18\x1A\x01\x50" TEST_BYTES };

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &ANJ_MAKE_RESOURCE_PATH(13, 26, 1),
                                _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, DATA, sizeof(DATA) - 1, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(TEST_BYTES) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                          sizeof(TEST_BYTES) - 1);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, TEST_BYTES);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

#    undef TEST_BYTES

#    define CHUNK1 "\x00\x11\x22\x33\x44\x55\x66\x77"
#    define CHUNK2 "\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF"

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, bytes_indefinite) {
    // {[13, 26, 1]: (_h'0011223344556677', h'8899AABBCCDDEEFF')}
    char DATA[] = { "\xA1\x83\x0D\x18\x1A\x01\x5F\x48" CHUNK1 "\x48" CHUNK2
                    "\xFF" };

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &ANJ_MAKE_RESOURCE_PATH(13, 26, 1),
                                _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, DATA, sizeof(DATA) - 1, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK1);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK2) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK2);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                          sizeof(CHUNK1) + sizeof(CHUNK2) - 2);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                          sizeof(CHUNK1) + sizeof(CHUNK2) - 2);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

#    undef CHUNK1
#    undef CHUNK2

#    define TEST_STRING "c--cossiezepsulo"

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, string) {
    // {[13, 26, 1]: "c--cossiezepsulo"}
    char DATA[] = { "\xA1\x83\x0D\x18\x1A\x01\x70" TEST_STRING };

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &ANJ_MAKE_RESOURCE_PATH(13, 26, 1),
                                _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, DATA, sizeof(DATA) - 1, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_STRING;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(TEST_STRING) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                          sizeof(TEST_STRING) - 1);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, TEST_STRING);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

#    undef TEST_STRING

ANJ_UNIT_TEST(lwm2m_cbor_in_resource_instance, null_and_int) {
    // {[13, 26, 1]: {2: null, 3: 5}}
    char DATA[] = { "\xA1\x83\x0D\x18\x1A\x01\xA2\x02\xF6\x03\x05" };

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &ANJ_MAKE_RESOURCE_PATH(13, 26, 1),
                                _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, DATA, sizeof(DATA) - 1, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_NULL);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 2));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 3));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 5);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 3));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, all_types) {
    // It's important to duplicate some type at the end to ensure that nesting
    // of the paths works correctly for all types.
    // {[13, 26]: {1: 1, 2: -1, 3: 2.5, 4: "test", 5: h'11223344', 6: "12:34",
    // 7: 1}}
    char DATA[] = { "\xA1\x82\x0D\x18\x1A\xA7\x01\x01\x02\x20\x03\xF9\x41\x00"
                    "\x04\x64\x74\x65\x73\x74\x05\x44\x11\x22\x33\x44\x06\x65"
                    "\x31\x32\x3A\x33\x34\x07\x01" };

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_INSTANCE_PATH(13, 26), _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, DATA, sizeof(DATA) - 1, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 1);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 2));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, -1);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 2));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 3));

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, 2.5);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 3));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 4));

    type = ANJ_DATA_TYPE_STRING;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 4));
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 4);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 4);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, "test");

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 5));
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 4);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 4);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data,
                                "\x11\x22\x33\x44");

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 6));

    type = ANJ_DATA_TYPE_OBJLNK;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_OBJLNK);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 6));
    ANJ_UNIT_ASSERT_EQUAL(value->objlnk.oid, 12);
    ANJ_UNIT_ASSERT_EQUAL(value->objlnk.iid, 34);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 7));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 1);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 7));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

static void test_composite(void *buff, size_t buff_size) {
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_COMP, &ANJ_MAKE_ROOT_PATH(),
            _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, buff, buff_size, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 1);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(14, 27, 2));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 2);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(14, 27, 2));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, composite) {
    // {13: {26: {1: 1}}, 14: {27: {2: 2}}}
    char DATA[] = {
        "\xA2\x0D\xA1\x18\x1A\xA1\x01\x01\x0E\xA1\x18\x1B\xA1\x02\x02"
    };
    test_composite(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, composite_indefinite_maps) {
    // {13: {26: {1: 1}}, 14: {27: {2: 2}}}
    char DATA[] = { "\xBF\x0D\xBF\x18\x1A\xBF\x01\x01\xFF\xFF\x0E\xBF\x18\x1B"
                    "\xBF\x02\x02\xFF\xFF\xFF" };
    test_composite(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in_resource, composite_indefinite_maps_and_arrays) {
    // {[13]: {[26]: {[1]: 1}}, [14]: {[27]: {[2]: 2}}}
    char DATA[] = {
        "\xBF\x9F\x0D\xFF\xBF\x9F\x18\x1A\xFF\xBF\x9F\x01\xFF\x01\xFF\xFF\x9F"
        "\x0E\xFF\xBF\x9F\x18\x1B\xFF\xBF\x9F\x02\xFF\x02\xFF\xFF\xFF"
    };
    test_composite(DATA, sizeof(DATA) - 1);
}

ANJ_UNIT_TEST(lwm2m_cbor_in, path_too_long_1) {
    // {[1, 2, 3, 4, 5]: 5}
    char DATA[] = { "\xA1\x85\x0D\x18\x1A\x03\x04\x05\x05" };

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_COMP, &ANJ_MAKE_ROOT_PATH(),
            _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, DATA, sizeof(DATA) - 1, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(lwm2m_cbor_in, path_too_long_2) {
    // {[13, 26, 1]: {2: 5, [3, 4]: 6}}
    char DATA[] = { "\xA1\x83\x0D\x18\x1A\x01\xA2\x02\x05\x82\x03\x04\x05" };

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_COMP, &ANJ_MAKE_ROOT_PATH(),
            _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, DATA, sizeof(DATA) - 1, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 2));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 5);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 2));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(lwm2m_cbor_in, path_too_long_3) {
    // {13: {26: {1: {2: {3: 4}}}}}
    char DATA[] = { "\xA1\x0D\xA1\x18\x1A\xA1\x01\xA1\x02\xA1\x03\x04" };

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_COMP, &ANJ_MAKE_ROOT_PATH(),
            _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_feed_payload(&ctx, DATA, sizeof(DATA) - 1, true));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &(size_t) { 0 }),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(lwm2m_cbor_in, split_payload) {
    // {[13]: {[26]: {[1]: (_ h'0011223344556677', h'8899AABBCCDDEEFF')}},
    //  [14]: {[27]: {[2]: (_ "01234567", "89abcdef")}}}
    static const char DATA[] = {
        "\xBF\x9F\x0D\xFF\xBF\x9F\x18\x1A\xFF\xBF\x9F\x01\xFF\x5F\x48\x00\x11"
        "\x22\x33\x44\x55\x66\x77\x48\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF\xFF\xFF"
        "\xFF\x9F\x0E\xFF\xBF\x9F\x18\x1B\xFF\xBF\x9F\x02\xFF\x7F\x68\x30\x31"
        "\x32\x33\x34\x35\x36\x37\x68\x38\x39\x61\x62\x63\x64\x65\x66\xFF\xFF"
        "\xFF\xFF"
    };

    for (size_t split = 0; split <= sizeof(DATA) - 1; ++split) {
        char copy[sizeof(DATA)];
        memcpy(copy, DATA, sizeof(copy));

        _anj_io_in_ctx_t ctx;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
                &ctx, ANJ_OP_DM_WRITE_COMP, &ANJ_MAKE_ROOT_PATH(),
                _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR));
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_feed_payload(&ctx, copy, split, false));

        bool second_chunk_fed = false;

        anj_data_type_t type = ANJ_DATA_TYPE_ANY;
        const anj_res_value_t *value;
        const anj_uri_path_t *path;
        int result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, copy + split, sizeof(copy) - split - 1, true));
            second_chunk_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 8);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
        ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data,
                                    "\x00\x11\x22\x33\x44\x55\x66\x77");
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

        result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_FALSE(second_chunk_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, copy + split, sizeof(copy) - split - 1, true));
            second_chunk_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 8);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 8);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
        ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data,
                                    "\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF");
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

        result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_FALSE(second_chunk_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, copy + split, sizeof(copy) - split - 1, true));
            second_chunk_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 16);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 16);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

        type = ANJ_DATA_TYPE_ANY;
        result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_FALSE(second_chunk_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, copy + split, sizeof(copy) - split - 1, true));
            second_chunk_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
        ANJ_UNIT_ASSERT_EQUAL(type,
                              ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
        ANJ_UNIT_ASSERT_NULL(value);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(14, 27, 2));

        type = ANJ_DATA_TYPE_STRING;
        result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_FALSE(second_chunk_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, copy + split, sizeof(copy) - split - 1, true));
            second_chunk_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 8);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
        ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, "01234567");
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(14, 27, 2));

        result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_FALSE(second_chunk_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, copy + split, sizeof(copy) - split - 1, true));
            second_chunk_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 8);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 8);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
        ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, "89abcdef");
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(14, 27, 2));

        result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_FALSE(second_chunk_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, copy + split, sizeof(copy) - split - 1, true));
            second_chunk_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 16);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 16);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(14, 27, 2));

        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_EOF);
    }
}

#endif // ANJ_WITH_LWM2M_CBOR
