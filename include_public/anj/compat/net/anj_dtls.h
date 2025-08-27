/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_DTLS_H
#    define ANJ_DTLS_H

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_DTLS_BINDING

#        include <anj/compat/net/anj_net_api.h>

anj_net_get_system_socket_t anj_dtls_get_system_socket;
anj_net_close_t anj_dtls_close;
anj_net_connect_t anj_dtls_connect;
anj_net_create_ctx_t anj_dtls_create_ctx;
anj_net_send_t anj_dtls_send;
anj_net_recv_t anj_dtls_recv;
anj_net_shutdown_t anj_dtls_shutdown;
anj_net_cleanup_ctx_t anj_dtls_cleanup_ctx;
anj_net_reuse_last_port_t anj_dtls_reuse_last_port;

anj_net_get_bytes_received_t anj_dtls_get_bytes_received;
anj_net_get_bytes_sent_t anj_dtls_bytes_sent;
anj_net_get_connection_id_resumed_t anj_dtls_get_connection_id_resumed;
anj_net_get_inner_mtu_t anj_dtls_get_inner_mtu;
anj_net_get_session_resumed_t anj_dtls_get_session_resumed;
anj_net_get_state_t anj_dtls_get_state;

anj_net_set_dane_tlsa_array_t anj_dtls_set_dane_tls_array;
anj_net_set_dtls_handshake_timeouts_t anj_dtls_set_handshake_timeout;

#    endif // ANJ_WITH_DTLS_BINDING

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DTLS_H
