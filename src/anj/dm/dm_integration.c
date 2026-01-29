/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 23

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/log.h>
#include <anj/utils.h>

#ifdef ANJ_WITH_OBSERVE
#    include "../observe/observe.h"
#endif // ANJ_WITH_OBSERVE

#include "../coap/coap.h"
#include "../io/io.h"
#include "../utils.h"
#include "dm_core.h"
#include "dm_integration.h"
#include "dm_io.h"

#define _DM_CONTINUE 2

static uint8_t map_anj_io_err_to_coap_code(int code) {
    switch (code) {
    case _ANJ_IO_ERR_INPUT_ARG:
    case _ANJ_IO_ERR_IO_TYPE:
    case _ANJ_IO_ERR_LOGIC:
        return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    case _ANJ_IO_ERR_FORMAT:
        return ANJ_COAP_CODE_BAD_REQUEST;
    case _ANJ_IO_ERR_UNSUPPORTED_FORMAT:
        return ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT;
    default:
        break;
    }
    ANJ_UNREACHABLE("error code is not recognized as anj_io error");
    return (uint8_t) code;
}

static uint8_t map_err_to_coap_code(int error_code) {
    assert(error_code != 0);
    switch (error_code) {
    case ANJ_DM_ERR_INTERNAL:
    case _ANJ_DM_ERR_MEMORY:
    case _ANJ_DM_ERR_LOGIC:
        return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    case ANJ_DM_ERR_BAD_REQUEST:
        return ANJ_COAP_CODE_BAD_REQUEST;
    case ANJ_DM_ERR_UNAUTHORIZED:
        return ANJ_COAP_CODE_UNAUTHORIZED;
    case ANJ_DM_ERR_NOT_FOUND:
        return ANJ_COAP_CODE_NOT_FOUND;
    case ANJ_DM_ERR_METHOD_NOT_ALLOWED:
        return ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
    case _ANJ_DM_ERR_INPUT_ARG:
        return ANJ_COAP_CODE_BAD_REQUEST;
    case ANJ_DM_ERR_NOT_IMPLEMENTED:
        return ANJ_COAP_CODE_NOT_IMPLEMENTED;
    case ANJ_DM_ERR_SERVICE_UNAVAILABLE:
        return ANJ_COAP_CODE_SERVICE_UNAVAILABLE;
    // anj_io errors are already mapped to coap codes
    case ANJ_COAP_CODE_BAD_REQUEST:
        return ANJ_COAP_CODE_BAD_REQUEST;
    case ANJ_COAP_CODE_NOT_ACCEPTABLE:
        return ANJ_COAP_CODE_NOT_ACCEPTABLE;
    case ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT:
        return ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT;
    case ANJ_COAP_CODE_INTERNAL_SERVER_ERROR:
        return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    default:
        return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    }
}

static void resource_uri_trace_log(const anj_uri_path_t *path) {
    if (anj_uri_path_is(path, ANJ_ID_RID)) {
        dm_log(L_TRACE, "/%" PRIu16 "/%" PRIu16 "/%" PRIu16, path->ids[0],
               path->ids[1], path->ids[2]);
    } else if (anj_uri_path_is(path, ANJ_ID_RIID)) {
        dm_log(L_TRACE, "/%" PRIu16 "/%" PRIu16 "/%" PRIu16 "/%" PRIu16,
               path->ids[0], path->ids[1], path->ids[2], path->ids[3]);
    }
}

static void uri_log(const anj_uri_path_t *path) {
    if (anj_uri_path_is(path, ANJ_ID_OID)) {
        dm_log(L_DEBUG, "/%" PRIu16, path->ids[0]);
    } else if (anj_uri_path_is(path, ANJ_ID_IID)) {
        dm_log(L_DEBUG, "/%" PRIu16 "/%" PRIu16, path->ids[0], path->ids[1]);
    } else if (anj_uri_path_is(path, ANJ_ID_RID)) {
        dm_log(L_DEBUG, "/%" PRIu16 "/%" PRIu16 "/%" PRIu16, path->ids[0],
               path->ids[1], path->ids[2]);
    } else if (anj_uri_path_is(path, ANJ_ID_RIID)) {
        dm_log(L_DEBUG, "/%" PRIu16 "/%" PRIu16 "/%" PRIu16 "/%" PRIu16,
               path->ids[0], path->ids[1], path->ids[2], path->ids[3]);
    }
}

static int handle_read_payload_result(_anj_dm_data_model_t *ctx,
                                      int anj_io_return_code,
                                      int dm_return_code,
                                      size_t offset,
                                      size_t out_buff_len) {
    (void) out_buff_len;
    (void) offset;
    if (!anj_io_return_code) {
        ctx->data_to_copy = false;
    } else if (anj_io_return_code == ANJ_IO_NEED_NEXT_CALL) {
        ctx->data_to_copy = true;
        assert(offset == out_buff_len);
        return _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
    } else {
        dm_log(L_ERROR, "anj_io ctx error: %d", anj_io_return_code);
        ctx->data_to_copy = false;
        return map_anj_io_err_to_coap_code(anj_io_return_code);
    }
    if (dm_return_code == _ANJ_DM_LAST_RECORD) {
        return 0;
    }
    if (offset == out_buff_len) {
        return _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
    }

    return _DM_CONTINUE;
}
#ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
static int process_bootstrap_discover(anj_t *anj,
                                      uint8_t *buff,
                                      size_t buff_len,
                                      size_t *out_payload_len) {
    int ret_dm = 0;
    int ret_anj = 0;
    anj_uri_path_t path;
    const char *version;
    const uint16_t *ssid;
    const char *uri;
    *out_payload_len = 0;
    size_t copied_bytes;
    _anj_dm_data_model_t *ctx = &anj->dm;

    while (true) {
        if (!ctx->data_to_copy && ctx->op_count > 0) {
            ret_dm = _anj_dm_get_bootstrap_discover_record(anj, &path, &version,
                                                           &ssid, &uri);
            if (ret_dm && ret_dm != _ANJ_DM_LAST_RECORD) {
                return ret_dm;
            }
            ret_anj = _anj_io_bootstrap_discover_ctx_new_entry(
                    &anj->anj_io.bootstrap_discover_ctx, &path, version, ssid,
                    uri);
            if (ret_anj) {
                dm_log(L_ERROR, "anj_io bootstrap discover ctx error %d",
                       ret_anj);
                return map_anj_io_err_to_coap_code(ret_anj);
            }
        } else if (ctx->op_count == 0) {
            ret_dm = _ANJ_DM_LAST_RECORD;
        }
        ret_anj = _anj_io_bootstrap_discover_ctx_get_payload(
                &anj->anj_io.bootstrap_discover_ctx,
                &buff[*out_payload_len],
                buff_len - *out_payload_len,
                &copied_bytes);
        *out_payload_len += copied_bytes;
        int ret = handle_read_payload_result(ctx, ret_anj, ret_dm,
                                             *out_payload_len, buff_len);
        if (ret != _DM_CONTINUE) {
            return ret;
        }
    }
}
#endif // ANJ_WITH_BOOTSTRAP_DISCOVER

#ifdef ANJ_WITH_DISCOVER
static int process_discover(anj_t *anj,
                            uint8_t *buff,
                            size_t buff_len,
                            size_t *out_payload_len) {
    int ret_dm = 0;
    int ret_anj = 0;
    anj_uri_path_t path;
    const char *version;
    const uint16_t *dim;
    *out_payload_len = 0;
    size_t copied_bytes;
    _anj_dm_data_model_t *ctx = &anj->dm;
    while (true) {
        if (!ctx->data_to_copy && ctx->op_count > 0) {
            ret_dm = _anj_dm_get_discover_record(anj, &path, &version, &dim);
            if (ret_dm && ret_dm != _ANJ_DM_LAST_RECORD) {
                return ret_dm;
            }
            _anj_attr_notification_t *attr_ptr = NULL;
#    ifdef ANJ_WITH_DISCOVER_ATTR
            bool first_call =
                    (ctx->op_count + 1) == ctx->op_ctx.disc_ctx.total_op_count;
            _anj_attr_notification_t attr;
            if (!_anj_observe_get_attr_storage(anj, ctx->ssid, first_call,
                                               &path, &attr)) {
                attr_ptr = &attr;
            }
#    endif // ANJ_WITH_DISCOVER_ATTR
            ret_anj = _anj_io_discover_ctx_new_entry(
                    &anj->anj_io.discover_ctx, &path, attr_ptr, version, dim);
            if (ret_anj == _ANJ_IO_WARNING_DEPTH) {
                continue;
            } else if (ret_anj) {
                dm_log(L_ERROR, "anj_io discover ctx error %d", ret_anj);
                return map_anj_io_err_to_coap_code(ret_anj);
            }
        } else if (ctx->op_count == 0) {
            ret_dm = _ANJ_DM_LAST_RECORD;
        }
        if (ret_anj == _ANJ_IO_WARNING_DEPTH) {
            // @ref _ANJ_IO_WARNING_DEPTH means that last @ref
            // _anj_io_discover_ctx_new_entry call was skipped
            ret_anj = 0;
        } else {
            ret_anj = _anj_io_discover_ctx_get_payload(
                    &anj->anj_io.discover_ctx,
                    &buff[*out_payload_len],
                    buff_len - *out_payload_len,
                    &copied_bytes);
            *out_payload_len += copied_bytes;
        }
        int ret = handle_read_payload_result(ctx, ret_anj, ret_dm,
                                             *out_payload_len, buff_len);
        if (ret != _DM_CONTINUE) {
            return ret;
        }
    }
}
#endif // ANJ_WITH_DISCOVER

static int process_read(anj_t *anj,
                        uint8_t *buff,
                        size_t buff_len,
                        size_t *out_payload_len,
                        const anj_uri_path_t *path,
                        bool composite) {
    (void) path;
    int ret_dm = 0;
    int ret_anj = 0;
    *out_payload_len = 0;
    size_t copied_bytes;
    _anj_dm_data_model_t *ctx = &anj->dm;
    while (true) {
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
        if (!ctx->data_to_copy && composite && ctx->op_count == 0) {
            // start processing composite objects from the beginning for reach
            // path
            anj->dm.comp_read_current_object = 0;
            ret_dm = _anj_dm_composite_next_path(anj, path);
            if (ret_dm == _ANJ_DM_NO_RECORD) {
                return 0;
            } else if (ret_dm) {
                return ret_dm;
            }
        }
#else  // ANJ_WITH_COMPOSITE_OPERATIONS
        assert(composite == false);
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
        if (!ctx->data_to_copy && ctx->op_count > 0) {
            ret_dm = _anj_dm_get_read_entry(anj, &ctx->out_record);
            if (ret_dm && ret_dm != _ANJ_DM_LAST_RECORD) {
                return ret_dm;
            }
            dm_log(L_TRACE, "Reading from:");
            resource_uri_trace_log(&ctx->out_record.path);
            ret_anj = _anj_io_out_ctx_new_entry(&anj->anj_io.out_ctx,
                                                &ctx->out_record);
            if (ret_anj) {
                dm_log(L_ERROR, "anj_io out ctx error %d", ret_anj);
                return map_anj_io_err_to_coap_code(ret_anj);
            }
        } else if (ctx->op_count == 0) {
            ret_dm = _ANJ_DM_LAST_RECORD;
        }
        ret_anj = _anj_io_out_ctx_get_payload(&anj->anj_io.out_ctx,
                                              &buff[*out_payload_len],
                                              buff_len - *out_payload_len,
                                              &copied_bytes);
        *out_payload_len += copied_bytes;
        int ret = handle_read_payload_result(ctx, ret_anj, ret_dm,
                                             *out_payload_len, buff_len);
        if (ret != _DM_CONTINUE) {
            return ret;
        }
    }
}

static int process_register(anj_t *anj,
                            uint8_t *buff,
                            size_t buff_len,
                            size_t *out_payload_len) {
    int ret_dm = 0;
    int ret_anj = 0;
    anj_uri_path_t path;
    const char *version;
    *out_payload_len = 0;
    size_t copied_bytes;

    _anj_dm_data_model_t *ctx = &anj->dm;
    while (true) {
        if (!ctx->data_to_copy && ctx->op_count > 0) {
            ret_dm = _anj_dm_get_register_record(anj, &path, &version);
            if (ret_dm && ret_dm != _ANJ_DM_LAST_RECORD) {
                return ret_dm;
            }
            ret_anj = _anj_io_register_ctx_new_entry(&anj->anj_io.register_ctx,
                                                     &path, version);
            if (ret_anj) {
                dm_log(L_ERROR, "anj_io register ctx error %d", ret_anj);
                return map_anj_io_err_to_coap_code(ret_anj);
            }
        } else if (ctx->op_count == 0) {
            ret_dm = _ANJ_DM_LAST_RECORD;
        }
        ret_anj = _anj_io_register_ctx_get_payload(&anj->anj_io.register_ctx,
                                                   &buff[*out_payload_len],
                                                   buff_len - *out_payload_len,
                                                   &copied_bytes);
        *out_payload_len += copied_bytes;
        int ret = handle_read_payload_result(ctx, ret_anj, ret_dm,
                                             *out_payload_len, buff_len);
        if (ret != _DM_CONTINUE) {
            return ret;
        }
    }
}

#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
static int read_composite(anj_t *anj,
                          const void *uri_paths,
                          const size_t uri_path_count,
                          bool paths_as_pointers,
                          size_t *already_processed,
                          uint8_t *buff,
                          size_t buff_len,
                          size_t *out_payload_len,
                          uint16_t *out_format) {
    int res;
    size_t copied_bytes;
    *out_payload_len = 0;
    *out_format = _anj_io_out_ctx_get_format(&anj->anj_io.out_ctx);

    if (uri_path_count == 0) {
        // for handling empty responses
        res = _anj_io_out_ctx_get_payload(&anj->anj_io.out_ctx, buff, buff_len,
                                          out_payload_len);
        return res;
    }

    for (; *already_processed < uri_path_count; (*already_processed)++) {
        res = process_read(
                anj, &buff[*out_payload_len], buff_len - *out_payload_len,
                &copied_bytes,
                paths_as_pointers ? ((const anj_uri_path_t *const *) (uintptr_t)
                                             uri_paths)[*already_processed]
                                  : &((const anj_uri_path_t *)
                                              uri_paths)[*already_processed],
                true);
        *out_payload_len += copied_bytes;
        if (res) {
            return res;
        }
    }
    return 0;
}
#endif // ANJ_WITH_COMPOSITE_OPERATIONS

static uint8_t _dm_read_payload(void *arg_ptr,
                                uint8_t *buff,
                                size_t buff_len,
                                _anj_exchange_read_result_t *out_params) {
    anj_t *anj = (anj_t *) arg_ptr;
    _anj_dm_data_model_t *ctx = &anj->dm;
    int ret_val = 0;

    switch (ctx->operation) {
    case ANJ_OP_REGISTER:
    case ANJ_OP_UPDATE:
        out_params->format = _ANJ_COAP_FORMAT_LINK_FORMAT;
        ret_val =
                process_register(anj, buff, buff_len, &out_params->payload_len);
        break;
    case ANJ_OP_DM_DISCOVER:
        out_params->format = _ANJ_COAP_FORMAT_LINK_FORMAT;
        out_params->payload_len = 0;
#ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
        if (ctx->bootstrap_operation) {
            ret_val = process_bootstrap_discover(anj, buff, buff_len,
                                                 &out_params->payload_len);
        }
#endif // ANJ_WITH_BOOTSTRAP_DISCOVER
#ifdef ANJ_WITH_DISCOVER
        if (!ctx->bootstrap_operation) {
            ret_val = process_discover(anj, buff, buff_len,
                                       &out_params->payload_len);
        }
#endif // ANJ_WITH_DISCOVER
        break;
    case ANJ_OP_DM_READ:
        out_params->format = _anj_io_out_ctx_get_format(&anj->anj_io.out_ctx);
        ret_val = process_read(anj, buff, buff_len, &out_params->payload_len,
                               NULL, false);
        break;
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    case ANJ_OP_DM_READ_COMP:
        ret_val = read_composite(anj, ctx->comp_read_paths,
                                 ctx->comp_read_path_count, false,
                                 &ctx->comp_read_already_processed, buff,
                                 buff_len, &out_params->payload_len,
                                 &out_params->format);
        break;
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
    case ANJ_OP_DM_CREATE:
        // HACK: Create operation has no payload, so we have to create an
        // instance here
        if (!ctx->op_ctx.write_ctx.instance_creation_attempted) {
            ret_val = _anj_dm_create_object_instance(anj, ANJ_ID_INVALID);
            if (ret_val) {
                return map_err_to_coap_code(ret_val);
            }
            ctx->op_ctx.write_ctx.instance_creation_attempted = true;
            ret_val = _anj_dm_operation_validate(anj);
        }
        if (!ctx->iid_provided) {
            dm_log(L_DEBUG, "Adding new object instance to the path");
            out_params->with_create_path = true;
            out_params->created_oid =
                    ctx->op_ctx.write_ctx.path.ids[ANJ_ID_OID];
            out_params->created_iid =
                    ctx->op_ctx.write_ctx.path.ids[ANJ_ID_IID];
        }
        // fall through
    default:
        out_params->format = _ANJ_COAP_FORMAT_NOT_DEFINED;
        out_params->payload_len = 0;
        break;
    }

    if (ret_val && ret_val != _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED) {
        return map_err_to_coap_code(ret_val);
    } else if (ret_val == _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED) {
        return _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
    }
    return 0;
}

static int process_write(anj_t *anj,
                         uint8_t *payload,
                         size_t payload_len,
                         bool last_block) {
    _anj_dm_data_model_t *ctx = &anj->dm;
    int ret_anj = _anj_io_in_ctx_feed_payload(&anj->anj_io.in_ctx, payload,
                                              payload_len, last_block);
    if (ret_anj) {
        dm_log(L_ERROR, "anj_io in ctx error: %d", ret_anj);
        return map_anj_io_err_to_coap_code(ret_anj);
    }

    const anj_res_value_t *value;
    const anj_uri_path_t *path;
    anj_io_out_entry_t record;
    int ret_dm;

    while (true) {
        record.type = ANJ_DATA_TYPE_ANY;
        ret_anj = _anj_io_in_ctx_get_entry(&anj->anj_io.in_ctx, &record.type,
                                           &value, &path);
        if (!ret_anj || ret_anj == _ANJ_IO_WANT_TYPE_DISAMBIGUATION) {
            if (!path) {
                // SenML CBOR allows to build message with path on the end, so
                // record without path is technically possible for block
                // transfers
                dm_log(L_ERROR, "anj_io in ctx no path given");
                return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
            }
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
            // It must be checked before _anj_dm_get_resource_type call
            if (ctx->operation == ANJ_OP_DM_WRITE_COMP
                    && _anj_uri_path_to_security_or_oscore_obj(path)) {
                return ANJ_DM_ERR_UNAUTHORIZED;
            }
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
            if (ctx->operation == ANJ_OP_DM_CREATE
                    && !ctx->op_ctx.write_ctx.instance_creation_attempted) {
                ret_dm = _anj_dm_create_object_instance(anj,
                                                        path->ids[ANJ_ID_IID]);
                if (ret_dm) {
                    return ret_dm;
                }
            }
            record.path = *path;
            if (ctx->operation == ANJ_OP_DM_CREATE) {
                record.path.ids[ANJ_ID_IID] =
                        ctx->op_ctx.write_ctx.path.ids[ANJ_ID_IID];
            }
        }
        if (ret_anj == _ANJ_IO_WANT_TYPE_DISAMBIGUATION) {
            assert(ctx->operation != ANJ_OP_DM_READ_COMP);
            ret_dm = _anj_dm_get_resource_type(anj, &record.path, &record.type);
            if (ret_dm) {
                return ret_dm;
            }
            ret_anj = _anj_io_in_ctx_get_entry(&anj->anj_io.in_ctx,
                                               &record.type, &value, &path);
        }
        if (!ret_anj) {
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
            if (ctx->operation == ANJ_OP_DM_READ_COMP) {
                if (ctx->comp_read_path_count == ANJ_DM_MAX_COMP_READ_ENTRIES) {
                    /* No space for another path, respond with
                     * ANJ_COAP_CODE_INTERNAL_SERVER_ERROR */
                    dm_log(L_ERROR,
                           "Exceeded maximum number of composite read paths");
                    return _ANJ_DM_ERR_LOGIC;
                } else if (_anj_dm_path_has_readable_resources(&anj->dm, path)
                           == 0) {
                    ctx->comp_read_paths[ctx->comp_read_path_count++] = *path;
                    ret_dm = 0;
                }
            } else
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
            {
                if (!value) {
                    if (ctx->operation == ANJ_OP_DM_CREATE) {
                        // if value is not provided, we assume that only iid was
                        // given in payload
                        return 0;
                    }
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
                    if (ctx->operation != ANJ_OP_DM_WRITE_COMP
                            || record.type != ANJ_DATA_TYPE_NULL)
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
                    {
                        dm_log(L_ERROR, "anj_io in ctx no value given");
                        return ANJ_DM_ERR_BAD_REQUEST;
                    }
                } else {
                    record.value = *value;
                }
                dm_log(L_TRACE, "Writing to:");
                resource_uri_trace_log(&record.path);
                ret_dm = _anj_dm_write_entry(anj, &record);
                if (ret_dm) {
                    return ret_dm;
                }
            }
        } else if ((ret_anj == _ANJ_IO_WANT_NEXT_PAYLOAD && !last_block)
                   || ret_anj == _ANJ_IO_EOF) {
            return 0;
        } else {
            dm_log(L_ERROR, "anj_io in ctx error %d", ret_anj);
            return map_anj_io_err_to_coap_code(ret_anj);
        }
    }
}

static uint8_t _dm_write_payload(void *arg_ptr,
                                 uint8_t *payload,
                                 size_t payload_len,
                                 bool last_block) {
    anj_t *anj = (anj_t *) arg_ptr;
    _anj_dm_data_model_t *ctx = &anj->dm;
    int ret_val;

    switch (ctx->operation) {
    case ANJ_OP_DM_WRITE_REPLACE:
    case ANJ_OP_DM_WRITE_PARTIAL_UPDATE:
    case ANJ_OP_DM_WRITE_COMP:
    case ANJ_OP_DM_CREATE:
    case ANJ_OP_DM_READ_COMP:
        ret_val = process_write(anj, payload, payload_len, last_block);
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
        if (ret_val == 0 && ctx->operation == ANJ_OP_DM_READ_COMP
                && last_block) {
            size_t res_count = 0;

            for (size_t i = 0, path_res_count; i < ctx->comp_read_path_count;
                 i++) {
                ret_val = _anj_dm_count_readable_res_if_allowed(
                        anj, &ctx->comp_read_paths[i], &path_res_count);
                if (ret_val) {
                    break;
                }

                res_count += path_res_count;
            }

            if (ret_val) {
                break;
            }

            ret_val = _anj_io_out_ctx_init(&anj->anj_io.out_ctx,
                                           ANJ_OP_DM_READ_COMP,
                                           &ANJ_MAKE_ROOT_PATH(), res_count,
                                           ctx->comp_read_format);

            if (res_count == 0) {
                ctx->comp_read_path_count = 0;
            }
        }
#else  // ANJ_WITH_COMPOSITE_OPERATIONS
        assert(ctx->operation != ANJ_OP_DM_READ_COMP);
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
        break;
    case ANJ_OP_DM_EXECUTE:
        ret_val = _anj_dm_execute(anj, (const char *) payload, payload_len);
        break;
    default:
        dm_log(L_ERROR, "Writing not supported for operation: %d",
               (int) ctx->operation);
        ret_val = ANJ_DM_ERR_BAD_REQUEST;
        break;
    }
    // Perform validation only for the final block of a transactional operation.
    // Validation is skipped if an error has already occurred, or if the
    // operation is non-transactional (e.g., a composite read).
    if (last_block && ctx->is_transactional && !ret_val) {
        ret_val = _anj_dm_operation_validate(anj);
    }
    if (ret_val) {
        return map_err_to_coap_code(ret_val);
    }
    return 0;
}

static void _dm_process_finalization(void *arg_ptr,
                                     const _anj_coap_msg_t *response,
                                     int result) {
    (void) response;
    anj_t *anj = (anj_t *) arg_ptr;
#ifdef ANJ_WITH_EXTERNAL_DATA
    if (result != _ANJ_EXCHANGE_RESULT_SUCCESS
            && (anj->dm.out_record.type & ANJ_DATA_TYPE_FLAG_EXTERNAL)
            && anj->dm.data_to_copy
            && (anj->dm.operation == ANJ_OP_DM_READ
#    ifdef ANJ_WITH_COMPOSITE_OPERATIONS
                || anj->dm.operation == ANJ_OP_DM_READ_COMP
#    endif // ANJ_WITH_COMPOSITE_OPERATIONS
                )) {
        _anj_io_out_ctx_close_external_data_cb(&anj->dm.out_record);
    }
#endif // ANJ_WITH_EXTERNAL_DATA
    if (anj->dm.op_in_progress) {
        dm_log(L_TRACE, "Data model operation finalized");
        _anj_dm_operation_end(anj, (result == _ANJ_EXCHANGE_RESULT_SUCCESS)
                                           ? ANJ_DM_TRANSACTION_SUCCESS
                                           : ANJ_DM_TRANSACTION_FAILURE);
    }
}

void _anj_dm_process_request(anj_t *anj,
                             const _anj_coap_msg_t *request,
                             uint16_t ssid,
                             uint8_t *out_response_code,
                             _anj_exchange_handlers_t *out_handlers) {
    assert(anj && request && out_response_code && out_handlers);
    _anj_dm_data_model_t *ctx = &anj->dm;
    assert(!ctx->op_in_progress);

    ctx->data_to_copy = false;

    *out_handlers = (_anj_exchange_handlers_t) {
        .read_payload = _dm_read_payload,
        .write_payload = _dm_write_payload,
        .completion = _dm_process_finalization,
        .arg = anj
    };

    bool bootstrap_call = (ssid == _ANJ_SSID_BOOTSTRAP);
#ifdef ANJ_WITH_OBSERVE
    ctx->ssid = ssid;
#endif // ANJ_WITH_OBSERVE
    int ret_val = _anj_dm_operation_begin(anj, request->operation,
                                          bootstrap_call, &request->uri);
    if (!ret_val) {
        size_t res_count = 0;
        switch (ctx->operation) {
        case ANJ_OP_DM_DISCOVER:
            dm_log(L_DEBUG, "Discover operation");
            ret_val = ANJ_DM_ERR_NOT_IMPLEMENTED;
#ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
            if (bootstrap_call) {
                ret_val = _anj_io_bootstrap_discover_ctx_init(
                        &anj->anj_io.bootstrap_discover_ctx, &request->uri);
            }
#endif // ANJ_WITH_BOOTSTRAP_DISCOVER
#ifdef ANJ_WITH_DISCOVER
            if (!bootstrap_call) {
                ret_val = _anj_io_discover_ctx_init(
                        &anj->anj_io.discover_ctx, &request->uri,
                        request->attr.discover_attr.has_depth
                                ? &request->attr.discover_attr.depth
                                : NULL);
            }
#endif // ANJ_WITH_DISCOVER
            if (ret_val) {
                ret_val = map_anj_io_err_to_coap_code(ret_val);
            }
            *out_response_code = ANJ_COAP_CODE_CONTENT;
            break;
        case ANJ_OP_DM_WRITE_REPLACE:
        case ANJ_OP_DM_WRITE_PARTIAL_UPDATE:
        case ANJ_OP_DM_CREATE:
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
        case ANJ_OP_DM_WRITE_COMP:
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
            dm_log(L_DEBUG, "Write/create operation");
            ret_val =
                    _anj_io_in_ctx_init(&anj->anj_io.in_ctx, ctx->operation,
                                        &request->uri, request->content_format);
            *out_response_code = (ctx->operation == ANJ_OP_DM_CREATE)
                                         ? ANJ_COAP_CODE_CREATED
                                         : ANJ_COAP_CODE_CHANGED;
            if (ret_val) {
                ret_val = map_anj_io_err_to_coap_code(ret_val);
            }
            break;
        case ANJ_OP_DM_READ:
            dm_log(L_DEBUG, "Read operation");
            _anj_dm_get_readable_res_count(anj, &res_count);
            if (!res_count) {
                dm_log(L_INFO, "No readable resources for given path");
            }
            ret_val = _anj_io_out_ctx_init(&anj->anj_io.out_ctx, ANJ_OP_DM_READ,
                                           &request->uri, res_count,
                                           request->accept);
            if (ret_val) {
                ret_val = ret_val != _ANJ_IO_ERR_UNSUPPORTED_FORMAT
                                  ? map_anj_io_err_to_coap_code(ret_val)
                                  : ANJ_COAP_CODE_NOT_ACCEPTABLE;
            }
            *out_response_code = ANJ_COAP_CODE_CONTENT;
            break;
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
        case ANJ_OP_DM_READ_COMP:
            dm_log(L_DEBUG, "Read composite operation");
            ctx->comp_read_path_count = 0;
            ctx->comp_read_already_processed = 0;
            ctx->comp_read_format = request->accept;
            ret_val = _anj_io_in_ctx_init(&anj->anj_io.in_ctx, ctx->operation,
                                          NULL, request->content_format);
            if (ret_val) {
                ret_val = map_anj_io_err_to_coap_code(ret_val);
            }

            *out_response_code = ANJ_COAP_CODE_CONTENT;
            break;
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
        case ANJ_OP_DM_EXECUTE:
            dm_log(L_DEBUG, "Execute operation");
            if (!request->payload_size) {
                // write_handler won't be called for empty payload
                ret_val = _anj_dm_execute(anj, NULL, 0);
            }
            *out_response_code = ANJ_COAP_CODE_CHANGED;
            break;
        case ANJ_OP_DM_DELETE:
            dm_log(L_DEBUG, "Delete operation");
            ret_val = _anj_dm_operation_validate(anj);
            *out_response_code = ANJ_COAP_CODE_DELETED;
            break;
        default:
            dm_log(L_ERROR, "Operation not supported: %d",
                   (int) ctx->operation);
            ret_val = ANJ_DM_ERR_NOT_IMPLEMENTED;
            break;
        }
        uri_log(&request->uri);
    }
    if (ret_val) {
        *out_response_code = map_err_to_coap_code(ret_val);
        // error code is printed in exchange module
        dm_log(L_ERROR, "Operation initialization failed");
        _anj_dm_operation_end(anj, ANJ_DM_TRANSACTION_FAILURE);
    }
}

void _anj_dm_process_register_update_payload(
        anj_t *anj, _anj_exchange_handlers_t *out_handlers) {
    assert(anj && out_handlers);
    _anj_dm_data_model_t *ctx = &anj->dm;
    assert(!ctx->op_in_progress);
    ctx->data_to_copy = false;
    *out_handlers = (_anj_exchange_handlers_t) {
        .read_payload = _dm_read_payload,
        .completion = _dm_process_finalization,
        .arg = anj
    };
    // ignore the return value, as it is always 0 for register/update
    _anj_dm_operation_begin(anj, ANJ_OP_REGISTER, false, NULL);
    dm_log(L_DEBUG, "Register/update operation");
    _anj_io_register_ctx_init(&anj->anj_io.register_ctx);
}

void _anj_dm_observe_finalize_operation(anj_t *anj, int result) {
    assert(anj);
    _dm_process_finalization(anj, NULL, result);
}

#ifdef ANJ_WITH_OBSERVE
int _anj_dm_observe_is_any_resource_readable(anj_t *anj,
                                             const anj_uri_path_t *path) {
    assert(anj && path);
    int res = _anj_dm_path_has_readable_resources(&anj->dm, path);
    if (res) {
        return map_err_to_coap_code(res);
    }
    return 0;
}

int _anj_dm_observe_read_resource(anj_t *anj,
                                  anj_res_value_t *out_value,
                                  anj_data_type_t *out_type,
                                  bool *out_multi_res,
                                  const anj_uri_path_t *res_path) {
    assert(anj && res_path && (out_value || out_type)
           && anj_uri_path_has(res_path, ANJ_ID_RID));

    int res = _anj_dm_get_resource_value(anj, res_path, out_value, out_type,
                                         out_multi_res);
    if (res) {
        if (out_multi_res && *out_multi_res) {
            return 0;
        }
        return map_err_to_coap_code(res);
    }
    return 0;
}

#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
int _anj_dm_observe_build_msg(anj_t *anj,
                              const anj_uri_path_t *const *paths,
                              size_t uri_path_count,
                              size_t *already_processed,
                              uint8_t *out_buff,
                              size_t *out_len,
                              size_t buff_len,
                              uint16_t *inout_format,
                              bool composite) {
    assert(anj && paths && out_buff && out_len && buff_len && inout_format
           && already_processed);

    _anj_dm_data_model_t *dm = &anj->dm;

    int res = 0;
    _anj_op_t op = composite ? ANJ_OP_DM_READ_COMP : ANJ_OP_DM_READ;

    size_t path_res_count;
    if (!dm->op_in_progress) {
        size_t res_count = 0;

        res = _anj_dm_operation_begin(anj, op, false,
                                      composite ? NULL : paths[0]);
        if (res) {
            return map_err_to_coap_code(res);
        }
        for (size_t i = 0; i < uri_path_count; i++) {
            if (composite) {
                res = _anj_dm_count_readable_res_if_allowed(anj, paths[i],
                                                            &path_res_count);
                if (res) {
                    return map_err_to_coap_code(res);
                }
            } else {
                _anj_dm_get_readable_res_count(anj, &path_res_count);
            }
            if (path_res_count == 0 && anj_uri_path_has(paths[i], ANJ_ID_RID)) {
                return ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
            }

            res_count += path_res_count;
        }

        res = _anj_io_out_ctx_init(&anj->anj_io.out_ctx, op,
                                   composite ? &ANJ_MAKE_ROOT_PATH() : paths[0],
                                   res_count, *inout_format);
        if (res) {
            return map_anj_io_err_to_coap_code(res);
        }
        if (res_count == 0) {
            uri_path_count = 0;
        }
    }

    return read_composite(anj, paths, uri_path_count, true, already_processed,
                          out_buff, buff_len, out_len, inout_format);
}
#    else  // ANJ_WITH_OBSERVE_COMPOSITE
int _anj_dm_observe_build_msg(anj_t *anj,
                              const anj_uri_path_t *const *paths,
                              const size_t uri_path_count,
                              size_t *already_processed,
                              uint8_t *out_buff,
                              size_t *out_len,
                              size_t buff_len,
                              uint16_t *inout_format,
                              bool composite) {
    assert(anj && paths && out_buff && out_len && buff_len && inout_format
           && uri_path_count != 0 && already_processed);
    (void) composite;
    (void) uri_path_count;
    (void) already_processed;

    _anj_dm_data_model_t *dm = &anj->dm;

    int res = 0;

    if (!dm->op_in_progress) {
        size_t res_count = 0;

        res = _anj_dm_operation_begin(anj, ANJ_OP_DM_READ, false, paths[0]);
        if (res) {
            return map_err_to_coap_code(res);
        }

        _anj_dm_get_readable_res_count(anj, &res_count);

        if (res_count == 0 && anj_uri_path_has(paths[0], ANJ_ID_RID)) {
            return ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
        }

        res = _anj_io_out_ctx_init(&anj->anj_io.out_ctx, ANJ_OP_DM_READ,
                                   paths[0], res_count, *inout_format);
        if (res) {
            return map_anj_io_err_to_coap_code(res);
        }
    }

    *inout_format = _anj_io_out_ctx_get_format(&anj->anj_io.out_ctx);
    return process_read(anj, out_buff, buff_len, out_len, paths[0], false);
}
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
#endif     // ANJ_WITH_OBSERVE

#ifdef ANJ_WITH_BOOTSTRAP
int _anj_dm_bootstrap_validation(anj_t *anj) {
    assert(anj);
    _anj_dm_data_model_t *dm = &anj->dm;

    const anj_dm_obj_t *server_obj = _anj_dm_find_obj(dm, ANJ_OBJ_ID_SERVER);
    if (!server_obj) {
        return -1;
    }
    // after bootstrap, there must be at least one server instance
    uint16_t server_inst_count = _anj_dm_count_obj_insts(server_obj);
    if (server_inst_count == 0) {
        return -1;
    }
    const anj_dm_obj_t *security_obj =
            _anj_dm_find_obj(dm, ANJ_OBJ_ID_SECURITY);
    if (!security_obj) {
        return -1;
    }
    uint16_t sec_inst_count = _anj_dm_count_obj_insts(security_obj);

    for (size_t server_inst = 0; server_inst < server_inst_count;
         server_inst++) {
        anj_res_value_t server_ssid;
        if (anj_dm_res_read(
                    anj,
                    &ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER,
                                            server_obj->insts[server_inst].iid,
                                            _ANJ_DM_OBJ_SERVER_SSID_RID),
                    &server_ssid)) {
            return -1;
        }

        for (size_t security_inst = 0; security_inst < sec_inst_count;
             security_inst++) {
            anj_res_value_t security_ssid;
            if (!anj_dm_res_read(anj,
                                 &ANJ_MAKE_RESOURCE_PATH(
                                         ANJ_OBJ_ID_SECURITY,
                                         security_obj->insts[security_inst].iid,
                                         _ANJ_DM_OBJ_SECURITY_SSID_RID),
                                 &security_ssid)
                    && security_ssid.int_value == server_ssid.int_value) {
                // there is at least one non-bootstrap server Security Object
                // Instance that matches a Server Object Instance
                return 0;
            }
        }
    }
    // there is no SSID matching pair of server instance and security instance
    return -1;
}
#endif // ANJ_WITH_BOOTSTRAP

int _anj_dm_get_server_obj_instance_data(anj_t *anj,
                                         uint16_t *out_ssid,
                                         anj_iid_t *out_iid) {
    assert(anj && out_ssid && out_iid);

    _anj_dm_data_model_t *dm = &anj->dm;
    const anj_dm_obj_t *server_obj = _anj_dm_find_obj(dm, ANJ_OBJ_ID_SERVER);
    if (!server_obj || _anj_dm_count_obj_insts(server_obj) == 0) {
        dm_log(L_INFO, "Server Object Instance not found");
        *out_ssid = ANJ_ID_INVALID;
        *out_iid = ANJ_ID_INVALID;
        return 0;
    }
    anj_res_value_t server_ssid;
    if (anj_dm_res_read(anj,
                        &ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER,
                                                server_obj->insts[0].iid,
                                                _ANJ_DM_OBJ_SERVER_SSID_RID),
                        &server_ssid)) {
        dm_log(L_ERROR, "Failed to read Server Object Instance SSID");
        return -1;
    }
    *out_ssid = (uint16_t) server_ssid.int_value;
    *out_iid = server_obj->insts[0].iid;
    return 0;
}

int _anj_dm_get_security_obj_instance_iid(anj_t *anj,
                                          uint16_t ssid,
                                          anj_iid_t *out_iid) {
    assert(anj && out_iid);

    _anj_dm_data_model_t *dm = &anj->dm;
    const anj_dm_obj_t *security_obj =
            _anj_dm_find_obj(dm, ANJ_OBJ_ID_SECURITY);
    if (!security_obj || _anj_dm_count_obj_insts(security_obj) == 0) {
        dm_log(L_ERROR, "Security Object Instance not found");
        return -1;
    }
    uint16_t inst_count = _anj_dm_count_obj_insts(security_obj);
    for (uint16_t security_inst = 0; security_inst < inst_count;
         security_inst++) {
        anj_res_value_t res_value;
        anj_uri_path_t path =
                ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY,
                                       security_obj->insts[security_inst].iid,
                                       _ANJ_DM_OBJ_SECURITY_SSID_RID);
        // If SSID is _ANJ_SSID_BOOTSTRAP, it means that we are looking for a
        // Bootstrap Server
        if (ssid == _ANJ_SSID_BOOTSTRAP) {
            path.ids[ANJ_ID_RID] = _ANJ_DM_OBJ_SECURITY_BOOTSTRAP_SERVER_RID;
        }

        if (anj_dm_res_read(anj, &path, &res_value)) {
            dm_log(L_ERROR, "Failed to read Security Object Instance");
            return -1;
        }
        if ((ssid == _ANJ_SSID_BOOTSTRAP && res_value.bool_value)
                || (ssid != _ANJ_SSID_BOOTSTRAP
                    && (uint16_t) res_value.int_value == ssid)) {
            *out_iid = (anj_iid_t) security_obj->insts[security_inst].iid;
            return 0;
        }
    }
    dm_log(L_ERROR, "Security Object Instance with SSID %" PRIu16 " not found",
           ssid);
    return -1;
}
