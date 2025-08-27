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

#include <anj/coap_downloader.h>
#include <anj/compat/net/anj_net_api.h>
#include <anj/compat/time.h>
#include <anj/defs.h>
#include <anj/log/log.h>

#include "coap/coap.h"
#include "core/core_utils.h"
#include "core/server.h"
#include "exchange.h"

#ifdef ANJ_WITH_COAP_DOWNLOADER

#    define downloader_log(...) anj_log(coap_downloader, __VA_ARGS__)

static int connect_with_server(anj_coap_downloader_t *ctx) {
    char hostname[ANJ_SERVER_URI_MAX_SIZE] = { 0 };
    char port[ANJ_U16_STR_MAX_LEN + 1] = { 0 };
    memcpy(hostname, ctx->host, ctx->host_len);
    memcpy(port, ctx->port, ctx->port_len);
    return _anj_server_connect(&ctx->connection_ctx,
                               ctx->binding,
                               &ctx->net_socket_cfg,
                               hostname,
                               port,
                               false);
}

static void exchange_completion(void *arg_ptr,
                                const _anj_coap_msg_t *response,
                                int result) {
    (void) response;
    anj_coap_downloader_t *ctx = (anj_coap_downloader_t *) arg_ptr;

    if (!result) {
        ctx->status = ANJ_COAP_DOWNLOADER_STATUS_FINISHED;
        downloader_log(L_DEBUG, "Download finished successfully");
        return;
    }
    downloader_log(L_ERROR, "Exchange failed with error: %d", result);
    if (ctx->error_code) {
        return; // already set by anj_coap_downloader_step
    }
    // For network error or termination a ctx->error_code is already set
    if (result == _ANJ_EXCHANGE_ERROR_TIMEOUT) {
        ctx->error_code = ANJ_COAP_DOWNLOADER_ERR_TIMEOUT;
    } else if (result) {
        ctx->error_code = ANJ_COAP_DOWNLOADER_ERR_INVALID_RESPONSE;
    }
}

static uint8_t exchange_write_payload(void *arg_ptr,
                                      uint8_t *payload,
                                      size_t payload_len,
                                      bool last_block) {
    (void) last_block;
    anj_coap_downloader_t *ctx = (anj_coap_downloader_t *) arg_ptr;
    ctx->event_cb(ctx->event_cb_arg, ctx,
                  ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING, payload, payload_len);
    return 0;
}

static int get_paths_from_uri(const char *uri, _anj_attr_downloader_t *attr) {
    attr->paths_count = 0;
    const char *path_start = strstr(uri, "://");
    if (!path_start) {
        // theoretically, this should never happen, because the URI is checked
        // in anj_coap_downloader_start, but let's be safe
        downloader_log(L_ERROR, "Invalid URI scheme");
        return ANJ_COAP_DOWNLOADER_ERR_INVALID_URI; // invalid scheme
    }
    path_start += 3; // skip "://"
    // find first '/' - this is the start of the path
    path_start = strchr(path_start, '/');
    if (!path_start) {
        return 0;
    }

    path_start++; // skip first '/', because it's not a segment but a separator
    const char *path_end = path_start;

    while (*path_end) {
        if (*path_end == '/') {
            if (attr->paths_count >= ANJ_COAP_DOWNLOADER_MAX_PATHS_NUMBER) {
                downloader_log(L_ERROR,
                               "ANJ_COAP_DOWNLOADER_MAX_PATHS_NUMBER exceeded");
                return ANJ_COAP_DOWNLOADER_ERR_INTERNAL;
            }
            attr->path[attr->paths_count] = path_start;
            attr->path_len[attr->paths_count++] =
                    (uint16_t) (path_end - path_start);
            path_start = path_end + 1;
        }
        path_end++;
    }

    // final segment (also empty if URI ends with '/')
    if (path_start <= path_end) {
        if (attr->paths_count >= ANJ_COAP_DOWNLOADER_MAX_PATHS_NUMBER) {
            downloader_log(L_ERROR,
                           "ANJ_COAP_DOWNLOADER_MAX_PATHS_NUMBER exceeded");
            return ANJ_COAP_DOWNLOADER_ERR_INTERNAL;
        }
        attr->path[attr->paths_count] = path_start;
        attr->path_len[attr->paths_count++] =
                (uint16_t) (path_end - path_start);
    }

    return 0;
}

static int start_new_exchange(anj_coap_downloader_t *ctx) {
    _anj_exchange_handlers_t handlers = {
        .read_payload = NULL,
        .completion = exchange_completion,
        .write_payload = exchange_write_payload,
        .arg = ctx,
    };

    _anj_coap_msg_t request;
    memset(&request, 0, sizeof(request));
    request.operation = ANJ_OP_COAP_DOWNLOADER_GET;
    int res = get_paths_from_uri(ctx->uri, &request.attr.downloader_attr);
    if (res) {
        return res;
    }

    // start new exchange, this function can't return error if no payload is get
    // payload pointer can be set to ctx->msg_buffer because it is not used
    _anj_exchange_new_client_request(&ctx->exchange_ctx, &request, &handlers,
                                     ctx->msg_buffer,
                                     ANJ_COAP_DOWNLOADER_MAX_MSG_SIZE);

    res = _anj_coap_encode_udp(&request, ctx->msg_buffer,
                               ANJ_COAP_DOWNLOADER_MAX_MSG_SIZE,
                               &ctx->out_msg_len);
    if (res) {
        _anj_exchange_terminate(&ctx->exchange_ctx);
        downloader_log(L_ERROR, "anj_coap_encode_udp failed: %d", res);
    }
    return res;
}

static int check_etag_mismatch(_anj_etag_t *etag,
                               const _anj_etag_t *etag_from_msg) {
    if (etag->size == 0) {
        // No ETag set, so no mismatch
        *etag = *etag_from_msg;
        return 0;
    }
    if (etag->size != etag_from_msg->size) {
        return ANJ_COAP_DOWNLOADER_ERR_ETAG_MISMATCH;
    }
    for (size_t i = 0; i < etag->size; i++) {
        if (etag->bytes[i] != etag_from_msg->bytes[i]) {
            return ANJ_COAP_DOWNLOADER_ERR_ETAG_MISMATCH;
        }
    }
    return 0;
}

static int handle_request(anj_coap_downloader_t *ctx) {
    int result = 0;
    _anj_exchange_state_t exchange_state =
            _anj_exchange_get_state(&ctx->exchange_ctx);
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    while (1) {
        if (exchange_state == ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION
                || exchange_state == ANJ_EXCHANGE_STATE_MSG_TO_SEND) {
            // For both cases we need to send a message but for new message we
            // also need to build CoAP message first.
            if (exchange_state == ANJ_EXCHANGE_STATE_MSG_TO_SEND) {
                result = _anj_coap_encode_udp(&msg, ctx->msg_buffer,
                                              ANJ_COAP_DOWNLOADER_MAX_MSG_SIZE,
                                              &ctx->out_msg_len);
                if (result) {
                    downloader_log(L_ERROR, "Failed to encode CoAP message: %d",
                                   result);
                    return ANJ_COAP_DOWNLOADER_ERR_INTERNAL;
                }
            }
            result = _anj_server_send(&ctx->connection_ctx, ctx->msg_buffer,
                                      ctx->out_msg_len);
            if (anj_net_is_again(result)) {
                // check for send ACK timeout, error suggests network issue
                exchange_state =
                        _anj_exchange_process(&ctx->exchange_ctx,
                                              ANJ_EXCHANGE_EVENT_NONE, &msg);
                if (exchange_state == ANJ_EXCHANGE_STATE_FINISHED) {
                    return ANJ_COAP_DOWNLOADER_ERR_NETWORK;
                }
                return result;
            } else if (result) {
                return ANJ_COAP_DOWNLOADER_ERR_NETWORK;
            }
            exchange_state =
                    _anj_exchange_process(&ctx->exchange_ctx,
                                          ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                          &msg);
        }

        if (exchange_state == ANJ_EXCHANGE_STATE_WAITING_MSG) {
            size_t msg_size;
            result = _anj_server_receive(&ctx->connection_ctx, ctx->msg_buffer,
                                         &msg_size,
                                         ANJ_COAP_DOWNLOADER_MAX_MSG_SIZE);
            if (anj_net_is_again(result)) {
                // check for receive timeout, if occurred, it will be set in
                // completion callback
                exchange_state =
                        _anj_exchange_process(&ctx->exchange_ctx,
                                              ANJ_EXCHANGE_EVENT_NONE, &msg);
                if (exchange_state == ANJ_EXCHANGE_STATE_WAITING_MSG) {
                    // we're still waiting for a message
                    return result;
                }
            } else if (result) {
                return ANJ_COAP_DOWNLOADER_ERR_NETWORK;
            } else {
                result = _anj_coap_decode_udp(ctx->msg_buffer, msg_size, &msg);
                if (result) {
                    downloader_log(L_ERROR, "Failed to decode CoAP message: %d",
                                   result);
                    return ANJ_COAP_DOWNLOADER_ERR_INTERNAL;
                } else {
                    if (msg.attr.downloader_attr.total_size != 0) {
                        downloader_log(L_INFO,
                                       "Total resource size: %" PRIu32 " bytes",
                                       msg.attr.downloader_attr.total_size);
                    }
                    // check ETag mismatch only for responses that are not
                    // error responses or reset messages
                    if (msg.operation == ANJ_OP_RESPONSE
                            && msg.msg_code < ANJ_COAP_CODE_BAD_REQUEST
                            && check_etag_mismatch(
                                       &ctx->etag,
                                       &msg.attr.downloader_attr.etag)) {
                        downloader_log(L_ERROR, "ETag mismatch");
                        return ANJ_COAP_DOWNLOADER_ERR_ETAG_MISMATCH;
                    }
                    exchange_state =
                            _anj_exchange_process(&ctx->exchange_ctx,
                                                  ANJ_EXCHANGE_EVENT_NEW_MSG,
                                                  &msg);
                }
            }
        }

        // exchange finished, but operation status is not checked here
        if (exchange_state == ANJ_EXCHANGE_STATE_FINISHED) {
            return 0;
        }
    }
}

static void handle_event_cb(anj_coap_downloader_t *ctx,
                            anj_coap_downloader_status_t current_status) {
    if (ctx->last_reported_status != current_status) {
        ctx->event_cb(ctx->event_cb_arg, ctx, current_status, NULL, 0);
        ctx->last_reported_status = current_status;
    }
}

void anj_coap_downloader_step(anj_coap_downloader_t *ctx) {
    assert(ctx);

    switch (ctx->status) {
    case ANJ_COAP_DOWNLOADER_STATUS_STARTING: {
        handle_event_cb(ctx, ANJ_COAP_DOWNLOADER_STATUS_STARTING);
        int result = connect_with_server(ctx);
        if (anj_net_is_again(result)) {
            break;
        }
        if (!anj_net_is_ok(result)) {
            ctx->status = ANJ_COAP_DOWNLOADER_STATUS_FINISHING;
            ctx->error_code = ANJ_COAP_DOWNLOADER_ERR_NETWORK;
            downloader_log(L_ERROR, "Failed to connect to server: %d", result);
            break;
        }
        downloader_log(L_DEBUG, "Connected to server");
        result = start_new_exchange(ctx);
        if (result) {
            ctx->status = ANJ_COAP_DOWNLOADER_STATUS_FINISHING;
            ctx->error_code = result;
            break;
        }
        ctx->status = ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING;
        break;
    }
    case ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING: {
        int result = handle_request(ctx);
        if (anj_net_is_again(result)) {
            break;
        }
        if (result) {
            ctx->error_code = result;
            downloader_log(L_ERROR, "Download failed with error: %d", result);
            _anj_exchange_terminate(&ctx->exchange_ctx);
        }
        ctx->status = ANJ_COAP_DOWNLOADER_STATUS_FINISHING;
        break;
    }
    case ANJ_COAP_DOWNLOADER_STATUS_FINISHING: {
        handle_event_cb(ctx, ANJ_COAP_DOWNLOADER_STATUS_FINISHING);
        int result = _anj_server_close(&ctx->connection_ctx, true);
        if (anj_net_is_again(result)) {
            break;
        }
        // set ctx->error_code if it is not set yet
        if (result && ctx->error_code == 0) {
            ctx->error_code = ANJ_COAP_DOWNLOADER_ERR_NETWORK;
            downloader_log(L_ERROR, "Socket closed with error: %d", result);
        }
        if (ctx->error_code) {
            ctx->status = ANJ_COAP_DOWNLOADER_STATUS_FAILED;
        } else {
            ctx->status = ANJ_COAP_DOWNLOADER_STATUS_FINISHED;
            downloader_log(L_DEBUG, "Download finished successfully");
        }
        break;
    }
    default:
        handle_event_cb(ctx, ctx->status);
        break;
    }
}

int anj_coap_downloader_init(
        anj_coap_downloader_t *ctx,
        const anj_coap_downloader_configuration_t *config) {
    assert(ctx);
    assert(config);

    downloader_log(L_DEBUG, "Initializing CoAP downloader");

    memset(ctx, 0, sizeof(*ctx));

    if (config->event_cb == NULL) {
        downloader_log(L_ERROR, "Status callback is not set");
        return ANJ_COAP_DOWNLOADER_ERR_INVALID_CONFIGURATION;
    }
    ctx->event_cb = config->event_cb;
    ctx->event_cb_arg = config->event_cb_arg;

    if (config->net_socket_cfg) {
        ctx->net_socket_cfg = *config->net_socket_cfg;
    }
    _anj_exchange_init(&ctx->exchange_ctx, (unsigned int) anj_time_real_now());
    if (config->udp_tx_params) {
        _anj_exchange_set_udp_tx_params(&ctx->exchange_ctx,
                                        config->udp_tx_params);
    }

    ctx->status = ANJ_COAP_DOWNLOADER_STATUS_INITIAL;
    ctx->last_reported_status = ANJ_COAP_DOWNLOADER_STATUS_INITIAL;
    return 0;
}

int anj_coap_downloader_get_error(anj_coap_downloader_t *ctx) {
    assert(ctx);
    return ctx->error_code;
}

int anj_coap_downloader_start(anj_coap_downloader_t *ctx, const char *uri) {
    assert(ctx);
    assert(uri);
    if (ctx->status == ANJ_COAP_DOWNLOADER_STATUS_STARTING
            || ctx->status == ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING
            || ctx->status == ANJ_COAP_DOWNLOADER_STATUS_FINISHING) {
        downloader_log(L_ERROR, "Download already in progress");
        return ANJ_COAP_DOWNLOADER_ERR_IN_PROGRESS;
    }

    downloader_log(L_INFO, "Starting CoAP download from %s", uri);

    _anj_uri_components_t uri_components;
    if (_anj_parse_uri_components(uri, false, &uri_components)) {
        downloader_log(L_ERROR, "Invalid URI scheme");
        return ANJ_COAP_DOWNLOADER_ERR_INVALID_URI;
    }
    ctx->binding = uri_components.binding_type;
    ctx->host = uri_components.host;
    ctx->host_len = uri_components.host_len;
    ctx->port = uri_components.port;
    ctx->port_len = uri_components.port_len;

    ctx->uri = uri;
    ctx->error_code = 0;
    ctx->status = ANJ_COAP_DOWNLOADER_STATUS_STARTING;
    ctx->etag.size = 0;
    return 0;
}

void anj_coap_downloader_terminate(anj_coap_downloader_t *ctx) {
    assert(ctx);

    if (ctx->status != ANJ_COAP_DOWNLOADER_STATUS_STARTING
            && ctx->status != ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING) {
        downloader_log(L_DEBUG, "No download in progress");
        return;
    }
    downloader_log(L_INFO, "Terminating CoAP download");
    ctx->error_code = ANJ_COAP_DOWNLOADER_ERR_TERMINATED;
    ctx->status = ANJ_COAP_DOWNLOADER_STATUS_FINISHING;
    _anj_exchange_terminate(&ctx->exchange_ctx);
}

#endif // ANJ_WITH_COAP_DOWNLOADER
