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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include "../coap/coap.h"
#include "cbor_encoder.h"
#include "cbor_encoder_ll.h"
#include "internal.h"
#include "io.h"

#ifdef ANJ_WITH_LWM2M_CBOR

static size_t uri_path_span(const anj_uri_path_t *a, const anj_uri_path_t *b) {
    size_t equal_ids = 0;
    for (; equal_ids < ANJ_MIN(anj_uri_path_length(a), anj_uri_path_length(b));
         equal_ids++) {
        if (a->ids[equal_ids] != b->ids[equal_ids]) {
            break;
        }
    }

    return equal_ids;
}

static void
end_maps(_anj_io_buff_t *buff_ctx, uint8_t *map_counter, size_t count) {
    for (size_t i = 0; i < count; i++) {
        size_t bytes_written = anj_cbor_ll_indefinite_record_end(
                &buff_ctx->internal_buff[buff_ctx->bytes_in_internal_buff]);
        buff_ctx->bytes_in_internal_buff += bytes_written;
        assert(buff_ctx->bytes_in_internal_buff <= _ANJ_IO_CTX_BUFFER_LENGTH);
        (*map_counter)--;
    }
}

static void encode_subpath(_anj_io_buff_t *buff_ctx,
                           uint8_t *map_counter,
                           const anj_uri_path_t *path,
                           size_t begin_idx) {
    for (size_t idx = begin_idx; idx < anj_uri_path_length(path); idx++) {
        size_t bytes_written = 0;
        // for the first record anj_cbor_ll_indefinite_map_begin() is
        // called in _anj_lwm2m_cbor_encoder_init(), for the rest, the first ID
        // is a continuation of the open map
        if (idx != begin_idx) {
            bytes_written = anj_cbor_ll_indefinite_map_begin(
                    &buff_ctx->internal_buff[buff_ctx->bytes_in_internal_buff]);
            (*map_counter)++;
        }
        bytes_written += anj_cbor_ll_encode_uint(
                &buff_ctx->internal_buff[buff_ctx->bytes_in_internal_buff
                                         + bytes_written],
                path->ids[idx]);
        buff_ctx->bytes_in_internal_buff += bytes_written;
        assert(buff_ctx->bytes_in_internal_buff <= _ANJ_IO_CTX_BUFFER_LENGTH);
    }
}

static void encode_path(_anj_io_out_ctx_t *ctx, const anj_uri_path_t *path) {
    assert(anj_uri_path_has(path, ANJ_ID_RID));
    _anj_lwm2m_cbor_encoder_t *lwm2m_cbor = &ctx->encoder.lwm2m;

    size_t path_span;

    // we don't want to compare both paths if they are the same
    // since encoding following sequence:
    // /3/0/1
    // /3/0/1
    // is the same case as this case:
    // /3/0/1
    // /3/0/2
    // so we just return span as length -1
    if (anj_uri_path_equal(&lwm2m_cbor->last_path, path)) {
        path_span = anj_uri_path_length(&lwm2m_cbor->last_path) - 1;
    } else {
        path_span = uri_path_span(&lwm2m_cbor->last_path, path);
    }

    if (anj_uri_path_length(&lwm2m_cbor->last_path)) {
        assert(path_span < anj_uri_path_length(&lwm2m_cbor->last_path));
        // close open maps to the level where the paths do not differ
        end_maps(&ctx->buff, &ctx->encoder.lwm2m.maps_opened,
                 anj_uri_path_length(&lwm2m_cbor->last_path) - (path_span + 1));
    }
    // write path from the level where the path differs from the last one
    encode_subpath(&ctx->buff, &ctx->encoder.lwm2m.maps_opened, path,
                   path_span);

    lwm2m_cbor->last_path = *path;
}

static int prepare_payload(_anj_io_out_ctx_t *ctx,
                           const anj_io_out_entry_t *entry) {
    encode_path(ctx, &entry->path);

    _anj_io_buff_t *buff_ctx = &ctx->buff;
    int ret_val = _anj_cbor_encode_value(buff_ctx, entry);
    if (ret_val) {
        return ret_val;
    }

    _anj_lwm2m_cbor_encoder_t *lwm2m_cbor = &ctx->encoder.lwm2m;
    lwm2m_cbor->items_count--;
    // last record, add map endings
    if (!lwm2m_cbor->items_count) {
        buff_ctx->is_extended_type = true;
        buff_ctx->remaining_bytes += lwm2m_cbor->maps_opened;
    }
    return 0;
}

int _anj_lwm2m_cbor_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                      const anj_io_out_entry_t *entry) {
    assert(ctx->format == _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR);
    _anj_lwm2m_cbor_encoder_t *lwm2m_cbor = &ctx->encoder.lwm2m;

    if (ctx->buff.remaining_bytes || !lwm2m_cbor->items_count) {
        return _ANJ_IO_ERR_LOGIC;
    }
    if (anj_uri_path_outside_base(&entry->path, &lwm2m_cbor->base_path)
            || !anj_uri_path_has(&entry->path, ANJ_ID_RID)) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }

    return prepare_payload(ctx, entry);
}

int _anj_lwm2m_cbor_encoder_init(_anj_io_out_ctx_t *ctx,
                                 const anj_uri_path_t *base_path,
                                 size_t items_count) {
    assert(base_path);

    _anj_lwm2m_cbor_encoder_t *lwm2m_cbor = &ctx->encoder.lwm2m;

    lwm2m_cbor->items_count = items_count;
    lwm2m_cbor->last_path = ANJ_MAKE_ROOT_PATH();
    lwm2m_cbor->base_path = *base_path;

    lwm2m_cbor->maps_opened = 1;
    _anj_io_buff_t *buff_ctx = &ctx->buff;
    buff_ctx->bytes_in_internal_buff =
            anj_cbor_ll_indefinite_map_begin(buff_ctx->internal_buff);
    return 0;
}

int _anj_get_lwm2m_cbor_map_ends(_anj_io_out_ctx_t *ctx,
                                 void *out_buff,
                                 size_t out_buff_len,
                                 size_t *inout_copied_bytes) {
    _anj_io_buff_t *buff_ctx = &ctx->buff;
    _anj_lwm2m_cbor_encoder_t *lwm2m_cbor = &ctx->encoder.lwm2m;

    size_t maps_to_end = ANJ_MIN(out_buff_len - *inout_copied_bytes,
                                 lwm2m_cbor->maps_opened);
    memset(&((uint8_t *) out_buff)[*inout_copied_bytes],
           CBOR_INDEFINITE_STRUCTURE_BREAK, maps_to_end);
    *inout_copied_bytes += maps_to_end;
    lwm2m_cbor->maps_opened -= (uint8_t) maps_to_end;
    buff_ctx->remaining_bytes -= maps_to_end;

    if (buff_ctx->remaining_bytes) {
        return ANJ_IO_NEED_NEXT_CALL;
    }
    return 0;
}

#endif // ANJ_WITH_LWM2M_CBOR
