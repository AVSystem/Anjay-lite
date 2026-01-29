/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 33

#include <assert.h>
#include <string.h>

#include <anj/defs.h>

#include "../coap/coap.h"
#include "cbor_encoder.h"
#include "cbor_encoder_ll.h"
#include "internal.h"
#include "io.h"

#if defined(ANJ_WITH_CBOR) || defined(ANJ_WITH_LWM2M_CBOR)
// HACK:
// The size of the internal_buff has been calculated so that a
// single record never exceeds its size.
int _anj_cbor_encode_value(_anj_io_buff_t *buff_ctx,
                           const anj_io_out_entry_t *entry) {
    size_t buf_pos = buff_ctx->bytes_in_internal_buff;

    switch (entry->type) {
    case ANJ_DATA_TYPE_BYTES: {
        if (entry->value.bytes_or_string.offset != 0
                || (entry->value.bytes_or_string.full_length_hint
                    && entry->value.bytes_or_string.full_length_hint
                               != entry->value.bytes_or_string.chunk_length)) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }
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
        buf_pos += anj_cbor_ll_indefinite_string_begin(
                &buff_ctx->internal_buff[buf_pos]);
        buff_ctx->is_extended_type = true;
        buff_ctx->remaining_bytes = 1;
        break;
    }
#    endif // ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_TIME: {
        buf_pos += anj_cbor_ll_encode_tag(&buff_ctx->internal_buff[buf_pos],
                                          CBOR_TAG_INTEGER_DATE_TIME);
        buf_pos += anj_cbor_ll_encode_int(&buff_ctx->internal_buff[buf_pos],
                                          entry->value.time_value);
        break;
    }
    case ANJ_DATA_TYPE_INT: {
        buf_pos += anj_cbor_ll_encode_int(&buff_ctx->internal_buff[buf_pos],
                                          entry->value.int_value);
        break;
    }
    case ANJ_DATA_TYPE_DOUBLE: {
        buf_pos += anj_cbor_ll_encode_double(&buff_ctx->internal_buff[buf_pos],
                                             entry->value.double_value);
        break;
    }
    case ANJ_DATA_TYPE_BOOL: {
        buf_pos += anj_cbor_ll_encode_bool(&buff_ctx->internal_buff[buf_pos],
                                           entry->value.bool_value);
        break;
    }
    case ANJ_DATA_TYPE_OBJLNK: {
        buf_pos += _anj_io_out_add_objlink(buff_ctx, buf_pos,
                                           entry->value.objlnk.oid,
                                           entry->value.objlnk.iid);
        break;
    }
    case ANJ_DATA_TYPE_UINT: {
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
#endif // defined(ANJ_WITH_CBOR) || defined(ANJ_WITH_LWM2M_CBOR)

#ifdef ANJ_WITH_CBOR
int _anj_cbor_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                const anj_io_out_entry_t *entry) {
    assert(ctx->format == _ANJ_COAP_FORMAT_CBOR);

    if (ctx->encoder.cbor.entry_added) {
        return _ANJ_IO_ERR_LOGIC;
    }

    int res = _anj_cbor_encode_value(&ctx->buff, entry);
    if (res) {
        return res;
    }
    ctx->encoder.cbor.entry_added = true;
    return 0;
}

int _anj_cbor_encoder_init(_anj_io_out_ctx_t *ctx) {
    ctx->encoder.cbor.entry_added = false;
    return 0;
}
#endif // ANJ_WITH_CBOR
