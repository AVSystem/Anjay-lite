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

#include <anj/compat/net/anj_net_api.h>
#include <anj/compat/net/anj_net_wrapper.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/log.h>
#include <anj/time.h>
#include <anj/utils.h>

#include "../coap/coap.h"
#include "../exchange.h"
#include "../exchange_cache.h"
#include "core_utils.h"
#include "server.h"

#define _ANJ_SERVER_MINIMAL_BLOCK_SIZE 16
#define _ANJ_SERVER_GENERIC_ERROR -1

static int net_again_is_error(int result) {
    return ((result == ANJ_NET_EAGAIN) || result == ANJ_NET_EINPROGRESS)
                   ? _ANJ_SERVER_GENERIC_ERROR
                   : result;
}

int _anj_server_connect(_anj_server_connection_ctx_t *ctx,
                        anj_net_binding_type_t type,
                        const anj_net_config_t *net_socket_cfg,
                        const char *hostname,
                        const char *port) {
    assert(ctx && hostname && port && !ctx->send_in_progress);
    int result;

    if (!ctx->net_ctx) {
        memset(ctx, 0, sizeof(*ctx));
        result = anj_net_create_ctx(type, &ctx->net_ctx, net_socket_cfg);
        if (!anj_net_is_ok(result)) {
            log(L_ERROR, "Could not create socket: %d", result);
            return result;
        }
        log(L_DEBUG, "Socket created successfully");
    }
    ctx->type = type;

    anj_net_socket_state_t state;
    result = anj_net_get_state(ctx->type, ctx->net_ctx, &state);
    if (!anj_net_is_ok(result)) {
        log(L_ERROR, "Could not get socket state: %d", result);
        return net_again_is_error(result);
    }

    result = anj_net_connect(ctx->type, ctx->net_ctx, hostname, port);
    if (anj_net_is_ok(result)) {
        result = anj_net_get_inner_mtu(ctx->type, ctx->net_ctx, &ctx->mtu);
        if (anj_net_is_ok(result) && ctx->mtu <= 0) {
            log(L_ERROR, "Invalid MTU");
            return _ANJ_SERVER_GENERIC_ERROR;
        }
        if (!anj_net_is_ok(result)) {
            log(L_ERROR, "Could not get MTU: %d", result);
            return net_again_is_error(result);
        }
        log(L_INFO, "Connected to %s:%s", hostname, port);
    } else if (!anj_net_is_inprogress(result)) {
        log(L_ERROR, "Connection failed: %d", result);
    }
    return result;
}

int _anj_server_close(_anj_server_connection_ctx_t *ctx, bool cleanup) {
    assert(ctx);
    ctx->bytes_sent = 0;
    ctx->send_in_progress = false;

    if (!ctx->net_ctx) {
        // nothing to do, net_ctx was not created or already cleaned up
        return ANJ_NET_OK;
    }

    int result = ANJ_NET_OK;
    // in case of anj_net_get_state failure, call anj_net_shutdown anyway
    anj_net_socket_state_t state = ANJ_NET_SOCKET_STATE_CONNECTED;
    anj_net_get_state(ctx->type, ctx->net_ctx, &state);

    if (state == ANJ_NET_SOCKET_STATE_CONNECTED) {
        result = anj_net_shutdown(ctx->type, ctx->net_ctx);
        if (anj_net_is_inprogress(result)) {
            return result;
        }
        log(L_TRACE, "Socket shutdown");
    }

    anj_net_get_state(ctx->type, ctx->net_ctx, &state);
    if (state == ANJ_NET_SOCKET_STATE_SHUTDOWN
            // connection might not be open yet, but we still need to clean it
            || (state == ANJ_NET_SOCKET_STATE_CLOSED && cleanup)) {
        result = cleanup ? anj_net_cleanup_ctx(ctx->type, &ctx->net_ctx)
                         : anj_net_close(ctx->type, ctx->net_ctx);
        if (anj_net_is_inprogress(result)) {
            return result;
        }
        if (cleanup) {
            assert(ctx->net_ctx == NULL);
            memset(ctx, 0, sizeof(*ctx));
        }
        if (anj_net_is_ok(result)) {
            log(L_INFO, "Connection closed");
        } else {
            log(L_WARNING, "Connection closed with error %d", result);
        }
    }
    return result;
}

int _anj_server_send(_anj_server_connection_ctx_t *ctx,
                     const uint8_t *buffer,
                     size_t length) {
    assert(ctx && ctx->net_ctx);
    size_t consumed_bytes;
    ctx->send_in_progress = true;
    int result =
            anj_net_send(ctx->type, ctx->net_ctx, &consumed_bytes,
                         &buffer[ctx->bytes_sent], length - ctx->bytes_sent);
    if (anj_net_is_ok(result)) {
        log(L_TRACE, "Sent %zu bytes", consumed_bytes);
        ctx->bytes_sent += consumed_bytes;
        if (ctx->bytes_sent == length) {
            // next send will be a new message
            ctx->bytes_sent = 0;
            ctx->send_in_progress = false;
        } else {
            return ANJ_NET_EINPROGRESS;
        }
        assert(ctx->bytes_sent <= length);
    } else if (!anj_net_is_inprogress(result)) {
        ctx->send_in_progress = false;
    }
    return result;
}

int _anj_server_receive(_anj_server_connection_ctx_t *ctx,
                        uint8_t *buffer,
                        size_t *out_length,
                        size_t length) {
    assert(ctx && ctx->net_ctx && !ctx->send_in_progress);
    size_t bytes_received;

    int result = anj_net_recv(ctx->type, ctx->net_ctx, &bytes_received, buffer,
                              length);
    if (anj_net_is_ok(result)) {
        *out_length = bytes_received;
        log(L_TRACE, "Received %zu bytes", bytes_received);
    } else {
        *out_length = 0;
    }

    if (result == ANJ_NET_EMSGSIZE) {
        log(L_ERROR, "Message too long, dropping");
        // this error does not require connection reset
        result = ANJ_NET_EAGAIN;
    }
    return result;
}

int _anj_server_calculate_max_payload_size(_anj_server_connection_ctx_t *ctx,
                                           _anj_coap_msg_t *msg,
                                           size_t payload_buff_size,
                                           size_t out_msg_buffer_size,
                                           bool server_request,
                                           size_t *out_payload_size) {
    assert(ctx && ctx->net_ctx && msg && payload_buff_size > 0
           && out_msg_buffer_size > 0);

    size_t max_msg_size = ANJ_MIN(out_msg_buffer_size, (size_t) ctx->mtu);
    size_t header_max_size =
            server_request ? _ANJ_COAP_UDP_RESPONSE_MSG_HEADER_MAX_SIZE
                           : _anj_coap_calculate_msg_header_max_size(msg);
    if (header_max_size > max_msg_size) {
        log(L_ERROR, "Buffer too small for message");
        return _ANJ_SERVER_GENERIC_ERROR;
    }
    size_t max_payload_size =
            ANJ_MIN((max_msg_size - header_max_size), payload_buff_size);
    if (max_payload_size < _ANJ_SERVER_MINIMAL_BLOCK_SIZE) {
        log(L_ERROR, "Buffer too small for payload");
        return _ANJ_SERVER_GENERIC_ERROR;
    }
    *out_payload_size = max_payload_size;
    return 0;
}

// For the first _anj_server_handle_request() call, _anj_exchange_get_state()
// always returns ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION even though
// message is not sent yet (check exchange.h API documentation).
int _anj_server_handle_request(anj_t *anj) {
    int result = 0;
    _anj_exchange_state_t exchange_state =
            _anj_exchange_get_state(&anj->exchange_ctx);
    _anj_coap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    while (1) {
#ifdef ANJ_WITH_CACHE
        if (anj->exchange_cache.handling_retransmission) {
            _anj_exchange_cache_get(&anj->exchange_cache, &msg);
            result = _anj_coap_encode_udp(&msg,
                                          anj->out_buffer,
                                          ANJ_OUT_MSG_BUFFER_SIZE,
                                          &anj->out_msg_len);
            if (result) {
                ANJ_CORE_LOG_COAP_ERROR(result);
                // If something goes wrong then just drop retransmitted request
                anj->exchange_cache.handling_retransmission = false;
                continue;
            }
            result = _anj_server_send(&anj->connection_ctx, anj->out_buffer,
                                      anj->out_msg_len);

            if (anj_net_is_inprogress(result)) {
                return result;
            }
            anj->exchange_cache.handling_retransmission = false;
            if (result) {
                _anj_exchange_terminate(&anj->exchange_ctx,
                                        _ANJ_EXCHANGE_ERROR_NETWORK);
                return result;
            }
        } else
#endif // ANJ_WITH_CACHE
                if (exchange_state
                            == ANJ_EXCHANGE_STATE_WAITING_SEND_CONFIRMATION
                    || exchange_state == ANJ_EXCHANGE_STATE_MSG_TO_SEND) {
            // For both cases we need to send a message but for new message we
            // also need to build CoAP message first.
            if (exchange_state == ANJ_EXCHANGE_STATE_MSG_TO_SEND) {
                result = _anj_coap_encode_udp(&msg,
                                              anj->out_buffer,
                                              ANJ_OUT_MSG_BUFFER_SIZE,
                                              &anj->out_msg_len);
                if (result) {
                    ANJ_CORE_LOG_COAP_ERROR(result);
                    _anj_exchange_terminate(&anj->exchange_ctx,
                                            _ANJ_EXCHANGE_ERROR_PROTOCOL);
                    return result;
                }
            }
            result = _anj_server_send(&anj->connection_ctx, anj->out_buffer,
                                      anj->out_msg_len);
            if (anj_net_is_inprogress(result)) {
                // check for send ACK timeout, error suggests network issue
                exchange_state =
                        _anj_exchange_process(&anj->exchange_ctx,
                                              ANJ_EXCHANGE_EVENT_NONE, &msg);
                if (exchange_state == ANJ_EXCHANGE_STATE_FINISHED) {
                    return -1;
                }
                return result;
            } else if (result) {
                _anj_exchange_terminate(&anj->exchange_ctx,
                                        _ANJ_EXCHANGE_ERROR_NETWORK);
                return result;
            }
            exchange_state =
                    _anj_exchange_process(&anj->exchange_ctx,
                                          ANJ_EXCHANGE_EVENT_SEND_CONFIRMATION,
                                          &msg);
        }

        if (exchange_state == ANJ_EXCHANGE_STATE_WAITING_MSG) {
            size_t msg_size;
            result = _anj_server_receive(&anj->connection_ctx, anj->in_buffer,
                                         &msg_size, ANJ_IN_MSG_BUFFER_SIZE);
            if (anj_net_is_again(result) || anj_net_is_inprogress(result)) {
                // check for receive timeout
                exchange_state =
                        _anj_exchange_process(&anj->exchange_ctx,
                                              ANJ_EXCHANGE_EVENT_NONE, &msg);
                if (exchange_state == ANJ_EXCHANGE_STATE_WAITING_MSG) {
                    // we're still waiting for a message
                    return result;
                }
            } else if (result) {
                _anj_exchange_terminate(&anj->exchange_ctx,
                                        _ANJ_EXCHANGE_ERROR_NETWORK);
                return result;
            } else {
                result = _anj_coap_decode_udp(anj->in_buffer, msg_size, &msg);
                if (result) {
                    ANJ_CORE_LOG_COAP_ERROR(result);
                    // drop message and continue waiting
                } else {
#ifdef ANJ_WITH_CACHE
                    // check if it isn't a retransmission
                    if (_anj_exchange_cache_check(
                                &anj->exchange_cache,
                                msg.coap_binding_data.udp.message_id)
                            == _ANJ_EXCHANGE_CACHE_MISS)
#endif // ANJ_WITH_CACHE
                    {
                        exchange_state = _anj_exchange_process(
                                &anj->exchange_ctx,
                                ANJ_EXCHANGE_EVENT_NEW_MSG,
                                &msg);
                    }
                }
            }
        }

        // exchange finished, but operation status is not checked here
        if (exchange_state == ANJ_EXCHANGE_STATE_FINISHED) {
            // exchange might be terminated during _anj_server_send, clear
            // related variables
            anj->connection_ctx.bytes_sent = 0;
            anj->connection_ctx.send_in_progress = false;
            return 0;
        }
    }
}

static int encode_coap_msg(anj_t *anj, _anj_coap_msg_t *msg) {
    int res = _anj_coap_encode_udp(msg, anj->out_buffer,
                                   ANJ_OUT_MSG_BUFFER_SIZE, &anj->out_msg_len);
    if (res) {
        _anj_exchange_terminate(&anj->exchange_ctx,
                                _ANJ_EXCHANGE_ERROR_PROTOCOL);
        ANJ_CORE_LOG_COAP_ERROR(res);
    }
    return res;
}

int _anj_server_prepare_client_request(anj_t *anj,
                                       _anj_coap_msg_t *new_request,
                                       _anj_exchange_handlers_t *handlers) {
    size_t payload_size;
    if (_anj_server_calculate_max_payload_size(&anj->connection_ctx,
                                               new_request,
                                               ANJ_OUT_PAYLOAD_BUFFER_SIZE,
                                               ANJ_OUT_MSG_BUFFER_SIZE,
                                               false,
                                               &payload_size)) {
        return -1;
    }
    if (_anj_exchange_new_client_request(&anj->exchange_ctx, new_request,
                                         handlers, anj->payload_buffer,
                                         payload_size)
            != ANJ_EXCHANGE_STATE_MSG_TO_SEND) {
        return -1;
    }
    return encode_coap_msg(anj, new_request);
}

int _anj_server_prepare_server_request(anj_t *anj,
                                       _anj_coap_msg_t *request,
                                       uint8_t response_code,
                                       _anj_exchange_handlers_t *handlers) {
    size_t payload_size;
    if (_anj_server_calculate_max_payload_size(&anj->connection_ctx,
                                               request,
                                               ANJ_OUT_PAYLOAD_BUFFER_SIZE,
                                               ANJ_OUT_MSG_BUFFER_SIZE,
                                               true,
                                               &payload_size)) {
        return -1;
    }
    _anj_exchange_state_t state =
            _anj_exchange_new_server_request(&anj->exchange_ctx,
                                             response_code,
                                             request,
                                             handlers,
                                             anj->payload_buffer,
                                             payload_size);
    // _anj_exchange_new_server_request can't return different state
    assert(state == ANJ_EXCHANGE_STATE_MSG_TO_SEND);
    (void) state;
    return encode_coap_msg(anj, request);
}

anj_time_duration_t _anj_server_calculate_max_transmit_wait(
        const anj_exchange_udp_tx_params_t *params) {
    // TODO: add TCP support
    // MAX_TRANSMIT_WAIT = ACK_TIMEOUT * ((2 ** (MAX_RETRANSMIT + 1)) - 1) *
    //                     ACK_RANDOM_FACTOR
    return anj_time_duration_fmul(params->ack_timeout,
                                  ((double) (1 << (params->max_retransmit + 1))
                                   - 1) * params->ack_random_factor);
}

int _anj_server_queue_mode_rx_off(_anj_server_connection_ctx_t *ctx) {
    return anj_net_queue_mode_rx_off(ctx->type, ctx->net_ctx);
}
