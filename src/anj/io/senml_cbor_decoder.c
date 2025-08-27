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
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include "cbor_decoder.h"
#include "cbor_decoder_ll.h"
#include "internal.h"
#include "io.h"

#ifdef ANJ_WITH_SENML_CBOR

static int ensure_in_toplevel_array(_anj_io_in_ctx_t *ctx) {
    if (ctx->decoder.senml_cbor.toplevel_array_entered) {
        return 0;
    }
    int result = anj_cbor_ll_decoder_enter_array(
            &ctx->decoder.senml_cbor.ctx, &ctx->decoder.senml_cbor.entry_count);
    if (!result) {
        ctx->decoder.senml_cbor.toplevel_array_entered = true;
    }
    return result;
}

static int get_i64(_anj_io_in_ctx_t *ctx, int64_t *out_value) {
    _anj_cbor_ll_number_t value;
    int result =
            anj_cbor_ll_decoder_number(&ctx->decoder.senml_cbor.ctx, &value);
    if (result) {
        return result;
    }
    return _anj_cbor_get_i64_from_ll_number(&value, out_value, false);
}

static int
get_short_string(_anj_io_in_ctx_t *ctx, char *out_string, size_t size) {
    return _anj_cbor_get_short_string(
            &ctx->decoder.senml_cbor.ctx,
            &ctx->decoder.senml_cbor.entry_parse.bytes_ctx,
            &ctx->decoder.senml_cbor.entry_parse.bytes_consumed, out_string,
            size);
}

static int get_senml_cbor_label(_anj_io_in_ctx_t *ctx) {
    _anj_senml_entry_parse_state_t *const state =
            &ctx->decoder.senml_cbor.entry_parse;
    _anj_cbor_ll_value_type_t type;
    int result =
            anj_cbor_ll_decoder_current_value_type(&ctx->decoder.senml_cbor.ctx,
                                                   &type);
    if (result) {
        return result;
    }
    /**
     * SenML numerical labels do not contain anything related to LwM2M objlnk
     * datatype. Additionally:
     *
     * > 6.  CBOR Representation (application/senml+cbor)
     * > [...]
     * >
     * > For compactness, the CBOR representation uses integers for the
     * > labels, as defined in Table 4.  This table is conclusive, i.e.,
     * > there is no intention to define any additional integer map keys;
     * > any extensions will use **string** map keys.
     */
    if (type == ANJ_CBOR_LL_VALUE_TEXT_STRING) {
        if ((result = get_short_string(ctx, state->short_string_buf,
                                       sizeof(state->short_string_buf)))) {
            return result;
        }
        if (strcmp(state->short_string_buf, SENML_EXT_OBJLNK_REPR)) {
            return _ANJ_IO_ERR_FORMAT;
        }
        state->label = SENML_EXT_LABEL_OBJLNK;
        return 0;
    }
    int64_t numeric_label;
    if ((result = get_i64(ctx, &numeric_label))) {
        return result;
    }
    switch (numeric_label) {
    case SENML_LABEL_BASE_TIME:
    case SENML_LABEL_BASE_NAME:
    case SENML_LABEL_NAME:
    case SENML_LABEL_VALUE:
    case SENML_LABEL_VALUE_STRING:
    case SENML_LABEL_VALUE_BOOL:
    case SENML_LABEL_TIME:
    case SENML_LABEL_VALUE_OPAQUE:
        state->label = (senml_label_t) numeric_label;
        return 0;
    default:
        return _ANJ_IO_ERR_FORMAT;
    }
}

static int parse_id(uint16_t *out_id, const char **id_begin) {
    const char *id_end = *id_begin;
    while (isdigit(*id_end)) {
        ++id_end;
    }
    uint32_t value;
    int result = anj_string_to_uint32_value(&value, *id_begin,
                                            (size_t) (id_end - *id_begin));
    if (result) {
        return result;
    }
    if (value >= ANJ_ID_INVALID) {
        return -1;
    }
    *out_id = (uint16_t) value;
    *id_begin = id_end;
    return 0;
}

static int parse_absolute_path(anj_uri_path_t *out_path, const char *input) {
    if (!*input) {
        return _ANJ_IO_ERR_FORMAT;
    }
    *out_path = ANJ_MAKE_ROOT_PATH();

    if (!strcmp(input, "/")) {
        return 0;
    }
    for (const char *ch = input; *ch;) {
        if (*ch++ != '/') {
            return _ANJ_IO_ERR_FORMAT;
        }
        if (out_path->uri_len >= ANJ_ARRAY_SIZE(out_path->ids)) {
            return _ANJ_IO_ERR_FORMAT;
        }
        if (parse_id(&out_path->ids[out_path->uri_len], &ch)) {
            return _ANJ_IO_ERR_FORMAT;
        }
        out_path->uri_len++;
    }
    return 0;
}

static int parse_next_absolute_path(_anj_io_in_ctx_t *ctx) {
    _anj_senml_cbor_decoder_t *const senml = &ctx->decoder.senml_cbor;
    char full_path[_ANJ_IO_MAX_PATH_STRING_SIZE];
    size_t len1 = strlen(senml->basename);
    size_t len2 = strlen(senml->entry.path);
    if (len1 + len2 >= sizeof(full_path)) {
        return _ANJ_IO_ERR_FORMAT;
    }
    memcpy(full_path, senml->basename, len1);
    memcpy(full_path + len1, senml->entry.path, len2 + 1);
    if (parse_absolute_path(&ctx->out_path, full_path)
            || anj_uri_path_outside_base(&ctx->out_path, &senml->base)
            || (
#    ifdef ANJ_WITH_COMPOSITE_OPERATIONS
                       !senml->composite_read_observe &&
#    endif // ANJ_WITH_COMPOSITE_OPERATIONS
                       !anj_uri_path_has(&ctx->out_path, ANJ_ID_RID))) {
        return _ANJ_IO_ERR_FORMAT;
    }
    return 0;
}

static int parse_senml_name(_anj_io_in_ctx_t *ctx) {
    _anj_senml_cbor_decoder_t *const senml = &ctx->decoder.senml_cbor;
    if (senml->entry_parse.has_name) {
        return _ANJ_IO_ERR_FORMAT;
    }

    _anj_cbor_ll_value_type_t type;
    int result = anj_cbor_ll_decoder_current_value_type(&senml->ctx, &type);
    if (result) {
        return result;
    }
    if (type != ANJ_CBOR_LL_VALUE_TEXT_STRING) {
        return _ANJ_IO_ERR_FORMAT;
    }

    if (!(result = get_short_string(ctx, senml->entry.path,
                                    sizeof(senml->entry.path)))) {
        senml->entry_parse.has_name = true;
    }
    return result;
}

static int process_bytes_value(_anj_io_in_ctx_t *ctx) {
    _anj_senml_entry_parse_state_t *const state =
            &ctx->decoder.senml_cbor.entry_parse;
    anj_bytes_or_string_value_t *const value =
            &ctx->decoder.senml_cbor.entry.value.bytes;
    int result;
    if (!state->bytes_ctx) {
        assert(value->offset == 0);
        assert(value->chunk_length == 0);
        assert(value->full_length_hint == 0);
        ptrdiff_t total_size = 0;
        if ((result = anj_cbor_ll_decoder_bytes(&ctx->decoder.senml_cbor.ctx,
                                                &state->bytes_ctx,
                                                &total_size))) {
            return result;
        }
        if (total_size >= 0) {
            value->full_length_hint = (size_t) total_size;
        }
    }
    value->offset += value->chunk_length;
    value->chunk_length = 0;
    bool message_finished;
    result = anj_cbor_ll_decoder_bytes_get_some(state->bytes_ctx, &value->data,
                                                &value->chunk_length,
                                                &message_finished);
    if (!result && message_finished) {
        state->bytes_ctx = NULL;
        value->full_length_hint = value->offset + value->chunk_length;
        state->has_value = true;
    }
    return result;
}

static int parse_senml_value(_anj_io_in_ctx_t *ctx) {
    _anj_senml_entry_parse_state_t *const state =
            &ctx->decoder.senml_cbor.entry_parse;
    anj_senml_cached_entry_t *const entry = &ctx->decoder.senml_cbor.entry;
    if (state->has_value) {
        return _ANJ_IO_ERR_FORMAT;
    }

    _anj_cbor_ll_value_type_t type;
    int result =
            anj_cbor_ll_decoder_current_value_type(&ctx->decoder.senml_cbor.ctx,
                                                   &type);
    if (result) {
        return result;
    }
    switch (type) {
    case ANJ_CBOR_LL_VALUE_NULL:
        if (state->label != SENML_LABEL_VALUE) {
            return _ANJ_IO_ERR_FORMAT;
        }
        entry->type = ANJ_DATA_TYPE_NULL;
        if ((result = anj_cbor_ll_decoder_null(&ctx->decoder.senml_cbor.ctx))) {
            return result;
        }
        state->has_value = true;
        return 0;
    case ANJ_CBOR_LL_VALUE_BYTE_STRING:
        if (state->label != SENML_LABEL_VALUE_OPAQUE) {
            return _ANJ_IO_ERR_FORMAT;
        }
        entry->type = ANJ_DATA_TYPE_BYTES;
        return process_bytes_value(ctx);
    case ANJ_CBOR_LL_VALUE_TEXT_STRING:
        switch (state->label) {
        case SENML_LABEL_VALUE_STRING:
            entry->type = ANJ_DATA_TYPE_STRING;
            return process_bytes_value(ctx);
        case SENML_EXT_LABEL_OBJLNK:
            entry->type = ANJ_DATA_TYPE_OBJLNK;
            if ((result = get_short_string(ctx, state->short_string_buf,
                                           sizeof(state->short_string_buf)))) {
                return result;
            }
            if (anj_string_to_objlnk_value(&entry->value.objlnk,
                                           state->short_string_buf)) {
                return _ANJ_IO_ERR_FORMAT;
            }
            state->has_value = true;
            return 0;
        default:
            return _ANJ_IO_ERR_FORMAT;
        }
    case ANJ_CBOR_LL_VALUE_BOOL:
        if (state->label != SENML_LABEL_VALUE_BOOL) {
            return _ANJ_IO_ERR_FORMAT;
        }
        entry->type = ANJ_DATA_TYPE_BOOL;
        if ((result = anj_cbor_ll_decoder_bool(&ctx->decoder.senml_cbor.ctx,
                                               &entry->value.boolean))) {
            return result;
        }
        state->has_value = true;
        return 0;
    default:
        if (state->label != SENML_LABEL_VALUE) {
            return _ANJ_IO_ERR_FORMAT;
        }
        if (type == ANJ_CBOR_LL_VALUE_TIMESTAMP) {
            entry->type = ANJ_DATA_TYPE_TIME;
        } else {
            entry->type = ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE
                          | ANJ_DATA_TYPE_UINT;
        }
        if ((result = anj_cbor_ll_decoder_number(&ctx->decoder.senml_cbor.ctx,
                                                 &entry->value.number))) {
            return result;
        }
        state->has_value = true;
        return 0;
    }
}

static int parse_senml_basename(_anj_io_in_ctx_t *ctx) {
    _anj_senml_cbor_decoder_t *const senml = &ctx->decoder.senml_cbor;
    if (senml->entry_parse.has_basename) {
        return _ANJ_IO_ERR_FORMAT;
    }

    _anj_cbor_ll_value_type_t type;
    int result = anj_cbor_ll_decoder_current_value_type(&senml->ctx, &type);
    if (result) {
        return result;
    }
    if (type != ANJ_CBOR_LL_VALUE_TEXT_STRING) {
        return _ANJ_IO_ERR_FORMAT;
    }

    if (!(result = get_short_string(ctx, senml->basename,
                                    sizeof(senml->basename)))) {
        senml->entry_parse.has_basename = true;
    }
    return result;
}

int _anj_senml_cbor_decoder_init(_anj_io_in_ctx_t *ctx,
                                 _anj_op_t operation_type,
                                 const anj_uri_path_t *base_path) {
    anj_cbor_ll_decoder_init(&ctx->decoder.senml_cbor.ctx);
#    ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    ctx->decoder.senml_cbor.base =
            (operation_type == ANJ_OP_DM_READ_COMP
             || operation_type == ANJ_OP_INF_OBSERVE_COMP)
                    ? (anj_uri_path_t) { 0 }
                    : *base_path;
    ctx->decoder.senml_cbor.composite_read_observe =
            (operation_type == ANJ_OP_DM_READ_COMP
             || operation_type == ANJ_OP_INF_OBSERVE_COMP);
#    else  // ANJ_WITH_COMPOSITE_OPERATIONS
    (void) operation_type;
    ctx->decoder.senml_cbor.base = *base_path;
#    endif // ANJ_WITH_COMPOSITE_OPERATIONS

    return 0;
}

int _anj_senml_cbor_decoder_feed_payload(_anj_io_in_ctx_t *ctx,
                                         void *buff,
                                         size_t buff_size,
                                         bool payload_finished) {
    return anj_cbor_ll_decoder_feed_payload(&ctx->decoder.senml_cbor.ctx, buff,
                                            buff_size, payload_finished);
}

static bool entry_has_pairs_remaining(_anj_io_in_ctx_t *ctx, int *out_error) {
    assert(!*out_error);
    if (ctx->decoder.senml_cbor.entry_parse.pairs_remaining == 0) {
        return false;
    }
    if (ctx->decoder.senml_cbor.entry_parse.pairs_remaining > 0) {
        return true;
    }
    // ctx->decoder.senml_cbor.entry_parse.pairs_remaining < 0
    // i.e. indefinite map
    size_t current_level;
    int result = anj_cbor_ll_decoder_nesting_level(&ctx->decoder.senml_cbor.ctx,
                                                   &current_level);
    if (result) {
        *out_error = result;
        return false;
    }
    if (current_level > 1) {
        return true;
    }
    ctx->decoder.senml_cbor.entry_parse.pairs_remaining = 0;
    return false;
}

int _anj_senml_cbor_decoder_get_entry(_anj_io_in_ctx_t *ctx,
                                      anj_data_type_t *inout_type_bitmask,
                                      const anj_res_value_t **out_value,
                                      const anj_uri_path_t **out_path) {
    _anj_senml_entry_parse_state_t *const state =
            &ctx->decoder.senml_cbor.entry_parse;
    anj_senml_cached_entry_t *const entry = &ctx->decoder.senml_cbor.entry;
    *out_value = NULL;
    *out_path = NULL;
    int result;
    if ((result = ensure_in_toplevel_array(ctx))) {
        return result;
    }
    if (!state->map_entered) {
        size_t nesting_level;
        if ((result = anj_cbor_ll_decoder_errno(&ctx->decoder.senml_cbor.ctx))
                || (result = anj_cbor_ll_decoder_nesting_level(
                            &ctx->decoder.senml_cbor.ctx, &nesting_level))) {
            return result;
        }
        if (nesting_level != 1) {
            return _ANJ_IO_ERR_FORMAT;
        }
        if ((result = anj_cbor_ll_decoder_enter_map(
                     &ctx->decoder.senml_cbor.ctx, &state->pairs_remaining))) {
            return result;
        }
        state->map_entered = true;
        memset(entry, 0, sizeof(*entry));
    }
    result = 0;
    while (!result && entry_has_pairs_remaining(ctx, &result)) {
        if (!state->label_ready) {
            if ((result = get_senml_cbor_label(ctx))) {
                return result;
            }
            state->label_ready = true;
        }
        switch (state->label) {
        case SENML_LABEL_NAME:
            result = parse_senml_name(ctx);
            break;
        case SENML_LABEL_VALUE:
        case SENML_LABEL_VALUE_BOOL:
        case SENML_LABEL_VALUE_OPAQUE:
        case SENML_LABEL_VALUE_STRING:
        case SENML_EXT_LABEL_OBJLNK:
#    ifdef ANJ_WITH_COMPOSITE_OPERATIONS
            if (ctx->decoder.senml_cbor.composite_read_observe) {
                result = _ANJ_IO_ERR_FORMAT;
            } else
#    endif // ANJ_WITH_COMPOSITE_OPERATIONS
            {
                result = parse_senml_value(ctx);
            }
            break;
        case SENML_LABEL_BASE_NAME:
            result = parse_senml_basename(ctx);
            break;
        default:
            result = _ANJ_IO_ERR_FORMAT;
        }
        if (!result) {
            if (state->bytes_ctx) {
                // We only have a partial byte or text string
                // Don't advance as we need to pass all the chunks to the user
                assert(entry->type
                       & (ANJ_DATA_TYPE_BYTES | ANJ_DATA_TYPE_STRING));
                assert(entry->value.bytes.offset
                               + entry->value.bytes.chunk_length
                       != entry->value.bytes.full_length_hint);
                break;
            }
            if (state->pairs_remaining >= 0) {
                --state->pairs_remaining;
            }
            state->label_ready = false;
        }
    }
    if (entry->type & (ANJ_DATA_TYPE_BYTES | ANJ_DATA_TYPE_STRING)) {
        // Bytes or String
        if (result
                || (result = anj_cbor_ll_decoder_errno(
                            &ctx->decoder.senml_cbor.ctx))
                               < 0) {
            return result;
        }
        if (!state->path_processed
                && ((state->has_basename && state->has_name)
                    || !state->pairs_remaining
                    || (state->bytes_ctx && state->pairs_remaining == 1))) {
            int parse_path_result = parse_next_absolute_path(ctx);
            if (parse_path_result) {
                return parse_path_result;
            }
            state->path_processed = true;
        }
        if (state->path_processed) {
            *out_path = &ctx->out_path;
        }
        switch ((*inout_type_bitmask &= entry->type)) {
        default:
            ANJ_UNREACHABLE("Bytes and String types are explicitly marked and "
                            "shall not require disambiguation");
            return _ANJ_IO_WANT_TYPE_DISAMBIGUATION;
        case ANJ_DATA_TYPE_NULL:
            return _ANJ_IO_ERR_FORMAT;
        case ANJ_DATA_TYPE_BYTES:
        case ANJ_DATA_TYPE_STRING:;
        }
        ctx->out_value.bytes_or_string = entry->value.bytes;
        *out_value = &ctx->out_value;
        if (state->path_processed
                && entry->value.bytes.offset + entry->value.bytes.chunk_length
                               == entry->value.bytes.full_length_hint) {
            memset(state, 0, sizeof(*state));
        }
        assert(!result || result == _ANJ_IO_EOF
               || result == _ANJ_IO_WANT_NEXT_PAYLOAD);
        // Note: In case of _ANJ_IO_EOF, it will be delivered next time.
        // In case of _ANJ_IO_WANT_NEXT_PAYLOAD, it will also be delivered next
        // time, through either the anj_cbor_ll_decoder_errno() near the top of
        // this function, or one of the get/parse functions in the while loop
        // above.
        return 0;
    } else {
        // simple data types
        if (result
                || (result = anj_cbor_ll_decoder_errno(
                            &ctx->decoder.senml_cbor.ctx))
                               < 0) {
            return result;
        }
        if (!state->path_processed) {
            int parse_path_result = parse_next_absolute_path(ctx);
            if (parse_path_result) {
                return parse_path_result;
            }
            state->path_processed = true;
        }
        *out_path = &ctx->out_path;
        switch ((*inout_type_bitmask &= entry->type)) {
        case ANJ_DATA_TYPE_NULL:
            if (entry->type == ANJ_DATA_TYPE_NULL) {
                *out_value = NULL;
                memset(state, 0, sizeof(*state));
                return 0;
            }
            return _ANJ_IO_ERR_FORMAT;
        case ANJ_DATA_TYPE_INT:
            if ((result = _anj_cbor_get_i64_from_ll_number(
                         &entry->value.number, &ctx->out_value.int_value,
                         false))) {
                return result;
            }
            break;
        case ANJ_DATA_TYPE_DOUBLE:
            if ((result = _anj_cbor_get_double_from_ll_number(
                         &entry->value.number, &ctx->out_value.double_value))) {
                return result;
            }
            break;
        case ANJ_DATA_TYPE_BOOL:
            ctx->out_value.bool_value = entry->value.boolean;
            break;
        case ANJ_DATA_TYPE_OBJLNK:
            ctx->out_value.objlnk = entry->value.objlnk;
            break;
        case ANJ_DATA_TYPE_UINT:
            if ((result = _anj_cbor_get_u64_from_ll_number(
                         &entry->value.number, &ctx->out_value.uint_value))) {
                return result;
            }
            break;
        case ANJ_DATA_TYPE_TIME:
            if ((result = _anj_cbor_get_i64_from_ll_number(
                         &entry->value.number, &ctx->out_value.time_value,
                         true))) {
                return result;
            }
            break;
        default:
            return _ANJ_IO_WANT_TYPE_DISAMBIGUATION;
        }
        *out_value = &ctx->out_value;
        memset(state, 0, sizeof(*state));
        return 0;
    }
}

int _anj_senml_cbor_decoder_get_entry_count(_anj_io_in_ctx_t *ctx,
                                            size_t *out_count) {
    int result = ensure_in_toplevel_array(ctx);
    if (result) {
        return result < 0 ? result : _ANJ_IO_ERR_LOGIC;
    }
    if (ctx->decoder.senml_cbor.entry_count < 0) {
        return _ANJ_IO_ERR_FORMAT;
    }
    *out_count = (size_t) ctx->decoder.senml_cbor.entry_count;
    return 0;
}

#endif // ANJ_WITH_SENML_CBOR
