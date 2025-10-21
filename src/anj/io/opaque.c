/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <anj/defs.h>
#include <anj/utils.h>

#include "../coap/coap.h"
#include "io.h"
#include "opaque.h"

#ifdef ANJ_WITH_OPAQUE

#    define EXT_DATA_BUF_SIZE 64

static int prepare_payload(const anj_io_out_entry_t *entry,
                           _anj_io_buff_t *buff_ctx) {
    buff_ctx->bytes_in_internal_buff = 0;
    buff_ctx->is_extended_type = true;
    switch (entry->type) {
    case ANJ_DATA_TYPE_BYTES: {
        if (entry->value.bytes_or_string.offset != 0
                || (entry->value.bytes_or_string.full_length_hint
                    && entry->value.bytes_or_string.full_length_hint
                               != entry->value.bytes_or_string.chunk_length)) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }

        buff_ctx->remaining_bytes = entry->value.bytes_or_string.chunk_length;
        break;
    }
#    ifdef ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_EXTERNAL_BYTES: {
        if (!entry->value.external_data.get_external_data) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }
        // HACK: we don't know the length yet
        buff_ctx->remaining_bytes = 1;
        break;
    }
#    endif // ANJ_WITH_EXTERNAL_DATA
    default: { return _ANJ_IO_ERR_FORMAT; }
    }

    return 0;
}

int _anj_opaque_out_init(_anj_io_out_ctx_t *ctx) {
    ctx->encoder.opaque.entry_added = false;
    return 0;
}

int _anj_opaque_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                  const anj_io_out_entry_t *entry) {
    assert(ctx);
    assert(ctx->format == _ANJ_COAP_FORMAT_OPAQUE_STREAM);
    assert(entry);

    if (ctx->encoder.opaque.entry_added) {
        return _ANJ_IO_ERR_LOGIC;
    }

    int res = prepare_payload(entry, &ctx->buff);
    if (res) {
        return res;
    }
    ctx->encoder.opaque.entry_added = true;
    return 0;
}

int _anj_opaque_get_extended_data_payload(void *out_buff,
                                          size_t out_buff_len,
                                          size_t *inout_copied_bytes,
                                          _anj_io_buff_t *ctx,
                                          const anj_io_out_entry_t *entry) {
    size_t bytes_to_copy = 0;

    switch (entry->type) {

    case ANJ_DATA_TYPE_BYTES: {
        bytes_to_copy = ANJ_MIN(out_buff_len, ctx->remaining_bytes);
        memcpy((uint8_t *) out_buff,
               (const uint8_t *) entry->value.bytes_or_string.data
                       + ctx->offset,
               bytes_to_copy);
        ctx->remaining_bytes -= bytes_to_copy;
        break;
    }
#    ifdef ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_EXTERNAL_BYTES: {
        int ret;
        bytes_to_copy = out_buff_len;
        ret = entry->value.external_data.get_external_data(
                out_buff, &bytes_to_copy, ctx->offset,
                entry->value.external_data.user_args);
        if (ret && ret != ANJ_IO_NEED_NEXT_CALL) {
            return ret;
        } else if (!ret) {
            ctx->remaining_bytes = 0;
        }
        break;
    }
#    endif // ANJ_WITH_EXTERNAL_DATA
    }
    *inout_copied_bytes = bytes_to_copy;
    ctx->offset += bytes_to_copy;

    if (ctx->remaining_bytes) {
        return ANJ_IO_NEED_NEXT_CALL;
    } else {
        return 0;
    }
}

int _anj_opaque_decoder_init(_anj_io_in_ctx_t *ctx,
                             const anj_uri_path_t *request_uri) {
    assert(ctx);
    assert(request_uri);
    if (!(anj_uri_path_has(request_uri, ANJ_ID_RID))) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    memset(&ctx->out_value, 0x00, sizeof(ctx->out_value));
    ctx->out_path = *request_uri;
    ctx->decoder.opaque.want_payload = true;
    return 0;
}

int _anj_opaque_decoder_feed_payload(_anj_io_in_ctx_t *ctx,
                                     void *buff,
                                     size_t buff_size,
                                     bool payload_finished) {
    if (ctx->decoder.opaque.want_payload
            && !ctx->decoder.opaque.payload_finished) {
        ctx->out_value.bytes_or_string.offset +=
                ctx->out_value.bytes_or_string.chunk_length;
        ctx->out_value.bytes_or_string.chunk_length = buff_size;
        if (buff_size) {
            ctx->out_value.bytes_or_string.data = buff;
        } else {
            ctx->out_value.bytes_or_string.data = NULL;
        }
        if (payload_finished) {
            ctx->out_value.bytes_or_string.full_length_hint =
                    ctx->out_value.bytes_or_string.offset
                    + ctx->out_value.bytes_or_string.chunk_length;
        } else {
            ctx->out_value.bytes_or_string.full_length_hint = 0;
        }
        ctx->decoder.opaque.payload_finished = payload_finished;
        ctx->decoder.opaque.want_payload = false;
        return 0;
    }
    return _ANJ_IO_ERR_LOGIC;
}

int _anj_opaque_decoder_get_entry(_anj_io_in_ctx_t *ctx,
                                  anj_data_type_t *inout_type_bitmask,
                                  const anj_res_value_t **out_value,
                                  const anj_uri_path_t **out_path) {
    assert(ctx && inout_type_bitmask && out_value && out_path);
    if (ctx->decoder.opaque.eof_already_returned) {
        return _ANJ_IO_ERR_LOGIC;
    }
    *out_value = NULL;
    *out_path = &ctx->out_path;
    *inout_type_bitmask &= ANJ_DATA_TYPE_BYTES;
    assert(*inout_type_bitmask == ANJ_DATA_TYPE_BYTES
           || *inout_type_bitmask == ANJ_DATA_TYPE_NULL);
    if (*inout_type_bitmask == ANJ_DATA_TYPE_NULL) {
        return _ANJ_IO_ERR_FORMAT;
    }
    if (ctx->decoder.opaque.want_payload) {
        if (ctx->decoder.opaque.payload_finished) {
            ctx->decoder.opaque.eof_already_returned = true;
            return _ANJ_IO_EOF;
        }
        return _ANJ_IO_WANT_NEXT_PAYLOAD;
    }
    ctx->decoder.opaque.want_payload = true;
    *out_value = &ctx->out_value;
    return 0;
}

int _anj_opaque_decoder_get_entry_count(_anj_io_in_ctx_t *ctx,
                                        size_t *out_count) {
    assert(ctx);
    assert(out_count);
    (void) ctx;
    *out_count = 1;
    return 0;
}

#endif // ANJ_WITH_OPAQUE
