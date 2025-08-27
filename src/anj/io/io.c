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
#include "../utils.h"
#include "cbor_decoder.h"
#include "cbor_encoder.h"
#include "cbor_encoder_ll.h"
#include "internal.h"
#include "io.h"
#include "opaque.h"
#include "text_decoder.h"
#include "text_encoder.h"
#include "tlv_decoder.h"

ANJ_STATIC_ASSERT(_ANJ_IO_CTX_BUFFER_LENGTH >= ANJ_CBOR_LL_SINGLE_CALL_MAX_LEN,
                  CBOR_buffer_too_small);

static const uint16_t supported_formats_list[] = {
#ifdef ANJ_WITH_OPAQUE
    _ANJ_COAP_FORMAT_OPAQUE_STREAM,
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_PLAINTEXT
    _ANJ_COAP_FORMAT_PLAINTEXT,
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_CBOR
    _ANJ_COAP_FORMAT_CBOR,
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_LWM2M_CBOR
    _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR,
#endif // ANJ_WITH_LWM2M_CBOR
#ifdef ANJ_WITH_SENML_CBOR
    _ANJ_COAP_FORMAT_SENML_CBOR,     _ANJ_COAP_FORMAT_SENML_ETCH_CBOR
#endif // ANJ_WITH_SENML_CBOR
};

void _anj_io_reset_internal_buff(_anj_io_buff_t *ctx) {
    assert(ctx);
    ctx->offset = 0;
    ctx->bytes_in_internal_buff = 0;
    ctx->is_extended_type = false;
    ctx->remaining_bytes = 0;
}

static int
check_format(uint16_t given_format, size_t items_count, _anj_op_t op) {
    if (given_format == _ANJ_COAP_FORMAT_NOT_DEFINED) {
        return 0;
    }
    bool is_present = false;
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(supported_formats_list); i++) {
        if (given_format == supported_formats_list[i]) {
            is_present = true;
            break;
        }
    }
    if (!is_present) {
        return _ANJ_IO_ERR_UNSUPPORTED_FORMAT;
    }
    // OPAQUE, CBOR and PLAINTEXT formats are allowed only for single record and
    // for 3 types of requests
    if ((given_format == _ANJ_COAP_FORMAT_OPAQUE_STREAM
         || given_format == _ANJ_COAP_FORMAT_CBOR
         || given_format == _ANJ_COAP_FORMAT_PLAINTEXT)
            && (items_count > 1
                || (op != ANJ_OP_DM_READ && op != ANJ_OP_INF_OBSERVE
                    && op != ANJ_OP_INF_CANCEL_OBSERVE))) {
        return _ANJ_IO_ERR_FORMAT;
    }
    return 0;
}

static uint16_t choose_format(uint16_t given_format) {
    if (given_format != _ANJ_COAP_FORMAT_NOT_DEFINED) {
        return given_format;
    }
#ifdef ANJ_WITH_LWM2M_CBOR
    return _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
#else  // ANJ_WITH_LWM2M_CBOR
    return _ANJ_COAP_FORMAT_SENML_CBOR;
#endif // ANJ_WITH_LWM2M_CBOR
}

static int get_cbor_bytes_string_data(_anj_io_buff_t *buff_ctx,
                                      const anj_io_out_entry_t *entry,
                                      void *out_buff,
                                      size_t out_buff_len,
                                      size_t *copied_bytes,
                                      size_t bytes_at_the_end_to_ignore) {
    size_t extended_offset =
            buff_ctx->offset - buff_ctx->bytes_in_internal_buff;
    size_t bytes_to_copy =
            ANJ_MIN(buff_ctx->remaining_bytes - bytes_at_the_end_to_ignore,
                    out_buff_len - *copied_bytes);
    memcpy(&((uint8_t *) out_buff)[*copied_bytes],
           &((const uint8_t *)
                     entry->value.bytes_or_string.data)[extended_offset],
           bytes_to_copy);
    *copied_bytes += bytes_to_copy;

    buff_ctx->remaining_bytes -= bytes_to_copy;
    buff_ctx->offset += bytes_to_copy;

    // no more records
    if (!buff_ctx->remaining_bytes) {
        _anj_io_reset_internal_buff(buff_ctx);
        return 0;
    }
    return ANJ_IO_NEED_NEXT_CALL;
}

#ifdef ANJ_WITH_EXTERNAL_DATA
static size_t start_cbor_byte_text_string(uint8_t *buff,
                                          anj_data_type_t type,
                                          size_t record_size) {
    return (type == ANJ_DATA_TYPE_EXTERNAL_BYTES)
                   ? anj_cbor_ll_bytes_begin(buff, record_size)
                   : anj_cbor_ll_string_begin(buff, record_size);
}

#    define CBOR_1BYTE_STRING_HEADER_BYTES_LIMIT 23U
#    define CBOR_MINIMAL_STRING_HEADER_SIZE 1U

static size_t add_break_reset_internal_buff(uint8_t *buff,
                                            _anj_io_buff_t *buff_ctx,
                                            size_t bytes_at_the_end_to_ignore) {
    size_t copied_bytes = anj_cbor_ll_indefinite_record_end(buff);
    if (bytes_at_the_end_to_ignore) {
        // for LwM2M CBOR last record we still have to add map endings
        buff_ctx->remaining_bytes = bytes_at_the_end_to_ignore;
    } else {
        _anj_io_reset_internal_buff(buff_ctx);
    }
    assert(copied_bytes == 1);
    return copied_bytes;
}

static inline uint8_t utf8_character_length(uint8_t character) {
    // https://en.wikipedia.org/wiki/UTF-8#Description
    // how long is the character: 110xxxxx, 1110xxxx, 11110xxx
    if ((character & 0xE0) == 0xC0) {
        return 2;
    } else if ((character & 0xF0) == 0xE0) {
        return 3;
    } else if ((character & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

static uint8_t utf8_truncated_tail_length(const uint8_t *buf, uint8_t len) {
    if (len == 0) {
        return 0;
    }
    uint8_t continuation_count = 0;
    uint8_t max_check = len < 4 ? len : 4;

    for (uint8_t i = 1; i <= max_check; ++i) {
        uint8_t byte = buf[len - i];
        // https://en.wikipedia.org/wiki/UTF-8#Description
        if ((byte & 0xC0) == 0x80) {
            // Continuation byte (10xxxxxx)
            continuation_count++;
        } else {
            uint8_t expected = utf8_character_length(byte);
            if (expected == 1) {
                return 0;
            }
            uint8_t actual = continuation_count + 1;
            // If actual byte count is less than expected, it's a truncated
            // character
            return (actual < expected) ? actual : 0;
        }
    }
    // Invalid UTF-8 sequence
    return 0;
}

void _anj_io_out_ctx_close_external_data_cb(const anj_io_out_entry_t *entry) {
    assert(entry && (entry->type & ANJ_DATA_TYPE_FLAG_EXTERNAL));
    if (entry->value.external_data.close_external_data) {
        entry->value.external_data.close_external_data(
                entry->value.external_data.user_args);
    }
}

#endif // ANJ_WITH_EXTERNAL_DATA

static int get_cbor_extended_data(_anj_io_buff_t *buff_ctx,
                                  const anj_io_out_entry_t *entry,
                                  void *out_buff,
                                  size_t out_buff_len,
                                  size_t *copied_bytes,
                                  size_t bytes_at_the_end_to_ignore) {
    if (bytes_at_the_end_to_ignore >= buff_ctx->remaining_bytes) {
        return 0;
    }
#ifdef ANJ_WITH_EXTERNAL_DATA
    if (entry->type == ANJ_DATA_TYPE_BYTES
            || entry->type == ANJ_DATA_TYPE_STRING)
#endif // ANJ_WITH_EXTERNAL_DATA
    {
        return get_cbor_bytes_string_data(buff_ctx, entry, out_buff,
                                          out_buff_len, copied_bytes,
                                          bytes_at_the_end_to_ignore);
    }
#ifdef ANJ_WITH_EXTERNAL_DATA
    while (1) {
        size_t extended_offset =
                buff_ctx->offset - buff_ctx->bytes_in_internal_buff;
        size_t buff_remaining_space = out_buff_len - *copied_bytes;

        // for worst case scenario (4 bytes UTF-8 character) we have to leave
        // space to encode 4 bytes + header + break
        if (buff_remaining_space < 6) {
            for (uint8_t i = 0; i < buff_remaining_space; i++) {
                *copied_bytes += start_cbor_byte_text_string(
                        &((uint8_t *) out_buff)[*copied_bytes], entry->type, 0);
            }
            // Don't increase offset and don't decrease remaining bytes
            // all start_cbor_byte_text_string() calls added additional bytes.
            return ANJ_IO_NEED_NEXT_CALL;
        }

        // first copy the buffered bytes from previous call
        uint8_t bytes_copied_from_utf8_buff = 0;
        if (buff_ctx->bytes_in_utf8_buff) {
            memcpy(&((uint8_t *) out_buff)[*copied_bytes
                                           + CBOR_MINIMAL_STRING_HEADER_SIZE],
                   buff_ctx->utf8_buff, buff_ctx->bytes_in_utf8_buff);
            *copied_bytes += buff_ctx->bytes_in_utf8_buff;
            buff_remaining_space -= buff_ctx->bytes_in_utf8_buff;
            bytes_copied_from_utf8_buff = buff_ctx->bytes_in_utf8_buff;
            buff_ctx->bytes_in_utf8_buff = 0;
        }

        // leave space for 1byte header and break character
        buff_remaining_space -= 2;
        size_t read_bytes = ANJ_MIN(buff_remaining_space,
                                    CBOR_1BYTE_STRING_HEADER_BYTES_LIMIT
                                            - bytes_copied_from_utf8_buff);
        int res = entry->value.external_data.get_external_data(
                &((uint8_t *) out_buff)[*copied_bytes
                                        // leave space for the header
                                        + CBOR_MINIMAL_STRING_HEADER_SIZE],
                &read_bytes, extended_offset,
                entry->value.external_data.user_args);

        if (res && res != ANJ_IO_NEED_NEXT_CALL) {
            return res;
        }

        if (entry->type == ANJ_DATA_TYPE_EXTERNAL_STRING) {
            // "...Note that this implies that the UTF-8 bytes of a single
            // Unicode code point (scalar value) cannot be spread between
            // chunks: a new chunk of a text string can only be started at a
            // code point boundary"
            buff_ctx->bytes_in_utf8_buff = utf8_truncated_tail_length(
                    &((uint8_t *) out_buff)[*copied_bytes
                                            + CBOR_MINIMAL_STRING_HEADER_SIZE],
                    (uint8_t) read_bytes);
            read_bytes -= buff_ctx->bytes_in_utf8_buff;
            if (buff_ctx->bytes_in_utf8_buff) {
                // there are still bytes to get from external source
                if (res != ANJ_IO_NEED_NEXT_CALL) {
                    return _ANJ_IO_ERR_LOGIC;
                }
                memcpy(buff_ctx->utf8_buff,
                       &((uint8_t *) out_buff)[*copied_bytes
                                               + CBOR_MINIMAL_STRING_HEADER_SIZE
                                               + read_bytes],
                       buff_ctx->bytes_in_utf8_buff);
            }
        }

        // encode header with real size
        *copied_bytes += start_cbor_byte_text_string(
                &((uint8_t *) out_buff)[*copied_bytes
                                        - bytes_copied_from_utf8_buff],
                entry->type, read_bytes + bytes_copied_from_utf8_buff);
        *copied_bytes += read_bytes;
        // update offset, we already read the bytes in bytes_in_utf8_buff so
        // count it too
        buff_ctx->offset += (read_bytes + buff_ctx->bytes_in_utf8_buff);
        if (res == ANJ_IO_NEED_NEXT_CALL) {
            if (*copied_bytes == out_buff_len) {
                return ANJ_IO_NEED_NEXT_CALL;
            }
        } else {
            *copied_bytes += add_break_reset_internal_buff(
                    &((uint8_t *) out_buff)[*copied_bytes], buff_ctx,
                    bytes_at_the_end_to_ignore);
            return 0;
        }
    }
#endif // ANJ_WITH_EXTERNAL_DATA
}

int _anj_io_out_ctx_init(_anj_io_out_ctx_t *ctx,
                         _anj_op_t operation_type,
                         const anj_uri_path_t *base_path,
                         size_t items_count,
                         uint16_t format) {
    assert(ctx);
    bool use_base_path = false;
#ifdef ANJ_WITH_SENML_CBOR
    bool encode_time = false;
#endif // ANJ_WITH_SENML_CBOR

    switch (operation_type) {
    case ANJ_OP_DM_READ:
    case ANJ_OP_INF_OBSERVE:
    case ANJ_OP_INF_CANCEL_OBSERVE:
        use_base_path = true;
        assert(base_path);
        break;
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    case ANJ_OP_DM_READ_COMP:
    case ANJ_OP_INF_OBSERVE_COMP:
    case ANJ_OP_INF_CANCEL_OBSERVE_COMP:
        break;
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
    case ANJ_OP_INF_INITIAL_NOTIFY:
    case ANJ_OP_INF_NON_CON_NOTIFY:
    case ANJ_OP_INF_CON_NOTIFY:
    case ANJ_OP_INF_CON_SEND:
    case ANJ_OP_INF_NON_CON_SEND:
#ifdef ANJ_WITH_SENML_CBOR
        encode_time = true;
#endif // ANJ_WITH_SENML_CBOR
        break;
    default:
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    int result = check_format(format, items_count, operation_type);
    if (result) {
        return result;
    }

    const anj_uri_path_t path =
            use_base_path ? *base_path : ANJ_MAKE_ROOT_PATH();

    memset(ctx, 0, sizeof(_anj_io_out_ctx_t));
    ctx->format = choose_format(format);

    if (items_count == 0) {
        ctx->empty = true;
        // choose_format() always returns LwM2M_CBOR or SenML_CBOR, so in
        // practical use, empty read is only possible for hierarchical formats,
        // read on resource that not exists will result in 4.04 response
        if (ctx->format == _ANJ_COAP_FORMAT_SENML_CBOR) {
            // empty CBOR array
            ctx->buff.internal_buff[0] = 0x80;
            ctx->buff.bytes_in_internal_buff = 1;
            ctx->buff.remaining_bytes = 1;
        } else if (ctx->format == _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR) {
            // indefinite-length map with no records
            ctx->buff.internal_buff[0] = 0xBF;
            ctx->buff.internal_buff[1] = 0xFF;
            ctx->buff.bytes_in_internal_buff = 2;
            ctx->buff.remaining_bytes = 2;
        }
        return 0;
    }

    switch (ctx->format) {
#ifdef ANJ_WITH_PLAINTEXT
    case _ANJ_COAP_FORMAT_PLAINTEXT:
        return _anj_text_encoder_init(ctx);
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_OPAQUE
    case _ANJ_COAP_FORMAT_OPAQUE_STREAM:
        return _anj_opaque_out_init(ctx);
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_CBOR
    case _ANJ_COAP_FORMAT_CBOR:
        return _anj_cbor_encoder_init(ctx);
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_SENML_CBOR
    case _ANJ_COAP_FORMAT_SENML_CBOR:
    case _ANJ_COAP_FORMAT_SENML_ETCH_CBOR:
        return _anj_senml_cbor_encoder_init(ctx, &path, items_count,
                                            encode_time);
#endif // ANJ_WITH_SENML_CBOR
#ifdef ANJ_WITH_LWM2M_CBOR
    case _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR:
        return _anj_lwm2m_cbor_encoder_init(ctx, &path, items_count);
#endif // ANJ_WITH_LWM2M_CBOR
    default:
        // not implemented yet
        return _ANJ_IO_ERR_UNSUPPORTED_FORMAT;
    }
}

int _anj_io_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                              const anj_io_out_entry_t *entry) {
    assert(ctx && entry);
    int res = _ANJ_IO_ERR_INPUT_ARG;

    if (ctx->empty) {
        return _ANJ_IO_ERR_LOGIC;
    }

    switch (ctx->format) {
#ifdef ANJ_WITH_PLAINTEXT
    case _ANJ_COAP_FORMAT_PLAINTEXT:
        res = _anj_text_out_ctx_new_entry(ctx, entry);
        break;
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_OPAQUE
    case _ANJ_COAP_FORMAT_OPAQUE_STREAM:
        res = _anj_opaque_out_ctx_new_entry(ctx, entry);
        break;
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_CBOR
    case _ANJ_COAP_FORMAT_CBOR:
        res = _anj_cbor_out_ctx_new_entry(ctx, entry);
        break;
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_SENML_CBOR
    case _ANJ_COAP_FORMAT_SENML_CBOR:
    case _ANJ_COAP_FORMAT_SENML_ETCH_CBOR:
        res = _anj_senml_cbor_out_ctx_new_entry(ctx, entry);
        break;
#endif // ANJ_WITH_SENML_CBOR
#ifdef ANJ_WITH_LWM2M_CBOR
    case _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR:
        res = _anj_lwm2m_cbor_out_ctx_new_entry(ctx, entry);
        break;
#endif // ANJ_WITH_LWM2M_CBOR
    default:
        break;
    }

#ifdef ANJ_WITH_EXTERNAL_DATA
    if (!res && (entry->type & ANJ_DATA_TYPE_FLAG_EXTERNAL)
            && entry->value.external_data.open_external_data) {
        res = entry->value.external_data.open_external_data(
                entry->value.external_data.user_args);
    }
#endif // ANJ_WITH_EXTERNAL_DATA

    if (!res) {
        ctx->entry = entry;
    }
    return res;
}

void _anj_io_get_payload_from_internal_buff(_anj_io_buff_t *buff_ctx,
                                            void *out_buff,
                                            size_t out_buff_len,
                                            size_t *copied_bytes) {
    if (buff_ctx->offset >= buff_ctx->bytes_in_internal_buff
            || !buff_ctx->bytes_in_internal_buff) {
        *copied_bytes = 0;
        return;
    }

    size_t bytes_to_copy =
            ANJ_MIN(buff_ctx->bytes_in_internal_buff - buff_ctx->offset,
                    out_buff_len);
    memcpy(out_buff, &(buff_ctx->internal_buff[buff_ctx->offset]),
           bytes_to_copy);
    buff_ctx->remaining_bytes -= bytes_to_copy;
    buff_ctx->offset += bytes_to_copy;
    *copied_bytes = bytes_to_copy;
}

static inline int io_out_ctx_get_payload(_anj_io_out_ctx_t *ctx,
                                         void *out_buff,
                                         size_t out_buff_len,
                                         size_t *out_copied_bytes) {
    _anj_io_buff_t *buff_ctx = &ctx->buff;

    /* Empty packets are illegal for all types apart from extended strings and
     * extended bytes in plain text format or opaque stream. Empty buffer is
     * also allowed for empty read. */
    if (!buff_ctx->remaining_bytes
            && !((ctx->format == _ANJ_COAP_FORMAT_PLAINTEXT
                  || ctx->format == _ANJ_COAP_FORMAT_OPAQUE_STREAM)
                 && buff_ctx->is_extended_type)
            && !ctx->empty) {
        return _ANJ_IO_ERR_LOGIC;
    }
    _anj_io_get_payload_from_internal_buff(&ctx->buff, out_buff, out_buff_len,
                                           out_copied_bytes);

    if (!buff_ctx->remaining_bytes && !buff_ctx->b64_cache.cache_offset) {
        _anj_io_reset_internal_buff(buff_ctx);
        return 0;
    } else if (!buff_ctx->is_extended_type
               || out_buff_len == *out_copied_bytes) {
        return ANJ_IO_NEED_NEXT_CALL;
    }

    switch (ctx->format) {
#ifdef ANJ_WITH_PLAINTEXT
    case _ANJ_COAP_FORMAT_PLAINTEXT:
        return _anj_text_get_extended_data_payload(
                out_buff, out_buff_len, out_copied_bytes, buff_ctx, ctx->entry);
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_OPAQUE
    case _ANJ_COAP_FORMAT_OPAQUE_STREAM:
        return _anj_opaque_get_extended_data_payload(
                out_buff, out_buff_len, out_copied_bytes, buff_ctx, ctx->entry);
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_CBOR
    case _ANJ_COAP_FORMAT_CBOR:
        return get_cbor_extended_data(&ctx->buff, ctx->entry, out_buff,
                                      out_buff_len, out_copied_bytes, 0);
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_SENML_CBOR
    case _ANJ_COAP_FORMAT_SENML_CBOR:
        return get_cbor_extended_data(&ctx->buff, ctx->entry, out_buff,
                                      out_buff_len, out_copied_bytes, 0);
#endif // ANJ_WITH_SENML_CBOR
#ifdef ANJ_WITH_LWM2M_CBOR
    case _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR: {
        _anj_lwm2m_cbor_encoder_t *lwm2m = &ctx->encoder.lwm2m;
        /**
         * For the last record, the additional data are also refers to the ends
         * of indefinite maps, get_cbor_extended_data will ignore last
         * lwm2m->maps_opened bytes, and they will be copied to out_buff in the
         * _anj_get_lwm2m_cbor_map_ends.
         */
        int ret_val = get_cbor_extended_data(
                &ctx->buff, ctx->entry, out_buff, out_buff_len,
                out_copied_bytes, lwm2m->items_count ? 0 : lwm2m->maps_opened);
        // ret_val == ANJ_IO_NEED_NEXT_CALL means that there are still bytes to
        // be copied from internal buffer but maybe
        // _anj_get_lwm2m_cbor_map_ends will copy more bytes
        if (ret_val && ret_val != ANJ_IO_NEED_NEXT_CALL) {
            return ret_val;
        }
        if (!lwm2m->items_count
                && ctx->buff.remaining_bytes <= lwm2m->maps_opened) {
            ret_val = _anj_get_lwm2m_cbor_map_ends(ctx, out_buff, out_buff_len,
                                                   out_copied_bytes);
        }
        if (!ctx->buff.remaining_bytes) {
            _anj_io_reset_internal_buff(buff_ctx);
        }
        return ret_val;
    }
#endif // ANJ_WITH_LWM2M_CBOR
    default:
        return _ANJ_IO_ERR_LOGIC;
    }
}

int _anj_io_out_ctx_get_payload(_anj_io_out_ctx_t *ctx,
                                void *out_buff,
                                size_t out_buff_len,
                                size_t *out_copied_bytes) {
    assert(ctx && out_buff && out_buff_len && out_copied_bytes);
    int ret = io_out_ctx_get_payload(ctx, out_buff, out_buff_len,
                                     out_copied_bytes);
#ifdef ANJ_WITH_EXTERNAL_DATA
    if (ctx->entry && ret != ANJ_IO_NEED_NEXT_CALL
            && (ctx->entry->type & ANJ_DATA_TYPE_FLAG_EXTERNAL)) {
        _anj_io_out_ctx_close_external_data_cb(ctx->entry);
    }
#endif // ANJ_WITH_EXTERNAL_DATA
    return ret;
}

uint16_t _anj_io_out_ctx_get_format(_anj_io_out_ctx_t *ctx) {
    return ctx->format;
}

size_t _anj_io_out_add_objlink(_anj_io_buff_t *buff_ctx,
                               size_t buf_pos,
                               anj_oid_t oid,
                               anj_iid_t iid) {
    char buffer[_ANJ_IO_CBOR_SIMPLE_RECORD_MAX_LENGTH] = { 0 };

    size_t str_size = anj_uint16_to_string_value(buffer, oid);
    buffer[str_size++] = ':';
    str_size += anj_uint16_to_string_value(&buffer[str_size], iid);

    size_t header_size =
            anj_cbor_ll_string_begin(&buff_ctx->internal_buff[buf_pos],
                                     str_size);
    memcpy(&((uint8_t *) buff_ctx->internal_buff)[buf_pos + header_size],
           buffer, str_size);
    return header_size + str_size;
}

int _anj_io_add_link_format_record(const anj_uri_path_t *uri_path,
                                   const char *version,
                                   const uint16_t *dim,
                                   bool first_record,
                                   _anj_io_buff_t *buff_ctx) {
    assert(buff_ctx->remaining_bytes == buff_ctx->bytes_in_internal_buff);
    size_t write_pointer = buff_ctx->bytes_in_internal_buff;

    if (!first_record) {
        buff_ctx->internal_buff[write_pointer++] = ',';
    }
    buff_ctx->internal_buff[write_pointer++] = '<';
    for (uint16_t i = 0; i < uri_path->uri_len; i++) {
        buff_ctx->internal_buff[write_pointer++] = '/';
        write_pointer += anj_uint16_to_string_value(
                (char *) &buff_ctx->internal_buff[write_pointer],
                uri_path->ids[i]);
    }
    buff_ctx->internal_buff[write_pointer++] = '>';

    if (dim) {
        if (!anj_uri_path_is(uri_path, ANJ_ID_RID)) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }
        memcpy(&buff_ctx->internal_buff[write_pointer], ";dim=", 5);
        write_pointer += 5;
        write_pointer += anj_uint16_to_string_value(
                (char *) &buff_ctx->internal_buff[write_pointer], *dim);
    }
    if (version) {
        if (_anj_validate_obj_version(version)) {
            return _ANJ_IO_ERR_INPUT_ARG;
        }
        memcpy(&buff_ctx->internal_buff[write_pointer], ";ver=", 5);
        write_pointer += 5;
        size_t ver_len = strlen(version);
        memcpy(&buff_ctx->internal_buff[write_pointer], version, ver_len);
        write_pointer += ver_len;
    }

    buff_ctx->bytes_in_internal_buff = write_pointer;
    buff_ctx->remaining_bytes = write_pointer;
    return 0;
}

int _anj_io_in_ctx_init(_anj_io_in_ctx_t *ctx,
                        _anj_op_t operation_type,
                        const anj_uri_path_t *base_path,
                        uint16_t format) {
    (void) operation_type;
    assert(ctx);
    memset(ctx, 0, sizeof(*ctx));
    ctx->format = format;
    switch (format) {
#ifdef ANJ_WITH_PLAINTEXT
    case _ANJ_COAP_FORMAT_PLAINTEXT:
        return _anj_text_decoder_init(ctx, base_path);
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_OPAQUE
    case _ANJ_COAP_FORMAT_OPAQUE_STREAM:
        return _anj_opaque_decoder_init(ctx, base_path);
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_CBOR
    case _ANJ_COAP_FORMAT_CBOR:
        return _anj_cbor_decoder_init(ctx, base_path);
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_SENML_CBOR
    case _ANJ_COAP_FORMAT_SENML_CBOR:
    case _ANJ_COAP_FORMAT_SENML_ETCH_CBOR:
        return _anj_senml_cbor_decoder_init(ctx, operation_type, base_path);
#endif // ANJ_WITH_SENML_CBOR
#ifdef ANJ_WITH_LWM2M_CBOR
    case _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR:
        return _anj_lwm2m_cbor_decoder_init(ctx, base_path);
#endif // ANJ_WITH_LWM2M_CBOR
#ifdef ANJ_WITH_TLV
    case _ANJ_COAP_FORMAT_OMA_LWM2M_TLV:
        return _anj_tlv_decoder_init(ctx, base_path);
#endif // ANJ_WITH_TLV
    default:
        return _ANJ_IO_ERR_UNSUPPORTED_FORMAT;
    }
}

int _anj_io_in_ctx_feed_payload(_anj_io_in_ctx_t *ctx,
                                void *buff,
                                size_t buff_size,
                                bool payload_finished) {
    assert(ctx);
    switch (ctx->format) {
#ifdef ANJ_WITH_PLAINTEXT
    case _ANJ_COAP_FORMAT_PLAINTEXT:
        return _anj_text_decoder_feed_payload(ctx, buff, buff_size,
                                              payload_finished);
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_OPAQUE
    case _ANJ_COAP_FORMAT_OPAQUE_STREAM:
        return _anj_opaque_decoder_feed_payload(ctx, buff, buff_size,
                                                payload_finished);
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_CBOR
    case _ANJ_COAP_FORMAT_CBOR:
        return _anj_cbor_decoder_feed_payload(ctx, buff, buff_size,
                                              payload_finished);
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_SENML_CBOR
    case _ANJ_COAP_FORMAT_SENML_CBOR:
    case _ANJ_COAP_FORMAT_SENML_ETCH_CBOR:
        return _anj_senml_cbor_decoder_feed_payload(ctx, buff, buff_size,
                                                    payload_finished);
#endif // ANJ_WITH_SENML_CBOR
#ifdef ANJ_WITH_LWM2M_CBOR
    case _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR:
        return _anj_lwm2m_cbor_decoder_feed_payload(ctx, buff, buff_size,
                                                    payload_finished);
#endif // ANJ_WITH_LWM2M_CBOR
#ifdef ANJ_WITH_TLV
    case _ANJ_COAP_FORMAT_OMA_LWM2M_TLV:
        return _anj_tlv_decoder_feed_payload(ctx, buff, buff_size,
                                             payload_finished);
#endif // ANJ_WITH_TLV
    default:
        return _ANJ_IO_ERR_LOGIC;
    }
}

int _anj_io_in_ctx_get_entry(_anj_io_in_ctx_t *ctx,
                             anj_data_type_t *inout_type_bitmask,
                             const anj_res_value_t **out_value,
                             const anj_uri_path_t **out_path) {
    assert(ctx);
    switch (ctx->format) {
#ifdef ANJ_WITH_PLAINTEXT
    case _ANJ_COAP_FORMAT_PLAINTEXT:
        return _anj_text_decoder_get_entry(ctx, inout_type_bitmask, out_value,
                                           out_path);
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_OPAQUE
    case _ANJ_COAP_FORMAT_OPAQUE_STREAM:
        return _anj_opaque_decoder_get_entry(ctx, inout_type_bitmask, out_value,
                                             out_path);
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_CBOR
    case _ANJ_COAP_FORMAT_CBOR:
        return _anj_cbor_decoder_get_entry(ctx, inout_type_bitmask, out_value,
                                           out_path);
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_SENML_CBOR
    case _ANJ_COAP_FORMAT_SENML_CBOR:
    case _ANJ_COAP_FORMAT_SENML_ETCH_CBOR:
        return _anj_senml_cbor_decoder_get_entry(ctx, inout_type_bitmask,
                                                 out_value, out_path);
#endif // ANJ_WITH_SENML_CBOR
#ifdef ANJ_WITH_LWM2M_CBOR
    case _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR:
        return _anj_lwm2m_cbor_decoder_get_entry(ctx, inout_type_bitmask,
                                                 out_value, out_path);
#endif // ANJ_WITH_LWM2M_CBOR
#ifdef ANJ_WITH_TLV
    case _ANJ_COAP_FORMAT_OMA_LWM2M_TLV:
        return _anj_tlv_decoder_get_entry(ctx, inout_type_bitmask, out_value,
                                          out_path);
#endif // ANJ_WITH_TLV
    default:
        return _ANJ_IO_ERR_LOGIC;
    }
}

int _anj_io_in_ctx_get_entry_count(_anj_io_in_ctx_t *ctx, size_t *out_count) {
    assert(ctx);
    assert(out_count);
    switch (ctx->format) {
#ifdef ANJ_WITH_PLAINTEXT
    case _ANJ_COAP_FORMAT_PLAINTEXT:
        return _anj_text_decoder_get_entry_count(ctx, out_count);
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_OPAQUE
    case _ANJ_COAP_FORMAT_OPAQUE_STREAM:
        return _anj_opaque_decoder_get_entry_count(ctx, out_count);
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_CBOR
    case _ANJ_COAP_FORMAT_CBOR:
        return _anj_cbor_decoder_get_entry_count(ctx, out_count);
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_SENML_CBOR
    case _ANJ_COAP_FORMAT_SENML_CBOR:
    case _ANJ_COAP_FORMAT_SENML_ETCH_CBOR:
        return _anj_senml_cbor_decoder_get_entry_count(ctx, out_count);
#endif // ANJ_WITH_SENML_CBOR
    default:
        return _ANJ_IO_ERR_FORMAT;
    }
}

int _anj_io_register_ctx_new_entry(_anj_io_register_ctx_t *ctx,
                                   const anj_uri_path_t *path,
                                   const char *version) {
    assert(ctx);
    if (ctx->buff.bytes_in_internal_buff) {
        return _ANJ_IO_ERR_LOGIC;
    }
    if (!(anj_uri_path_is(path, ANJ_ID_OID)
          || anj_uri_path_is(path, ANJ_ID_IID))
            || !anj_uri_path_increasing(&ctx->last_path, path)) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    if (_anj_uri_path_to_security_or_oscore_obj(path)) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    if (anj_uri_path_is(path, ANJ_ID_IID) && version) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }

    int res = _anj_io_add_link_format_record(
            path, version, NULL, !ctx->first_record_added, &ctx->buff);
    if (res) {
        return res;
    }

    ctx->last_path = *path;
    ctx->first_record_added = true;
    return 0;
}

int _anj_io_register_ctx_get_payload(_anj_io_register_ctx_t *ctx,
                                     void *out_buff,
                                     size_t out_buff_len,
                                     size_t *out_copied_bytes) {
    assert(ctx && out_buff && out_buff_len && out_copied_bytes);
    if (!ctx->buff.remaining_bytes) {
        return _ANJ_IO_ERR_LOGIC;
    }
    _anj_io_get_payload_from_internal_buff(&ctx->buff, out_buff, out_buff_len,
                                           out_copied_bytes);
    if (!ctx->buff.remaining_bytes) {
        ctx->buff.offset = 0;
        ctx->buff.bytes_in_internal_buff = 0;
    } else {
        return ANJ_IO_NEED_NEXT_CALL;
    }
    return 0;
}

void _anj_io_register_ctx_init(_anj_io_register_ctx_t *ctx) {
    assert(ctx);
    memset(ctx, 0, sizeof(_anj_io_register_ctx_t));
}
