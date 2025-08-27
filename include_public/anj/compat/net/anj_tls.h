/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_TLS_H
#    define ANJ_TLS_H

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_TLS_BINDING

#        include <anj/compat/net/anj_net_api.h>

anj_net_get_system_socket_t anj_tls_get_system_socket;
anj_net_close_t anj_tls_close;
anj_net_connect_t anj_tls_connect;
anj_net_create_ctx_t anj_tls_create_ctx;
anj_net_send_t anj_tls_send;
anj_net_recv_t anj_tls_recv;
anj_net_shutdown_t anj_tls_shutdown;
anj_net_cleanup_ctx_t anj_tls_cleanup_ctx;
anj_net_reuse_last_port_t anj_tls_reuse_last_port;

anj_net_get_bytes_received_t anj_tls_get_bytes_received;
anj_net_get_bytes_sent_t anj_tls_bytes_sent;
anj_net_get_connection_id_resumed_t anj_tls_get_connection_id_resumed;
anj_net_get_inner_mtu_t anj_tls_get_inner_mtu;
anj_net_get_session_resumed_t anj_tls_get_session_resumed;
anj_net_get_state_t anj_tls_get_state;

anj_net_set_dane_tlsa_array_t anj_tls_set_dane_tls_array;
anj_net_set_tls_handshake_timeouts_t anj_tls_set_handshake_timeout;

#    endif // ANJ_WITH_TLS_BINDING

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_TLS_H
