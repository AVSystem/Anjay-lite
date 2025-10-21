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
 * @brief Thin dispatch layer for network backends.
 *
 * This header provides inline wrappers that route generic
 * @ref anj_net_api.h calls (create, connect, send, recv, etc.)
 * to the selected transport backend:
 * - UDP
 * - TCP
 * - DTLS
 * - TLS
 * - Non-IP bindings
 *
 * Which backends are compiled in is controlled via
 * `ANJ_NET_WITH_*` macros. Each wrapper chooses the
 * appropriate implementation or returns @ref ANJ_NET_ENOTSUP
 * if not available.
 *
 * This lets the core code call a uniform API, while integrators
 * can enable/disable specific transports at build time.
 */

#ifndef ANJ_NET_WRAPPER_H
#    define ANJ_NET_WRAPPER_H

#    include <anj/compat/net/anj_net_api.h>
#    ifdef ANJ_NET_WITH_UDP
#        include <anj/compat/net/anj_udp.h>
#    endif // ANJ_NET_WITH_UDP
#    ifdef ANJ_NET_WITH_TCP
#        include <anj/compat/net/anj_tcp.h>
#    endif // ANJ_NET_WITH_TCP
#    ifdef ANJ_NET_WITH_DTLS
#        include <anj/compat/net/anj_dtls.h>
#    endif // ANJ_NET_WITH_DTLS
#    ifdef ANJ_NET_WITH_TLS
#        include <anj/compat/net/anj_tls.h>
#    endif // ANJ_NET_WITH_TLS
#    ifdef ANJ_NET_WITH_NON_IP_BINDING
#        include <anj/compat/net/anj_non_ip.h>
#    endif // ANJ_NET_WITH_NON_IP_BINDING

#    ifdef __cplusplus
extern "C" {
#    endif

/** @see anj_net_create_ctx_t */
static inline int anj_net_create_ctx(anj_net_binding_type_t type,
                                     anj_net_ctx_t **ctx,
                                     const anj_net_config_t *config) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_create_ctx(ctx, config);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_create_ctx(ctx, config);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_create_ctx(ctx, config);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_create_ctx(ctx, config);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_create_ctx(ctx, config);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

/** @see anj_net_connect_t */
static inline int anj_net_connect(anj_net_binding_type_t type,
                                  anj_net_ctx_t *ctx,
                                  const char *hostname,
                                  const char *port) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_connect(ctx, hostname, port);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_connect(ctx, hostname, port);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_connect(ctx, hostname, port);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_connect(ctx, hostname, port);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_connect(ctx, hostname, port);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

/** @see anj_net_send_t */
static inline int anj_net_send(anj_net_binding_type_t type,
                               anj_net_ctx_t *ctx,
                               size_t *bytes_sent,
                               const uint8_t *buf,
                               size_t length) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_send(ctx, bytes_sent, buf, length);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_send(ctx, bytes_sent, buf, length);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_send(ctx, bytes_sent, buf, length);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_send(ctx, bytes_sent, buf, length);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_send(ctx, bytes_sent, buf, length);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

/** @see anj_net_recv_t */
static inline int anj_net_recv(anj_net_binding_type_t type,
                               anj_net_ctx_t *ctx,
                               size_t *bytes_received,
                               uint8_t *buf,
                               size_t length) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_recv(ctx, bytes_received, buf, length);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_recv(ctx, bytes_received, buf, length);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_recv(ctx, bytes_received, buf, length);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_recv(ctx, bytes_received, buf, length);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_recv(ctx, bytes_received, buf, length);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

/** @see anj_net_close_t */
static inline int anj_net_close(anj_net_binding_type_t type,
                                anj_net_ctx_t *ctx) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_close(ctx);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_close(ctx);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_close(ctx);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_close(ctx);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_close(ctx);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

/** @see anj_net_shutdown_t */
static inline int anj_net_shutdown(anj_net_binding_type_t type,
                                   anj_net_ctx_t *ctx) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_shutdown(ctx);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_shutdown(ctx);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_shutdown(ctx);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_shutdown(ctx);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_shutdown(ctx);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

/** @see anj_net_cleanup_ctx_t */
static inline int anj_net_cleanup_ctx(anj_net_binding_type_t type,
                                      anj_net_ctx_t **ctx) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_cleanup_ctx(ctx);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_cleanup_ctx(ctx);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_cleanup_ctx(ctx);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_cleanup_ctx(ctx);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_cleanup_ctx(ctx);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

/** @see anj_net_get_inner_mtu_t */
static inline int anj_net_get_inner_mtu(anj_net_binding_type_t type,
                                        anj_net_ctx_t *ctx,
                                        int32_t *out_value) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_get_inner_mtu(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_get_inner_mtu(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_get_inner_mtu(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_get_inner_mtu(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_get_inner_mtu(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

/** @see anj_net_get_state_t */
static inline int anj_net_get_state(anj_net_binding_type_t type,
                                    anj_net_ctx_t *ctx,
                                    anj_net_socket_state_t *out_value) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_get_state(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_get_state(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_get_state(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_get_state(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_get_state(ctx, out_value);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

/** @see anj_net_queue_mode_rx_off_t */
static inline int anj_net_queue_mode_rx_off(anj_net_binding_type_t type,
                                            anj_net_ctx_t *ctx) {
    switch (type) {
#    if defined(ANJ_NET_WITH_UDP)
    case ANJ_NET_BINDING_UDP:
        return anj_udp_queue_mode_rx_off(ctx);
#    endif // defined(ANJ_NET_WITH_UDP)
#    if defined(ANJ_NET_WITH_TCP)
    case ANJ_NET_BINDING_TCP:
        return anj_tcp_queue_mode_rx_off(ctx);
#    endif // defined(ANJ_NET_WITH_TCP)
#    if defined(ANJ_NET_WITH_DTLS)
    case ANJ_NET_BINDING_DTLS:
        return anj_dtls_queue_mode_rx_off(ctx);
#    endif // defined(ANJ_NET_WITH_DTLS)
#    if defined(ANJ_NET_WITH_TLS)
    case ANJ_NET_BINDING_TLS:
        return anj_tls_queue_mode_rx_off(ctx);
#    endif // defined(ANJ_NET_WITH_TLS)
#    if defined(ANJ_NET_WITH_NON_IP_BINDING)
    case ANJ_NET_BINDING_NON_IP:
        return anj_non_ip_queue_mode_rx_off(ctx);
#    endif // defined(ANJ_NET_WITH_NON_IP_BINDING)
    default:
        return ANJ_NET_ENOTSUP;
    }
}

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_NET_WRAPPER_H
