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
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/log/log.h>
#include <anj/lwm2m_send.h>
#include <anj/utils.h>

#include "../coap/coap.h"
#include "../exchange.h"
#include "../io/io.h"
#include "core.h"
#include "core_utils.h"
#include "lwm2m_send.h"

#ifdef ANJ_WITH_LWM2M_SEND

int anj_send_new_request(anj_t *anj,
                         const anj_send_request_t *send_request,
                         uint16_t *out_send_id) {
    assert(anj && send_request);

    if (!send_request->finished_handler || !send_request->records
            || send_request->records_cnt == 0) {
        log(L_ERROR, "Invalid Send request");
        return ANJ_SEND_ERR_DATA_NOT_VALID;
    }
    for (size_t i = 0; i < send_request->records_cnt; i++) {
        if (!anj_uri_path_has(&send_request->records[i].path, ANJ_ID_RID)) {
            log(L_ERROR, "Invalid path");
            return ANJ_SEND_ERR_DATA_NOT_VALID;
        }
#    ifdef ANJ_WITH_LWM2M_CBOR
        if (send_request->content_format
                == ANJ_SEND_CONTENT_FORMAT_LWM2M_CBOR) {
            for (size_t j = i + 1; j < send_request->records_cnt; j++) {
                if (anj_uri_path_equal(&send_request->records[i].path,
                                       &send_request->records[j].path)) {
                    log(L_ERROR, "Duplicate path");
                    return ANJ_SEND_ERR_DATA_NOT_VALID;
                }
            }
        }
#    endif // ANJ_WITH_LWM2M_CBOR
    }
    if (!_anj_core_client_registered(anj)) {
        log(L_ERROR, "Client not registered");
        return ANJ_SEND_ERR_NOT_ALLOWED;
    }
    // check Mute Send resource
    if (anj->server_instance.mute_send) {
        log(L_ERROR, "Mute Send resource is set to true");
        return ANJ_SEND_ERR_NOT_ALLOWED;
    }

    // find free slot in the queue
    _anj_send_ctx_t *ctx = &anj->send_ctx;
    size_t idx;
    bool found = false;
    for (idx = 0; idx < ANJ_LWM2M_SEND_QUEUE_SIZE; idx++) {
        if (ctx->ids[idx] == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        log(L_ERROR, "Send queue is full");
        return ANJ_SEND_ERR_NO_SPACE;
    }

    ctx->send_id_counter++;
    if (ctx->send_id_counter == 0 || ctx->send_id_counter == ANJ_SEND_ID_ALL) {
        ctx->send_id_counter = 1;
    }
    ctx->ids[idx] = ctx->send_id_counter;
    if (out_send_id) {
        *out_send_id = ctx->ids[idx];
    }
    ctx->requests_queue[idx] = send_request;
    log(L_INFO, "New Send request registered with ID: %" PRIu16, ctx->ids[idx]);
    return 0;
}

int anj_send_abort(anj_t *anj, uint16_t send_id) {
    // send_id can't be 0
    assert(anj && send_id != 0);

    _anj_send_ctx_t *ctx = &anj->send_ctx;

    // If user calls anj_send_abort() in finished_handler(), it may cause
    // infinite recursion, we want to avoid that
    if (ctx->abort_in_progress) {
        log(L_ERROR, "Abort already in progress");
        return ANJ_SEND_ERR_ABORT;
    }
    if (!ctx->ids[0]) {
        // If the first element is empty then the whole Send queue empty,
        // nothing to do
        return 0;
    }
    // If the active send ID is the one we want to abort, terminate ongoing
    // exchange, if all requests are to be aborted, terminate the active
    // exchange only if it is Send request (active_exchange is set)
    if (ctx->active_exchange
            && (send_id == ANJ_SEND_ID_ALL || send_id == ctx->ids[0])) {
        // active exchange will be cleared in send_completion_callback
        _anj_exchange_terminate(&anj->exchange_ctx);
        log(L_INFO, "Aborted active Send request");
        // clear other requests if no specific ID is given
        if (send_id != ANJ_SEND_ID_ALL) {
            return 0;
        }
    }

    if (send_id == ANJ_SEND_ID_ALL) {
        ctx->abort_in_progress = true;
        for (size_t i = 0; i < ANJ_LWM2M_SEND_QUEUE_SIZE; i++) {
            if (ctx->ids[i] != 0) {
                ctx->requests_queue[i]->finished_handler(
                        anj,
                        ctx->ids[i],
                        ANJ_SEND_ERR_ABORT,
                        ctx->requests_queue[i]->data);
                ctx->ids[i] = 0;
            }
        }
        ctx->abort_in_progress = false;
        return 0;
    }

    // find the request with given ID
    void *user_data;
    anj_send_finished_handler_t *finished_handler = NULL;
    bool found = false;
    for (size_t i = 0; i < ANJ_LWM2M_SEND_QUEUE_SIZE; i++) {
        if (ctx->ids[i] == send_id) {
            user_data = ctx->requests_queue[i]->data;
            finished_handler = ctx->requests_queue[i]->finished_handler;
            found = true;
            ctx->ids[i] = 0;
        } else if (found) {
            ctx->requests_queue[i - 1] = ctx->requests_queue[i];
            ctx->ids[i - 1] = ctx->ids[i];
        }
    }
    if (!found) {
        log(L_ERROR, "No request with ID %" PRIu16 " found", send_id);
        return ANJ_SEND_ERR_NO_REQUEST_FOUND;
    }
    // clear the last one
    ctx->ids[ANJ_LWM2M_SEND_QUEUE_SIZE - 1] = 0;
    // call the finished handler
    finished_handler(anj, send_id, ANJ_SEND_ERR_ABORT, user_data);
    return 0;
}

static uint8_t send_read_payload(void *arg_ptr,
                                 uint8_t *buff,
                                 size_t buff_len,
                                 _anj_exchange_read_result_t *out_params) {
    anj_t *anj = (anj_t *) arg_ptr;
    _anj_send_ctx_t *ctx = &anj->send_ctx;
    assert(ctx->active_exchange);
    int res;
    size_t copied_bytes;

    out_params->format = _anj_io_out_ctx_get_format(&anj->anj_io.out_ctx);

    while (true) {
        if (!ctx->data_to_copy) {
            res = _anj_io_out_ctx_new_entry(
                    &anj->anj_io.out_ctx,
                    &ctx->requests_queue[0]->records[ctx->op_count++]);
            if (res) {
                log(L_ERROR, "anj_io out ctx error %d", res);
                return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
            }
        }
        res = _anj_io_out_ctx_get_payload(&anj->anj_io.out_ctx,
                                          &buff[out_params->payload_len],
                                          buff_len - out_params->payload_len,
                                          &copied_bytes);
        out_params->payload_len += copied_bytes;
        // last record copied
        if (res == 0 && ctx->op_count == ctx->requests_queue[0]->records_cnt) {
            return 0;
        }
        if (res == ANJ_IO_NEED_NEXT_CALL) {
            assert(out_params->payload_len == buff_len);
            ctx->data_to_copy = true;
            return _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
        } else if (res) {
            log(L_ERROR, "anj_io out ctx error %d", res);
            ctx->data_to_copy = false;
            return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
        }
        ctx->data_to_copy = false;
        if (buff_len == out_params->payload_len) {
            return _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
        }
    }
}

static void send_completion_callback(void *arg_ptr,
                                     const _anj_coap_msg_t *response,
                                     int result) {
    (void) response;
    anj_t *anj = (anj_t *) arg_ptr;
    _anj_send_ctx_t *ctx = &anj->send_ctx;
    assert(ctx->active_exchange);

    int send_result;
    if (!result) {
        log(L_INFO, "Send request completed successfully: %" PRIu16,
            ctx->ids[0]);
        send_result = ANJ_SEND_SUCCESS;
    } else if (result == _ANJ_EXCHANGE_ERROR_TERMINATED) {
        log(L_ERROR, "Send request terminated: %" PRIu16, ctx->ids[0]);
        send_result = ANJ_SEND_ERR_ABORT;
    } else if (result == _ANJ_EXCHANGE_ERROR_TIMEOUT) {
        log(L_DEBUG, "Send request timeout: %" PRIu16, ctx->ids[0]);
        send_result = ANJ_SEND_ERR_TIMEOUT;
    } else {
        log(L_ERROR,
            "Send request failed: %" PRIu16 " with %" PRIu8 " error code",
            ctx->ids[0], result);
        send_result = ANJ_SEND_ERR_REJECTED;
    }

#    ifdef ANJ_WITH_EXTERNAL_DATA
    if (ctx->op_count != 0) {
        const anj_io_out_entry_t *record =
                &ctx->requests_queue[0]->records[ctx->op_count - 1];
        if (result != 0 && (record->type & ANJ_DATA_TYPE_FLAG_EXTERNAL)
                && ctx->data_to_copy) {
            _anj_io_out_ctx_close_external_data_cb(record);
        }
    }
#    endif // ANJ_WITH_EXTERNAL_DATA

    void *user_data = ctx->requests_queue[0]->data;
    uint16_t send_id = ctx->ids[0];
    anj_send_finished_handler_t *finished_handler =
            ctx->requests_queue[0]->finished_handler;
    // first move all index and free the last one..
    for (size_t i = 1; i < ANJ_LWM2M_SEND_QUEUE_SIZE; i++) {
        ctx->requests_queue[i - 1] = ctx->requests_queue[i];
        ctx->ids[i - 1] = ctx->ids[i];
    }
    ctx->ids[ANJ_LWM2M_SEND_QUEUE_SIZE - 1] = 0;

    ctx->active_exchange = false;
    ctx->data_to_copy = false;
    ctx->op_count = 0;

    // ..then call the finished handler
    finished_handler(anj, send_id, send_result, user_data);
}

void _anj_lwm2m_send_process(anj_t *anj,
                             _anj_exchange_handlers_t *out_handlers,
                             _anj_coap_msg_t *out_msg) {
    assert(anj && out_handlers && out_msg);

    _anj_send_ctx_t *ctx = &anj->send_ctx;
    assert(!ctx->active_exchange);

    out_msg->operation = ANJ_OP_NONE;
    // there is no Send request to be sent
    if (ctx->ids[0] == 0) {
        return;
    }

    // if there are any other requests in the queue, and Mute Send resource
    // changed to true - abort all requests
    if (anj->server_instance.mute_send) {
        anj_send_abort(anj, ANJ_SEND_ID_ALL);
        return;
    }

#    if defined(ANJ_WITH_SENML_CBOR) && defined(ANJ_WITH_LWM2M_CBOR)
    uint16_t format = (ctx->requests_queue[0]->content_format
                       == ANJ_SEND_CONTENT_FORMAT_SENML_CBOR)
                              ? _ANJ_COAP_FORMAT_SENML_CBOR
                              : _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
#    elif defined(ANJ_WITH_SENML_CBOR)
    uint16_t format = _ANJ_COAP_FORMAT_SENML_CBOR;
#    elif defined(ANJ_WITH_LWM2M_CBOR)
    uint16_t format = _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR;
#    endif

    int res = _anj_io_out_ctx_init(&anj->anj_io.out_ctx, ANJ_OP_INF_CON_SEND,
                                   NULL, ctx->requests_queue[0]->records_cnt,
                                   format);
    if (res) {
        log(L_ERROR, "anj_io out ctx error %d", res);
        anj_send_abort(anj, ctx->ids[0]);
        return;
    }
    *out_handlers = (_anj_exchange_handlers_t) {
        .completion = send_completion_callback,
        .read_payload = send_read_payload,
        .arg = anj
    };
    out_msg->operation = ANJ_OP_INF_CON_SEND;
    ctx->active_exchange = true;
    ctx->data_to_copy = false;
    ctx->op_count = 0;
}

#endif // ANJ_WITH_LWM2M_SEND
