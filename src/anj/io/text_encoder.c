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

#include <anj/defs.h>
#include <anj/utils.h>

#include "../coap/coap.h"
#include "base64.h"
#include "io.h"
#include "text_encoder.h"

#ifdef ANJ_WITH_PLAINTEXT

#    define BASE64_NO_PADDING_MULTIPIER 3
#    define _ANJ_BASE64_ENCODED_MULTIPLIER 4
#    define MAX_CHUNK_FOR_BASE64(x) \
        (BASE64_NO_PADDING_MULTIPIER * ((x) / _ANJ_BASE64_ENCODED_MULTIPLIER))

#    define EXT_DATA_BUF_SIZE (16 * BASE64_NO_PADDING_MULTIPIER)

// HACK:
// The size of the internal_buff has been calculated so that a
// single record never exceeds its size.
static int prepare_payload(const anj_io_out_entry_t *entry,
                           _anj_io_buff_t *buff_ctx) {
    switch (entry->type) {
    case ANJ_DATA_TYPE_BYTES: {
        if (entry->value.bytes_or_string.offset != 0
                || (entry->value.bytes_or_string.full_length_hint
                    && entry->value.bytes_or_string.full_length_hint
                               != entry->value.bytes_or_string.chunk_length)) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }

        buff_ctx->remaining_bytes = entry->value.bytes_or_string.chunk_length;
        buff_ctx->bytes_in_internal_buff = 0;
        buff_ctx->is_extended_type = true;
        break;
    }
#    ifdef ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_EXTERNAL_STRING:
    case ANJ_DATA_TYPE_EXTERNAL_BYTES: {
        if (!entry->value.external_data.get_external_data) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }
        // HACK: For ANJ_WITH_EXTERNAL_* types, use a fixed high value, since
        // the actual length is unknown. Due to the formula:
        // ANJ_MIN(MAX_CHUNK_FOR_BASE64(not_used_size),buff_ctx->remaining_bytes),
        // we set a high constant to ensure maximum single chunk size, typically
        // 1024 bytes. Future binding types might require an even larger buffer
        // size.
        buff_ctx->remaining_bytes = UINT32_MAX;
        buff_ctx->bytes_in_internal_buff = 0;
        buff_ctx->is_extended_type = true;
        break;
    }
#    endif // ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_STRING: {
        if (entry->value.bytes_or_string.offset != 0
                || (entry->value.bytes_or_string.full_length_hint
                    && entry->value.bytes_or_string.full_length_hint
                               != entry->value.bytes_or_string.chunk_length)) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }

        size_t entry_len;
        if (!(entry_len = entry->value.bytes_or_string.chunk_length)
                && entry->value.bytes_or_string.data) {
            entry_len =
                    strlen((const char *) entry->value.bytes_or_string.data);
        }

        buff_ctx->bytes_in_internal_buff = 0;
        buff_ctx->remaining_bytes = entry_len;
        buff_ctx->is_extended_type = true;
        break;
    }
    case ANJ_DATA_TYPE_INT: {
        buff_ctx->bytes_in_internal_buff =
                anj_int64_to_string_value((char *) buff_ctx->internal_buff,
                                          entry->value.int_value);
        buff_ctx->remaining_bytes = buff_ctx->bytes_in_internal_buff;
        break;
    }
    case ANJ_DATA_TYPE_DOUBLE: {
        buff_ctx->bytes_in_internal_buff =
                anj_double_to_string_value((char *) buff_ctx->internal_buff,
                                           entry->value.double_value);
        buff_ctx->remaining_bytes = buff_ctx->bytes_in_internal_buff;
        break;
    }
    case ANJ_DATA_TYPE_BOOL: {
        buff_ctx->bytes_in_internal_buff = 1;
        buff_ctx->internal_buff[0] = entry->value.bool_value ? '1' : '0';
        buff_ctx->remaining_bytes = buff_ctx->bytes_in_internal_buff;
        break;
    }
    case ANJ_DATA_TYPE_OBJLNK: {
        buff_ctx->bytes_in_internal_buff =
                anj_uint16_to_string_value((char *) buff_ctx->internal_buff,
                                           entry->value.objlnk.oid);
        buff_ctx->internal_buff[buff_ctx->bytes_in_internal_buff++] = ':';
        buff_ctx->bytes_in_internal_buff += anj_uint16_to_string_value(
                (char *) buff_ctx->internal_buff
                        + buff_ctx->bytes_in_internal_buff,
                entry->value.objlnk.iid);
        buff_ctx->remaining_bytes = buff_ctx->bytes_in_internal_buff;
        break;
    }
    case ANJ_DATA_TYPE_UINT: {
        buff_ctx->bytes_in_internal_buff =
                anj_uint64_to_string_value((char *) buff_ctx->internal_buff,
                                           entry->value.uint_value);
        buff_ctx->remaining_bytes = buff_ctx->bytes_in_internal_buff;
        break;
    }
    case ANJ_DATA_TYPE_TIME: {
        buff_ctx->bytes_in_internal_buff =
                anj_int64_to_string_value((char *) buff_ctx->internal_buff,
                                          entry->value.time_value);
        buff_ctx->remaining_bytes = buff_ctx->bytes_in_internal_buff;
        break;
    }
    default: { return _ANJ_IO_ERR_LOGIC; }
    }
    assert(buff_ctx->bytes_in_internal_buff <= _ANJ_IO_CTX_BUFFER_LENGTH);

    return 0;
}

const anj_base64_config_t ANJ_BASE64_CONFIG = {
    .alphabet = ANJ_BASE64_CHARS,
    .padding_char = '=',
    .allow_whitespace = false,
    .require_padding = true,
    .without_null_termination = true
};

static size_t encode_base64_payload(char *out_buff,
                                    size_t out_buff_len,
                                    const void *entry_buf,
                                    size_t bytes_to_encode) {
    if (!bytes_to_encode) {
        return 0;
    }

    assert(anj_base64_encoded_size_custom(bytes_to_encode, ANJ_BASE64_CONFIG)
           <= out_buff_len);
    anj_base64_encode_custom(out_buff, out_buff_len,
                             (const uint8_t *) entry_buf, bytes_to_encode,
                             ANJ_BASE64_CONFIG);

    return anj_base64_encoded_size_custom(bytes_to_encode, ANJ_BASE64_CONFIG);
}

int _anj_text_encoder_init(_anj_io_out_ctx_t *ctx) {
    ctx->encoder.text.entry_added = false;
    return 0;
}

int _anj_text_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                const anj_io_out_entry_t *entry) {
    assert(ctx);
    assert(ctx->format == _ANJ_COAP_FORMAT_PLAINTEXT);
    assert(entry);

    if (ctx->encoder.text.entry_added) {
        return _ANJ_IO_ERR_LOGIC;
    }

    int res = prepare_payload(entry, &ctx->buff);
    if (res) {
        return res;
    }
    ctx->encoder.text.entry_added = true;
    return 0;
}

static inline void
shift_ctx(_anj_io_buff_t *buff_ctx, size_t bytes_read, anj_data_type_t type) {
    (void) type;
    // don't decrease remaining_bytes for external data
    if (
#    ifdef ANJ_WITH_EXTERNAL_DATA
                type != ANJ_DATA_TYPE_EXTERNAL_BYTES &&
#    endif // ANJ_WITH_EXTERNAL_DATA
                    buff_ctx->remaining_bytes) {
        buff_ctx->remaining_bytes -= bytes_read;
    }
    buff_ctx->offset += bytes_read;
}

static int encode_bytes(char *encoded_buf,
                        size_t encoded_buf_size,
                        size_t *out_copied_bytes,
                        const anj_io_out_entry_t *entry,
                        size_t input_offset,
                        size_t *bytes_to_encode,
                        bool *last_chunk) {
    assert(encoded_buf);
    assert(entry);
    assert(bytes_to_encode);
    assert(*bytes_to_encode);
    assert(last_chunk);

    size_t copied_bytes = 0;
    switch (entry->type) {
    case ANJ_DATA_TYPE_BYTES: {
        copied_bytes = encode_base64_payload(
                encoded_buf, encoded_buf_size,
                (const uint8_t *) entry->value.bytes_or_string.data
                        + input_offset,
                *bytes_to_encode);
        break;
    }
#    ifdef ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_EXTERNAL_BYTES: {
        char ext_data_buf[EXT_DATA_BUF_SIZE] = { 0 };
        *bytes_to_encode = ANJ_MIN(*bytes_to_encode, EXT_DATA_BUF_SIZE);
        int ret = entry->value.external_data.get_external_data(
                ext_data_buf, bytes_to_encode, input_offset,
                entry->value.external_data.user_args);
        if (ret && ret != ANJ_IO_NEED_NEXT_CALL) {
            return ret;
        } else if (!ret) {
            *last_chunk = true;
        }
        copied_bytes = encode_base64_payload(encoded_buf, encoded_buf_size,
                                             ext_data_buf, *bytes_to_encode);
        break;
    }
#    endif // ANJ_WITH_EXTERNAL_DATA
    }

    if (out_copied_bytes) {
        *out_copied_bytes = copied_bytes;
    }

    return 0;
}

static int get_extended_data(void *out_buff,
                             size_t out_buff_len,
                             size_t *out_copied_bytes,
                             _anj_io_buff_t *buff_ctx,
                             const anj_io_out_entry_t *entry) {
    size_t bytes_to_get;
    assert(*out_copied_bytes == 0);
    int ret;
    /* Copy cached base64 bytes to out_buff */
    if (buff_ctx->b64_cache.cache_offset) {
        bytes_to_get = ANJ_MIN(sizeof(buff_ctx->b64_cache.buf)
                                       - buff_ctx->b64_cache.cache_offset,
                               out_buff_len);
        memcpy((char *) out_buff,
               buff_ctx->b64_cache.buf + buff_ctx->b64_cache.cache_offset,
               bytes_to_get);
        *out_copied_bytes = bytes_to_get;
        buff_ctx->b64_cache.cache_offset += bytes_to_get;
        /* Clear Base64 cache */
        if (buff_ctx->b64_cache.cache_offset
                >= sizeof(buff_ctx->b64_cache.buf)) {
            buff_ctx->b64_cache.cache_offset = 0;
        }
    }

    /* Exit if whole buff was filled with cached bytes */
    size_t not_used_size = out_buff_len - *out_copied_bytes;
    if (!not_used_size && buff_ctx->remaining_bytes) {
        return ANJ_IO_NEED_NEXT_CALL;
    }

    /* Encode next chunk of remaining_bytes to out_buff */
    while ((not_used_size = out_buff_len - *out_copied_bytes)
                   > BASE64_NO_PADDING_MULTIPIER
           && buff_ctx->remaining_bytes) {
        bytes_to_get = ANJ_MIN(MAX_CHUNK_FOR_BASE64(not_used_size),
                               buff_ctx->remaining_bytes);
        size_t copied_bytes;
        bool last_chunk = false;
        ret = encode_bytes(((char *) out_buff) + *out_copied_bytes,
                           not_used_size, &copied_bytes, entry,
                           buff_ctx->offset, &bytes_to_get, &last_chunk);
        if (ret) {
            return ret;
        }
        if (last_chunk) {
            buff_ctx->remaining_bytes = 0;
        }
        *out_copied_bytes += copied_bytes;
        shift_ctx(buff_ctx, bytes_to_get, entry->type);
    }

    /* Fill empty bytes in out_buff */
    if (buff_ctx->remaining_bytes && not_used_size) {
        assert(!buff_ctx->b64_cache.cache_offset);
        assert(not_used_size <= sizeof(buff_ctx->b64_cache.buf));
        size_t bytes_to_append =
                ANJ_MIN(MAX_CHUNK_FOR_BASE64(sizeof(buff_ctx->b64_cache.buf)),
                        buff_ctx->remaining_bytes);
        bool last_chunk = false;
        ret = encode_bytes((char *) buff_ctx->b64_cache.buf,
                           _ANJ_BASE64_ENCODED_MULTIPLIER, NULL, entry,
                           buff_ctx->offset, &bytes_to_append, &last_chunk);
        if (ret) {
            return ret;
        }
        if (last_chunk) {
            buff_ctx->remaining_bytes = 0;
        }
        memcpy((char *) out_buff + *out_copied_bytes, buff_ctx->b64_cache.buf,
               not_used_size);
        *out_copied_bytes += not_used_size;
        buff_ctx->b64_cache.cache_offset = not_used_size;
        shift_ctx(buff_ctx, bytes_to_append, entry->type);
    }

    return 0;
}

int _anj_text_get_extended_data_payload(void *out_buff,
                                        size_t out_buff_len,
                                        size_t *inout_copied_bytes,
                                        _anj_io_buff_t *buff_ctx,
                                        const anj_io_out_entry_t *entry) {
    assert(*inout_copied_bytes == 0);
    int ret;
    size_t bytes_to_get;

    switch (entry->type) {
    case ANJ_DATA_TYPE_BYTES:
#    ifdef ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_EXTERNAL_BYTES:
#    endif // ANJ_WITH_EXTERNAL_DATA
    {
        ret = get_extended_data(out_buff, out_buff_len, inout_copied_bytes,
                                buff_ctx, entry);
        if (ret) {
            return ret;
        }
        break;
    }
    case ANJ_DATA_TYPE_STRING: {
        bytes_to_get = ANJ_MIN(out_buff_len, buff_ctx->remaining_bytes);
        memcpy((uint8_t *) out_buff,
               (const uint8_t *) entry->value.bytes_or_string.data
                       + buff_ctx->offset,
               bytes_to_get);
        *inout_copied_bytes = bytes_to_get;
        shift_ctx(buff_ctx, bytes_to_get, entry->type);
        break;
    }
#    ifdef ANJ_WITH_EXTERNAL_DATA
    case ANJ_DATA_TYPE_EXTERNAL_STRING: {
        size_t in_out_size = out_buff_len;
        ret = entry->value.external_data.get_external_data(
                out_buff, &in_out_size, buff_ctx->offset,
                entry->value.external_data.user_args);
        buff_ctx->offset += in_out_size;
        *inout_copied_bytes = in_out_size;
        if (ret) {
            return ret;
        }
        buff_ctx->remaining_bytes = 0;
        break;
    }
#    endif // ANJ_WITH_EXTERNAL_DATA
    }

    if (buff_ctx->remaining_bytes || buff_ctx->b64_cache.cache_offset) {
        return ANJ_IO_NEED_NEXT_CALL;
    } else {
        return 0;
    }
}

#endif // ANJ_WITH_PLAINTEXT
