/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/anj_config.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include "../utils.h"
#include "cbor_encoder.h"
#include "cbor_encoder_ll.h"
#include "internal.h"
#include "io.h"

#ifdef ANJ_WITH_SENML_CBOR

#    define _SENML_CBOR_PATH_MAX_LEN sizeof("/65534/65534/65534/65534")

static size_t add_path(uint8_t *out_buff,
                       const anj_uri_path_t *path,
                       size_t start_index,
                       size_t end_index,
                       int64_t label) {
    size_t path_buf_pos = 0;
    size_t out_buf_pos = 0;
    char path_buff[_SENML_CBOR_PATH_MAX_LEN] = { 0 };

    for (size_t i = start_index; i < end_index; i++) {
        path_buff[path_buf_pos++] = '/';
        path_buf_pos += anj_uint16_to_string_value(&path_buff[path_buf_pos],
                                                   path->ids[i]);
    }

    out_buf_pos += anj_cbor_ll_encode_int(out_buff, label);
    out_buf_pos +=
            anj_cbor_ll_string_begin(&out_buff[out_buf_pos], path_buf_pos);
    memcpy(&out_buff[out_buf_pos], path_buff, path_buf_pos);

    return out_buf_pos + path_buf_pos;
}

// HACK:
// The size of the internal_buff has been calculated so that a
// single record never exceeds its size.
static int prepare_payload(const anj_io_out_entry_t *entry,
                           _anj_senml_cbor_encoder_t *senml_cbor,
                           _anj_io_buff_t *buff_ctx,
                           bool first_entry) {
    size_t buf_pos = 0;
    size_t path_len = anj_uri_path_length(&entry->path);
    if (anj_uri_path_outside_base(&entry->path, &senml_cbor->base_path)
            || !anj_uri_path_has(&entry->path, ANJ_ID_RID)) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }

    double time_s = entry->timestamp;
    if (isnan(time_s)) {
        time_s = 0.0;
    }

    bool with_base_name = (first_entry && senml_cbor->base_path_len);
    bool with_name = (path_len != senml_cbor->base_path_len);
    bool with_time =
            (senml_cbor->encode_time && senml_cbor->last_timestamp != time_s);

    // array
    if (first_entry) {
        buf_pos += anj_cbor_ll_definite_array_begin(
                &buff_ctx->internal_buff[buf_pos], senml_cbor->items_count);
    }
    // map
    size_t map_size = (size_t) (with_base_name + with_name + with_time + 1);
    buf_pos += anj_cbor_ll_definite_map_begin(&buff_ctx->internal_buff[buf_pos],
                                              map_size);

    // basename - only once for READ operation
    if (with_base_name) {
        buf_pos += add_path(&buff_ctx->internal_buff[buf_pos],
                            &senml_cbor->base_path, 0,
                            senml_cbor->base_path_len, SENML_LABEL_BASE_NAME);
    }
    // name
    if (with_name) {
        buf_pos +=
                add_path(&buff_ctx->internal_buff[buf_pos], &entry->path,
                         senml_cbor->base_path_len, path_len, SENML_LABEL_NAME);
    }
    // base time
    if (with_time) {
        senml_cbor->last_timestamp = time_s;
        buf_pos += anj_cbor_ll_encode_int(&buff_ctx->internal_buff[buf_pos],
                                          SENML_LABEL_BASE_TIME);
        buf_pos += anj_cbor_ll_encode_double(&buff_ctx->internal_buff[buf_pos],
                                             time_s);
    }

    // value
    switch (entry->type) {
    case ANJ_DATA_TYPE_BYTES: {
        if (entry->value.bytes_or_string.offset != 0
                || (entry->value.bytes_or_string.full_length_hint
                    && entry->value.bytes_or_string.full_length_hint
                               != entry->value.bytes_or_string.chunk_length)) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           SENML_LABEL_VALUE_OPAQUE);
        buf_pos += anj_cbor_ll_bytes_begin(
                &buff_ctx->internal_buff[buf_pos],
                entry->value.bytes_or_string.chunk_length);
        buff_ctx->is_extended_type = true;
        buff_ctx->remaining_bytes = entry->value.bytes_or_string.chunk_length;
        break;
    }
    case ANJ_DATA_TYPE_STRING: {
        if (entry->value.bytes_or_string.offset != 0
                || (entry->value.bytes_or_string.full_length_hint
                    && entry->value.bytes_or_string.full_length_hint
                               != entry->value.bytes_or_string.chunk_length)) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }
        size_t string_length = entry->value.bytes_or_string.chunk_length;
        if (!string_length && entry->value.bytes_or_string.data
                && *(const char *) entry->value.bytes_or_string.data) {
            string_length =
                    strlen((const char *) entry->value.bytes_or_string.data);
        }
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           SENML_LABEL_VALUE_STRING);
        buf_pos += anj_cbor_ll_string_begin(&buff_ctx->internal_buff[buf_pos],
                                            string_length);
        buff_ctx->is_extended_type = true;
        buff_ctx->remaining_bytes = string_length;
        break;
    }
#    ifdef ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_EXTERNAL_BYTES: {
        if (!entry->value.external_data.get_external_data) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           SENML_LABEL_VALUE_OPAQUE);
        buf_pos += anj_cbor_ll_indefinite_bytes_begin(
                &buff_ctx->internal_buff[buf_pos]);
        buff_ctx->is_extended_type = true;
        // HACK: for ANJ_WITH_EXTERNAL_* types set it to constant value
        // because we don't know the length
        buff_ctx->remaining_bytes = 1;
        break;
    }
    case ANJ_DATA_TYPE_EXTERNAL_STRING: {
        if (!entry->value.external_data.get_external_data) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           SENML_LABEL_VALUE_STRING);
        buf_pos += anj_cbor_ll_indefinite_string_begin(
                &buff_ctx->internal_buff[buf_pos]);
        buff_ctx->is_extended_type = true;
        buff_ctx->remaining_bytes = 1;
        break;
    }
#    endif // ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_TIME: {
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           SENML_LABEL_VALUE);
        buf_pos += anj_cbor_ll_encode_tag(&buff_ctx->internal_buff[buf_pos],
                                          CBOR_TAG_INTEGER_DATE_TIME);
        buf_pos += anj_cbor_ll_encode_int(&buff_ctx->internal_buff[buf_pos],
                                          entry->value.time_value);
        break;
    }
    case ANJ_DATA_TYPE_INT: {
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           SENML_LABEL_VALUE);
        buf_pos += anj_cbor_ll_encode_int(&buff_ctx->internal_buff[buf_pos],
                                          entry->value.int_value);
        break;
    }
    case ANJ_DATA_TYPE_DOUBLE: {
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           SENML_LABEL_VALUE);
        buf_pos += anj_cbor_ll_encode_double(&buff_ctx->internal_buff[buf_pos],
                                             entry->value.double_value);
        break;
    }
    case ANJ_DATA_TYPE_BOOL: {
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           SENML_LABEL_VALUE_BOOL);
        buf_pos += anj_cbor_ll_encode_bool(&buff_ctx->internal_buff[buf_pos],
                                           entry->value.bool_value);
        break;
    }
    case ANJ_DATA_TYPE_OBJLNK: {
        size_t objlink_repr_len = sizeof(SENML_EXT_OBJLNK_REPR) - 1;
        buf_pos += anj_cbor_ll_string_begin(&buff_ctx->internal_buff[buf_pos],
                                            objlink_repr_len);
        memcpy(&buff_ctx->internal_buff[buf_pos], SENML_EXT_OBJLNK_REPR,
               objlink_repr_len);
        buf_pos += objlink_repr_len;
        buf_pos += _anj_io_out_add_objlink(buff_ctx, buf_pos,
                                           entry->value.objlnk.oid,
                                           entry->value.objlnk.iid);
        break;
    }
    case ANJ_DATA_TYPE_UINT: {
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           SENML_LABEL_VALUE);
        buf_pos += anj_cbor_ll_encode_uint(&buff_ctx->internal_buff[buf_pos],
                                           entry->value.uint_value);
        break;
    }
    default: { return _ANJ_IO_ERR_IO_TYPE; }
    }
    assert(buf_pos <= _ANJ_IO_CTX_BUFFER_LENGTH);
    buff_ctx->bytes_in_internal_buff = buf_pos;
    buff_ctx->remaining_bytes += buff_ctx->bytes_in_internal_buff;
    return 0;
}

int _anj_senml_cbor_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                      const anj_io_out_entry_t *entry) {
    assert(ctx->format == _ANJ_COAP_FORMAT_SENML_CBOR
           || ctx->format == _ANJ_COAP_FORMAT_SENML_ETCH_CBOR);

    _anj_senml_cbor_encoder_t *senml_cbor = &ctx->encoder.senml;
    _anj_io_buff_t *buff_ctx = &ctx->buff;

    if (buff_ctx->remaining_bytes) {
        return _ANJ_IO_ERR_LOGIC;
    }

    int res = prepare_payload(entry, senml_cbor, buff_ctx,
                              !senml_cbor->first_entry_added);
    if (res) {
        return res;
    }
    senml_cbor->first_entry_added = true;
    return 0;
}

int _anj_senml_cbor_encoder_init(_anj_io_out_ctx_t *ctx,
                                 const anj_uri_path_t *base_path,
                                 size_t items_count,
                                 bool encode_time) {
    if (!base_path) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }

    _anj_senml_cbor_encoder_t *senml_cbor = &ctx->encoder.senml;
    senml_cbor->first_entry_added = false;
    senml_cbor->base_path_len = anj_uri_path_length(base_path);
    if (senml_cbor->base_path_len) {
        senml_cbor->base_path = *base_path;
    }
    senml_cbor->items_count = items_count;
    senml_cbor->encode_time = encode_time;
    senml_cbor->last_timestamp = 0.0;
    return 0;
}
#endif // ANJ_WITH_SENML_CBOR
