/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef NET_API_MOCK_H
#define NET_API_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <anj/compat/net/anj_net_api.h>

typedef enum {
    ANJ_NET_FUN_CREATE,
    ANJ_NET_FUN_CONNECT,
    ANJ_NET_FUN_SEND,
    ANJ_NET_FUN_RECV,
    ANJ_NET_FUN_SHUTDOWN,
    ANJ_NET_FUN_CLOSE,
    ANJ_NET_FUN_CLEANUP,
    ANJ_NET_FUN_REUSE_LAST_PORT,
    ANJ_NET_FUN_GET_INNER_MTU,
    ANJ_NET_FUN_GET_BYTES_RECEIVED,
    ANJ_NET_FUN_GET_BYTES_SENT,
    ANJ_NET_FUN_GET_STATE,
    ANJ_NET_FUN_LAST
} anj_net_fun_t;

typedef struct {
    int call_result[ANJ_NET_FUN_LAST];
    int call_count[ANJ_NET_FUN_LAST];
    size_t net_eagain_calls;

    size_t bytes_to_send;
    uint8_t send_data_buffer[1500];
    size_t bytes_sent;

    size_t bytes_to_recv;
    uint8_t *data_to_recv;

    size_t inner_mtu_value;
    const char *hostname;
    const char *port;
    // state is handled by mock
    anj_net_socket_state_t state;

    bool dont_overwrite_buffer;
    bool force_connection_failure;
} net_api_mock_t;

// mock pointer must be set before calling any anj_net function
void net_api_mock_ctx_init(net_api_mock_t *mock);
// below function is used to handle very specific case when anj_net_send is
// called twice but unit tests want to check only first call
void net_api_mock_dont_overwrite_buffer(anj_net_ctx_t *ctx);
void net_api_mock_force_connection_failure(void);
void net_api_mock_force_send_failure(void);

#endif /* NET_API_MOCK_H */
