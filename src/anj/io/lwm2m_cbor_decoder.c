/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 37

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include "cbor_decoder.h"
#include "cbor_decoder_ll.h"
#include "io.h"

#ifdef ANJ_WITH_LWM2M_CBOR

#    pragma GCC poison _cbor
#    pragma GCC poison _senml_cbor

typedef struct {
    uint16_t ids[ANJ_URI_PATH_MAX_LENGTH];
    uint8_t length;
} lwm2m_cbor_relative_path_t;

static int ensure_in_toplevel_map(_anj_lwm2m_cbor_decoder_t *ctx) {
    if (ctx->toplevel_map_entered) {
        return 0;
    }
    int result = anj_cbor_ll_decoder_enter_map(&ctx->ctx, NULL);
    if (!result) {
        ctx->toplevel_map_entered = true;
    }
    return result;
}

static int read_id(_anj_cbor_ll_decoder_t *ctx, uint16_t *out_id) {
    _anj_cbor_ll_number_t number;
    int result = anj_cbor_ll_decoder_number(ctx, &number);
    if (result) {
        return result;
    }
    if (number.type != ANJ_CBOR_LL_VALUE_UINT
            || number.value.u64 >= ANJ_ID_INVALID) {
        return _ANJ_IO_ERR_FORMAT;
    }

    *out_id = (uint16_t) number.value.u64;
    return 0;
}

static int path_add_id(_anj_lwm2m_cbor_path_stack_t *stack, uint16_t id) {
    if (stack->relative_paths_num
                    >= ANJ_ARRAY_SIZE(stack->relative_paths_lengths)
            || stack->path.uri_len >= ANJ_ARRAY_SIZE(stack->path.ids)) {
        return _ANJ_IO_ERR_FORMAT;
    }

    ++stack->relative_paths_lengths[stack->relative_paths_num];
    stack->path.ids[stack->path.uri_len++] = id;
    return 0;
}

static int read_and_add_path_id(_anj_lwm2m_cbor_decoder_t *ctx) {
    uint16_t id;
    int result = read_id(&ctx->ctx, &id);
    if (!result) {
        result = path_add_id(&ctx->path_stack, id);
    }
    return result;
}

static int path_commit(_anj_lwm2m_cbor_path_stack_t *stack) {
    // Empty relative path is invalid
    if (stack->relative_paths_lengths[stack->relative_paths_num] == 0) {
        return _ANJ_IO_ERR_FORMAT;
    }
    ++stack->relative_paths_num;
    return 0;
}

static void path_pop(_anj_lwm2m_cbor_path_stack_t *stack) {
    assert(stack->relative_paths_num > 0);

    uint8_t last_relative_path_length =
            stack->relative_paths_lengths[stack->relative_paths_num - 1];
    stack->relative_paths_lengths[stack->relative_paths_num - 1] = 0;
    while (last_relative_path_length--) {
        stack->path.ids[--stack->path.uri_len] = ANJ_ID_INVALID;
    }

    stack->relative_paths_num--;
}

static inline size_t
expected_nesting_level(_anj_lwm2m_cbor_path_stack_t *stack) {
    return (size_t) stack->relative_paths_num + 1;
}

static int
decode_path_fragment_and_update_stack(_anj_lwm2m_cbor_decoder_t *ctx) {
    int result;
    _anj_cbor_ll_value_type_t type;
    if (ctx->in_path_array) {
        type = ANJ_CBOR_LL_VALUE_ARRAY;
    } else {
        size_t nesting_level;
        if ((result = anj_cbor_ll_decoder_nesting_level(&ctx->ctx,
                                                        &nesting_level))) {
            return result;
        }
        if (!nesting_level) {
            if (!(result = anj_cbor_ll_decoder_errno(&ctx->ctx))) {
                // If nesting level is 0, we've exited the outermost map.
                // No more values are expected, so if we end up here,
                // it's an error.
                result = _ANJ_IO_ERR_FORMAT;
            }
            return result;
        }
        if (nesting_level > expected_nesting_level(&ctx->path_stack)) {
            return _ANJ_IO_ERR_FORMAT;
        }
        while (nesting_level < expected_nesting_level(&ctx->path_stack)) {
            path_pop(&ctx->path_stack);
        }
        if ((result = anj_cbor_ll_decoder_current_value_type(&ctx->ctx,
                                                             &type))) {
            return result;
        }
    }

    if (type == ANJ_CBOR_LL_VALUE_ARRAY) {
        if (!ctx->in_path_array) {
            if ((result = anj_cbor_ll_decoder_enter_array(&ctx->ctx, NULL))) {
                return result;
            }
            ctx->in_path_array = true;
        }

        while (ctx->in_path_array) {
            size_t nesting_level;
            if ((result = anj_cbor_ll_decoder_nesting_level(&ctx->ctx,
                                                            &nesting_level))) {
                return result;
            }
            // we're in the path array, i.e. one level deeper down
            if (nesting_level != expected_nesting_level(&ctx->path_stack) + 1) {
                ctx->in_path_array = false;
            } else if ((result = read_and_add_path_id(ctx))) {
                return result;
            }
        }
    } else if (type == ANJ_CBOR_LL_VALUE_UINT) {
        if ((result = read_and_add_path_id(ctx))) {
            return result;
        }
    } else {
        return _ANJ_IO_ERR_FORMAT;
    }

    return path_commit(&ctx->path_stack);
}

int _anj_lwm2m_cbor_decoder_init(_anj_io_in_ctx_t *ctx,
                                 const anj_uri_path_t *base_path) {
    anj_cbor_ll_decoder_init(&ctx->decoder.lwm2m_cbor.ctx);
    ctx->decoder.lwm2m_cbor.base = *base_path;
    ctx->decoder.lwm2m_cbor.path_stack.path = ANJ_MAKE_ROOT_PATH();
    return 0;
}

int _anj_lwm2m_cbor_decoder_feed_payload(_anj_io_in_ctx_t *ctx,
                                         void *buff,
                                         size_t buff_size,
                                         bool payload_finished) {
    return anj_cbor_ll_decoder_feed_payload(&ctx->decoder.lwm2m_cbor.ctx, buff,
                                            buff_size, payload_finished);
}

int _anj_lwm2m_cbor_decoder_get_entry(_anj_io_in_ctx_t *ctx,
                                      anj_data_type_t *inout_type_bitmask,
                                      const anj_res_value_t **out_value,
                                      const anj_uri_path_t **out_path) {
    _anj_lwm2m_cbor_decoder_t *lwm2m_cbor = &ctx->decoder.lwm2m_cbor;
    *out_value = NULL;
    *out_path = NULL;
    int result;
    if ((result = ensure_in_toplevel_map(lwm2m_cbor))) {
        return result;
    }
    _anj_cbor_ll_value_type_t type;
    while (true) {
        if (!lwm2m_cbor->path_parsed) {
            if ((result = decode_path_fragment_and_update_stack(lwm2m_cbor))) {
                return result;
            }
            lwm2m_cbor->path_parsed = true;
        }

        if (!lwm2m_cbor->expects_map) {
            if ((result = anj_cbor_ll_decoder_current_value_type(
                         &lwm2m_cbor->ctx, &type))) {
                return result;
            }
            if (type == ANJ_CBOR_LL_VALUE_MAP) {
                lwm2m_cbor->expects_map = true;
            }
        }

        if (!lwm2m_cbor->expects_map) {
            // Value expected
            break;
        }

        if ((result = anj_cbor_ll_decoder_enter_map(&lwm2m_cbor->ctx, NULL))) {
            return result;
        }

        lwm2m_cbor->path_parsed = false;
        lwm2m_cbor->expects_map = false;
    }
    if (anj_uri_path_outside_base(&lwm2m_cbor->path_stack.path,
                                  &lwm2m_cbor->base)) {
        return _ANJ_IO_ERR_FORMAT;
    }
    *out_path = &lwm2m_cbor->path_stack.path;
    if (type == ANJ_CBOR_LL_VALUE_NULL) {
        *inout_type_bitmask = ANJ_DATA_TYPE_NULL;
        if ((result = anj_cbor_ll_decoder_null(&lwm2m_cbor->ctx))) {
            return result;
        }
    } else {
        if ((result = _anj_cbor_extract_value(
                     &lwm2m_cbor->ctx, &lwm2m_cbor->bytes_ctx,
                     &lwm2m_cbor->bytes_consumed, &lwm2m_cbor->objlnk_buf,
                     inout_type_bitmask, &ctx->out_value))) {
            return result;
        }
        *out_value = &ctx->out_value;
    }
    if (!lwm2m_cbor->bytes_ctx) {
        lwm2m_cbor->path_parsed = false;
    }
    return 0;
}

#endif // ANJ_WITH_LWM2M_CBOR
