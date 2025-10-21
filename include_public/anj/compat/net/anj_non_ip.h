/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Network backend interface for Non-IP transport.
 *
 * Declares the Non-IP variant of the generic @ref anj_net_api.h
 * functions (create, connect, send, recv, shutdown, etc.).
 *
 * These symbols are defined only if @c ANJ_NET_WITH_NON_IP_BINDING is enabled.
 * They provide the Non-IP binding used by @ref anj_net_wrapper.h for dispatch.
 */

#ifndef ANJ_NON_IP_H
#    define ANJ_NON_IP_H

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_NET_WITH_NON_IP_BINDING
#        include <anj/compat/net/anj_net_api.h>

anj_net_get_system_socket_t anj_non_ip_get_system_socket;
anj_net_close_t anj_non_ip_close;
anj_net_connect_t anj_non_ip_connect;
anj_net_create_ctx_t anj_non_ip_create_ctx;
anj_net_send_t anj_non_ip_send;
anj_net_recv_t anj_non_ip_recv;
anj_net_shutdown_t anj_non_ip_shutdown;
anj_net_cleanup_ctx_t anj_non_ip_cleanup_ctx;

anj_net_get_bytes_received_t anj_non_ip_get_bytes_received;
anj_net_get_bytes_sent_t anj_non_ip_get_bytes_sent;
anj_net_get_inner_mtu_t anj_non_ip_get_inner_mtu;
anj_net_get_state_t anj_non_ip_get_state;

#    endif // ANJ_NET_WITH_NON_IP_BINDING

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_NON_IP_H
