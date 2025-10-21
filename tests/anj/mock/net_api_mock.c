/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stddef.h>
#include <string.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/compat/net/anj_udp.h>
#include <anj/utils.h>

#include "net_api_mock.h"

#define HANLDE_RETURN_AND_COUNT(Mock, Fun) \
    Mock->call_count[Fun]++;               \
    return Mock->call_result[Fun]

#define FORCED_ERROR (-20)

// after anj_XXX_create_ctx call this pointer can be reused
static net_api_mock_t *net_api_mock;
static bool g_force_connection_failure = false;
static bool g_force_send_failure = false;

void net_api_mock_ctx_init(net_api_mock_t *mock) {
    net_api_mock = mock;
    memset(net_api_mock, 0, sizeof(*net_api_mock));
}

void net_api_mock_dont_overwrite_buffer(anj_net_ctx_t *ctx) {
    net_api_mock_t *mock = (net_api_mock_t *) ctx;
    mock->dont_overwrite_buffer = true;
    mock->bytes_sent = 0;
}

void net_api_mock_force_connection_failure(void) {
    g_force_connection_failure = true;
}

void net_api_mock_force_send_failure(void) {
    g_force_send_failure = true;
}

int anj_udp_send(anj_net_ctx_t *ctx,
                 size_t *bytes_sent,
                 const uint8_t *buf,
                 size_t length) {
    net_api_mock_t *mock = (net_api_mock_t *) ctx;
    if (g_force_send_failure) {
        g_force_send_failure = false;
        return FORCED_ERROR;
    }
    *bytes_sent = 0;
    mock->call_count[ANJ_NET_FUN_SEND]++;
    if (mock->bytes_to_send > 0 && !mock->call_result[ANJ_NET_FUN_SEND]) {
        if (mock->bytes_sent && mock->dont_overwrite_buffer) {
            return ANJ_NET_OK;
        }
        mock->bytes_sent = ANJ_MIN(mock->bytes_to_send, length);
        memcpy(mock->send_data_buffer, buf, mock->bytes_sent);
        *bytes_sent = mock->bytes_sent;
        return ANJ_NET_OK;
    }
    return mock->call_result[ANJ_NET_FUN_SEND];
}

int anj_udp_recv(anj_net_ctx_t *ctx,
                 size_t *bytes_received,
                 uint8_t *buf,
                 size_t length) {
    net_api_mock_t *mock = (net_api_mock_t *) ctx;
    mock->call_count[ANJ_NET_FUN_RECV]++;
    if (mock->call_result[ANJ_NET_FUN_RECV] != 0) {
        return mock->call_result[ANJ_NET_FUN_RECV];
    }
    if (mock->bytes_to_recv > 0) {
        *bytes_received = ANJ_MIN(mock->bytes_to_recv, length);
        memcpy(buf, mock->data_to_recv, *bytes_received);
        mock->bytes_to_recv -= *bytes_received;
        return ANJ_NET_OK;
    }
    return ANJ_NET_EAGAIN;
}

int anj_udp_create_ctx(anj_net_ctx_t **ctx, const anj_net_config_t *config) {
    (void) config;
    *ctx = (anj_net_ctx_t *) net_api_mock;
    net_api_mock->state = ANJ_NET_SOCKET_STATE_CLOSED;
    HANLDE_RETURN_AND_COUNT(net_api_mock, ANJ_NET_FUN_CREATE);
}

int anj_udp_connect(anj_net_ctx_t *ctx,
                    const char *hostname,
                    const char *port) {
    net_api_mock_t *mock = (net_api_mock_t *) ctx;
    if (g_force_connection_failure) {
        g_force_connection_failure = false;
        return FORCED_ERROR;
    }
    strcpy(mock->hostname, hostname);
    strcpy(mock->port, port);
    if (mock->call_result[ANJ_NET_FUN_CONNECT] == ANJ_NET_OK
            && mock->call_result[ANJ_NET_FUN_CONNECT] == 0) {
        mock->state = ANJ_NET_SOCKET_STATE_CONNECTED;
    }
    HANLDE_RETURN_AND_COUNT(mock, ANJ_NET_FUN_CONNECT);
}

int anj_udp_shutdown(anj_net_ctx_t *ctx) {
    net_api_mock_t *mock = (net_api_mock_t *) ctx;
    if (mock->call_result[ANJ_NET_FUN_SHUTDOWN] != ANJ_NET_EINPROGRESS) {
        mock->state = ANJ_NET_SOCKET_STATE_SHUTDOWN;
    }
    HANLDE_RETURN_AND_COUNT(mock, ANJ_NET_FUN_SHUTDOWN);
}

int anj_udp_close(anj_net_ctx_t *ctx) {
    net_api_mock_t *mock = (net_api_mock_t *) ctx;
    if (mock->call_result[ANJ_NET_FUN_CLOSE] != ANJ_NET_EINPROGRESS) {
        mock->state = ANJ_NET_SOCKET_STATE_CLOSED;
    }
    HANLDE_RETURN_AND_COUNT(mock, ANJ_NET_FUN_CLOSE);
}

int anj_udp_cleanup_ctx(anj_net_ctx_t **ctx) {
    net_api_mock_t *mock = (net_api_mock_t *) *ctx;
    if (mock->call_result[ANJ_NET_FUN_CLEANUP] != ANJ_NET_EINPROGRESS) {
        mock->state = ANJ_NET_SOCKET_STATE_CLOSED;
        *ctx = NULL;
    }
    HANLDE_RETURN_AND_COUNT(mock, ANJ_NET_FUN_CLEANUP);
}

int anj_udp_get_state(anj_net_ctx_t *ctx, anj_net_socket_state_t *out_value) {
    net_api_mock_t *mock = (net_api_mock_t *) ctx;
    *out_value = mock->state;
    HANLDE_RETURN_AND_COUNT(mock, ANJ_NET_FUN_GET_STATE);
}

int anj_udp_get_inner_mtu(anj_net_ctx_t *ctx, int32_t *out_value) {
    net_api_mock_t *mock = (net_api_mock_t *) ctx;
    *out_value = mock->inner_mtu_value;
    HANLDE_RETURN_AND_COUNT(mock, ANJ_NET_FUN_GET_INNER_MTU);
}

int anj_udp_queue_mode_rx_off(anj_net_ctx_t *ctx) {
    net_api_mock_t *mock = (net_api_mock_t *) ctx;
    HANLDE_RETURN_AND_COUNT(mock, ANJ_NET_FUN_QUEUE_MODE_RX_OFF);
}
