/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */
#include <anj/compat/net/anj_net_api.h>
#include <anj/compat/net/anj_udp.h>
#include <anj/utils.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define INVALID_SOCKET -1
#define NET_GENERAL_ERROR -1
typedef int sockfd_t;

typedef struct net_ctx_posix_impl {
    sockfd_t sockfd;
    anj_net_socket_state_t state;
} net_ctx_posix_impl_t;

int anj_udp_create_ctx(anj_net_ctx_t **ctx_, const anj_net_config_t *config) {
    (void) config;

    net_ctx_posix_impl_t *ctx =
            (net_ctx_posix_impl_t *) malloc(sizeof(net_ctx_posix_impl_t));
    if (!ctx) {
        return NET_GENERAL_ERROR;
    }
    ctx->sockfd = INVALID_SOCKET;
    ctx->state = ANJ_NET_SOCKET_STATE_CLOSED;

    *ctx_ = (anj_net_ctx_t *) ctx;
    return ANJ_NET_OK;
}

static void set_socket_non_blocking(sockfd_t sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    }
}

int anj_udp_connect(anj_net_ctx_t *ctx_,
                    const char *hostname,
                    const char *port_str) {
    net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;

    struct addrinfo *serverinfo = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(hostname, port_str, &hints, &serverinfo) || !serverinfo) {
        if (serverinfo) {
            freeaddrinfo(serverinfo);
        }
        return NET_GENERAL_ERROR;
    }

    ctx->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->sockfd < 0) {
        freeaddrinfo(serverinfo);
        return NET_GENERAL_ERROR;
    }

    if (connect(ctx->sockfd, serverinfo->ai_addr, serverinfo->ai_addrlen)) {
        freeaddrinfo(serverinfo);
        return NET_GENERAL_ERROR;
    }
    set_socket_non_blocking(ctx->sockfd);
    ctx->state = ANJ_NET_SOCKET_STATE_CONNECTED;

    freeaddrinfo(serverinfo);
    return ANJ_NET_OK;
}

static bool would_block(int errno_val) {
    switch (errno_val) {
#ifdef EAGAIN
    case EAGAIN:
        return true;
#endif
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
    case EWOULDBLOCK:
        return true;
#endif
#ifdef EINPROGRESS
    case EINPROGRESS:
        return true;
#endif
#ifdef EBUSY
    case EBUSY:
        return true;
#endif
    default:
        return false;
    }
}

int anj_udp_send(anj_net_ctx_t *ctx_,
                 size_t *bytes_sent,
                 const uint8_t *buf,
                 size_t length) {
    net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;
    errno = 0;
    ssize_t result = send(ctx->sockfd, buf, length, 0);
    if (result < 0) {
        return would_block(errno) ? ANJ_NET_EAGAIN : NET_GENERAL_ERROR;
    }
    *bytes_sent = (size_t) result;
    if (*bytes_sent < length) {
        /* Partial sent not allowed in case of UDP */
        return NET_GENERAL_ERROR;
    }
    return ANJ_NET_OK;
}

int anj_udp_recv(anj_net_ctx_t *ctx_,
                 size_t *bytes_received,
                 uint8_t *buf,
                 size_t length) {
    net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;
    errno = 0;
    ssize_t result = recv(ctx->sockfd, buf, length, 0);
    if (result < 0) {
        return would_block(errno) ? ANJ_NET_EAGAIN : NET_GENERAL_ERROR;
    }
    *bytes_received = (size_t) result;
    if (*bytes_received == length) {
        /**
         * Buffer entirely filled - data possibly truncated. This will
         * incorrectly reject packets that have exactly buffer_length
         * bytes, but we have no means of distinguishing the edge case
         * without recvmsg.
         * This does only apply to datagram sockets (in our case: UDP).
         */
        return ANJ_NET_EMSGSIZE;
    }
    return ANJ_NET_OK;
}

int anj_udp_shutdown(anj_net_ctx_t *ctx_) {
    net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;

    shutdown(ctx->sockfd, SHUT_RDWR);

    ctx->state = ANJ_NET_SOCKET_STATE_SHUTDOWN;
    return ANJ_NET_OK;
}

int anj_udp_close(anj_net_ctx_t *ctx_) {
    net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;

    close(ctx->sockfd);

    ctx->sockfd = INVALID_SOCKET;
    ctx->state = ANJ_NET_SOCKET_STATE_CLOSED;
    return ANJ_NET_OK;
}

int anj_udp_cleanup_ctx(anj_net_ctx_t **ctx_) {
    net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) *ctx_;
    *ctx_ = NULL;

    close(ctx->sockfd);
    free(ctx);

    return ANJ_NET_OK;
}

int anj_udp_get_inner_mtu(anj_net_ctx_t *ctx, int32_t *out_value) {
    (void) ctx;
    *out_value = 548; /* 576 (IPv4 MTU) - 28 bytes of headers */
    return ANJ_NET_OK;
}

int anj_udp_get_state(anj_net_ctx_t *ctx_, anj_net_socket_state_t *out_value) {
    net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;
    *(anj_net_socket_state_t *) out_value = ctx->state;
    return ANJ_NET_OK;
}

int anj_udp_reuse_last_port(anj_net_ctx_t *ctx) {
    (void) ctx;
    return ANJ_NET_ENOTSUP;
}
