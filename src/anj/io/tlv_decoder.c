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

#include "../utils.h"
#include "io.h"
#include "tlv_decoder.h"

#ifdef ANJ_WITH_TLV

typedef enum {
    TLV_ID_IID = 0,
    TLV_ID_RIID = 1,
    TLV_ID_RID_ARRAY = 2,
    TLV_ID_RID = 3
} anj_tlv_id_type_t;

static _tlv_entry_t *tlv_entry_push(_anj_tlv_decoder_t *tlv) {
    if (tlv->entries == NULL) {
        tlv->entries = &tlv->entries_block[0];
    } else {
        ANJ_ASSERT(tlv->entries < &tlv->entries_block[_ANJ_TLV_MAX_DEPTH - 1],
                   "TLV decoder stack overflow");
        tlv->entries++;
    }
    return tlv->entries;
}

static void tlv_entry_pop(_anj_tlv_decoder_t *tlv) {
    assert(tlv->entries);
    if (tlv->entries == &tlv->entries_block[0]) {
        tlv->entries = NULL;
    } else {
        tlv->entries--;
    }
}

static int tlv_get_all_remaining_bytes(_anj_io_in_ctx_t *ctx,
                                       size_t *out_tlv_value_size,
                                       const void **out_chunk) {
    if (ctx->decoder.tlv.buff_size == ctx->decoder.tlv.buff_offset) {
        return -1;
    }
    *out_chunk =
            ((uint8_t *) ctx->decoder.tlv.buff) + ctx->decoder.tlv.buff_offset;
    // in the buff there could be: exactly one TLV entry, more than one TLV
    // entry as well as only a partial TLV entry
    *out_tlv_value_size =
            ANJ_MIN(ctx->decoder.tlv.entries->length
                            - ctx->decoder.tlv.entries->bytes_read,
                    ctx->decoder.tlv.buff_size - ctx->decoder.tlv.buff_offset);
    ctx->decoder.tlv.entries->bytes_read += *out_tlv_value_size;
    ctx->decoder.tlv.buff_offset += *out_tlv_value_size;
    return 0;
}

static int tlv_buff_read_by_copy(_anj_io_in_ctx_t *ctx,
                                 void *out_chunk,
                                 size_t chunk_size) {
    if (ctx->decoder.tlv.buff_size - ctx->decoder.tlv.buff_offset
            < chunk_size) {
        return -1;
    }
    memcpy(out_chunk,
           ((uint8_t *) ctx->decoder.tlv.buff) + ctx->decoder.tlv.buff_offset,
           chunk_size);
    ctx->decoder.tlv.buff_offset += chunk_size;
    return 0;
}

static int tlv_get_bytes(_anj_io_in_ctx_t *ctx) {
    size_t already_read = ctx->out_value.bytes_or_string.chunk_length;
    int result = tlv_get_all_remaining_bytes(
            ctx, &ctx->out_value.bytes_or_string.chunk_length,
            &ctx->out_value.bytes_or_string.data);
    if (result == -1 && ctx->decoder.tlv.entries->length) {
        ctx->decoder.tlv.want_payload = true;
        return _ANJ_IO_WANT_NEXT_PAYLOAD;
    }
    if (already_read) {
        ctx->out_value.bytes_or_string.offset += already_read;
    }
    ctx->out_value.bytes_or_string.full_length_hint =
            ctx->decoder.tlv.entries->length;
    return 0;
}

static int tlv_get_int(_anj_io_in_ctx_t *ctx, int64_t *value) {
    /** Note: value is either &ctx->out_value.int_value or
     * &ctx->out_value.time_value, so its value will be preserved between
     * calls. */
    uint8_t *bytes;
    size_t bytes_read;
    if (!_anj_is_power_of_2(ctx->decoder.tlv.entries->length)
            || ctx->decoder.tlv.entries->length > 8) {
        return _ANJ_IO_ERR_FORMAT;
    }
    int result =
            tlv_get_all_remaining_bytes(ctx, &bytes_read,
                                        (const void **) (uintptr_t) &bytes);
    if (result == -1) {
        ctx->decoder.tlv.want_payload = true;
        return _ANJ_IO_WANT_NEXT_PAYLOAD;
    }
    if (ctx->decoder.tlv.entries->bytes_read - bytes_read == 0) {
        *value = (bytes_read > 0 && ((int8_t) bytes[0]) < 0) ? -1 : 0;
    }
    for (size_t i = 0; i < bytes_read; ++i) {
        *(uint64_t *) value <<= 8;
        *value += bytes[i];
    }
    return 0;
}

static int tlv_get_uint(_anj_io_in_ctx_t *ctx) {
    uint8_t *bytes;
    size_t bytes_read;
    if (!_anj_is_power_of_2(ctx->decoder.tlv.entries->length)
            || ctx->decoder.tlv.entries->length > 8) {
        return _ANJ_IO_ERR_FORMAT;
    }
    int result =
            tlv_get_all_remaining_bytes(ctx, &bytes_read,
                                        (const void **) (uintptr_t) &bytes);
    if (result == -1) {
        ctx->decoder.tlv.want_payload = true;
        return _ANJ_IO_WANT_NEXT_PAYLOAD;
    }
    if (ctx->decoder.tlv.entries->bytes_read - bytes_read == 0) {
        ctx->out_value.uint_value = 0;
    }
    for (size_t i = 0; i < bytes_read; ++i) {
        ctx->out_value.uint_value <<= 8;
        ctx->out_value.uint_value += bytes[i];
    }
    return 0;
}

static int tlv_get_double(_anj_io_in_ctx_t *ctx) {
    uint8_t *bytes;
    size_t bytes_already_read = ctx->decoder.tlv.entries->bytes_read;
    size_t bytes_read;
    int result =
            tlv_get_all_remaining_bytes(ctx, &bytes_read,
                                        (const void **) (uintptr_t) &bytes);
    if (result == -1) {
        ctx->decoder.tlv.want_payload = true;
        return _ANJ_IO_WANT_NEXT_PAYLOAD;
    }
    memcpy((uint8_t *) &ctx->out_value.double_value + bytes_already_read, bytes,
           bytes_read);
    if (ctx->decoder.tlv.entries->bytes_read
            == ctx->decoder.tlv.entries->length) {
        switch (ctx->decoder.tlv.entries->length) {
        case 4: {
            // this double is stored in 4 bytes, to avoid compilation warnings
            // we need to copy it to a uint32_t before converting it
            uint32_t tmp;
            memcpy(&tmp, &ctx->out_value.uint_value,
                   ctx->decoder.tlv.entries->length);
            ctx->out_value.double_value = _anj_ntohf(tmp);
            break;
        }
        case 8: {
            ctx->out_value.double_value = _anj_ntohd(ctx->out_value.uint_value);
            break;
        }
        default:
            return _ANJ_IO_ERR_FORMAT;
        }
    }
    return 0;
}

static int tlv_get_bool(_anj_io_in_ctx_t *ctx) {
    uint8_t *bytes;
    size_t bytes_read;
    if (ctx->decoder.tlv.entries->length != 1) {
        return _ANJ_IO_ERR_FORMAT;
    }
    int result =
            tlv_get_all_remaining_bytes(ctx, &bytes_read,
                                        (const void **) (uintptr_t) &bytes);
    if (result == -1) {
        ctx->decoder.tlv.want_payload = true;
        return _ANJ_IO_WANT_NEXT_PAYLOAD;
    }
    switch (bytes[0]) {
    case 0:
        ctx->out_value.bool_value = false;
        return 0;
    case 1:
        ctx->out_value.bool_value = true;
        return 0;
    default:
        return _ANJ_IO_ERR_FORMAT;
    }
}

static int tlv_get_objlnk(_anj_io_in_ctx_t *ctx) {
    uint8_t *bytes;
    size_t bytes_already_read = ctx->decoder.tlv.entries->bytes_read;
    size_t bytes_read;
    if (ctx->decoder.tlv.entries->length != 4) {
        return _ANJ_IO_ERR_FORMAT;
    }
    int result =
            tlv_get_all_remaining_bytes(ctx, &bytes_read,
                                        (const void **) (uintptr_t) &bytes);
    if (result == -1) {
        ctx->decoder.tlv.want_payload = true;
        return _ANJ_IO_WANT_NEXT_PAYLOAD;
    }
    for (size_t i = 0; i < bytes_read; ++i) {
        if (bytes_already_read + i < 2) {
            memcpy((uint8_t *) &ctx->out_value.objlnk.oid + bytes_already_read
                           + i,
                   &bytes[i], 1);
        } else {
            memcpy((uint8_t *) &ctx->out_value.objlnk.iid + bytes_already_read
                           + i - 2,
                   &bytes[i], 1);
        }
    }
    if (ctx->decoder.tlv.entries->bytes_read
            == ctx->decoder.tlv.entries->length) {
        ctx->out_value.objlnk.oid =
                _anj_convert_be16(ctx->out_value.objlnk.oid);
        ctx->out_value.objlnk.iid =
                _anj_convert_be16(ctx->out_value.objlnk.iid);
    }
    return 0;
}

static int tlv_id_length_buff_read_by_copy(_anj_io_in_ctx_t *ctx,
                                           uint8_t *out,
                                           size_t length) {
    if (ctx->decoder.tlv.id_length_buff_read_offset + length
            > sizeof(ctx->decoder.tlv.id_length_buff)) {
        return -1;
    }
    memcpy(out,
           ((uint8_t *) ctx->decoder.tlv.id_length_buff)
                   + ctx->decoder.tlv.id_length_buff_read_offset,
           length);
    ctx->decoder.tlv.id_length_buff_read_offset += length;
    return 0;
}

#    define DEF_READ_SHORTENED(Type)                                           \
        static int read_shortened_##Type(_anj_io_in_ctx_t *ctx, size_t length, \
                                         Type *out) {                          \
            uint8_t bytes[sizeof(Type)];                                       \
            if (tlv_id_length_buff_read_by_copy(ctx, bytes, length)) {         \
                return -1;                                                     \
            }                                                                  \
            *out = 0;                                                          \
            for (size_t i = 0; i < length; ++i) {                              \
                *out = (Type) ((*out << 8) + bytes[i]);                        \
            }                                                                  \
            return 0;                                                          \
        }

DEF_READ_SHORTENED(uint16_t)
DEF_READ_SHORTENED(size_t)

static anj_tlv_id_type_t tlv_type_from_typefield(uint8_t typefield) {
    return (anj_tlv_id_type_t) ((typefield >> 6) & 3);
}

static anj_id_type_t convert_id_type(uint8_t typefield) {
    switch (tlv_type_from_typefield(typefield)) {
    default:
        ANJ_UNREACHABLE("Invalid TLV ID type");
    case TLV_ID_IID:
        return ANJ_ID_IID;
    case TLV_ID_RIID:
        return ANJ_ID_RIID;
    case TLV_ID_RID_ARRAY:
    case TLV_ID_RID:
        return ANJ_ID_RID;
    }
}

static int get_id(_anj_io_in_ctx_t *ctx,
                  anj_id_type_t *out_type,
                  uint16_t *out_id,
                  bool *out_has_value,
                  size_t *out_bytes_read) {
    uint8_t typefield = ctx->decoder.tlv.type_field;
    *out_bytes_read = 1;
    anj_tlv_id_type_t tlv_type = tlv_type_from_typefield(typefield);
    *out_type = convert_id_type(typefield);
    size_t id_length = (typefield & 0x20) ? 2 : 1;
    if (read_shortened_uint16_t(ctx, id_length, out_id)) {
        return _ANJ_IO_ERR_FORMAT;
    }
    *out_bytes_read += id_length;

    size_t length_length = ((typefield >> 3) & 3);
    if (!length_length) {
        ctx->decoder.tlv.entries->length = (typefield & 7);
    } else if (read_shortened_size_t(ctx, length_length,
                                     &ctx->decoder.tlv.entries->length)) {
        return _ANJ_IO_ERR_FORMAT;
    }
    *out_bytes_read += length_length;
    /**
     * This may seem a little bit strange, but entries that do not have any
     * payload may be considered as having a value - that is, an empty one. On
     * the other hand, if they DO have the payload, then it only makes sense to
     * return them if they're "terminal" - i.e. they're either resource
     * instances or single resources with value.
     */
    *out_has_value = !ctx->decoder.tlv.entries->length
                     || tlv_type == TLV_ID_RIID || tlv_type == TLV_ID_RID;
    ctx->decoder.tlv.entries->bytes_read = 0;
    ctx->decoder.tlv.entries->type = *out_type;
    return 0;
}

static int get_type_and_header(_anj_io_in_ctx_t *ctx) {
    if (ctx->decoder.tlv.type_field == 0xFF) {
        if (tlv_buff_read_by_copy(ctx, &ctx->decoder.tlv.type_field, 1)) {
            ctx->decoder.tlv.want_payload = true;
            return _ANJ_IO_WANT_NEXT_PAYLOAD;
        }
        if (ctx->decoder.tlv.type_field == 0xFF) {
            return _ANJ_IO_ERR_FORMAT;
        }
        size_t id_length = (ctx->decoder.tlv.type_field & 0x20) ? 2 : 1;
        size_t length_length = ((ctx->decoder.tlv.type_field >> 3) & 3);
        ctx->decoder.tlv.id_length_buff_bytes_need = id_length + length_length;
    }
    if (ctx->decoder.tlv.id_length_buff_bytes_need > 0) {
        if (ctx->decoder.tlv.buff_size - ctx->decoder.tlv.buff_offset <= 0) {
            ctx->decoder.tlv.want_payload = true;
            return _ANJ_IO_WANT_NEXT_PAYLOAD;
        }
        size_t bytes_to_read = ANJ_MIN(
                ctx->decoder.tlv.id_length_buff_bytes_need,
                ctx->decoder.tlv.buff_size - ctx->decoder.tlv.buff_offset);
        if (tlv_buff_read_by_copy(
                    ctx,
                    ctx->decoder.tlv.id_length_buff
                            + ctx->decoder.tlv.id_length_buff_write_offset,
                    bytes_to_read)) {
            return _ANJ_IO_ERR_FORMAT;
        }
        ctx->decoder.tlv.id_length_buff_write_offset += bytes_to_read;
        ctx->decoder.tlv.id_length_buff_bytes_need -= bytes_to_read;
        if (ctx->decoder.tlv.id_length_buff_bytes_need == 0) {
            return 0;
        }
        ctx->decoder.tlv.want_payload = true;
        return _ANJ_IO_WANT_NEXT_PAYLOAD;
    }
    return 0;
}

static int tlv_get_path(_anj_io_in_ctx_t *ctx) {
    if (ctx->decoder.tlv.has_path) {
        ctx->out_path = ctx->decoder.tlv.current_path;
        return 0;
    }
    bool has_value = false;
    anj_id_type_t type;
    uint16_t id;
    while (!has_value) {
        int result;
        if ((result = get_type_and_header(ctx))) {
            return result;
        }
        _tlv_entry_t *parent = ctx->decoder.tlv.entries;
        if (!tlv_entry_push(&ctx->decoder.tlv)) {
            return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
        }
        size_t header_len;
        if ((result = get_id(ctx, &type, &id, &has_value, &header_len))) {
            return result;
        }
        if (id == ANJ_ID_INVALID) {
            return _ANJ_IO_ERR_FORMAT;
        }
        if (parent) {
            // Assume the child entry is fully read (which is in fact necessary
            // to be able to return back to the parent).
            parent->bytes_read += ctx->decoder.tlv.entries->length + header_len;
            if (parent->bytes_read > parent->length) {
                return _ANJ_IO_ERR_FORMAT;
            }
        }
        ctx->decoder.tlv.current_path.ids[(size_t) type] = id;
        ctx->decoder.tlv.current_path.uri_len = (size_t) type + 1;

        if (anj_uri_path_outside_base(&ctx->decoder.tlv.current_path,
                                      &ctx->decoder.tlv.uri_path)) {
            return _ANJ_IO_ERR_FORMAT;
        }
        ctx->decoder.tlv.type_field = 0xFF;
    }
    ctx->out_path = ctx->decoder.tlv.current_path;
    ctx->decoder.tlv.has_path = true;
    return 0;
}

static int tlv_next_entry(_anj_io_in_ctx_t *ctx) {
    if (!ctx->decoder.tlv.has_path) {
        // Next entry is already available and should be processed.
        return 0;
    }
    if (!ctx->decoder.tlv.entries) {
        return _ANJ_IO_ERR_FORMAT;
    }
    if (ctx->decoder.tlv.entries->length
            > ctx->decoder.tlv.entries->bytes_read) {
        const void *ignored;
        size_t ignored_bytes_read;
        int result =
                tlv_get_all_remaining_bytes(ctx, &ignored_bytes_read, &ignored);
        if (result) {
            return result;
        }
    }
    ctx->decoder.tlv.has_path = false;
    ctx->decoder.tlv.type_field = 0xFF;
    while (ctx->decoder.tlv.entries
           && ctx->decoder.tlv.entries->length
                      == ctx->decoder.tlv.entries->bytes_read) {
        ctx->decoder.tlv.current_path.ids[ctx->decoder.tlv.entries->type] =
                ANJ_ID_INVALID;
        ctx->decoder.tlv.current_path.uri_len =
                (size_t) ctx->decoder.tlv.entries->type;
        tlv_entry_pop(&ctx->decoder.tlv);
    }
    return 0;
}

int _anj_tlv_decoder_init(_anj_io_in_ctx_t *ctx,
                          const anj_uri_path_t *request_uri) {
    assert(ctx);
    assert(request_uri);
    assert(!anj_uri_path_equal(request_uri, &ANJ_MAKE_ROOT_PATH()));
    memset(&ctx->out_value, 0x00, sizeof(ctx->out_value));
    memset(&ctx->out_path, 0x00, sizeof(ctx->out_path));
    ctx->decoder.tlv.uri_path = *request_uri;
    ctx->decoder.tlv.current_path = ctx->decoder.tlv.uri_path;
    ctx->decoder.tlv.type_field = 0xFF;
    ctx->decoder.tlv.want_payload = true;

    return 0;
}

int _anj_tlv_decoder_feed_payload(_anj_io_in_ctx_t *ctx,
                                  void *buff,
                                  size_t buff_size,
                                  bool payload_finished) {
    if (ctx->decoder.tlv.want_payload) {
        ctx->decoder.tlv.buff = buff;
        ctx->decoder.tlv.buff_size = buff_size;
        ctx->decoder.tlv.buff_offset = 0;
        ctx->decoder.tlv.payload_finished = payload_finished;
        ctx->decoder.tlv.want_payload = false;
        return 0;
    }
    return _ANJ_IO_ERR_LOGIC;
}

int _anj_tlv_decoder_get_entry(_anj_io_in_ctx_t *ctx,
                               anj_data_type_t *inout_type_bitmask,
                               const anj_res_value_t **out_value,
                               const anj_uri_path_t **out_path) {
    assert(ctx);
    assert(inout_type_bitmask);
    assert(out_value);
    assert(out_path);
    int result;
    if (ctx->decoder.tlv.want_payload) {
        return _ANJ_IO_WANT_NEXT_PAYLOAD;
    }
    *out_value = NULL;
    *out_path = NULL;
    if (ctx->decoder.tlv.payload_finished
            && (ctx->decoder.tlv.buff_size == ctx->decoder.tlv.buff_offset)
            && !ctx->decoder.tlv.want_disambiguation) {
        return _ANJ_IO_EOF;
    }

    if (!ctx->decoder.tlv.entries || !ctx->decoder.tlv.has_path) {
        memset(&ctx->out_value, 0x00, sizeof(ctx->out_value));
        memset(&ctx->out_path, 0x00, sizeof(ctx->out_path));
        result = tlv_get_path(ctx);
        if (result) {
            *out_path = NULL;
            if (result == _ANJ_IO_WANT_NEXT_PAYLOAD
                    && ctx->decoder.tlv.payload_finished) {
                return _ANJ_IO_ERR_FORMAT;
            }
            return result;
        }
        if (ctx->decoder.tlv.entries->length == 0) {
            if (ctx->decoder.tlv.entries->type == ANJ_ID_IID
                    || ctx->decoder.tlv.entries->type == ANJ_ID_RIID) {
                *out_path = &ctx->out_path;
                *inout_type_bitmask = ANJ_DATA_TYPE_NULL;
                return tlv_next_entry(ctx);
            }
        }
    }
    *out_path = &ctx->out_path;

    ctx->decoder.tlv.want_disambiguation = false;
    switch (*inout_type_bitmask) {
    case ANJ_DATA_TYPE_NULL:
        return _ANJ_IO_ERR_FORMAT;
    case ANJ_DATA_TYPE_BYTES:
    case ANJ_DATA_TYPE_STRING: {
        result = tlv_get_bytes(ctx);
        break;
    }
    case ANJ_DATA_TYPE_INT:
        result = tlv_get_int(ctx, &ctx->out_value.int_value);
        break;
    case ANJ_DATA_TYPE_UINT:
        result = tlv_get_uint(ctx);
        break;
    case ANJ_DATA_TYPE_DOUBLE:
        result = tlv_get_double(ctx);
        break;
    case ANJ_DATA_TYPE_BOOL:
        result = tlv_get_bool(ctx);
        break;
    case ANJ_DATA_TYPE_OBJLNK:
        result = tlv_get_objlnk(ctx);
        break;
    case ANJ_DATA_TYPE_TIME:
        result = tlv_get_int(ctx, &ctx->out_value.time_value);
        break;
    default:
        ctx->decoder.tlv.want_disambiguation = true;
        return _ANJ_IO_WANT_TYPE_DISAMBIGUATION;
    }
    if (result) {
        if (result == _ANJ_IO_WANT_NEXT_PAYLOAD
                && ctx->decoder.tlv.payload_finished) {
            return _ANJ_IO_ERR_FORMAT;
        }
        return result;
    }

    // reason about parsing state
    if (ctx->decoder.tlv.entries->bytes_read
            == ctx->decoder.tlv.entries->length) {
        if ((result = tlv_next_entry(ctx))) {
            return result;
        }
        *out_path = &ctx->out_path;
        *out_value = &ctx->out_value;
        return 0;
    } else {
        if (!ctx->decoder.tlv.payload_finished
                && (ctx->decoder.tlv.buff_size
                    == ctx->decoder.tlv.buff_offset)) {
            if (*inout_type_bitmask == ANJ_DATA_TYPE_BYTES
                    || *inout_type_bitmask == ANJ_DATA_TYPE_STRING) {
                *out_path = &ctx->out_path;
                *out_value = &ctx->out_value;
                return 0;
            }
            ctx->decoder.tlv.want_payload = true;
            return _ANJ_IO_WANT_NEXT_PAYLOAD;
        }
        return _ANJ_IO_ERR_FORMAT;
    }
}

#endif // ANJ_WITH_TLV
