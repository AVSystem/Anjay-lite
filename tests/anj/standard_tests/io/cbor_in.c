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

#ifdef ANJ_WITH_CBOR

static void anj_uri_path_t_compare(const anj_uri_path_t *a,
                                   const anj_uri_path_t *b) {
    ANJ_UNIT_ASSERT_EQUAL(a->uri_len, b->uri_len);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(a->ids, b->ids, sizeof(a->ids));
}

#    define _TEST_RESOURCE_PATH                   \
        const anj_uri_path_t TEST_RESOURCE_PATH = \
                ANJ_MAKE_RESOURCE_PATH(12, 34, 56);

ANJ_UNIT_TEST(raw_cbor_in, invalid_paths) {
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_init(&ctx,
                                              ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                              NULL, _ANJ_COAP_FORMAT_CBOR),
                          _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &ANJ_MAKE_ROOT_PATH(), _ANJ_COAP_FORMAT_CBOR),
            _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_init(&ctx,
                                              ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                              &ANJ_MAKE_OBJECT_PATH(12),
                                              _ANJ_COAP_FORMAT_CBOR),
                          _ANJ_IO_ERR_FORMAT);
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_init(&ctx,
                                              ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                              &ANJ_MAKE_INSTANCE_PATH(12, 34),
                                              _ANJ_COAP_FORMAT_CBOR),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(raw_cbor_in, invalid_type) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = {
        "\xF6" // null
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(raw_cbor_in, single_integer) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = {
        "\x18\x2A" // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 42);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, single_negative_integer) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = {
        "\x38\x29" // negative(41)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, -42);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, single_half_float) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = { "\xF9\x44\x80" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, 4.5);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, single_decimal_fraction) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = {
        "\xC4"     // tag(4)
        "\x82"     // array(2)
        "\x20"     // negative(0)
        "\x18\x2D" // unsigned(45)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, 4.5);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, single_boolean) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = { "\xF5" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BOOL);
    ANJ_UNIT_ASSERT_EQUAL(value->bool_value, true);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

#    ifdef ANJ_WITH_CBOR_DECODE_STRING_TIME
ANJ_UNIT_TEST(raw_cbor_in, single_string_time) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = "\xC0\x74"
                      "2003-12-13T18:30:02Z";
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_TIME);
    ANJ_UNIT_ASSERT_EQUAL(value->time_value, 1071340202);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}
#    endif // ANJ_WITH_CBOR_DECODE_STRING_TIME

ANJ_UNIT_TEST(raw_cbor_in, single_objlnk) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = {
        "\x69"
        "1234:5678" // text(9)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_OBJLNK;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_EQUAL(value->objlnk.oid, 1234);
    ANJ_UNIT_ASSERT_EQUAL(value->objlnk.iid, 5678);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, single_objlnk_split) {
    _TEST_RESOURCE_PATH
    const char RESOURCE[] = {
        "\x6B"
        "12345:65432" // text(11)
    };
    for (size_t split = 0; split < 9; ++split) {
        char tmp[sizeof(RESOURCE)];
        memcpy(tmp, RESOURCE, sizeof(tmp));

        _anj_io_in_ctx_t ctx;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
                &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, &TEST_RESOURCE_PATH,
                _ANJ_COAP_FORMAT_CBOR));
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_feed_payload(&ctx, tmp, split, false));

        size_t count;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
        ANJ_UNIT_ASSERT_EQUAL(count, 1);

        anj_data_type_t type = ANJ_DATA_TYPE_ANY;
        const anj_res_value_t *value;
        const anj_uri_path_t *path;
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                &ctx, tmp + split, sizeof(tmp) - split - 1, true));

        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
        ANJ_UNIT_ASSERT_EQUAL(type,
                              ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
        ANJ_UNIT_ASSERT_NULL(value);
        anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

        type = ANJ_DATA_TYPE_OBJLNK;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_OBJLNK);
        ANJ_UNIT_ASSERT_EQUAL(value->objlnk.oid, 12345);
        ANJ_UNIT_ASSERT_EQUAL(value->objlnk.iid, 65432);
        anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

        type = ANJ_DATA_TYPE_ANY;
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_EOF);
    }
    for (size_t split = 9; split < sizeof(RESOURCE) - 1; ++split) {
        char tmp[sizeof(RESOURCE)];
        memcpy(tmp, RESOURCE, sizeof(tmp));

        _anj_io_in_ctx_t ctx;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
                &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, &TEST_RESOURCE_PATH,
                _ANJ_COAP_FORMAT_CBOR));
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_feed_payload(&ctx, tmp, split, false));

        size_t count;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
        ANJ_UNIT_ASSERT_EQUAL(count, 1);

        anj_data_type_t type = ANJ_DATA_TYPE_ANY;
        const anj_res_value_t *value;
        const anj_uri_path_t *path;

        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
        ANJ_UNIT_ASSERT_EQUAL(type,
                              ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
        ANJ_UNIT_ASSERT_NULL(value);
        anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

        type = ANJ_DATA_TYPE_OBJLNK;
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                &ctx, tmp + split, sizeof(tmp) - split - 1, true));

        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_OBJLNK);
        ANJ_UNIT_ASSERT_EQUAL(value->objlnk.oid, 12345);
        ANJ_UNIT_ASSERT_EQUAL(value->objlnk.iid, 65432);
        anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

        type = ANJ_DATA_TYPE_ANY;
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_EOF);
    }
    {
        char tmp[sizeof(RESOURCE)];
        memcpy(tmp, RESOURCE, sizeof(tmp));

        _anj_io_in_ctx_t ctx;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
                &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, &TEST_RESOURCE_PATH,
                _ANJ_COAP_FORMAT_CBOR));
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_feed_payload(&ctx, tmp, sizeof(tmp) - 1, false));

        size_t count;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
        ANJ_UNIT_ASSERT_EQUAL(count, 1);

        anj_data_type_t type = ANJ_DATA_TYPE_ANY;
        const anj_res_value_t *value;
        const anj_uri_path_t *path;

        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
        ANJ_UNIT_ASSERT_EQUAL(type,
                              ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
        ANJ_UNIT_ASSERT_NULL(value);
        anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

        type = ANJ_DATA_TYPE_OBJLNK;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_OBJLNK);
        ANJ_UNIT_ASSERT_EQUAL(value->objlnk.oid, 12345);
        ANJ_UNIT_ASSERT_EQUAL(value->objlnk.iid, 65432);
        anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

        type = ANJ_DATA_TYPE_ANY;
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);

        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_feed_payload(&ctx, NULL, 0, true));

        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_EOF);
    }
}

ANJ_UNIT_TEST(raw_cbor_in, single_objlnk_invalid) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = {
        "\x69#StayHome" // text(9)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_OBJLNK;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(raw_cbor_in, single_string) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = {
        "\x6C#ZostanWDomu" // text(12)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_STRING;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, "#ZostanWDomu");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 12);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 12);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, empty_string) {
    _TEST_RESOURCE_PATH
    char RESOURCE[] = {
        "\x60" // text(0)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_STRING;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, "");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

#    define CHUNK1 "test"
#    define CHUNK2 "string"
#    define TEST_STRING (CHUNK1 CHUNK2)

ANJ_UNIT_TEST(raw_cbor_in, string_indefinite) {
    _TEST_RESOURCE_PATH
    // (_ "test", "string")
    char RESOURCE[] = { "\x7F\x64" CHUNK1 "\x66" CHUNK2 "\xFF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_STRING;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK2);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK2) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                          sizeof(TEST_STRING) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                          sizeof(TEST_STRING) - 1);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, string_indefinite_with_empty_strings) {
    _TEST_RESOURCE_PATH
    // (_ "", "test", "", "string", "")
    char RESOURCE[] = { "\x7F\x60\x64" CHUNK1 "\x60\x66" CHUNK2 "\x60\xFF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_STRING;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK2);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK2) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                          sizeof(TEST_STRING) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                          sizeof(TEST_STRING) - 1);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, string_indefinite_with_empty_strings_split) {
    _TEST_RESOURCE_PATH
    // (_ "", "test", "", "string", "")
    const char RESOURCE[] = { "\x7F\x60\x64" CHUNK1 "\x60\x66" CHUNK2
                              "\x60\xFF" };
    for (size_t split = 0; split < sizeof(RESOURCE); ++split) {
        char tmp[sizeof(RESOURCE)];
        memcpy(tmp, RESOURCE, sizeof(tmp));

        _anj_io_in_ctx_t ctx;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
                &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, &TEST_RESOURCE_PATH,
                _ANJ_COAP_FORMAT_CBOR));
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_feed_payload(&ctx, tmp, split, false));

        size_t count;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
        ANJ_UNIT_ASSERT_EQUAL(count, 1);

        anj_data_type_t type = ANJ_DATA_TYPE_ANY;
        const anj_res_value_t *value;
        const anj_uri_path_t *path;
        size_t expected_offset = 0;
        bool second_chunk_provided = false;
        int result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, tmp + split, sizeof(tmp) - split - 1, true));
            second_chunk_provided = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
        ANJ_UNIT_ASSERT_EQUAL(type,
                              ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
        ANJ_UNIT_ASSERT_NULL(value);
        anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

        type = ANJ_DATA_TYPE_STRING;
        do {
            if ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
                    == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_FALSE(second_chunk_provided);
                ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                        &ctx, tmp + split, sizeof(tmp) - split - 1, true));
                second_chunk_provided = true;
                result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            }
            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
            anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                                  expected_offset);
            if (expected_offset < sizeof(TEST_STRING) - 1) {
                ANJ_UNIT_ASSERT_TRUE(value->bytes_or_string.chunk_length > 0);
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      0);
                ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                        value->bytes_or_string.data,
                        &TEST_STRING[expected_offset],
                        value->bytes_or_string.chunk_length);
                expected_offset += value->bytes_or_string.chunk_length;
            } else {
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      sizeof(TEST_STRING) - 1);
            }
        } while (value->bytes_or_string.offset
                         + value->bytes_or_string.chunk_length
                 != value->bytes_or_string.full_length_hint);

        type = ANJ_DATA_TYPE_ANY;
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_EOF);
    }
}

ANJ_UNIT_TEST(raw_cbor_in, string_indefinite_empty_string) {
    _TEST_RESOURCE_PATH
    // (_ "")
    char RESOURCE[] = { "\x7F\x60\xFF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_STRING;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, string_indefinite_empty_struct) {
    _TEST_RESOURCE_PATH
    // (_ )
    char RESOURCE[] = { "\x7F\xFF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING | ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_STRING;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

#    undef TEST_STRING
#    undef CHUNK1
#    undef CHUNK2

#    define CHUNK1 "\x00\x11\x22\x33\x44\x55"
#    define CHUNK2 "\x66\x77\x88\x99"
#    define TEST_BYTES (CHUNK1 CHUNK2)

ANJ_UNIT_TEST(raw_cbor_in, bytes_indefinite) {
    _TEST_RESOURCE_PATH
    // (_ h'001122334455', h'66778899')
    char RESOURCE[] = { "\x5F\x46" CHUNK1 "\x44" CHUNK2 "\xFF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK2);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK2) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                          sizeof(TEST_BYTES) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                          sizeof(TEST_BYTES) - 1);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, bytes_indefinite_with_empty_strings) {
    _TEST_RESOURCE_PATH
    // (_ h'', h'001122334455', h'', h'66778899', h'')
    char RESOURCE[] = { "\x5F\x40\x46" CHUNK1 "\x40\x44" CHUNK2 "\x40\xFF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
                                &TEST_RESOURCE_PATH, _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, CHUNK2);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, sizeof(CHUNK1) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length,
                          sizeof(CHUNK2) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_BYTES);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                          sizeof(TEST_BYTES) - 1);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                          sizeof(TEST_BYTES) - 1);
    anj_uri_path_t_compare(path, &TEST_RESOURCE_PATH);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(raw_cbor_in, empty_input) {
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_INSTANCE_PATH(12, 34, 56, 78),
            _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(&ctx, NULL, 0, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(raw_cbor_in, invalid_input) {
    char RESOURCE[] = { "\xFF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_INSTANCE_PATH(12, 34, 56, 78),
            _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(raw_cbor_in, overlong_input) {
    char RESOURCE[] = {
        "\x15"     // unsigned(21)
        "\x18\x25" // unsigned(37)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_INSTANCE_PATH(12, 34, 56, 78),
            _ANJ_COAP_FORMAT_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(12, 34, 56, 78));

    type = ANJ_DATA_TYPE_UINT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value->uint_value, 21);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(12, 34, 56, 78));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

#    undef TEST_BYTES
#    undef CHUNK1
#    undef CHUNK2

#endif // ANJ_WITH_CBOR
