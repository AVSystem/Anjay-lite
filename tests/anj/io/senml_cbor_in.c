/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/defs.h>

#include "../../../src/anj/coap/coap.h"
#include "../../../src/anj/io/io.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_SENML_CBOR

static void anj_uri_path_t_compare(const anj_uri_path_t *a,
                                   const anj_uri_path_t *b) {
    ANJ_UNIT_ASSERT_EQUAL(a->uri_len, b->uri_len);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(a->ids, b->ids, sizeof(a->ids));
}

ANJ_UNIT_TEST(cbor_in_resource, single_instance) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_resource, single_instance_indefinite_array) {
    char RESOURCE[] = {
        "\x9F"         // array(*)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
        "\xFF"         // primitive(*)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &count),
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

ANJ_UNIT_TEST(cbor_in_resource, single_instance_indefinite_map) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xBF"         // map(*)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
        "\xFF"         // primitive(*)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_resource_permuted, single_instance) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_resource, single_instance_but_more_than_one) {
    char RESOURCES[] = {
        "\x82"         // array(2)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
                       // ,
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/2" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2B"     // unsigned(43)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

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
    // The resource is there, but the context doesn't return it because it is
    // not related to the request resource path /13/26/1. In order to actually
    // get it, we would have to do a request on an instance. Because the context
    // top-level path is restricted, obtaining next id results in error.
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_resource, single_instance_with_first_resource_unrelated) {
    char RESOURCES[] = {
        "\x82"         // array(2)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/2" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
                       // ,
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2B"     // unsigned(43)
    };
    // NOTE: Request is on /13/26/1 but the first resource in the payload is
    // /13/26/2.
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_resource_permuted, single_instance_but_more_than_one) {
    char RESOURCES[] = {
        "\x82"         // array(2)
        "\xA2"         // map(2)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
                       // ,
        "\xA2"         // map(2)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2B"     // unsigned(43)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/2" // text(8)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

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
    // The resource is there, but the context doesn't return it because it is
    // not related to the request resource path /13/26/1. In order to actually
    // get it, we would have to do a request on an instance. Because the context
    // top-level path is restricted, obtaining next id results in error.
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_resource, multiple_instance) {
    char RESOURCES[] = {
        "\x82"           // array(2)
        "\xA2"           // map(2)
        "\x00"           // unsigned(0) => SenML Name
        "\x6A/13/26/1/4" // text(10)
        "\x02"           // unsigned(2) => SenML Value
        "\x18\x2A"       // unsigned(42)
                         // ,
        "\xA2"           // map(2)
        "\x00"           // unsigned(0) => SenML Name
        "\x6A/13/26/1/5" // text(10)
        "\x02"           // unsigned(2) => SenML Value
        "\x18\x2B"       // unsigned(43)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 4));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 42);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 4));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 5));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 43);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 5));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_resource, multiple_instance_strings) {
    char RESOURCES[] = {
        "\x82"           // array(2)
        "\xA2"           // map(2)
        "\x00"           // unsigned(0) => SenML Name
        "\x6A/13/26/1/4" // text(10)
        "\x03"           // unsigned(3) => SenML String
        "\x66"
        "foobar"         // string(foobar)
                         // ,
        "\xA2"           // map(2)
        "\x00"           // unsigned(0) => SenML Name
        "\x6A/13/26/1/5" // text(10)
        "\x03"           // unsigned(3) => SenML String
        "\x63"
        "baz" // string(baz)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, "foobar");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 6);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 6);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 4));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, "baz");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 3);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 3);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 5));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_resource_permuted, multiple_instance) {
    char RESOURCES[] = {
        "\x82"           // array(2)
        "\xA2"           // map(2)
        "\x02"           // unsigned(2) => SenML Value
        "\x18\x2A"       // unsigned(42)
        "\x00"           // unsigned(0) => SenML Name
        "\x6A/13/26/1/4" // text(10)
                         // ,
        "\xA2"           // map(2)
        "\x02"           // unsigned(2) => SenML Value
        "\x18\x2B"       // unsigned(43)
        "\x00"           // unsigned(0) => SenML Name
        "\x6A/13/26/1/5" // text(10)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 4));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 42);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 4));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 5));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 43);
    anj_uri_path_t_compare(path,
                           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(13, 26, 1, 5));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_instance, with_simple_resource) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_INSTANCE_PATH(13, 26), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_instance, with_more_than_one_resource) {
    char RESOURCES[] = {
        "\x82"         // array(2)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
                       // ,
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/2" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2B"     // unsigned(43)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_INSTANCE_PATH(13, 26), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

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
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 43);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 2));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_object, with_single_instance_and_some_resources) {
    char RESOURCES[] = {
        "\x82"         // array(2)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
                       // ,
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/2" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2B"     // unsigned(43)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, &ANJ_MAKE_OBJECT_PATH(13),
            _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

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
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 43);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 2));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_object, with_some_instances_and_some_resources) {
    char RESOURCES[] = {
        "\x84"         // array(4)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
                       // ,
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/2" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2B"     // unsigned(43)
                       //
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/27/3" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2C"     // unsigned(44)
                       // ,
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/27/4" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2D"     // unsigned(45)

    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE, &ANJ_MAKE_OBJECT_PATH(13),
            _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 4);

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
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 43);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 2));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 27, 3));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 44);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 27, 3));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 27, 4));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 45);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 27, 4));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, explicit_null) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\xF6"         // null
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_NULL);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, boolean) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x04"         // unsigned(4) => SenML Boolean
        "\xF5"         // true
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    ANJ_UNIT_ASSERT_TRUE(value->bool_value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, string) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x03"         // unsigned(3) => SenML String
        "\x66"
        "foobar" // string(foobar)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, "foobar");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 6);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 6);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, bytes) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x08"         // unsigned(8) => SenML Data
        "\x46"
        "foobar" // bytes(foobar)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data, "foobar");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 6);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 6);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, value_too_large_for_int) {
    char RESOURCE[] = { "\x81"         // array(1)
                        "\xA2"         // map(2)
                        "\x00"         // unsigned(0) => SenML Name
                        "\x68/13/26/1" // text(8)
                        "\x02"         // unsigned(2) => SenML Value
                        // unsigned(9223372036854775808)
                        "\x1B\x80\x00\x00\x00\x00\x00\x00\x00" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_value, unsigned_int) {
    char RESOURCE[] = { "\x81"         // array(1)
                        "\xA2"         // map(2)
                        "\x00"         // unsigned(0) => SenML Name
                        "\x68/13/26/1" // text(8)
                        "\x02"         // unsigned(2) => SenML Value
                        // unsigned(9223372036854775808)
                        "\x1B\x80\x00\x00\x00\x00\x00\x00\x00" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_UINT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value->uint_value, 9223372036854775808ULL);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, negative_int) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x38\x2A"     // negative(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, -43);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, negative_int_as_unsigned) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x38\x2A"     // negative(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_UINT;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_value, time_losing_precision_ok) {
    char RESOURCE[] = { "\x81"         // array(1)
                        "\xA2"         // map(2)
                        "\x00"         // unsigned(0) => SenML Name
                        "\x68/13/26/1" // text(8)
                        "\x02"         // unsigned(2) => SenML Value
                        // numeric time: primitive(1112470662.694202137)
                        "\xC1\xFB\x41\xD0\x93\xBD\x21\xAC\x6D\xCF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    ANJ_UNIT_ASSERT_EQUAL(value->time_value, 1112470662);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, double) {
    char RESOURCE[] = { "\x81"         // array(1)
                        "\xA2"         // map(2)
                        "\x00"         // unsigned(0) => SenML Name
                        "\x68/13/26/1" // text(8)
                        "\x02"         // unsigned(2) => SenML Value
                        // primitive(1112470662.694202137)
                        "\xFB\x41\xD0\x93\xBD\x21\xAC\x6D\xCF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, 1112470662.694202137);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, u64_as_double_within_range) {
    char RESOURCE[] = {
        "\x81"                                 // array(1)
        "\xA2"                                 // map(2)
        "\x00"                                 // unsigned(0) => SenML Name
        "\x68/13/26/1"                         // text(8)
        "\x02"                                 // unsigned(2) => SenML Value
        "\x1B\x00\x20\x00\x00\x00\x00\x00\x00" // unsigned(9007199254740992)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, UINT64_C(9007199254740992));
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, u64_as_double_out_of_range) {
    char RESOURCE[] = {
        "\x81"                                 // array(1)
        "\xA2"                                 // map(2)
        "\x00"                                 // unsigned(0) => SenML Name
        "\x68/13/26/1"                         // text(8)
        "\x02"                                 // unsigned(2) => SenML Value
        "\x1B\x00\x20\x00\x00\x00\x00\x00\x01" // unsigned(9007199254740993)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    // precision is lost, but we don't care
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, 9007199254740992.0);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, i64_as_double_within_range) {
    char RESOURCE[] = {
        "\x81"                                 // array(1)
        "\xA2"                                 // map(2)
        "\x00"                                 // unsigned(0) => SenML Name
        "\x68/13/26/1"                         // text(8)
        "\x02"                                 // unsigned(2) => SenML Value
        "\x3B\x00\x1F\xFF\xFF\xFF\xFF\xFF\xFF" // negative(9007199254740991)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, -INT64_C(9007199254740992));
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, i64_as_double_out_of_range) {
    char RESOURCE[] = {
        "\x81"                                 // array(1)
        "\xA2"                                 // map(2)
        "\x00"                                 // unsigned(0) => SenML Name
        "\x68/13/26/1"                         // text(8)
        "\x02"                                 // unsigned(2) => SenML Value
        "\x3B\x00\x20\x00\x00\x00\x00\x00\x00" // negative(9007199254740993)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    // precision is lost, but we don't care
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, -9007199254740992.0);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, float_as_i64_when_convertible) {
    char RESOURCE[] = {
        "\x81"                 // array(1)
        "\xA2"                 // map(2)
        "\x00"                 // unsigned(0) => SenML Name
        "\x68/13/26/1"         // text(8)
        "\x02"                 // unsigned(2) => SenML Value
        "\xFA\x40\x40\x00\x00" // simple_f32(3.0)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 3);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, float_as_i64_when_not_convertible) {
    char RESOURCE[] = {
        "\x81"                 // array(1)
        "\xA2"                 // map(2)
        "\x00"                 // unsigned(0) => SenML Name
        "\x68/13/26/1"         // text(8)
        "\x02"                 // unsigned(2) => SenML Value
        "\xFA\x40\x49\x0f\xdb" // simple_f32(3.1415926535)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_value, double_as_i64_when_convertible) {
    char RESOURCE[] = {
        "\x81"                                 // array(1)
        "\xA2"                                 // map(2)
        "\x00"                                 // unsigned(0) => SenML Name
        "\x68/13/26/1"                         // text(8)
        "\x02"                                 // unsigned(2) => SenML Value
        "\xFB\x40\x08\x00\x00\x00\x00\x00\x00" // simple_f64(3)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 3);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, float_as_u64_when_convertible) {
    char RESOURCE[] = {
        "\x81"                 // array(1)
        "\xA2"                 // map(2)
        "\x00"                 // unsigned(0) => SenML Name
        "\x68/13/26/1"         // text(8)
        "\x02"                 // unsigned(2) => SenML Value
        "\xFA\x40\x40\x00\x00" // simple_f32(3.0)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_UINT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value->uint_value, 3);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, float_as_u64_when_not_convertible) {
    char RESOURCE[] = {
        "\x81"                 // array(1)
        "\xA2"                 // map(2)
        "\x00"                 // unsigned(0) => SenML Name
        "\x68/13/26/1"         // text(8)
        "\x02"                 // unsigned(2) => SenML Value
        "\xFA\x40\x49\x0f\xdb" // simple_f32(3.1415926535)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_UINT;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_value, double_as_u64_when_convertible) {
    char RESOURCE[] = {
        "\x81"                                 // array(1)
        "\xA2"                                 // map(2)
        "\x00"                                 // unsigned(0) => SenML Name
        "\x68/13/26/1"                         // text(8)
        "\x02"                                 // unsigned(2) => SenML Value
        "\xFB\x40\x08\x00\x00\x00\x00\x00\x00" // simple_f64(3)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_UINT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value->uint_value, 3);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, double_as_huge_u64_when_convertible) {
    char RESOURCE[] = { "\x81"         // array(1)
                        "\xA2"         // map(2)
                        "\x00"         // unsigned(0) => SenML Name
                        "\x68/13/26/1" // text(8)
                        "\x02"         // unsigned(2) => SenML Value
                        // simple_f64(1.844674407370955e19)
                        "\xFB\x43\xEF\xFF\xFF\xFF\xFF\xFF\xFF" };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_UINT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_EQUAL(value->uint_value, UINT64_MAX - 2047);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, double_as_i64_not_convertible) {
    char RESOURCE[] = {
        "\x81"                                 // array(1)
        "\xA2"                                 // map(2)
        "\x00"                                 // unsigned(0) => SenML Name
        "\x68/13/26/1"                         // text(8)
        "\x02"                                 // unsigned(2) => SenML Value
        "\xFB\x40\x09\x21\xfb\x54\x41\x17\x44" // simple_f64(3.1415926535)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_value, half_read_as_double) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\xF9\x50\x00" // simple_f16(32)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_DOUBLE;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_DOUBLE);
    ANJ_UNIT_ASSERT_EQUAL(value->double_value, 32.0);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, objlnk_valid) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x63vlo"      // text(3)
        "\x68"
        "32:42532" // string(32:42532)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_OBJLNK);
    ANJ_UNIT_ASSERT_EQUAL(value->objlnk.oid, 32);
    ANJ_UNIT_ASSERT_EQUAL(value->objlnk.iid, 42532);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_value, objlnk_with_trash_at_the_end) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x63vlo"      // text(3)
        "\x68"
        "32:42foo" // string(32:42foo)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_value, objlnk_with_overflow) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x63vlo"      // text(3)
        "\x68"
        "1:423444" // string(1:423444)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_value, objlnk_too_long) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x63vlo"      // text(3)
        "\x6D"
        "000001:000001" // string(000001:000001)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

#    ifdef ANJ_WITH_COMPOSITE_OPERATIONS
ANJ_UNIT_TEST(cbor_in_composite, composite_read_mode_additional_payload) {
    char RESOURCE_INSTANCE_WITH_PAYLOAD[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/3/0/0/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x63"         // text(3)
        "foo"
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_READ_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE_INSTANCE_WITH_PAYLOAD,
            sizeof(RESOURCE_INSTANCE_WITH_PAYLOAD) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_composite, composite_read_root_path) {
    char RESOURCE_INSTANCE_WITH_PAYLOAD[] = {
        "\x81"  // array(1)
        "\xA1"  // map(1)
        "\x00"  // unsigned(0) => SenML Name
        "\x61/" // text(1)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_READ_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE_INSTANCE_WITH_PAYLOAD,
            sizeof(RESOURCE_INSTANCE_WITH_PAYLOAD) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_NULL);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_ROOT_PATH());
}

ANJ_UNIT_TEST(cbor_in_composite, composite_observe_with_value) {
    char RESOURCE_INSTANCE_WITH_PAYLOAD[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/3/0/0/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x63"         // text(3)
        "foo"
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_READ_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE_INSTANCE_WITH_PAYLOAD,
            sizeof(RESOURCE_INSTANCE_WITH_PAYLOAD) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_composite, composite_observe_two_paths) {

    char RESOURCE_INSTANCE_WITH_PAYLOAD[] = {
        "\x82"         // array(2)
        "\xA1"         // map(1)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\xA1"         // map(1)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/14/27/2" // text(8)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_INF_OBSERVE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE_INSTANCE_WITH_PAYLOAD,
            sizeof(RESOURCE_INSTANCE_WITH_PAYLOAD) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_NULL);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_NULL);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(14, 27, 2));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_composite, composite_observe_two_paths_with_basename) {

    char RESOURCE_INSTANCE_WITH_PAYLOAD[] = {
        "\x82"       // array(2)
        "\xA2"       // map(2)
        "\x21"       // negative(1) => SenML Base Name
        "\x66/13/26" // text(6)
        "\x00"       // unsigned(0) => SenML Name
        "\x62/1"     // text(2)
        "\xA1"       // map(1)
        "\x00"       // unsigned(0) => SenML Name
        "\x62/2"     // text(2)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_INF_OBSERVE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE_INSTANCE_WITH_PAYLOAD,
            sizeof(RESOURCE_INSTANCE_WITH_PAYLOAD) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_NULL);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_NULL);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 2));

    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}
#    endif // ANJ_WITH_COMPOSITE_OPERATIONS

ANJ_UNIT_TEST(cbor_in_error, no_toplevel_array) {
    char RESOURCE[] = {
        "\x19\x08\x59" // unsigned(2137)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &count),
                          _ANJ_IO_ERR_FORMAT);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, time_label) {
    char RESOURCE[] = {
        "\x81"                 // array(1)
        "\xA3"                 // map(3)
        "\x00"                 // unsigned(0) => SenML Name
        "\x68/13/26/1"         // text(8)
        "\x02"                 // unsigned(2) => SenML Value
        "\x18\x2A"             // unsigned(42)
        "\x06"                 // unsigned(6) => SenML Time
        "\x1A\x65\xB1\x2B\x01" // unsigned(1706109697)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, bogus_map_label) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x44test"     // bytes(4)
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, invalid_string_label) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x64test"     // text(4)
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, invalid_long_string_label) {
    char RESOURCE[] = {
        "\x81"              // array(1)
        "\xA2"              // map(2)
        "\x6DJohnPaul2Pope" // text(13)
        "\x68/13/26/1"      // text(8)
        "\x02"              // unsigned(2) => SenML Value
        "\x18\x2A"          // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, invalid_numeric_label) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x19\x08\x59" // unsigned(2137)
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, unfinished_array) {
    char RESOURCES[] = {
        "\x82"           // array(2)
        "\xA2"           // map(2)
        "\x00"           // unsigned(0) => SenML Name
        "\x6A/13/26/1/4" // text(10)
        "\x02"           // unsigned(2) => SenML Value
        "\x18\x2A"       // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, out_of_range_id) {
    char RESOURCES[] = {
        "\x81"                 // array(1)
        "\xA2"                 // map(2)
        "\x00"                 // unsigned(0) => SenML Name
        "\x70/99999/13/26/1/4" // text(16)
        "\x02"                 // unsigned(2) => SenML Value
        "\x18\x2A"             // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, invalid_id_in_path) {
    char RESOURCES[] = {
        "\x81"               // array(1)
        "\xA2"               // map(2)
        "\x00"               // unsigned(0) => SenML Name
        "\x6E/NaN/13/26/1/4" // text(16)
        "\x02"               // unsigned(2) => SenML Value
        "\x18\x2A"           // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, empty_path) {
    char RESOURCES[] = {
        "\x81"     // array(1)
        "\xA2"     // map(2)
        "\x00"     // unsigned(0) => SenML Name
        "\x60"     // text(0)
        "\x02"     // unsigned(2) => SenML Value
        "\x18\x2A" // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, too_many_path_segments) {
    char RESOURCES[] = {
        "\x81"           // array(1)
        "\xA2"           // map(2)
        "\x00"           // unsigned(0) => SenML Name
        "\x6A/1/2/3/4/5" // text(10)
        "\x02"           // unsigned(2) => SenML Value
        "\x18\x2A"       // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, duplicate_name) {
    char RESOURCES[] = {
        "\x81"         // array(1)
        "\xA3"         // map(3)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/1/2/3/4" // text(8)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/1/2/3/4" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, non_string_name) {
    char RESOURCES[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x48/1/2/3/4" // bytes(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, name_too_long) {
    char RESOURCES[] = {
        "\x81"                              // array(1)
        "\xA2"                              // map(2)
        "\x00"                              // unsigned(0) => SenML Name
        "\x78\x19/10000/10000/10000/000001" // text(25)
        "\x02"                              // unsigned(2) => SenML Value
        "\x18\x2A"                          // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, multiple_values) {
    char RESOURCES[] = {
        "\x81"         // array(1)
        "\xA3"         // map(3)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/1/2/3/4" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
        "\x03"         // unsigned(3) => SenML String
        "\x66"
        "foobar" // string(foobar)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, duplicate_basename) {
    char RESOURCES[] = {
        "\x81"         // array(1)
        "\xA3"         // map(3)
        "\x21"         // negative(1) => SenML Base Name
        "\x68/1/2/3/4" // text(8)
        "\x21"         // negative(1) => SenML Base Name
        "\x68/1/2/3/4" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, non_string_basename) {
    char RESOURCES[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x21"         // negative(1) => SenML Base Name
        "\x48/1/2/3/4" // bytes(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, premature_eof_no_name) {
    char RESOURCES[] = {
        "\x81" // array(1)
        "\xA2" // map(2)
        "\x00" // unsigned(0) => SenML Name
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, premature_eof_no_basename) {
    char RESOURCES[] = {
        "\x81" // array(1)
        "\xA2" // map(2)
        "\x21" // negative(1) => SenML Base Name
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, premature_eof_no_value) {
    char RESOURCES[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/1/2/3/4" // text(8)
        "\x02"         // unsigned(2) => SenML Value
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, premature_eof_indefinite_map) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xBF"         // map(*)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, explicit_null_with_wrong_label) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x03"         // unsigned(3) => SenML String
        "\xF6"         // null
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, boolean_with_wrong_label) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\xF5"         // true
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, bytes_with_wrong_label) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x03"         // unsigned(3) => SenML String
        "\x46"
        "foobar" // bytes(foobar)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, string_with_wrong_label) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x08"         // unsigned(8) => SenML Data
        "\x66"
        "foobar" // string(foobar)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, number_with_wrong_label) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x03"         // unsigned(3) => SenML String
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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

ANJ_UNIT_TEST(cbor_in_error, number_incompatible_type_requested) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_OBJLNK;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, string_incompatible_type_requested) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x03"         // unsigned(3) => SenML String
        "\x66"
        "foobar" // string(foobar)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCE, sizeof(RESOURCE) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_INT;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_error, invalid_disambiguation_and_double_eof) {
    char RESOURCE[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x68/13/26/1" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
            &ctx, ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
            &ANJ_MAKE_RESOURCE_PATH(13, 26, 1), _ANJ_COAP_FORMAT_SENML_CBOR));
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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    // call again without disambiguating
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    // call with ANJ_DATA_TYPE_ANY again
    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    // now let's disambiguate properly
    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 42);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(13, 26, 1));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);

    // trying to read past EOF
    // returning _ANJ_IO_ERR_LOGIC would also be acceptable here
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_basename, out_of_order_concatenation) {
    char RESOURCES[] = {
        "\x81" // array(1)
        "\xA3" // map(3)
        "\x00" // unsigned(0) => SenML Name
        "\x69"
        "37/69/420" // text(9)
        "\x02"      // unsigned(2) => SenML Value
        "\x18\x2A"  // unsigned(42)
        "\x21"      // negative(1) => SenML Base Name
        "\x63/21"   // text(3)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(2137, 69, 420));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 42);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(2137, 69, 420));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_basename, basename_without_name) {
    char RESOURCES[] = {
        "\x81"         // array(1)
        "\xA2"         // map(2)
        "\x21"         // negative(1) => SenML Base Name
        "\x68/1/2/3/4" // text(8)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

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
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 2, 3, 4));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 42);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(1, 2, 3, 4));
}

ANJ_UNIT_TEST(cbor_in_basename, basename_persistence) {
    char RESOURCES[] = {
        "\x82" // array(2)
        "\xA3" // map(3)
        "\x00" // unsigned(0) => SenML Name
        "\x69"
        "37/69/420"    // text(9)
        "\x02"         // unsigned(2) => SenML Value
        "\x18\x2A"     // unsigned(42)
        "\x21"         // negative(1) => SenML Base Name
        "\x63/21"      // text(3)
        "\xA2"         // map(2)
        "\x00"         // unsigned(0) => SenML Name
        "\x64/3/7"     // text(4)
        "\x02"         // unsigned(2) => SenML Value
        "\x19\x08\x59" // unsigned(2137)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 2);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(2137, 69, 420));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 42);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(2137, 69, 420));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(21, 3, 7));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 2137);
    anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_PATH(21, 3, 7));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_basename, concatenated_path_too_long) {
    char RESOURCES[] = {
        "\x81"              // array(1)
        "\xA3"              // map(3)
        "\x21"              // negative(1) => SenML Base Name
        "\x6C/10000/10000"  // text(12)
        "\x00"              // unsigned(0) => SenML Name
        "\x6D/10000/000001" // text(13)
        "\x02"              // unsigned(2) => SenML Value
        "\x18\x2A"          // unsigned(42)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

ANJ_UNIT_TEST(cbor_in_basename, concatenated_path_too_long_string) {
    char RESOURCES[] = {
        "\x81"              // array(1)
        "\xA3"              // map(3)
        "\x21"              // negative(1) => SenML Base Name
        "\x6C/10000/10000"  // text(12)
        "\x00"              // unsigned(0) => SenML Name
        "\x6D/10000/000001" // text(13)
        "\x03"              // unsigned(3) => SenML String
        "\x66"
        "foobar" // string(foobar)
    };
    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 1);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_ERR_FORMAT);
}

const char HUGE_PAYLOAD[] = {
    "\x8C"             // array(12)
    "\xA3"             // map(3)
    "\x21"             // negative(1) => SenML Base Name
    "\x6C/65534/65533" // text(12)
    "\x00"             // unsigned(0) => SenML Name
    "\x6C/65532/65531" // text(12)
    "\x03"             // unsigned(3) => SenML String
    "\x78\x5E"         // text(94)
    "this is a rather long string and it will definitely not fit in the LL "
    "parser's prebuffer alone"
    "\xA2"             // map(2)
    "\x00"             // unsigned(0) => SenML Name
    "\x6C/65532/65532" // text(12)
    "\x03"             // unsigned(3) => SenML String
    "\x78\x55"         // text(85)
    "this is another pretty long string that will require splitting it into "
    "smaller chunks"
    "\xBF"             // map(*)
    "\x00"             // unsigned(0) => SenML Name
    "\x6C/65532/65533" // text(12)
    "\x03"             // unsigned(3) => SenML String
    "\x78\x3D"         // text(61)
    "this is a variant that uses an indefinite map for extra chaos"
    "\xFF"     // primitive(*)
    "\xA3"     // map(3)
    "\x03"     // unsigned(3) => SenML String
    "\x78\x5A" // text(90)
    "...and this variant specifies the basename and name after the value for "
    "extra hard parsing"
    "\x21"                                 // negative(1) => SenML Base Name
    "\x60"                                 // text(0)
    "\x00"                                 // unsigned(0) => SenML Name
    "\x78\x18/65531/65532/65533/65534"     // text(24)
    "\xA3"                                 // map(3)
    "\x21"                                 // negative(1) => SenML Base Name
    "\x6C/10000/10001"                     // text(12)
    "\x00"                                 // unsigned(0) => SenML Name
    "\x6C/10002/10003"                     // text(12)
    "\x02"                                 // unsigned(2) => SenML Value
    "\x1B\x39\x53\x0D\xD6\x60\xEB\x5F\xAB" // unsigned(4130660497629077419)
    "\xA2"                                 // map(2)
    "\x00"                                 // unsigned(0) => SenML Name
    "\x6C/10002/10004"                     // text(12)
    "\x02"                                 // unsigned(2) => SenML Value
    "\x1B\x27\xAE\x9D\x86\xCD\xFC\x47\x0F" // unsigned(2859396015733884687)
    "\xBF"                                 // map(*)
    "\x00"                                 // unsigned(0) => SenML Name
    "\x6C/10002/10005"                     // text(12)
    "\x02"                                 // unsigned(2) => SenML Value
    "\x1B\x70\x59\xB8\x34\x61\xA2\xC0\xC1" // unsigned(8095704340291043521)
    "\xFF"                                 // primitive(*)
    "\xA3"                                 // map(3)
    "\x02"                                 // unsigned(2) => SenML Value
    "\x1B\x62\x54\xF2\x8B\xF0\xF3\x75\x18" // unsigned(7085554796617495832)
    "\x21"                                 // negative(1) => SenML Base Name
    "\x78\x18/20001/20002/20003/20004"     // text(24)
    "\x00"                                 // unsigned(0) => SenML Name
    "\x60"                                 // text(0)
    "\xA3"                                 // map(3)
    "\x21"                                 // negative(1) => SenML Base Name
    "\x7F"                                 // text(*)
    "\x6C/55534/55533"                     // text(12)
    "\xFF"                                 // primitive(*)
    "\x00"                                 // unsigned(0) => SenML Name
    "\x7F"                                 // text(*)
    "\x6C/55532/55531"                     // text(12)
    "\xFF"                                 // primitive(*)
    "\x03"                                 // unsigned(3) => SenML String
    "\x7F"                                 // text(*)
    "\x78\x5E"                             // text(94)
    "this is a rather long string and it will definitely not fit in the LL "
    "parser's prebuffer alone"
    "\xFF"             // primitive(*)
    "\xA2"             // map(2)
    "\x00"             // unsigned(0) => SenML Name
    "\x7F"             // text(*)
    "\x6C/55532/55532" // text(12)
    "\xFF"             // primitive(*)
    "\x03"             // unsigned(3) => SenML String
    "\x7F"             // text(*)
    "\x78\x55"         // text(85)
    "this is another pretty long string that will require splitting it into "
    "smaller chunks"
    "\xFF"             // primitive(*)
    "\xBF"             // map(*)
    "\x00"             // unsigned(0) => SenML Name
    "\x7F"             // text(*)
    "\x6C/55532/55533" // text(12)
    "\xFF"             // primitive(*)
    "\x03"             // unsigned(3) => SenML String
    "\x7F"             // text(*)
    "\x78\x3D"         // text(61)
    "this is a variant that uses an indefinite map for extra chaos"
    "\xFF"     // primitive(*)
    "\xFF"     // primitive(*)
    "\xA3"     // map(3)
    "\x03"     // unsigned(3) => SenML String
    "\x7F"     // text(*)
    "\x78\x5A" // text(90)
    "...and this variant specifies the basename and name after the value for "
    "extra hard parsing"
    "\xFF"                             // primitive(*)
    "\x21"                             // negative(1) => SenML Base Name
    "\x7F"                             // text(*)
    "\x60"                             // text(0)
    "\xFF"                             // primitive(*)
    "\x00"                             // unsigned(0) => SenML Name
    "\x7F"                             // text(*)
    "\x78\x18/55531/55532/55533/55534" // text(24)
    "\xFF"                             // primitive(*)
};

ANJ_UNIT_TEST(cbor_in_huge, huge_payload) {
    char RESOURCES[sizeof(HUGE_PAYLOAD)];
    memcpy(RESOURCES, HUGE_PAYLOAD, sizeof(HUGE_PAYLOAD));

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES, sizeof(RESOURCES) - 1, true));

    size_t count;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_get_entry_count(&ctx, &count));
    ANJ_UNIT_ASSERT_EQUAL(count, 12);

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(
            value->bytes_or_string.data,
            "this is a rather long string and it will definitely not fit in "
            "the LL parser's prebuffer alone");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 94);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 94);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(65534, 65533, 65532, 65531));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data,
                                "this is another pretty long string that will "
                                "require splitting it into smaller chunks");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 85);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 85);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(65534, 65533, 65532, 65532));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(
            value->bytes_or_string.data,
            "this is a variant that uses an indefinite map for extra chaos");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 61);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 61);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(65534, 65533, 65532, 65533));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(
            value->bytes_or_string.data,
            "...and this variant specifies the basename and name after the "
            "value for extra hard parsing");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 90);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 90);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(65531, 65532, 65533, 65534));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10003));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 4130660497629077419);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10003));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10004));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 2859396015733884687);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10004));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10005));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 8095704340291043521);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10005));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(20001, 20002, 20003, 20004));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 7085554796617495832);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(20001, 20002, 20003, 20004));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(
            value->bytes_or_string.data,
            "this is a rather long string and it will definitely not fit in "
            "the LL parser's prebuffer alone");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 94);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(55534, 55533, 55532, 55531));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 94);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 94);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(55534, 55533, 55532, 55531));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(value->bytes_or_string.data,
                                "this is another pretty long string that will "
                                "require splitting it into smaller chunks");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 85);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(55534, 55533, 55532, 55532));

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 85);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 85);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(55534, 55533, 55532, 55532));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(
            value->bytes_or_string.data,
            "this is a variant that uses an indefinite map for extra chaos");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 61);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    ANJ_UNIT_ASSERT_NULL(path);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 61);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 61);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(55534, 55533, 55532, 55533));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL_BYTES(
            value->bytes_or_string.data,
            "...and this variant specifies the basename and name after the "
            "value for extra hard parsing");
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 90);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
    ANJ_UNIT_ASSERT_NULL(path);

    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, 90);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
    ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 90);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(55531, 55532, 55533, 55534));

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

ANJ_UNIT_TEST(cbor_in_huge, huge_payload_split) {
    for (size_t split = 0; split < sizeof(HUGE_PAYLOAD); ++split) {
        char RESOURCES[sizeof(HUGE_PAYLOAD)];
        memcpy(RESOURCES, HUGE_PAYLOAD, sizeof(HUGE_PAYLOAD));
        bool next_payload_fed = false;

        _anj_io_in_ctx_t ctx;
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(
                &ctx, ANJ_OP_DM_WRITE_COMP, &ANJ_MAKE_ROOT_PATH(),
                _ANJ_COAP_FORMAT_SENML_CBOR));
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_feed_payload(&ctx, RESOURCES, split, false));

        size_t count;
        anj_data_type_t type = ANJ_DATA_TYPE_ANY;
        const anj_res_value_t *value;
        const anj_uri_path_t *path;
        if (split >= 9) {
            ANJ_UNIT_ASSERT_SUCCESS(
                    _anj_io_in_ctx_get_entry_count(&ctx, &count));
            ANJ_UNIT_ASSERT_EQUAL(count, 12);
        } else {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry_count(&ctx, &count),
                                  _ANJ_IO_ERR_LOGIC);
        }

        if (split < 40) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                    true));
        }

        size_t expected_offset = 0;
        do {
            const char string[] =
                    "this is a rather long string and it will definitely not "
                    "fit in the LL parser's prebuffer alone";
            int result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type,
                                                               &value, &path),
                                      _ANJ_IO_WANT_NEXT_PAYLOAD);
                ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
                ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                        &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                        true));
                next_payload_fed = true;
                result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            }

            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
            anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                                 65534, 65533, 65532, 65531));
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                                  expected_offset);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                  sizeof(string) - 1);
            expected_offset += value->bytes_or_string.chunk_length;
            ANJ_UNIT_ASSERT_TRUE(expected_offset
                                 <= value->bytes_or_string.full_length_hint);
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                    value->bytes_or_string.data,
                    string + value->bytes_or_string.offset,
                    value->bytes_or_string.chunk_length);
        } while (value->bytes_or_string.offset
                         + value->bytes_or_string.chunk_length
                 != value->bytes_or_string.full_length_hint);

        type = ANJ_DATA_TYPE_ANY;
        expected_offset = 0;
        do {
            const char string[] =
                    "this is another pretty long string that will require "
                    "splitting it into smaller chunks";
            int result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type,
                                                               &value, &path),
                                      _ANJ_IO_WANT_NEXT_PAYLOAD);
                ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
                ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                        &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                        true));
                next_payload_fed = true;
                result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            }

            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
            anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                                 65534, 65533, 65532, 65532));
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                                  expected_offset);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                  sizeof(string) - 1);
            expected_offset += value->bytes_or_string.chunk_length;
            ANJ_UNIT_ASSERT_TRUE(expected_offset
                                 <= value->bytes_or_string.full_length_hint);
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                    value->bytes_or_string.data,
                    string + value->bytes_or_string.offset,
                    value->bytes_or_string.chunk_length);
        } while (value->bytes_or_string.offset
                         + value->bytes_or_string.chunk_length
                 != value->bytes_or_string.full_length_hint);

        type = ANJ_DATA_TYPE_ANY;
        expected_offset = 0;
        do {
            const char string[] = "this is a variant that uses an indefinite "
                                  "map for extra chaos";
            int result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type,
                                                               &value, &path),
                                      _ANJ_IO_WANT_NEXT_PAYLOAD);
                ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
                ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                        &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                        true));
                next_payload_fed = true;
                result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            }

            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                                  expected_offset);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                  sizeof(string) - 1);
            expected_offset += value->bytes_or_string.chunk_length;
            ANJ_UNIT_ASSERT_TRUE(expected_offset
                                 <= value->bytes_or_string.full_length_hint);
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                    value->bytes_or_string.data,
                    string + value->bytes_or_string.offset,
                    value->bytes_or_string.chunk_length);
            if (value->bytes_or_string.offset
                            + value->bytes_or_string.chunk_length
                    != value->bytes_or_string.full_length_hint) {
                ANJ_UNIT_ASSERT_NULL(path);
            }
        } while (value->bytes_or_string.offset
                         + value->bytes_or_string.chunk_length
                 != value->bytes_or_string.full_length_hint);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             65534, 65533, 65532, 65533));

        type = ANJ_DATA_TYPE_ANY;
        expected_offset = 0;
        do {
            const char string[] =
                    "...and this variant specifies the basename and name after "
                    "the value for extra hard parsing";
            int result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type,
                                                               &value, &path),
                                      _ANJ_IO_WANT_NEXT_PAYLOAD);
                ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
                ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                        &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                        true));
                next_payload_fed = true;
                result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            }

            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                                  expected_offset);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                  sizeof(string) - 1);
            expected_offset += value->bytes_or_string.chunk_length;
            ANJ_UNIT_ASSERT_TRUE(expected_offset
                                 <= value->bytes_or_string.full_length_hint);
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                    value->bytes_or_string.data,
                    string + value->bytes_or_string.offset,
                    value->bytes_or_string.chunk_length);
            if (value->bytes_or_string.offset
                            + value->bytes_or_string.chunk_length
                    != value->bytes_or_string.full_length_hint) {
                ANJ_UNIT_ASSERT_NULL(path);
            }
        } while (value->bytes_or_string.offset
                         + value->bytes_or_string.chunk_length
                 != value->bytes_or_string.full_length_hint);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             65531, 65532, 65533, 65534));

        type = ANJ_DATA_TYPE_ANY;
        int result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                    true));
            next_payload_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                            | ANJ_DATA_TYPE_UINT);
        ANJ_UNIT_ASSERT_NULL(value);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             10000, 10001, 10002, 10003));

        type = ANJ_DATA_TYPE_INT;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
        ANJ_UNIT_ASSERT_EQUAL(value->int_value, 4130660497629077419);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             10000, 10001, 10002, 10003));

        type = ANJ_DATA_TYPE_ANY;
        if ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
                == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                    true));
            next_payload_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                            | ANJ_DATA_TYPE_UINT);
        ANJ_UNIT_ASSERT_NULL(value);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             10000, 10001, 10002, 10004));

        type = ANJ_DATA_TYPE_INT;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
        ANJ_UNIT_ASSERT_EQUAL(value->int_value, 2859396015733884687);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             10000, 10001, 10002, 10004));

        type = ANJ_DATA_TYPE_ANY;
        if ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
                == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                    true));
            next_payload_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                            | ANJ_DATA_TYPE_UINT);
        ANJ_UNIT_ASSERT_NULL(value);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             10000, 10001, 10002, 10005));

        type = ANJ_DATA_TYPE_INT;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
        ANJ_UNIT_ASSERT_EQUAL(value->int_value, 8095704340291043521);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             10000, 10001, 10002, 10005));

        type = ANJ_DATA_TYPE_ANY;
        if ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
                == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                    true));
            next_payload_fed = true;
            result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
        }
        ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                            | ANJ_DATA_TYPE_UINT);
        ANJ_UNIT_ASSERT_NULL(value);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             20001, 20002, 20003, 20004));

        type = ANJ_DATA_TYPE_INT;
        ANJ_UNIT_ASSERT_SUCCESS(
                _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
        ANJ_UNIT_ASSERT_EQUAL(value->int_value, 7085554796617495832);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             20001, 20002, 20003, 20004));

        type = ANJ_DATA_TYPE_ANY;
        expected_offset = 0;
        do {
            const char string[] =
                    "this is a rather long string and it will definitely not "
                    "fit in the LL parser's prebuffer alone";
            if ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
                    == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type,
                                                               &value, &path),
                                      _ANJ_IO_WANT_NEXT_PAYLOAD);
                ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
                ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                        &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                        true));
                next_payload_fed = true;
                result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            }

            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
            anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                                 55534, 55533, 55532, 55531));
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                                  expected_offset);
            if (expected_offset < sizeof(string) - 1) {
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      0);
                ANJ_UNIT_ASSERT_TRUE(value->bytes_or_string.chunk_length > 0);
                expected_offset += value->bytes_or_string.chunk_length;
                ANJ_UNIT_ASSERT_TRUE(expected_offset < sizeof(string));
                ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                        value->bytes_or_string.data,
                        string + value->bytes_or_string.offset,
                        value->bytes_or_string.chunk_length);
            } else {
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      sizeof(string) - 1);
            }
        } while (value->bytes_or_string.offset
                         + value->bytes_or_string.chunk_length
                 != value->bytes_or_string.full_length_hint);

        type = ANJ_DATA_TYPE_ANY;
        expected_offset = 0;
        do {
            const char string[] =
                    "this is another pretty long string that will require "
                    "splitting it into smaller chunks";
            if ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
                    == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type,
                                                               &value, &path),
                                      _ANJ_IO_WANT_NEXT_PAYLOAD);
                ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
                ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                        &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                        true));
                next_payload_fed = true;
                result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            }

            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
            anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                                 55534, 55533, 55532, 55532));
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                                  expected_offset);
            if (expected_offset < sizeof(string) - 1) {
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      0);
                ANJ_UNIT_ASSERT_TRUE(value->bytes_or_string.chunk_length > 0);
                expected_offset += value->bytes_or_string.chunk_length;
                ANJ_UNIT_ASSERT_TRUE(expected_offset < sizeof(string));
                ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                        value->bytes_or_string.data,
                        string + value->bytes_or_string.offset,
                        value->bytes_or_string.chunk_length);
            } else {
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      sizeof(string) - 1);
            }
        } while (value->bytes_or_string.offset
                         + value->bytes_or_string.chunk_length
                 != value->bytes_or_string.full_length_hint);

        type = ANJ_DATA_TYPE_ANY;
        expected_offset = 0;
        do {
            const char string[] = "this is a variant that uses an indefinite "
                                  "map for extra chaos";
            if ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
                    == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type,
                                                               &value, &path),
                                      _ANJ_IO_WANT_NEXT_PAYLOAD);
                ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
                ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                        &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                        true));
                next_payload_fed = true;
                result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            }

            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                                  expected_offset);
            if (expected_offset < sizeof(string) - 1) {
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      0);
                ANJ_UNIT_ASSERT_TRUE(value->bytes_or_string.chunk_length > 0);
                expected_offset += value->bytes_or_string.chunk_length;
                ANJ_UNIT_ASSERT_TRUE(expected_offset < sizeof(string));
                ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                        value->bytes_or_string.data,
                        string + value->bytes_or_string.offset,
                        value->bytes_or_string.chunk_length);
                ANJ_UNIT_ASSERT_NULL(path);
            } else {
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      sizeof(string) - 1);
                anj_uri_path_t_compare(
                        path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(55534, 55533,
                                                               55532, 55533));
            }
        } while (value->bytes_or_string.offset
                         + value->bytes_or_string.chunk_length
                 != value->bytes_or_string.full_length_hint);

        type = ANJ_DATA_TYPE_ANY;
        expected_offset = 0;
        do {
            const char string[] =
                    "...and this variant specifies the basename and name after "
                    "the value for extra hard parsing";
            if ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
                    == _ANJ_IO_WANT_NEXT_PAYLOAD) {
                ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type,
                                                               &value, &path),
                                      _ANJ_IO_WANT_NEXT_PAYLOAD);
                ANJ_UNIT_ASSERT_FALSE(next_payload_fed);
                ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                        &ctx, RESOURCES + split, sizeof(RESOURCES) - split - 1,
                        true));
                next_payload_fed = true;
                result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path);
            }

            ANJ_UNIT_ASSERT_SUCCESS(result);
            ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset,
                                  expected_offset);
            if (expected_offset < sizeof(string) - 1) {
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      0);
                ANJ_UNIT_ASSERT_TRUE(value->bytes_or_string.chunk_length > 0);
                expected_offset += value->bytes_or_string.chunk_length;
                ANJ_UNIT_ASSERT_TRUE(expected_offset < sizeof(string));
                ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                        value->bytes_or_string.data,
                        string + value->bytes_or_string.offset,
                        value->bytes_or_string.chunk_length);
                ANJ_UNIT_ASSERT_NULL(path);
            } else {
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
                ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                      sizeof(string) - 1);
                anj_uri_path_t_compare(
                        path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(55531, 55532,
                                                               55533, 55534));
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

ANJ_UNIT_TEST(cbor_in_huge, huge_payload_byte_by_byte) {
    char RESOURCES[sizeof(HUGE_PAYLOAD)];
    memcpy(RESOURCES, HUGE_PAYLOAD, sizeof(HUGE_PAYLOAD));

    _anj_io_in_ctx_t ctx;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP,
                                                &ANJ_MAKE_ROOT_PATH(),
                                                _ANJ_COAP_FORMAT_SENML_CBOR));
    int result;
    size_t offset = 0;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
            &ctx, RESOURCES + offset, 1, offset + 2 == sizeof(HUGE_PAYLOAD)));
    ++offset;

    anj_data_type_t type = ANJ_DATA_TYPE_ANY;
    const anj_res_value_t *value;
    const anj_uri_path_t *path;

    size_t expected_offset = 0;
    do {
        const char string[] =
                "this is a rather long string and it will definitely not fit "
                "in the LL parser's prebuffer alone";
        while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
               == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + offset, 1,
                    offset + 2 == sizeof(HUGE_PAYLOAD)));
            ++offset;
        }
        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             65534, 65533, 65532, 65531));
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, expected_offset);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                              sizeof(string) - 1);
        expected_offset += value->bytes_or_string.chunk_length;
        ANJ_UNIT_ASSERT_TRUE(expected_offset
                             <= value->bytes_or_string.full_length_hint);
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                value->bytes_or_string.data,
                string + value->bytes_or_string.offset,
                value->bytes_or_string.chunk_length);
    } while (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
             != value->bytes_or_string.full_length_hint);

    type = ANJ_DATA_TYPE_ANY;
    expected_offset = 0;
    do {
        const char string[] =
                "this is another pretty long string that will require "
                "splitting it into smaller chunks";
        while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
               == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + offset, 1,
                    offset + 2 == sizeof(HUGE_PAYLOAD)));
            ++offset;
        }

        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             65534, 65533, 65532, 65532));
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, expected_offset);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                              sizeof(string) - 1);
        expected_offset += value->bytes_or_string.chunk_length;
        ANJ_UNIT_ASSERT_TRUE(expected_offset
                             <= value->bytes_or_string.full_length_hint);
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                value->bytes_or_string.data,
                string + value->bytes_or_string.offset,
                value->bytes_or_string.chunk_length);
    } while (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
             != value->bytes_or_string.full_length_hint);

    type = ANJ_DATA_TYPE_ANY;
    expected_offset = 0;
    do {
        const char string[] =
                "this is a variant that uses an indefinite map for extra chaos";
        while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
               == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + offset, 1,
                    offset + 2 == sizeof(HUGE_PAYLOAD)));
            ++offset;
        }

        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, expected_offset);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                              sizeof(string) - 1);
        expected_offset += value->bytes_or_string.chunk_length;
        ANJ_UNIT_ASSERT_TRUE(expected_offset
                             <= value->bytes_or_string.full_length_hint);
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                value->bytes_or_string.data,
                string + value->bytes_or_string.offset,
                value->bytes_or_string.chunk_length);
        if (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
                != value->bytes_or_string.full_length_hint) {
            ANJ_UNIT_ASSERT_NULL(path);
        }
    } while (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
             != value->bytes_or_string.full_length_hint);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(65534, 65533, 65532, 65533));

    type = ANJ_DATA_TYPE_ANY;
    expected_offset = 0;
    do {
        const char string[] = "...and this variant specifies the basename and "
                              "name after the value for extra hard parsing";
        while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
               == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + offset, 1,
                    offset + 2 == sizeof(HUGE_PAYLOAD)));
            ++offset;
        }

        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, expected_offset);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                              sizeof(string) - 1);
        expected_offset += value->bytes_or_string.chunk_length;
        ANJ_UNIT_ASSERT_TRUE(expected_offset
                             <= value->bytes_or_string.full_length_hint);
        ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                value->bytes_or_string.data,
                string + value->bytes_or_string.offset,
                value->bytes_or_string.chunk_length);
        if (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
                != value->bytes_or_string.full_length_hint) {
            ANJ_UNIT_ASSERT_NULL(path);
        }
    } while (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
             != value->bytes_or_string.full_length_hint);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(65531, 65532, 65533, 65534));

    type = ANJ_DATA_TYPE_ANY;
    while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
           == _ANJ_IO_WANT_NEXT_PAYLOAD) {
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                &ctx, RESOURCES + offset, 1,
                offset + 2 == sizeof(HUGE_PAYLOAD)));
        ++offset;
    }
    ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10003));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 4130660497629077419);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10003));

    type = ANJ_DATA_TYPE_ANY;
    while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
           == _ANJ_IO_WANT_NEXT_PAYLOAD) {
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                &ctx, RESOURCES + offset, 1,
                offset + 2 == sizeof(HUGE_PAYLOAD)));
        ++offset;
    }
    ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10004));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 2859396015733884687);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10004));

    type = ANJ_DATA_TYPE_ANY;
    while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
           == _ANJ_IO_WANT_NEXT_PAYLOAD) {
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                &ctx, RESOURCES + offset, 1,
                offset + 2 == sizeof(HUGE_PAYLOAD)));
        ++offset;
    }
    ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10005));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 8095704340291043521);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(10000, 10001, 10002, 10005));

    type = ANJ_DATA_TYPE_ANY;
    while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
           == _ANJ_IO_WANT_NEXT_PAYLOAD) {
        ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                       &path),
                              _ANJ_IO_WANT_NEXT_PAYLOAD);
        ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                &ctx, RESOURCES + offset, 1,
                offset + 2 == sizeof(HUGE_PAYLOAD)));
        ++offset;
    }
    ANJ_UNIT_ASSERT_EQUAL(result, _ANJ_IO_WANT_TYPE_DISAMBIGUATION);
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                                        | ANJ_DATA_TYPE_UINT);
    ANJ_UNIT_ASSERT_NULL(value);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(20001, 20002, 20003, 20004));

    type = ANJ_DATA_TYPE_INT;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path));
    ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_INT);
    ANJ_UNIT_ASSERT_EQUAL(value->int_value, 7085554796617495832);
    anj_uri_path_t_compare(
            path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(20001, 20002, 20003, 20004));

    type = ANJ_DATA_TYPE_ANY;
    expected_offset = 0;
    do {
        const char string[] =
                "this is a rather long string and it will definitely not fit "
                "in the LL parser's prebuffer alone";
        while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
               == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + offset, 1,
                    offset + 2 == sizeof(HUGE_PAYLOAD)));
            ++offset;
        }
        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             55534, 55533, 55532, 55531));
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, expected_offset);
        if (expected_offset < sizeof(string) - 1) {
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
            ANJ_UNIT_ASSERT_TRUE(value->bytes_or_string.chunk_length > 0);
            expected_offset += value->bytes_or_string.chunk_length;
            ANJ_UNIT_ASSERT_TRUE(expected_offset < sizeof(string));
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                    value->bytes_or_string.data,
                    string + value->bytes_or_string.offset,
                    value->bytes_or_string.chunk_length);
        } else {
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                  sizeof(string) - 1);
        }
    } while (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
             != value->bytes_or_string.full_length_hint);

    type = ANJ_DATA_TYPE_ANY;
    expected_offset = 0;
    do {
        const char string[] =
                "this is another pretty long string that will require "
                "splitting it into smaller chunks";
        while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
               == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + offset, 1,
                    offset + 2 == sizeof(HUGE_PAYLOAD)));
            ++offset;
        }

        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                             55534, 55533, 55532, 55532));
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, expected_offset);
        if (expected_offset < sizeof(string) - 1) {
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
            ANJ_UNIT_ASSERT_TRUE(value->bytes_or_string.chunk_length > 0);
            expected_offset += value->bytes_or_string.chunk_length;
            ANJ_UNIT_ASSERT_TRUE(expected_offset < sizeof(string));
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                    value->bytes_or_string.data,
                    string + value->bytes_or_string.offset,
                    value->bytes_or_string.chunk_length);
        } else {
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                  sizeof(string) - 1);
        }
    } while (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
             != value->bytes_or_string.full_length_hint);

    type = ANJ_DATA_TYPE_ANY;
    expected_offset = 0;
    do {
        const char string[] =
                "this is a variant that uses an indefinite map for extra chaos";
        while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
               == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + offset, 1,
                    offset + 2 == sizeof(HUGE_PAYLOAD)));
            ++offset;
        }

        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, expected_offset);
        if (expected_offset < sizeof(string) - 1) {
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
            ANJ_UNIT_ASSERT_TRUE(value->bytes_or_string.chunk_length > 0);
            expected_offset += value->bytes_or_string.chunk_length;
            ANJ_UNIT_ASSERT_TRUE(expected_offset < sizeof(string));
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                    value->bytes_or_string.data,
                    string + value->bytes_or_string.offset,
                    value->bytes_or_string.chunk_length);
            ANJ_UNIT_ASSERT_NULL(path);
        } else {
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                  sizeof(string) - 1);
            anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                                 55534, 55533, 55532, 55533));
        }
    } while (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
             != value->bytes_or_string.full_length_hint);

    type = ANJ_DATA_TYPE_ANY;
    expected_offset = 0;
    do {
        const char string[] = "...and this variant specifies the basename and "
                              "name after the value for extra hard parsing";
        while ((result = _anj_io_in_ctx_get_entry(&ctx, &type, &value, &path))
               == _ANJ_IO_WANT_NEXT_PAYLOAD) {
            ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value,
                                                           &path),
                                  _ANJ_IO_WANT_NEXT_PAYLOAD);
            ANJ_UNIT_ASSERT_SUCCESS(_anj_io_in_ctx_feed_payload(
                    &ctx, RESOURCES + offset, 1,
                    offset + 2 == sizeof(HUGE_PAYLOAD)));
            ++offset;
        }

        ANJ_UNIT_ASSERT_SUCCESS(result);
        ANJ_UNIT_ASSERT_EQUAL(type, ANJ_DATA_TYPE_STRING);
        ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.offset, expected_offset);
        if (expected_offset < sizeof(string) - 1) {
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint, 0);
            ANJ_UNIT_ASSERT_TRUE(value->bytes_or_string.chunk_length > 0);
            expected_offset += value->bytes_or_string.chunk_length;
            ANJ_UNIT_ASSERT_TRUE(expected_offset < sizeof(string));
            ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(
                    value->bytes_or_string.data,
                    string + value->bytes_or_string.offset,
                    value->bytes_or_string.chunk_length);
            ANJ_UNIT_ASSERT_NULL(path);
        } else {
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.chunk_length, 0);
            ANJ_UNIT_ASSERT_EQUAL(value->bytes_or_string.full_length_hint,
                                  sizeof(string) - 1);
            anj_uri_path_t_compare(path, &ANJ_MAKE_RESOURCE_INSTANCE_PATH(
                                                 55531, 55532, 55533, 55534));
        }
    } while (value->bytes_or_string.offset + value->bytes_or_string.chunk_length
             != value->bytes_or_string.full_length_hint);

    type = ANJ_DATA_TYPE_ANY;
    ANJ_UNIT_ASSERT_EQUAL(_anj_io_in_ctx_get_entry(&ctx, &type, &value, &path),
                          _ANJ_IO_EOF);
}

#endif // ANJ_WITH_SENML_CBOR
