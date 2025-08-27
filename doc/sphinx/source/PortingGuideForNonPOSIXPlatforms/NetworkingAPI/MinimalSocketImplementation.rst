..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Minimal Socket Implementation
=============================

.. contents:: :local:

Introduction
------------

This tutorial demonstrates how to implement a minimal UDP network compatibility
layer for Anjay Lite using POSIX sockets. Although Anjay Lite already provides
a built-in implementation for POSIX environments, this example is useful for
understanding how to create a custom network layer.

.. note::
   Code related to this tutorial can be found under `examples/custom-network/minimal`
   in the Anjay Lite source directory and is based on `examples/tutorial/BC-MandatoryObjects`
   example.

Update the build configuration
------------------------------

This example uses CMake as a build system and demonstrates how to provide a basic
UDP network layer implementation.

To disable the default POSIX socket implementation and enable the custom UDP
layer, apply the following changes in `CMakeLists.txt`:

.. highlight:: cmake
.. snippet-source:: examples/custom-network/minimal/CMakeLists.txt
    :emphasize-lines: 7-9,16

    cmake_minimum_required(VERSION 3.16.0)

    project(anjay_lite_minimal_network_api C)

    set(CMAKE_C_STANDARD 99)

    set(ANJ_WITH_SOCKET_POSIX_COMPAT OFF)
    set(ANJ_NET_WITH_UDP ON)
    set(ANJ_NET_WITH_TCP OFF)

    if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
        set(anjay_lite_DIR "../../../cmake")
        find_package(anjay_lite REQUIRED)
    endif()

    add_executable(anjay_lite_minimal_network_api src/main.c src/net.c)

The `examples/custom-network/minimal/src/net.c` file will contain the
custom network compatibility layer implementation.

Limitations
-----------

To ensure clarity, the implementation includes only essential functionality and
has the following limitations:

- Supports only the UDP protocol.
- Supports only IPv4 addresses. Connecting to IPv6 addresses is not possible.
- Does not preserve the local port between multiple connections to the same server.
- Does not validate input parameters.
- Implements minimal error handling. The return values of several POSIX functions
  are not checked, including:

    - ``fcntl``
    - ``close``
    - ``shutdown``

- Does not support socket configuration, such as selecting the address family.
- Uses a fixed inner MTU value, which may not reflect actual network conditions.

Despite these limitations, this implementation is sufficient to connect to a LwM2M server using the networking compatibility layer.

Create socket context
---------------------

The socket context is represented by a custom structure: ``net_ctx_posix_impl_t``.
This structure stores:

    - a file descriptor for the socket (``sockfd``)
    - the current socket state (state)

You can customize this structure to meet the requirements of your specific
implementation. The example below shows a basic implementation:

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c

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

The ``anj_udp_create_ctx`` function initializes the network context by allocating
memory for the ``net_ctx_posix_impl_t`` structure and initialize its values.

.. note::
   If dynamic memory allocation is not allowed in the project, this function
   can assign the ``ctx_`` pointer to a static global structure instead,
   omitting the need to use the ``malloc`` function.

.. note::
   The only positive values that network API functions may return are error codes
   with the ``ANJ_NET_E`` prefix defined in the
   `include_public/anj/compat/net/anj_net_api.h` file (e.g. ``ANJ_NET_EAGAIN``); all
   other positive values are forbidden - they are reserved for client-side logic for
   potential new error codes. Please refer to `anj_net_api.h` for a full list of
   defined error codes and description when specific network API functions can return
   them. 

   ``NET_GENERAL_ERROR`` is defined as ``-1``; however, any negative value may be
   returned to indicate an error, Anjay Lite will treat them in the same way.

Connect
-------

To establish a connection, first create a helper function that sets a socket to
non-blocking mode. Non-blocking sockets prevent Anjay Lite from being halted while
waiting for incoming data.

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c

    static void set_socket_non_blocking(sockfd_t sockfd) {
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
        }
    }

Now implement the ``anj_udp_connect`` function:

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c

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

How it works
^^^^^^^^^^^^

The ``anj_udp_connect`` function performs the following steps:

**Resolve server address**

It uses ``getaddrinfo()`` to convert the provided hostname and port into a list of address
structures suitable for an IPv4 UDP connection.

    - ``hints.ai_family`` is set to ``AF_INET`` (IPv4).
    - ``hints.ai_socktype`` is set to ``SOCK_DGRAM`` (UDP).

.. note::
   The ``getaddrinfo`` function may block, which can halt the execution of Anjay Lite 
   during the call. If non-blocking behavior is required, use an asynchronous variant
   if available. If the connect operation is pending in a non-blocking
   scenario, return ``ANJ_NET_EAGAIN`` to inform Anjay Lite that it needs to be
   called again to finish establishing the connection.

**Create a socket and connect it to the server**

It creates a new IPv4 UDP socket with ``socket(AF_INET, SOCK_DGRAM, 0)``. Then,
it connects the socket to the server address obtained earlier.

If the connection succeeds, the socket is ready for communication with the target host.

**Set socket to non-blocking mode**

It ensures the socket is configured as non-blocking to prevent delays during future send and recv operations.

**Update socket state**

It updates the socket's state to ``ANJ_NET_SOCKET_STATE_CONNECTED``.

**Release address information**

It frees the memory allocated by ``getaddrinfo()``.

.. note::
    Always keep the socket ``state`` correctly updated. Anjay Lite relies on the
    socket state to determine the current connection status.

Send
----

Before implementing the send functionality, create a helper function to check if
an error indicates a blocking condition:

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c

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

The ``would_block`` function checks the ``errno`` value set by the POSIX socket
system calls. It returns ``true`` if the error code indicates that the operation
would have blocked. Otherwise, it returns ``false``.

Now implement ``anj_udp_send``:

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c

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

**How it works**

The ``anj_udp_send`` function acts as a simple wrapper around the standard POSIX ``send`` call:

    - It sends the data from the buffer to the connected socket.
    - If an error occurs it checks whether the error indicates a non-blocking situation.
    - If not all of the data was sent, we return an error, as partial sent is not allowed in the case of UDP.
    - On success, it reports the number of bytes sent via the ``bytes_sent`` output parameter.

Receive
-------

The receive function follows a similar logic to the send function:

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c

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

.. note::
    If the buffer is too small to hold the incoming packet, or matches it exactly
    ``anj_udp_recv`` returns ``ANJ_NET_EMSGSIZE``. This informs Anjay Lite to drop the packet gracefully.
    Any other error is treated as fatal and triggers a connection reset.

Shutdown
--------

The ``anj_udp_shutdown`` function is straightforward but requires updating the socket
context's state to ``ANJ_NET_SOCKET_STATE_SHUTDOWN`` upon completion.

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c
    :emphasize-lines: 6

    int anj_udp_shutdown(anj_net_ctx_t *ctx_) {
        net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;

        shutdown(ctx->sockfd, SHUT_RDWR);

        ctx->state = ANJ_NET_SOCKET_STATE_SHUTDOWN;
        return ANJ_NET_OK;
    }

Close
-----

The ``anj_udp_close`` closes the underlying socket and updates the socket ``state``
to ``ANJ_NET_SOCKET_STATE_CLOSED`` indicating that it is no longer active.

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c
    :emphasize-lines: 7

    int anj_udp_close(anj_net_ctx_t *ctx_) {
        net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;

        close(ctx->sockfd);

        ctx->sockfd = INVALID_SOCKET;
        ctx->state = ANJ_NET_SOCKET_STATE_CLOSED;
        return ANJ_NET_OK;
    }

.. note::
   The context object itself is not cleared here to preserve data for possible reuse.

Context Cleanup
---------------

The cleanup function releases all resources associated with the socket context.
First, it closes the socket if it is still open, then it frees the dynamically
allocated memory that stored the socket context's state.

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c

    int anj_udp_cleanup_ctx(anj_net_ctx_t **ctx_) {
        net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) *ctx_;
        *ctx_ = NULL;

        close(ctx->sockfd);
        free(ctx);

        return ANJ_NET_OK;
    }


Get Inner MTU
-------------

The ``anj_udp_get_inner_mtu`` function returns the assumed maximum transmission
unit for UDP datagrams over IPv4 without the protocol header overhead.

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c

    int anj_udp_get_inner_mtu(anj_net_ctx_t *ctx, int32_t *out_value) {
        (void) ctx;
        *out_value = 548; /* 576 (IPv4 MTU) - 28 bytes of headers */
        return ANJ_NET_OK;
    }

.. note::
    This is a static implementation. In a real-world project, retrieve the
    actual MTU dynamically using ``getsockopt()``.

Get State
---------

The ``anj_udp_get_state`` function allows Anjay Lite to retrieve the current connection status of the context.

.. highlight:: c
.. snippet-source:: examples/custom-network/minimal/src/net.c

    int anj_udp_get_state(anj_net_ctx_t *ctx_, anj_net_socket_state_t *out_value) {
        net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;
        *(anj_net_socket_state_t *) out_value = ctx->state;
        return ANJ_NET_OK;
    }
