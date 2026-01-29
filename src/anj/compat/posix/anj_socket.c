/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 58

// IWYU pragma: no_include <sys/un.h>
// IWYU pragma: no_include <bits/socket-constants.h>

#ifdef ANJ_WITH_SOCKET_POSIX_COMPAT
// TODO: Instead of defining this, consider providing the option to configure
// a custom header to add definitions per specific platform, e.g. POSIX or lwIP.
#    if !defined(_POSIX_C_SOURCE) && !defined(__APPLE__)
#        define _POSIX_C_SOURCE 200809L
#    endif

#    include <stdint.h>
#    include <string.h>

#    include <anj/compat/net/anj_net_api.h>
#    ifdef ANJ_NET_WITH_UDP
#        include <anj/compat/net/anj_udp.h>
#    endif // ANJ_NET_WITH_UDP
#    include <anj/log.h>
#    include <anj/utils.h>

#    include <arpa/inet.h>
#    include <errno.h>
#    include <fcntl.h>
#    include <inttypes.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <stdbool.h>
#    include <stddef.h>
#    include <stdlib.h>
#    include <sys/socket.h>
#    include <sys/types.h>
#    include <unistd.h>

/**
 * The operation failed.
 */
#    define ANJ_NET_FAILED (-1)

/**
 * Error code meaning the input arguments were not valid and the function
 * could not perform it.
 */
#    define ANJ_NET_EINVAL (-2)

/**
 * Input/output error.
 */
#    define ANJ_NET_EIO (-3)

/**
 * The socket is not connected.
 */
#    define ANJ_NET_ENOTCONN (-4)

/**
 * The socket file descriptor is in a bad state to perform this operation.
 */
#    define ANJ_NET_EBADFD (-5)

/**
 * Insufficent memory is available. The socket cannot be created until
 * sufficient resources are freed.
 */
#    define ANJ_NET_ENOMEM (-6)

#    define net_log(...) anj_log(net, __VA_ARGS__)

#    define INVALID_SOCKET -1
typedef int sockfd_t;

typedef struct anj_net_ctx_posix_impl {
    sockfd_t sockfd;
    anj_net_socket_state_t state;

    anj_net_socket_configuration_t config;
} anj_net_ctx_posix_impl_t;

typedef enum {
    ANJ_POSIX_SOCKET_OPT_BYTES_SENT = 0,
    ANJ_POSIX_SOCKET_OPT_BYTES_RECEIVED,
    ANJ_POSIX_SOCKET_OPT_STATE,
    ANJ_POSIX_SOCKET_OPT_INNER_MTU
} anj_posix_socket_opt_t;

static int set_socket_non_blocking(sockfd_t sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        return ANJ_NET_EIO;
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return ANJ_NET_EIO;
    }

    return 0;
}

static int anj_net_map_errno(int errno_val) {
    switch (errno_val) {
/**
 * NOTE: EAGAIN and EWOULDBLOCK are allowed to have same value (thanks POSIX),
 * and we need to be prepared for that.
 */
#    ifdef EAGAIN
    case EAGAIN:
        return ANJ_NET_EINPROGRESS;
#    endif
#    if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
    case EWOULDBLOCK:
        return ANJ_NET_EINPROGRESS;
#    endif
#    ifdef EINPROGRESS
    case EINPROGRESS:
        return ANJ_NET_EINPROGRESS;
#    endif
#    ifdef EBADF
    case EBADF:
        return ANJ_NET_EBADFD;
#    endif
#    ifdef EBUSY
    case EBUSY:
        return ANJ_NET_EINPROGRESS;
#    endif
#    ifdef EINVAL
    case EINVAL:
        return ANJ_NET_EINVAL;
#    endif
#    ifdef EIO
    case EIO:
        return ANJ_NET_EIO;
#    endif
#    ifdef EMSGSIZE
    case EMSGSIZE:
        return ANJ_NET_EMSGSIZE;
#    endif
#    ifdef ENOMEM
    case ENOMEM:
        return ANJ_NET_ENOMEM;
#    endif
#    ifdef ENOTCONN
    case ENOTCONN:
        return ANJ_NET_ENOTCONN;
#    endif
/**
 * NOTE: ENOTSUP and EOPNOTSUPP are allowed to have same value (thanks POSIX),
 * and we need to be prepared for that.
 */
#    ifdef ENOTSUP
    case ENOTSUP:
        return ANJ_NET_ENOTSUP;
#    endif
#    if defined(EOPNOTSUPP) && (EOPNOTSUPP != ENOTSUP)
    case EOPNOTSUPP:
        return ANJ_NET_ENOTSUP;
#    endif
    default:
        return ANJ_NET_FAILED;
    }
}

static int failure_from_errno(void) {
    return anj_net_map_errno(errno);
}

static bool is_af_setting_weak(anj_net_address_family_setting_t af_setting) {
    return af_setting == ANJ_NET_AF_SETTING_UNSPEC
           || af_setting == ANJ_NET_AF_SETTING_PREFERRED_INET4
           || af_setting == ANJ_NET_AF_SETTING_PREFERRED_INET6;
}

static sa_family_t get_socket_family(sockfd_t fd) {
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    if (fd < 0) {
        return AF_UNSPEC;
    }

    if (getsockname(fd, &addr, &addrlen)) {
        return AF_UNSPEC;
    }
    return addr.sa_family;
}

static void copy_config(anj_net_ctx_posix_impl_t *ctx,
                        const anj_net_socket_configuration_t *config) {
    if (!config) {
        /* no configuration, clear the structure */
        memset(&ctx->config, '\0', sizeof(ctx->config));
    } else {
        memcpy(&ctx->config, config, sizeof(ctx->config));
    }
}

static void cleanup_ctx_internal(anj_net_ctx_posix_impl_t *ctx) {
    ctx->sockfd = INVALID_SOCKET;
    ctx->state = ANJ_NET_SOCKET_STATE_CLOSED;
}

static int net_close_internal(anj_net_ctx_posix_impl_t *ctx) {
    if (ctx->sockfd != INVALID_SOCKET) {
        errno = 0;
        if (close(ctx->sockfd) < 0) {
            return failure_from_errno();
        }
    }
    cleanup_ctx_internal(ctx);
    return ANJ_NET_OK;
}

static int create_net_socket(anj_net_ctx_posix_impl_t *ctx, sa_family_t af) {
    ctx->sockfd = socket(af, SOCK_DGRAM, 0);
    if (ctx->sockfd < 0) {
        return ANJ_NET_ENOMEM;
    }

    return ANJ_NET_OK;
}

static int create_net_ctx(anj_net_ctx_t **ctx_,
                          const anj_net_config_t *config) {
    if (!ctx_) {
        return ANJ_NET_EINVAL;
    }

    anj_net_ctx_posix_impl_t *ctx = (anj_net_ctx_posix_impl_t *) malloc(
            sizeof(anj_net_ctx_posix_impl_t));
    if (!ctx) {
        return ANJ_NET_ENOMEM;
    }

    cleanup_ctx_internal(ctx);
    copy_config(ctx, config ? &config->raw_socket_config : NULL);

    if (ctx->config.af_setting < ANJ_NET_AF_SETTING_UNSPEC
            || ctx->config.af_setting > ANJ_NET_AF_SETTING_PREFERRED_INET6) {
        free(ctx);
        return ANJ_NET_EINVAL;
    }

    *ctx_ = (anj_net_ctx_t *) ctx;

    return ANJ_NET_OK;
}

static int net_close(anj_net_ctx_t *ctx_) {
    if (!ctx_) {
        return ANJ_NET_EBADFD;
    }

    anj_net_ctx_posix_impl_t *ctx = (anj_net_ctx_posix_impl_t *) ctx_;
    if (ctx->sockfd < 0) {
        return ANJ_NET_EBADFD;
    }

    return net_close_internal(ctx);
}

static int cleanup_ctx(anj_net_ctx_t **ctx_) {
    if (!ctx_ || !*ctx_) {
        return ANJ_NET_EBADFD;
    }

    anj_net_ctx_posix_impl_t *ctx = (anj_net_ctx_posix_impl_t *) *ctx_;
    *ctx_ = NULL;

    int result = net_close_internal(ctx);
    free(ctx);
    return result;
}

static void update_ports(struct addrinfo *head,
                         const uint16_t port_in_net_order) {
    switch (head->ai_family) {
#    ifdef ANJ_NET_WITH_IPV4
    case AF_INET: {
        struct sockaddr_in *addr_in = (struct sockaddr_in *) head->ai_addr;
        addr_in->sin_port = port_in_net_order;
        break;
    }
#    endif // ANJ_NET_WITH_IPV4
#    ifdef ANJ_NET_WITH_IPV6
    case AF_INET6: {
        struct sockaddr_in6 *addr_in = (struct sockaddr_in6 *) head->ai_addr;
        addr_in->sin6_port = port_in_net_order;
        break;
    }
#    endif    // ANJ_NET_WITH_IPV6
    default:; // do nothing
    }
}

#    if defined(ANJ_NET_WITH_IPV4) && defined(ANJ_NET_WITH_IPV6)
static int
get_preferred_family(anj_net_address_family_setting_t af_preference) {
    switch (af_preference) {
    case ANJ_NET_AF_SETTING_UNSPEC:
    case ANJ_NET_AF_SETTING_PREFERRED_INET4:
        return AF_INET;
    case ANJ_NET_AF_SETTING_PREFERRED_INET6:
        return AF_INET6;
    default:
        return AF_INET;
    }
}

static int get_opposite_family(anj_net_address_family_setting_t af_preference) {
    switch (af_preference) {
    case ANJ_NET_AF_SETTING_UNSPEC:
    case ANJ_NET_AF_SETTING_PREFERRED_INET4:
        return AF_INET6;
    case ANJ_NET_AF_SETTING_PREFERRED_INET6:
        return AF_INET;
    default:
        return AF_INET;
    }
}
#    endif // defined(ANJ_NET_WITH_IPV4) && defined(ANJ_NET_WITH_IPV6)

static int set_ai_family(int *ai_family,
                         anj_net_address_family_setting_t af_setting,
                         bool first_call) {
    if (!is_af_setting_weak(af_setting)) {
        if (!first_call) {
            return -1;
        }

        if (af_setting == ANJ_NET_AF_SETTING_FORCE_INET4) {
            *ai_family = AF_INET;
        } else if (af_setting == ANJ_NET_AF_SETTING_FORCE_INET6) {
            *ai_family = AF_INET6;
        } else {
            ANJ_UNREACHABLE("incorrect af_setting option");
            return -1;
        }
    } else {
#    if defined(ANJ_NET_WITH_IPV4) && defined(ANJ_NET_WITH_IPV6)
        *ai_family = first_call ? get_preferred_family(af_setting)
                                : get_opposite_family(af_setting);
#    else // defined(ANJ_NET_WITH_IPV4) && defined(ANJ_NET_WITH_IPV6)
        if (!first_call) {
            return -1;
        }
#        ifdef ANJ_NET_WITH_IPV4
        *ai_family = AF_INET;
#        endif // ANJ_NET_WITH_IPV4
#        ifdef ANJ_NET_WITH_IPV6
        *ai_family = AF_INET6;
#        endif // ANJ_NET_WITH_IPV6
#    endif     // defined(ANJ_NET_WITH_IPV4) && defined(ANJ_NET_WITH_IPV6)
    }
    return 0;
}

static int net_addrinfo_resolve(anj_net_ctx_posix_impl_t *ctx,
                                const char *hostname,
                                const uint16_t port_in_net_order,
                                struct addrinfo **servinfo,
                                int ai_family) {
    if (!ctx || !servinfo || !hostname) {
        net_log(L_ERROR, "Invalid arguments for address resolution");
        return ANJ_NET_EINVAL;
    }

    // Configuration hints for address resolution
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = ai_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = 0; // Adjust flags as needed

    const int ret = getaddrinfo(hostname, NULL, &hints, servinfo);
    if (ret != 0) {
        net_log(L_ERROR, "Address resolution failed for %s:%u: %s", hostname,
                ntohs(port_in_net_order), gai_strerror(ret));
        /**
         * getaddrinfo is allowed to return EAGAIN,
         * but this is ignored and treated as failed - to proceed with
         * normal error handling procedure
         */

        return ANJ_NET_FAILED;
    }

    update_ports(*servinfo, port_in_net_order);

    net_log(L_DEBUG, "Address resolved successfully for %s:%u", hostname,
            ntohs(port_in_net_order));

    return ANJ_NET_OK;
}

static int net_connect_internal(anj_net_ctx_posix_impl_t *ctx,
                                struct addrinfo **serverinfo,
                                const char *hostname,
                                const char *port_str) {
    if (!port_str) {
        return ANJ_NET_EINVAL;
    }

    uint32_t port;
    if (anj_string_to_uint32_value(&port, port_str, strlen(port_str))
            || port > UINT16_MAX) {
        return ANJ_NET_EINVAL;
    }

    int ai_family;
    if (set_ai_family(&ai_family, ctx->config.af_setting, true)) {
        return ANJ_NET_FAILED;
    }

    int ret = net_addrinfo_resolve(ctx, hostname,
                                   (uint16_t) htons((uint16_t) port),
                                   serverinfo, ai_family);
    if (ret != ANJ_NET_OK) {
        if (!set_ai_family(&ai_family, ctx->config.af_setting, false)) {
            ret = net_addrinfo_resolve(ctx, hostname,
                                       (uint16_t) htons((uint16_t) port),
                                       serverinfo, ai_family);
        }
        if (ret != ANJ_NET_OK) {
            return ret;
        }
    }

    net_log(L_DEBUG, "Connecting to %s:%s", hostname, port_str);

    struct addrinfo *addr = *serverinfo;
    if (!addr) {
        return ANJ_NET_FAILED;
    }

    if (ctx->sockfd == INVALID_SOCKET) {
        ret = create_net_socket(ctx, (sa_family_t) addr->ai_family);
        if (ret != ANJ_NET_OK) {
            return ret;
        }
    }

    errno = 0;
    if (connect(ctx->sockfd, addr->ai_addr, addr->ai_addrlen) < 0) {
        return failure_from_errno();
    }

    ret = set_socket_non_blocking(ctx->sockfd);
    if (ret) {
        net_log(L_ERROR, "Failed to set socket to non-blocking mode");
        return ret;
    }

    return ANJ_NET_OK;
}

static int
net_connect(anj_net_ctx_t *ctx_, const char *hostname, const char *port_str) {
    if (!ctx_) {
        return ANJ_NET_EBADFD;
    }

    anj_net_ctx_posix_impl_t *ctx = (anj_net_ctx_posix_impl_t *) ctx_;

    struct addrinfo *serverinfo = NULL;
    int ret = net_connect_internal(ctx, &serverinfo, hostname, port_str);
    if (serverinfo) {
        freeaddrinfo(serverinfo);
    }

    if (ret == ANJ_NET_OK) {
        net_log(L_TRACE, "Connected");
        ctx->state = ANJ_NET_SOCKET_STATE_CONNECTED;
    } else if (ret != ANJ_NET_EINPROGRESS) {
        net_close_internal(ctx);
    }
    return ret;
}

static int net_send_internal(anj_net_ctx_posix_impl_t *ctx,
                             size_t *bytes_sent,
                             const uint8_t *data,
                             const size_t data_size) {
    errno = 0;
    ssize_t result = send(ctx->sockfd, data, data_size, 0);
    if (result < 0) {
        return failure_from_errno();
    }

    *bytes_sent = (size_t) result;

    /* we did send something but it might be less then we wanted */
    if ((size_t) result < data_size) {
        return ANJ_NET_FAILED;
    }
    /* all data send */
    return ANJ_NET_OK;
}

static int net_send(anj_net_ctx_t *ctx_,
                    size_t *bytes_sent,
                    const uint8_t *buf,
                    size_t length) {
    if (!ctx_) {
        return ANJ_NET_EBADFD;
    }

    if (!bytes_sent) {
        return ANJ_NET_EINVAL;
    }
    *bytes_sent = 0;

    anj_net_ctx_posix_impl_t *ctx = (anj_net_ctx_posix_impl_t *) ctx_;
    if (ctx->sockfd < 0) {
        return ANJ_NET_EBADFD;
    }

    return net_send_internal(ctx, bytes_sent, buf, length);
}

static int net_recv_internal(anj_net_ctx_posix_impl_t *ctx,
                             size_t *bytes_received,
                             uint8_t *data,
                             size_t data_size) {
    errno = 0;
    ssize_t result = recv(ctx->sockfd, data, data_size, 0);
    if (result < 0) {
        // recv (and only recv) has to differentiate EAGAIN from other errors
        if (errno == EAGAIN) {
            return ANJ_NET_EAGAIN;
        }
        return failure_from_errno();
    }

    *bytes_received = (size_t) result;

    if (result > 0 && (size_t) result == data_size) {
        /**
         * Buffer entirely filled - data possibly truncated. This will
         * incorrectly reject packets that have exactly buffer_length
         * bytes, but we have no means of distinguishing the edge case
         * without recvmsg.
         * This does only apply to datagram sockets (in our case: UDP).
         */
        return ANJ_NET_EMSGSIZE;
    }

    /* nothing more to read */
    return ANJ_NET_OK;
}

static int net_recv(anj_net_ctx_t *ctx_,
                    size_t *bytes_received,
                    uint8_t *buf,
                    size_t length) {
    if (!ctx_) {
        return ANJ_NET_EBADFD;
    }

    if (!bytes_received) {
        return ANJ_NET_EINVAL;
    }
    *bytes_received = 0;

    anj_net_ctx_posix_impl_t *ctx = (anj_net_ctx_posix_impl_t *) ctx_;
    if (ctx->sockfd < 0) {
        return ANJ_NET_EBADFD;
    }

    return net_recv_internal(ctx, bytes_received, buf, length);
}

static int get_mtu(anj_net_ctx_posix_impl_t *ctx, int32_t *out_mtu) {
    if (ctx->sockfd == INVALID_SOCKET) {
        net_log(L_ERROR, "Cannot get MTU for closed socket");
        return ANJ_NET_ENOTCONN;
    }

    int32_t mtu = -1;
    int err = ANJ_NET_OK;
    socklen_t dummy = sizeof(mtu);
    switch (get_socket_family(ctx->sockfd)) {
#    if defined(ANJ_NET_WITH_IPV4) && defined(IP_MTU)
    case AF_INET:
        errno = 0;
        if (getsockopt(ctx->sockfd, IPPROTO_IP, IP_MTU, &mtu, &dummy) < 0) {
            err = failure_from_errno();
        }
        break;
#    endif /* defined(ANJ_NET_WITH_IPV4) && defined(IP_MTU) */

#    if defined(ANJ_NET_WITH_IPV6) && defined(IPV6_MTU)
    case AF_INET6:
        errno = 0;
        if (getsockopt(ctx->sockfd, IPPROTO_IPV6, IPV6_MTU, &mtu, &dummy) < 0) {
            err = failure_from_errno();
        }
        break;
#    endif /* defined(ANJ_NET_WITH_IPV6) && defined(IPV6_MTU) */

    default:
        (void) dummy;
        err = ANJ_NET_EINVAL;
    }
    if (err == ANJ_NET_OK) {
        if (mtu >= 0) {
            *out_mtu = mtu;
            net_log(L_TRACE, "MTU obtained: %" PRId32, mtu);
        } else {
            err = ANJ_NET_FAILED;
        }
    }
    return err;
}

static int get_fallback_inner_udp_mtu(anj_net_ctx_posix_impl_t *ctx) {
#    ifdef ANJ_NET_WITH_IPV6
    if (get_socket_family(ctx->sockfd) == AF_INET6) { /* IPv6 */
        return 1232;                                  // minimum MTU for IPv6
                                                      // minus header 1280 - 48
    } else
#    endif // ANJ_NET_WITH_IPV6
    {      /* probably IPv4 */
        (void) ctx;
        return 548; /* 576 - 28 */
    }
}

static int get_udp_overhead(anj_net_ctx_posix_impl_t *ctx, int *out) {
    switch (get_socket_family(ctx->sockfd)) {
#    ifdef ANJ_NET_WITH_IPV4
    case AF_INET:
        *out = 28; /* 20 for IP + 8 for UDP */
        return ANJ_NET_OK;
#    endif /* ANJ_NET_WITH_IPV4 */

#    ifdef ANJ_NET_WITH_IPV6
    case AF_INET6:
        *out = 48; /* 40 for IPv6 + 8 for UDP */
        return ANJ_NET_OK;
#    endif /* ANJ_NET_WITH_IPV6 */

    default:
        return ANJ_NET_EINVAL;
    }
}

static int get_inner_mtu(anj_net_ctx_posix_impl_t *ctx, int32_t *out_mtu) {
    int err = get_mtu(ctx, out_mtu);
    if (err == ANJ_NET_OK) {
        int overhead;
        if ((err = get_udp_overhead(ctx, &overhead)) != ANJ_NET_OK) {
            return err;
        }

        *out_mtu -= overhead;
        if (*out_mtu < 0) {
            *out_mtu = 0;
        }
    } else {
        *out_mtu = get_fallback_inner_udp_mtu(ctx);
    }
    return ANJ_NET_OK;
}

static int net_get_opt(anj_net_ctx_t *ctx_,
                       void *out_value,
                       anj_posix_socket_opt_t opt_key) {
    if (!ctx_) {
        return ANJ_NET_EBADFD;
    }

    if (!out_value) {
        return ANJ_NET_EINVAL;
    }

    anj_net_ctx_posix_impl_t *ctx = (anj_net_ctx_posix_impl_t *) ctx_;

    switch (opt_key) {
    case ANJ_POSIX_SOCKET_OPT_STATE:
        *(anj_net_socket_state_t *) out_value = ctx->state;
        return ANJ_NET_OK;
    case ANJ_POSIX_SOCKET_OPT_INNER_MTU:
        return get_inner_mtu(ctx, (int32_t *) out_value);
    default:
        net_log(L_ERROR, "get option: unknow or unsupported option key %d",
                (int) opt_key);
        return ANJ_NET_EINVAL;
    }
}

static int net_queue_mode_rx_off(anj_net_ctx_t *ctx_) {
    (void) ctx_;
    return ANJ_NET_OK;
}

#    ifdef ANJ_NET_WITH_UDP

int anj_udp_create_ctx(anj_net_ctx_t **ctx, const anj_net_config_t *config) {
    return create_net_ctx(ctx, config);
}

int anj_udp_cleanup_ctx(anj_net_ctx_t **ctx) {
    return cleanup_ctx(ctx);
}

int anj_udp_connect(anj_net_ctx_t *ctx,
                    const char *hostname,
                    const char *port) {
    return net_connect(ctx, hostname, port);
}

int anj_udp_send(anj_net_ctx_t *ctx,
                 size_t *bytes_sent,
                 const uint8_t *buf,
                 size_t length) {
    return net_send(ctx, bytes_sent, buf, length);
}

int anj_udp_recv(anj_net_ctx_t *ctx,
                 size_t *bytes_received,
                 uint8_t *buf,
                 size_t length) {
    return net_recv(ctx, bytes_received, buf, length);
}

int anj_udp_close(anj_net_ctx_t *ctx) {
    return net_close(ctx);
}

int anj_udp_get_state(anj_net_ctx_t *ctx, anj_net_socket_state_t *out_value) {
    return net_get_opt(ctx, out_value, ANJ_POSIX_SOCKET_OPT_STATE);
}

int anj_udp_get_inner_mtu(anj_net_ctx_t *ctx, int32_t *out_value) {
    return net_get_opt(ctx, out_value, ANJ_POSIX_SOCKET_OPT_INNER_MTU);
}

int anj_udp_queue_mode_rx_off(anj_net_ctx_t *ctx) {
    return net_queue_mode_rx_off(ctx);
}
#    endif // ANJ_NET_WITH_UDP
#else      // ANJ_WITH_SOCKET_POSIX_COMPAT
// HACK: This typedef suppress empty translation unit warning.
typedef int _translation_unit_not_empty;
#endif     // ANJ_WITH_SOCKET_POSIX_COMPAT
