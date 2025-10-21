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
#include <anj/log.h>
#include <anj/time.h>

#include "../dm/dm_integration.h"
#include "register.h"

#define register_log(...) anj_log(register, __VA_ARGS__)

#define REGISTER_INTERNAL_STATE_INIT 0
#define REGISTER_INTERNAL_STATE_REGISTERING 1
#define REGISTER_INTERNAL_STATE_DEREGISTERING 2
#define REGISTER_INTERNAL_STATE_UPDATING 3
#define REGISTER_INTERNAL_STATE_FINISHED 4
#define REGISTER_INTERNAL_STATE_ERROR 5

static uint8_t register_read_payload(void *arg_ptr,
                                     uint8_t *buff,
                                     size_t buff_len,
                                     _anj_exchange_read_result_t *out_params) {
    _anj_register_ctx_t *ctx = (_anj_register_ctx_t *) arg_ptr;
    assert(ctx->with_payload);
    return ctx->dm_handlers.read_payload(ctx->dm_handlers.arg, buff, buff_len,
                                         out_params);
}

static void request_completion_callback(void *arg_ptr,
                                        const _anj_coap_msg_t *response,
                                        int result) {
    _anj_register_ctx_t *ctx = (_anj_register_ctx_t *) arg_ptr;
    if (result != _ANJ_EXCHANGE_RESULT_SUCCESS) {
        register_log(L_ERROR, "Operation failed with result %d", result);
        ctx->internal_state = REGISTER_INTERNAL_STATE_ERROR;
    } else {
        if (ctx->internal_state == REGISTER_INTERNAL_STATE_REGISTERING) {
            assert(response->location_path.location_count
                   <= ANJ_COAP_MAX_LOCATION_PATHS_NUMBER);
            for (size_t i = 0; i < response->location_path.location_count;
                 i++) {
                if (response->location_path.location_len[i]
                        > ANJ_COAP_MAX_LOCATION_PATH_SIZE) {
                    ctx->internal_state = REGISTER_INTERNAL_STATE_ERROR;
                    register_log(L_ERROR, "Location path too long");
                    return;
                }
                memcpy(ctx->location_path[i],
                       response->location_path.location[i],
                       response->location_path.location_len[i]);
                ctx->location_path_len[i] =
                        response->location_path.location_len[i];
            }
            register_log(L_INFO, "Registered successfully");
        } else if (ctx->internal_state == REGISTER_INTERNAL_STATE_UPDATING) {
            register_log(L_INFO, "Updated successfully");
        } else if (ctx->internal_state
                   == REGISTER_INTERNAL_STATE_DEREGISTERING) {
            register_log(L_INFO, "De-registered successfully");
        }
        ctx->internal_state = REGISTER_INTERNAL_STATE_FINISHED;
    }
    if (ctx->with_payload) {
        ctx->dm_handlers.completion(ctx->dm_handlers.arg, response, result);
    }
}

static void write_location_paths(const _anj_register_ctx_t *ctx,
                                 _anj_location_path_t *paths) {
    for (paths->location_count = 0;
         paths->location_count < ANJ_COAP_MAX_LOCATION_PATHS_NUMBER;
         paths->location_count++) {
        if (ctx->location_path_len[paths->location_count] == 0) {
            break;
        }
        paths->location[paths->location_count] =
                ctx->location_path[paths->location_count];
        paths->location_len[paths->location_count] =
                ctx->location_path_len[paths->location_count];
    }
}

void _anj_register_ctx_init(anj_t *anj) {
    assert(anj);
    _anj_register_ctx_t *ctx = &anj->register_ctx;
    memset(ctx, 0, sizeof(*ctx));
    ctx->internal_state = REGISTER_INTERNAL_STATE_INIT;
}

void _anj_register_register(anj_t *anj,
                            const _anj_attr_register_t *attr,
                            _anj_coap_msg_t *out_msg,
                            _anj_exchange_handlers_t *out_handlers) {
    assert(anj && attr && out_msg && out_handlers);
    _anj_register_ctx_t *ctx = &anj->register_ctx;

    register_log(L_DEBUG, "Preparing Register request");
    memset(ctx->location_path_len, 0, sizeof(ctx->location_path_len));

    _anj_dm_process_register_update_payload(anj, &ctx->dm_handlers);
    *out_handlers = (_anj_exchange_handlers_t) {
        .completion = request_completion_callback,
        .read_payload = register_read_payload,
        .arg = ctx
    };
    ctx->with_payload = true;

    out_msg->attr.register_attr = *attr;
    out_msg->operation = ANJ_OP_REGISTER;
    ctx->internal_state = REGISTER_INTERNAL_STATE_REGISTERING;
}

void _anj_register_update(anj_t *anj,
                          const anj_time_duration_t *lifetime,
                          bool with_payload,
                          _anj_coap_msg_t *out_msg,
                          _anj_exchange_handlers_t *out_handlers) {
    assert(anj && out_msg && out_handlers);
    _anj_register_ctx_t *ctx = &anj->register_ctx;

    register_log(L_DEBUG, "Preparing Update request");
    if (lifetime) {
        out_msg->attr.register_attr.has_lifetime = true;
        out_msg->attr.register_attr.lifetime = *lifetime;
    }
    if (with_payload) {
        _anj_dm_process_register_update_payload(anj, &ctx->dm_handlers);
        *out_handlers = (_anj_exchange_handlers_t) {
            .completion = request_completion_callback,
            .read_payload = register_read_payload,
            .arg = ctx
        };
    } else {
        *out_handlers = (_anj_exchange_handlers_t) {
            .completion = request_completion_callback,
            .arg = ctx
        };
    }
    ctx->with_payload = with_payload;
    out_msg->operation = ANJ_OP_UPDATE;
    write_location_paths(ctx, &out_msg->location_path);
    ctx->internal_state = REGISTER_INTERNAL_STATE_UPDATING;
}

void _anj_register_deregister(anj_t *anj,
                              _anj_coap_msg_t *out_msg,
                              _anj_exchange_handlers_t *out_handlers) {
    assert(anj && out_msg && out_handlers);
    _anj_register_ctx_t *ctx = &anj->register_ctx;

    register_log(L_DEBUG, "Preparing De-register request");
    out_msg->operation = ANJ_OP_DEREGISTER;
    write_location_paths(ctx, &out_msg->location_path);
    *out_handlers = (_anj_exchange_handlers_t) {
        .completion = request_completion_callback,
        .arg = ctx
    };
    ctx->with_payload = false;
    ctx->internal_state = REGISTER_INTERNAL_STATE_DEREGISTERING;
}

int _anj_register_operation_status(anj_t *anj) {
    assert(anj);
    _anj_register_ctx_t *ctx = &anj->register_ctx;
    switch (ctx->internal_state) {
    case REGISTER_INTERNAL_STATE_INIT:
    case REGISTER_INTERNAL_STATE_ERROR:
        return _ANJ_REGISTER_OPERATION_ERROR;
    case REGISTER_INTERNAL_STATE_REGISTERING:
    case REGISTER_INTERNAL_STATE_DEREGISTERING:
    case REGISTER_INTERNAL_STATE_UPDATING:
        return _ANJ_REGISTER_OPERATION_IN_PROGRESS;
    default:
        return _ANJ_REGISTER_OPERATION_FINISHED;
    }
}
