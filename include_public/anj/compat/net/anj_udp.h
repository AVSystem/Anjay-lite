/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_UDP_H
#    define ANJ_UDP_H

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_NET_WITH_UDP

#        include <anj/compat/net/anj_net_api.h>

anj_net_get_system_socket_t anj_udp_get_system_socket;
anj_net_close_t anj_udp_close;
anj_net_connect_t anj_udp_connect;
anj_net_create_ctx_t anj_udp_create_ctx;
anj_net_send_t anj_udp_send;
anj_net_recv_t anj_udp_recv;
anj_net_shutdown_t anj_udp_shutdown;
anj_net_cleanup_ctx_t anj_udp_cleanup_ctx;
anj_net_reuse_last_port_t anj_udp_reuse_last_port;

anj_net_get_bytes_received_t anj_udp_get_bytes_received;
anj_net_get_bytes_sent_t anj_udp_get_bytes_sent;
anj_net_get_inner_mtu_t anj_udp_get_inner_mtu;
anj_net_get_state_t anj_udp_get_state;

#    endif // ANJ_NET_WITH_UDP

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_UDP_H
