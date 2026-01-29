/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#if !defined(_POSIX_C_SOURCE) && !defined(__APPLE__)
#    define _POSIX_C_SOURCE 200809L
#endif

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/compat/net/anj_udp.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include <anj_unit_test.h>

#define DEFAULT_HOSTNAME "localhost"
#define DEFAULT_HOST_IPV4 "127.0.0.1"
#define DEFAULT_HOST_IPV6 "::1"
#define DEFAULT_PORT "9998"

#define ANJ_NET_FAILED (-1)
#define ANJ_NET_EINVAL (-2)
#define ANJ_NET_EIO (-3)
#define ANJ_NET_ENOTCONN (-4)
#define ANJ_NET_EBADFD (-5)
#define ANJ_NET_ENOMEM (-6)

static int setup_local_server(int af, const char *port_str) {
    int server_sock;
    uint16_t port_in_host_order = (uint16_t) atoi(port_str);

    server_sock = socket(af, SOCK_DGRAM, 0);
    if (server_sock == -1) {
        return -1;
    }

    if (af == AF_INET6) {
        struct sockaddr_in6 server_addr = { 0 };
        server_addr.sin6_family = af;
        server_addr.sin6_addr = in6addr_loopback;
        server_addr.sin6_port = htons(port_in_host_order);

        // Allow reusing the address immediately after the server is closed
        int one = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#ifdef SO_REUSEPORT
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
#endif
        if (bind(server_sock, (struct sockaddr *) &server_addr,
                 sizeof(server_addr))
                == -1) {
            close(server_sock);
            return -1;
        }
    } else {
        struct sockaddr_in server_addr = { 0 };
        server_addr.sin_family = af;
        server_addr.sin_addr.s_addr = inet_addr(DEFAULT_HOST_IPV4);
        server_addr.sin_port = htons(port_in_host_order);
        int one = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#ifdef SO_REUSEPORT
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
#endif
        if (bind(server_sock, (struct sockaddr *) &server_addr,
                 sizeof(server_addr))
                == -1) {
            close(server_sock);
            return -1;
        }
    }

    return server_sock;
}

static int accept_incomming_conn_keep_listening_sock_open(
        int listen_sockfd, uint16_t allowed_port_in_host_order) {
    struct sockaddr_in cli_addr;
    socklen_t len = sizeof(cli_addr);
    int conn_sockfd =
            accept(listen_sockfd, (struct sockaddr *) &cli_addr, &len);

    if (allowed_port_in_host_order != 0) {
        if (cli_addr.sin_port != htons(allowed_port_in_host_order)) {
            /* Incorrect connection port, drop connection */
            close(conn_sockfd);
            conn_sockfd = -1;
        }
    }

    return conn_sockfd;
}

static int accept_incomming_conn(int listen_sockfd, uint16_t allowed_port) {
    int ret = accept_incomming_conn_keep_listening_sock_open(listen_sockfd,
                                                             allowed_port);
    close(listen_sockfd);
    return ret;
}

static int
test_udp_connection_by_hostname(anj_net_ctx_t *ctx, int af, const char *host) {
    (void) af;
    /* Create server side socket */
    int sockfd = setup_local_server(AF_INET, DEFAULT_PORT);
    ANJ_UNIT_ASSERT_NOT_EQUAL(sockfd, -1);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_connect(ctx, host, DEFAULT_PORT), ANJ_NET_OK);

    return sockfd;
}

static int test_default_udp_connection(anj_net_ctx_t *ctx, int af) {
    return test_udp_connection_by_hostname(
            ctx, af, af == AF_INET ? DEFAULT_HOST_IPV4 : DEFAULT_HOST_IPV6);
}

// Checking if IPv4 and IPv6 addresses are available on the host under
// localhost
ANJ_UNIT_TEST(check_host_machine, check_localhost_address) {
    struct addrinfo *servinfo = NULL;
    struct addrinfo hints;
    bool is_ipv4 = false;
    bool is_ipv6 = false;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = 0;

    hints.ai_family = AF_INET;
    if (!getaddrinfo("localhost", NULL, &hints, &servinfo) && servinfo) {
        is_ipv4 = true;
    }

    freeaddrinfo(servinfo);

    hints.ai_family = AF_INET6;
    if (!getaddrinfo("localhost", NULL, &hints, &servinfo) && servinfo) {
        is_ipv6 = true;
    }

    freeaddrinfo(servinfo);

    ANJ_UNIT_ASSERT_TRUE(is_ipv4);
    ANJ_UNIT_ASSERT_TRUE(is_ipv6);
}

/*
 * UDP TESTS
 */
ANJ_UNIT_TEST(udp_socket, create_context) {
    anj_net_ctx_t *udp_sock_ctx = NULL;
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, NULL), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NOT_NULL(udp_sock_ctx);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);
}

ANJ_UNIT_TEST(udp_socket, get_opt) {
    anj_net_ctx_t *udp_sock_ctx = NULL;
    int32_t mtu;
    anj_net_socket_state_t state;

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, NULL), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NOT_NULL(udp_sock_ctx);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_get_state(udp_sock_ctx, &state), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(state, ANJ_NET_SOCKET_STATE_CLOSED);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_get_inner_mtu(udp_sock_ctx, &mtu),
                          ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(mtu, 548);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);
}

// skip test if IP_MTU is not defined - MacOS does not define it
#ifdef IP_MTU
ANJ_UNIT_TEST(udp_socket, get_mtu_after_connect) {
    anj_net_ctx_t *udp_sock_ctx = NULL;
    anj_net_socket_configuration_t sock_config = {
        .af_setting = ANJ_NET_AF_SETTING_FORCE_INET4
    };
    anj_net_config_t config = {
        .raw_socket_config = sock_config
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, &config),
                          ANJ_NET_OK);

    int sockfd = test_default_udp_connection(udp_sock_ctx, AF_INET);

    int32_t mtu;
    socklen_t dummy = sizeof(mtu);
    int32_t inner_mtu;
    // HACK: Generally, next line is an antipattern, but it's only a unit test
    // of a compat layer. The next line works only when sockfd of type int is
    // the first field of anj_net_ctx_t.
    getsockopt(*((int *) udp_sock_ctx), IPPROTO_IP, IP_MTU, &mtu, &dummy);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_get_inner_mtu(udp_sock_ctx, &inner_mtu),
                          ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(inner_mtu + 28, mtu);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

ANJ_UNIT_TEST(udp_socket, get_mtu_after_connect_ipv6) {
    anj_net_ctx_t *udp_sock_ctx = NULL;
    anj_net_socket_configuration_t sock_config = {
        .af_setting = ANJ_NET_AF_SETTING_FORCE_INET6
    };
    anj_net_config_t config = {
        .raw_socket_config = sock_config
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, &config),
                          ANJ_NET_OK);

    int sockfd = test_default_udp_connection(udp_sock_ctx, AF_INET6);

    int32_t mtu;
    socklen_t dummy = sizeof(mtu);
    int32_t inner_mtu;
    // HACK: Generally, next line is an antipattern, but it's only a unit test
    // of a compat layer. The next line works only when sockfd of type int is
    // the first field of anj_net_ctx_t.
    getsockopt(*((int *) udp_sock_ctx), IPPROTO_IP, IP_MTU, &mtu, &dummy);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_get_inner_mtu(udp_sock_ctx, &inner_mtu),
                          ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(inner_mtu + 48, mtu);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}
#endif // IP_MTU

// on MacOS even if data was sent, recv might return EAGAIN in the first call
#define UDP_RECV(Bytes_to_read)                                           \
    while (1) {                                                           \
        int ret = anj_udp_recv(udp_sock_ctx, &bytes_received, buf,        \
                               Bytes_to_read);                            \
        ANJ_UNIT_ASSERT_TRUE(ret == ANJ_NET_OK || ret == ANJ_NET_EAGAIN); \
        if (ret == ANJ_NET_OK) {                                          \
            break;                                                        \
        }                                                                 \
    }
// same as above but for MSG_SIZE error case
#define UDP_RECV_MSG_SIZE(Bytes_to_read)                           \
    while (1) {                                                    \
        int ret = anj_udp_recv(udp_sock_ctx, &bytes_received, buf, \
                               Bytes_to_read);                     \
        ANJ_UNIT_ASSERT_TRUE(ret == ANJ_NET_EMSGSIZE               \
                             || ret == ANJ_NET_EAGAIN);            \
        if (ret == ANJ_NET_EMSGSIZE) {                             \
            break;                                                 \
        }                                                          \
    }

ANJ_UNIT_TEST(udp_socket, connect_ipv4) {
    size_t bytes_sent = 0;
    size_t bytes_received = 0;

    anj_net_ctx_t *udp_sock_ctx = NULL;
    anj_net_socket_configuration_t sock_config = {
        .af_setting = ANJ_NET_AF_SETTING_FORCE_INET4
    };
    anj_net_config_t config = {
        .raw_socket_config = sock_config
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, &config),
                          ANJ_NET_OK);

    int sockfd = test_default_udp_connection(udp_sock_ctx, AF_INET);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_send(udp_sock_ctx, &bytes_sent,
                                       (const uint8_t *) "hello", 5),
                          ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(bytes_sent, 5);

    uint8_t buf[100];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    ANJ_UNIT_ASSERT_EQUAL(recvfrom(sockfd, buf, sizeof(buf), 0,
                                   (struct sockaddr *) &client_addr,
                                   &client_addr_len),
                          5);

    ANJ_UNIT_ASSERT_NOT_EQUAL(connect(sockfd, (struct sockaddr *) &client_addr,
                                      client_addr_len),
                              -1);

    ANJ_UNIT_ASSERT_EQUAL(send(sockfd, (const uint8_t *) "world!", 6, 0), 6);
    ANJ_UNIT_ASSERT_EQUAL(send(sockfd, "Have a nice day.", 16, 0), 16);
    UDP_RECV(sizeof(buf));
    ANJ_UNIT_ASSERT_EQUAL(bytes_received, 6);
    UDP_RECV(sizeof(buf));
    ANJ_UNIT_ASSERT_EQUAL(bytes_received, 16);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

ANJ_UNIT_TEST(udp_socket, msg_too_big_for_recv) {
    size_t bytes_sent = 0;
    size_t bytes_received = 0;

    anj_net_ctx_t *udp_sock_ctx = NULL;
    anj_net_socket_configuration_t sock_config = {
        .af_setting = ANJ_NET_AF_SETTING_FORCE_INET4
    };
    anj_net_config_t config = {
        .raw_socket_config = sock_config
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, &config),
                          ANJ_NET_OK);

    int sockfd = test_default_udp_connection(udp_sock_ctx, AF_INET);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_send(udp_sock_ctx, &bytes_sent,
                                       (const uint8_t *) "hello", 5),
                          ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(bytes_sent, 5);

    uint8_t buf[100] = { 0 };
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    ANJ_UNIT_ASSERT_EQUAL(recvfrom(sockfd, buf, sizeof(buf), 0,
                                   (struct sockaddr *) &client_addr,
                                   &client_addr_len),
                          5);

    ANJ_UNIT_ASSERT_NOT_EQUAL(connect(sockfd, (struct sockaddr *) &client_addr,
                                      client_addr_len),
                              -1);

    ANJ_UNIT_ASSERT_EQUAL(send(sockfd, (const uint8_t *) "world!", 6, 0), 6);
    UDP_RECV_MSG_SIZE(5);
    ANJ_UNIT_ASSERT_EQUAL(bytes_received, 5);

    ANJ_UNIT_ASSERT_EQUAL(send(sockfd, (const uint8_t *) "world!", 6, 0), 6);
    UDP_RECV_MSG_SIZE(6);
    ANJ_UNIT_ASSERT_EQUAL(bytes_received, 6);

    memset(buf, '\0', sizeof(buf));
    ANJ_UNIT_ASSERT_EQUAL(send(sockfd, (const uint8_t *) "world!", 6, 0), 6);
    UDP_RECV(7);
    ANJ_UNIT_ASSERT_EQUAL(bytes_received, 6);
    ANJ_UNIT_ASSERT_EQUAL_STRING((const char *) buf, "world!");

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

ANJ_UNIT_TEST(udp_socket, call_with_NULL_ctx) {
    uint8_t buf[100];
    size_t bytes_sent = 0;
    size_t bytes_received = 0;

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(NULL, NULL), ANJ_NET_EINVAL);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_connect(NULL, DEFAULT_HOSTNAME, DEFAULT_PORT),
                          ANJ_NET_EBADFD);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_send(NULL, &bytes_sent, buf, 10),
                          ANJ_NET_EBADFD);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_recv(NULL, &bytes_received, buf, 10),
                          ANJ_NET_EBADFD);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_close(NULL), ANJ_NET_EBADFD);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(NULL), ANJ_NET_EBADFD);
}

ANJ_UNIT_TEST(udp_socket, connect_ipv4_invalid_hostname) {
    anj_net_ctx_t *udp_sock_ctx = NULL;
    anj_net_socket_configuration_t sock_config = {
        .af_setting = ANJ_NET_AF_SETTING_FORCE_INET4
    };
    anj_net_config_t config = {
        .raw_socket_config = sock_config
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, &config),
                          ANJ_NET_OK);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_connect(udp_sock_ctx, NULL, DEFAULT_PORT),
                          ANJ_NET_EINVAL);
    ANJ_UNIT_ASSERT_EQUAL(
            anj_udp_connect(udp_sock_ctx,
                            "supper_dummy_host_name_not_exist.com",
                            DEFAULT_PORT),
            ANJ_NET_FAILED);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);
}

ANJ_UNIT_TEST(udp_socket, connect_invalid_port) {
    anj_net_ctx_t *udp_sock_ctx = NULL;
    anj_net_socket_configuration_t sock_config = {
        .af_setting = ANJ_NET_AF_SETTING_FORCE_INET4
    };
    anj_net_config_t config = {
        .raw_socket_config = sock_config
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, &config),
                          ANJ_NET_OK);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_connect(udp_sock_ctx, DEFAULT_HOST_IPV4, ""),
                          ANJ_NET_EINVAL);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_connect(udp_sock_ctx, DEFAULT_HOST_IPV4,
                                          "PORT_NUMBER"),
                          ANJ_NET_EINVAL);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_connect(udp_sock_ctx, DEFAULT_HOST_IPV4,
                                          NULL),
                          ANJ_NET_EINVAL);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);
}

ANJ_UNIT_TEST(udp_socket, connect_ipv6) {
    anj_net_ctx_t *udp_sock_ctx = NULL;
    anj_net_socket_configuration_t sock_config = {
        .af_setting = ANJ_NET_AF_SETTING_FORCE_INET6
    };
    anj_net_config_t config = {
        .raw_socket_config = sock_config
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, &config),
                          ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NOT_NULL(udp_sock_ctx);

    int sockfd = test_udp_connection_by_hostname(udp_sock_ctx, AF_INET6,
                                                 DEFAULT_HOSTNAME);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

ANJ_UNIT_TEST(udp_socket, state_transition) {
    anj_net_socket_state_t state;

    anj_net_ctx_t *udp_sock_ctx = NULL;
    anj_net_socket_configuration_t sock_config = {
        .af_setting = ANJ_NET_AF_SETTING_FORCE_INET4
    };
    anj_net_config_t config = {
        .raw_socket_config = sock_config
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, &config),
                          ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_get_state(udp_sock_ctx, &state), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(state, ANJ_NET_SOCKET_STATE_CLOSED);

    /* Create server side socket */
    int sockfd = setup_local_server(AF_INET, DEFAULT_PORT);
    ANJ_UNIT_ASSERT_NOT_EQUAL(sockfd, -1);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_connect(udp_sock_ctx, DEFAULT_HOST_IPV4,
                                          DEFAULT_PORT),
                          ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_get_state(udp_sock_ctx, &state), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(state, ANJ_NET_SOCKET_STATE_CONNECTED);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_close(udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_get_state(udp_sock_ctx, &state), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_EQUAL(state, ANJ_NET_SOCKET_STATE_CLOSED);

    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);
    ANJ_UNIT_ASSERT_NULL(udp_sock_ctx);

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

ANJ_UNIT_TEST(udp_socket, op_on_already_closed_socket) {
    uint8_t buf[100];
    size_t bytes_sent = 0;
    size_t bytes_received = 0;

    anj_net_ctx_t *udp_sock_ctx = NULL;
    anj_net_socket_configuration_t sock_config = {
        .af_setting = ANJ_NET_AF_SETTING_FORCE_INET4
    };
    anj_net_config_t config = {
        .raw_socket_config = sock_config
    };
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_create_ctx(&udp_sock_ctx, &config),
                          ANJ_NET_OK);

    int sockfd = test_default_udp_connection(udp_sock_ctx, AF_INET);

    /* after test cleanup */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_close(udp_sock_ctx), ANJ_NET_OK);

    /* repeat shutdown and close */
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_send(udp_sock_ctx, &bytes_sent, buf, 10),
                          ANJ_NET_EBADFD);
    ANJ_UNIT_ASSERT_EQUAL(bytes_sent, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_recv(udp_sock_ctx, &bytes_received, buf, 10),
                          ANJ_NET_EBADFD);
    ANJ_UNIT_ASSERT_EQUAL(bytes_received, 0);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_close(udp_sock_ctx), ANJ_NET_EBADFD);
    ANJ_UNIT_ASSERT_EQUAL(anj_udp_cleanup_ctx(&udp_sock_ctx), ANJ_NET_OK);

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}
