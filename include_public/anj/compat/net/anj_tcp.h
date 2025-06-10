/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_TCP_H
#define ANJ_TCP_H

#include <anj/anj_config.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ANJ_NET_WITH_TCP

#    include <anj/compat/net/anj_net_api.h>

anj_net_get_system_socket_t anj_tcp_get_system_socket;
anj_net_close_t anj_tcp_close;
anj_net_connect_t anj_tcp_connect;
anj_net_create_ctx_t anj_tcp_create_ctx;
anj_net_send_t anj_tcp_send;
anj_net_recv_t anj_tcp_recv;
anj_net_shutdown_t anj_tcp_shutdown;
anj_net_cleanup_ctx_t anj_tcp_cleanup_ctx;
anj_net_reuse_last_port_t anj_tcp_reuse_last_port;

anj_net_get_bytes_received_t anj_tcp_get_bytes_received;
anj_net_get_bytes_sent_t anj_tcp_get_bytes_sent;
anj_net_get_inner_mtu_t anj_tcp_get_inner_mtu;
anj_net_get_state_t anj_tcp_get_state;

#endif // ANJ_NET_WITH_TCP

#ifdef __cplusplus
}
#endif

#endif // ANJ_TCP_H
