/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Network backend interface for DTLS transport.
 *
 * Declares the DTLS variant of the generic @ref anj_net_api.h
 * functions (create, connect, send, recv, etc.).
 *
 * These symbols are defined only if @ref ANJ_NET_WITH_DTLS is
 * enabled. They provide the DTLS binding used by
 * @ref anj_net_wrapper.h for dispatch.
 */

#ifndef ANJ_DTLS_H
#    define ANJ_DTLS_H

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_NET_WITH_DTLS

#        include <anj/compat/net/anj_net_api.h>

anj_net_close_t anj_dtls_close;
anj_net_connect_t anj_dtls_connect;
anj_net_create_ctx_t anj_dtls_create_ctx;
anj_net_send_t anj_dtls_send;
anj_net_recv_t anj_dtls_recv;
anj_net_cleanup_ctx_t anj_dtls_cleanup_ctx;

anj_net_get_inner_mtu_t anj_dtls_get_inner_mtu;
anj_net_get_state_t anj_dtls_get_state;
anj_net_queue_mode_rx_off_t anj_dtls_queue_mode_rx_off;

#    endif // ANJ_NET_WITH_DTLS

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DTLS_H
