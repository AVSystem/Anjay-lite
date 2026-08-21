/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "../init_internal.h"

#define ANJ_LOG_SOURCE_FILE_ID 7

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/defs.h>
#include <anj/log.h>
#include <anj/time.h>

#include "../coap/coap.h"
#include "../dm/dm_integration.h"
#include "bootstrap.h"

#ifdef ANJ_WITH_BOOTSTRAP

#    define bootstrap_log(...) anj_log(bootstrap, __VA_ARGS__)

static void bootstrap_finish_completion_callback(
        void *arg_ptr, const _anj_coap_msg_t *response, int result) {
    (void) response;
    _anj_bootstrap_ctx_t *ctx = (_anj_bootstrap_ctx_t *) arg_ptr;
    if (result != _ANJ_EXCHANGE_RESULT_SUCCESS) {
        if (ctx->error_code != _ANJ_BOOTSTRAP_ERR_DATA_MODEL) {
            ctx->error_code = _ANJ_BOOTSTRAP_ERR_EXCHANGE_ERROR;
        }
        ctx->bootstrap_finish_handled = true;
        bootstrap_log(L_ERROR, "Bootstrap-Finish failed with result %d",
                      result);
        return;
    }
    ctx->bootstrap_finish_handled = true;
    bootstrap_log(L_TRACE, "Bootstrap-Finish response sent");
}

static void bootstrap_request_completion_callback(
        void *arg_ptr, const _anj_coap_msg_t *response, int result) {
    (void) response;
    _anj_bootstrap_ctx_t *ctx = (_anj_bootstrap_ctx_t *) arg_ptr;

    if (result != _ANJ_EXCHANGE_RESULT_SUCCESS) {
        ctx->error_code = _ANJ_BOOTSTRAP_ERR_EXCHANGE_ERROR;
        bootstrap_log(L_ERROR, "Bootstrap-Request failed with result %d",
                      result);
    }
    bootstrap_log(L_INFO, "Bootstrap-Request sent");
}

static void prepare_bootstrap_request(_anj_coap_msg_t *msg,
                                      const char *endpoint) {
    msg->operation = _ANJ_OP_BOOTSTRAP_REQ;
    msg->attr.bootstrap_attr.has_preferred_content_format = true;
    // One from SenML CBOR or LwM2M CBOR must be enabled
    msg->attr.bootstrap_attr.preferred_content_format =
#    ifdef ANJ_WITH_SENML_CBOR
            _ANJ_COAP_FORMAT_SENML_CBOR;
#    else
            _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
#    endif // ANJ_WITH_SENML_CBOR;
    msg->attr.bootstrap_attr.has_endpoint = true;
    msg->attr.bootstrap_attr.endpoint = endpoint;
}

int _anj_bootstrap_process(anj_t *anj,
                           _anj_coap_msg_t *out_msg,
                           _anj_exchange_handlers_t *out_handlers) {
    assert(anj && out_msg && out_handlers);
    _anj_bootstrap_ctx_t *ctx = &anj->bootstrap_ctx;

    if (!ctx->in_progress) {
        ctx->in_progress = true;
        ctx->bootstrap_finish_handled = false;
        ctx->error_code = _ANJ_BOOTSTRAP_IN_PROGRESS;
        ctx->bootstrap_finish_timeout =
                anj_time_monotonic_add(anj_time_monotonic_now(),
                                       ctx->bootstrap_lifetime);

        *out_handlers = (_anj_exchange_handlers_t) {
            .completion = bootstrap_request_completion_callback,
            .arg = ctx
        };
        prepare_bootstrap_request(out_msg, ctx->endpoint);
        bootstrap_log(L_INFO, "Bootstrap sequence started");
        return _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND;
    } // return validation error after bootstrap-finish handling
    else if ((ctx->bootstrap_finish_handled
              && ctx->error_code == _ANJ_BOOTSTRAP_ERR_DATA_MODEL)
             || ctx->error_code == _ANJ_BOOTSTRAP_ERR_EXCHANGE_ERROR
             || ctx->error_code == _ANJ_BOOTSTRAP_ERR_NETWORK) {
        ctx->in_progress = false;
        _anj_dm_bootstrap_finalize(anj, ANJ_DM_TRANSACTION_FAILURE);
        return ctx->error_code;
    } else if (ctx->bootstrap_finish_handled) {
        ctx->in_progress = false;
        bootstrap_log(L_INFO, "Bootstrap finished successfully");
        _anj_dm_bootstrap_finalize(anj, ANJ_DM_TRANSACTION_SUCCESS);
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        if (anj->security_credential_handlers
                && anj->security_credential_handlers->offload_keys_and_certs) {
            if (anj->security_credential_handlers->offload_keys_and_certs(
                        anj)) {
                bootstrap_log(L_ERROR, "Offloading keys failed");
            }
        } else {
            bootstrap_log(L_WARNING, "Callback for offloading keys is not set");
        }
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        return _ANJ_BOOTSTRAP_FINISHED;
    } else if (anj_time_monotonic_gt(anj_time_monotonic_now(),
                                     ctx->bootstrap_finish_timeout)) {
        ctx->in_progress = false;
        ctx->error_code = _ANJ_BOOTSTRAP_ERR_BOOTSTRAP_TIMEOUT;
        bootstrap_log(L_ERROR, "Bootstrap timeout");
        _anj_dm_bootstrap_finalize(anj, ANJ_DM_TRANSACTION_FAILURE);
        return ctx->error_code;
    }
    return _ANJ_BOOTSTRAP_IN_PROGRESS;
}

void _anj_bootstrap_finish_request(anj_t *anj,
                                   uint8_t *out_response_code,
                                   _anj_exchange_handlers_t *out_handlers) {
    assert(anj && out_response_code && out_handlers);
    _anj_bootstrap_ctx_t *ctx = &anj->bootstrap_ctx;
    bootstrap_log(L_INFO, "Bootstrap-Finish received");
    if (!ctx->in_progress) {
        *out_response_code = ANJ_COAP_CODE_NOT_ACCEPTABLE;
        bootstrap_log(L_ERROR, "Bootstrap-Finish received out of order");
        return;
    }

    if (ctx->error_code == _ANJ_BOOTSTRAP_IN_PROGRESS) {
        if (_anj_dm_bootstrap_validation(anj)) {
            bootstrap_log(L_ERROR, "Bootstrap data model validation failed");
            ctx->error_code = _ANJ_BOOTSTRAP_ERR_DATA_MODEL;
            *out_response_code = ANJ_COAP_CODE_NOT_ACCEPTABLE;
        }
    }

    if (!ctx->error_code) {
        *out_response_code = ANJ_COAP_CODE_CHANGED;
    }

    *out_handlers = (_anj_exchange_handlers_t) {
        .completion = bootstrap_finish_completion_callback,
        .arg = ctx
    };
}

void _anj_bootstrap_connection_lost(anj_t *anj) {
    assert(anj);
    anj->bootstrap_ctx.error_code = _ANJ_BOOTSTRAP_ERR_NETWORK;
    bootstrap_log(L_ERROR, "Connection lost");
}

void _anj_bootstrap_timeout_reset(anj_t *anj) {
    assert(anj);
    _anj_bootstrap_ctx_t *ctx = &anj->bootstrap_ctx;
    assert(ctx->in_progress);
    ctx->bootstrap_finish_timeout =
            anj_time_monotonic_add(anj_time_monotonic_now(),
                                   ctx->bootstrap_lifetime);
}

void _anj_bootstrap_ctx_init(anj_t *anj,
                             const char *endpoint,
                             const anj_time_duration_t lifetime) {
    assert(anj && endpoint);
    _anj_bootstrap_ctx_t *ctx = &anj->bootstrap_ctx;
    memset(ctx, 0, sizeof(*ctx));
    ctx->in_progress = false;
    ctx->endpoint = endpoint;
    ctx->bootstrap_lifetime = lifetime;
}

void _anj_bootstrap_reset(anj_t *anj) {
    assert(anj);
    _anj_bootstrap_ctx_t *ctx = &anj->bootstrap_ctx;
    ctx->in_progress = false;
}

#endif // ANJ_WITH_BOOTSTRAP
