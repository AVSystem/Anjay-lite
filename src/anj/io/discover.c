/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 35

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include "internal.h"
#include "io.h"

#ifdef ANJ_WITH_BOOTSTRAP_DISCOVER

int _anj_io_bootstrap_discover_ctx_init(_anj_io_bootstrap_discover_ctx_t *ctx,
                                        const anj_uri_path_t *base_path) {
    assert(ctx && base_path);
    if (anj_uri_path_has(base_path, ANJ_ID_IID)) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    memset(ctx, 0, sizeof(_anj_io_bootstrap_discover_ctx_t));
    ctx->base_path = *base_path;

    return 0;
}

int _anj_io_bootstrap_discover_ctx_new_entry(
        _anj_io_bootstrap_discover_ctx_t *ctx,
        const anj_uri_path_t *path,
        const char *version,
        const uint16_t *ssid,
        const char *uri) {
    assert(ctx);

    if (ctx->buff.bytes_in_internal_buff) {
        return _ANJ_IO_ERR_LOGIC;
    }
    if (!(anj_uri_path_is(path, ANJ_ID_OID)
          || anj_uri_path_is(path, ANJ_ID_IID))
            || anj_uri_path_outside_base(path, &ctx->base_path)
            || !anj_uri_path_increasing(&ctx->last_path, path)) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    if (ssid && path->ids[ANJ_ID_OID] != ANJ_OBJ_ID_SECURITY
            && path->ids[ANJ_ID_OID] != ANJ_OBJ_ID_SERVER
            && path->ids[ANJ_ID_OID] != ANJ_OBJ_ID_OSCORE) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    // SSID is mandatory for Server object instances, but not for Objects
    // themselves
    if (!ssid && path->ids[ANJ_ID_OID] == ANJ_OBJ_ID_SERVER
            && anj_uri_path_is(path, ANJ_ID_IID)) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    if (uri && path->ids[ANJ_ID_OID] != ANJ_OBJ_ID_SECURITY) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    if (anj_uri_path_is(path, ANJ_ID_OID) && (uri || ssid)) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    if (anj_uri_path_is(path, ANJ_ID_IID) && version) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }

    if (!ctx->first_record_added) {
#    ifdef ANJ_WITH_LWM2M12
        const char *payload_begin = "</>;lwm2m=1.2";
#    else  // ANJ_WITH_LWM2M12
        const char *payload_begin = "</>;lwm2m=1.1";
#    endif // ANJ_WITH_LWM2M12

        size_t record_len = strlen(payload_begin);
        memcpy(ctx->buff.internal_buff, payload_begin, record_len);
        ctx->buff.bytes_in_internal_buff = record_len;
        ctx->buff.remaining_bytes = record_len;
    }

    int res = _anj_io_add_link_format_record(path, version, NULL, false,
                                             &ctx->buff);
    if (res) {
        return res;
    }

    if (ssid) {
        size_t write_pointer = ctx->buff.bytes_in_internal_buff;
        memcpy(&ctx->buff.internal_buff[write_pointer], ";ssid=", 6);
        write_pointer += 6;
        char ssid_str[ANJ_U16_STR_MAX_LEN];
        size_t ssid_str_len = anj_uint16_to_string_value(ssid_str, *ssid);
        memcpy(&ctx->buff.internal_buff[write_pointer], ssid_str, ssid_str_len);
        write_pointer += ssid_str_len;
        ctx->buff.bytes_in_internal_buff = write_pointer;
        ctx->buff.remaining_bytes = write_pointer;
    }
    if (uri) {
        size_t write_pointer = ctx->buff.bytes_in_internal_buff;
        memcpy(&ctx->buff.internal_buff[write_pointer], ";uri=\"", 6);
        write_pointer += 6;
        ctx->buff.bytes_in_internal_buff = write_pointer;
        ctx->buff.remaining_bytes = write_pointer;
        ctx->buff.is_extended_type = true;
        // + 1 to add \" on the end
        ctx->buff.remaining_bytes += strlen(uri) + 1;
        ctx->uri = uri;
    }

    ctx->last_path = *path;
    ctx->first_record_added = true;
    return 0;
}

static int add_bootstrap_uri(_anj_io_buff_t *ctx,
                             const char *uri,
                             void *out_buff,
                             size_t out_buff_len,
                             size_t *copied_bytes) {
    size_t buff_len = out_buff_len - *copied_bytes;
    size_t bytes_to_copy = ANJ_MIN(ctx->remaining_bytes, buff_len);

    if (ctx->remaining_bytes <= buff_len) {
        memcpy(&((uint8_t *) out_buff)[*copied_bytes],
               &uri[ctx->offset - ctx->bytes_in_internal_buff],
               bytes_to_copy - 1);
        ((uint8_t *) out_buff)[*copied_bytes + bytes_to_copy - 1] = '\"';
    } else {
        memcpy(&((uint8_t *) out_buff)[*copied_bytes],
               &uri[ctx->offset - ctx->bytes_in_internal_buff], bytes_to_copy);
    }
    *copied_bytes += bytes_to_copy;
    ctx->remaining_bytes -= bytes_to_copy;
    ctx->offset += bytes_to_copy;

    // no more records
    if (!ctx->remaining_bytes) {
        _anj_io_reset_internal_buff(ctx);
        return 0;
    }
    return ANJ_IO_NEED_NEXT_CALL;
}

int _anj_io_bootstrap_discover_ctx_get_payload(
        _anj_io_bootstrap_discover_ctx_t *ctx,
        void *out_buff,
        size_t out_buff_len,
        size_t *out_copied_bytes) {
    assert(ctx && out_buff && out_buff_len && out_copied_bytes);
    _anj_io_buff_t *buff_ctx = &ctx->buff;

    if (!buff_ctx->remaining_bytes) {
        return _ANJ_IO_ERR_LOGIC;
    }
    _anj_io_get_payload_from_internal_buff(&ctx->buff, out_buff, out_buff_len,
                                           out_copied_bytes);

    if (buff_ctx->is_extended_type
            && buff_ctx->offset >= buff_ctx->bytes_in_internal_buff) {
        return add_bootstrap_uri(buff_ctx, ctx->uri, out_buff, out_buff_len,
                                 out_copied_bytes);
    }

    if (!buff_ctx->remaining_bytes) {
        _anj_io_reset_internal_buff(buff_ctx);
    } else {
        return ANJ_IO_NEED_NEXT_CALL;
    }
    return 0;
}

#endif // ANJ_WITH_BOOTSTRAP_DISCOVER

#ifdef ANJ_WITH_DISCOVER

static bool res_instances_will_be_written(const anj_uri_path_t *base_path,
                                          uint8_t depth) {
    return (uint8_t) base_path->uri_len + depth > ANJ_ID_RIID;
}

static size_t add_attribute(_anj_io_buff_t *ctx,
                            const char *name,
                            const uint32_t *uint_value,
                            const double *double_value) {
    assert(!!uint_value != !!double_value);

    size_t record_len = 0;

    ctx->internal_buff[record_len++] = ';';
    memcpy(&ctx->internal_buff[record_len], name, strlen(name));
    record_len += strlen(name);
    ctx->internal_buff[record_len++] = '=';

    if (uint_value) {
        record_len += anj_uint32_to_string_value(
                (char *) &ctx->internal_buff[record_len], *uint_value);
    } else {
        record_len += anj_double_to_string_value(
                (char *) &ctx->internal_buff[record_len], *double_value);
    }
    return record_len;
}

static size_t get_attribute_record(_anj_io_buff_t *ctx,
                                   _anj_attr_notification_t *attributes) {
    if (attributes->has_min_period) {
        attributes->has_min_period = false;
        return add_attribute(ctx, "pmin", &attributes->min_period, NULL);
    }
    if (attributes->has_max_period) {
        attributes->has_max_period = false;
        return add_attribute(ctx, "pmax", &attributes->max_period, NULL);
    }
    if (attributes->has_greater_than) {
        attributes->has_greater_than = false;
        return add_attribute(ctx, "gt", NULL, &attributes->greater_than);
    }
    if (attributes->has_less_than) {
        attributes->has_less_than = false;
        return add_attribute(ctx, "lt", NULL, &attributes->less_than);
    }
    if (attributes->has_step) {
        attributes->has_step = false;
        return add_attribute(ctx, "st", NULL, &attributes->step);
    }
    if (attributes->has_min_eval_period) {
        attributes->has_min_eval_period = false;
        return add_attribute(ctx, "epmin", &attributes->min_eval_period, NULL);
    }
    if (attributes->has_max_eval_period) {
        attributes->has_max_eval_period = false;
        return add_attribute(ctx, "epmax", &attributes->max_eval_period, NULL);
    }
#    ifdef ANJ_WITH_LWM2M12
    if (attributes->has_edge) {
        attributes->has_edge = false;
        return add_attribute(ctx, "edge", &attributes->edge, NULL);
    }
    if (attributes->has_con) {
        attributes->has_con = false;
        return add_attribute(ctx, "con", &attributes->con, NULL);
    }
    if (attributes->has_hqmax) {
        attributes->has_hqmax = false;
        return add_attribute(ctx, "hqmax", &attributes->hqmax, NULL);
    }
#    endif // ANJ_WITH_LWM2M12

    return 0;
}

static int get_attributes_payload(_anj_io_discover_ctx_t *ctx,
                                  void *out_buff,
                                  size_t out_buff_len,
                                  size_t *copied_bytes) {
    while (true) {
        if (ctx->attr_record_offset == ctx->attr_record_len) {
            ctx->attr_record_len = get_attribute_record(&ctx->buff, &ctx->attr);
            ctx->attr_record_offset = 0;
        }

        size_t bytes_to_copy =
                ANJ_MIN(ctx->attr_record_len - ctx->attr_record_offset,
                        out_buff_len - *copied_bytes);

        memcpy(&((uint8_t *) out_buff)[*copied_bytes],
               &(ctx->buff.internal_buff[ctx->attr_record_offset]),
               bytes_to_copy);
        *copied_bytes += bytes_to_copy;

        // no more records
        if (ctx->attr_record_len == ctx->attr_record_offset) {
            ctx->buff.offset = 0;
            ctx->buff.bytes_in_internal_buff = 0;
            ctx->buff.is_extended_type = false;
            return 0;
        }
        ctx->attr_record_offset += bytes_to_copy;

        if (!(out_buff_len - *copied_bytes)) {
            return ANJ_IO_NEED_NEXT_CALL;
        }
    }
}

int _anj_io_discover_ctx_init(_anj_io_discover_ctx_t *ctx,
                              const anj_uri_path_t *base_path,
                              const uint32_t *depth) {
    assert(ctx && base_path);

    if ((depth && *depth > 3) || !anj_uri_path_has(base_path, ANJ_ID_OID)
            || anj_uri_path_is(base_path, ANJ_ID_RIID)) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    memset(ctx, 0, sizeof(_anj_io_discover_ctx_t));
    ctx->base_path = *base_path;

    if (depth) {
        ctx->depth = (uint8_t) *depth;
    } else {
        // default depth value
        if (anj_uri_path_is(base_path, ANJ_ID_OID)) {
            ctx->depth = 2;
        } else {
            ctx->depth = 1;
        }
    }

    return 0;
}

int _anj_io_discover_ctx_new_entry(_anj_io_discover_ctx_t *ctx,
                                   const anj_uri_path_t *path,
                                   const _anj_attr_notification_t *attributes,
                                   const char *version,
                                   const uint16_t *dim) {
    assert(ctx && path);

    if (ctx->buff.bytes_in_internal_buff || ctx->buff.is_extended_type) {
        return _ANJ_IO_ERR_LOGIC;
    }
    if (path->uri_len - ctx->base_path.uri_len > ctx->depth) {
        return _ANJ_IO_WARNING_DEPTH;
    }
    if ((ctx->dim_counter && !anj_uri_path_is(path, ANJ_ID_RIID))
            || (!ctx->dim_counter && anj_uri_path_is(path, ANJ_ID_RIID))) {
        return _ANJ_IO_ERR_LOGIC;
    }
    if (anj_uri_path_outside_base(path, &ctx->base_path)
            || !anj_uri_path_has(path, ANJ_ID_OID)
            || !anj_uri_path_increasing(&ctx->last_path, path)
            || (version && !anj_uri_path_is(path, ANJ_ID_OID))
            || (dim && !anj_uri_path_is(path, ANJ_ID_RID))) {
        return _ANJ_IO_ERR_INPUT_ARG;
    }

    if (dim && res_instances_will_be_written(&ctx->base_path, ctx->depth)) {
        ctx->dim_counter = *dim;
    }

    int res = _anj_io_add_link_format_record(
            path, version, dim, !ctx->first_record_added, &ctx->buff);
    if (res) {
        return res;
    }

    if (attributes) {
        ctx->attr = *attributes;
        ctx->buff.is_extended_type = true;
    }

    ctx->first_record_added = true;
    ctx->last_path = *path;
    if (ctx->dim_counter && anj_uri_path_is(path, ANJ_ID_RIID)) {
        ctx->dim_counter--;
    }
    return 0;
}

int _anj_io_discover_ctx_get_payload(_anj_io_discover_ctx_t *ctx,
                                     void *out_buff,
                                     size_t out_buff_len,
                                     size_t *out_copied_bytes) {
    assert(ctx && out_buff && out_buff_len && out_copied_bytes);
    _anj_io_buff_t *buff_ctx = &ctx->buff;

    if (!buff_ctx->remaining_bytes && !buff_ctx->is_extended_type) {
        return _ANJ_IO_ERR_LOGIC;
    }
    _anj_io_get_payload_from_internal_buff(&ctx->buff, out_buff, out_buff_len,
                                           out_copied_bytes);

    // there are attributes left and link_format record is copied
    if (buff_ctx->is_extended_type
            && buff_ctx->offset >= buff_ctx->bytes_in_internal_buff) {
        return get_attributes_payload(ctx, out_buff, out_buff_len,
                                      out_copied_bytes);
    }

    if (!buff_ctx->remaining_bytes) {
        _anj_io_reset_internal_buff(buff_ctx);
    } else {
        return ANJ_IO_NEED_NEXT_CALL;
    }
    return 0;
}

#endif // ANJ_WITH_DISCOVER
