..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Reuse last port operation
=========================

This tutorial explains how to implement the ``anj_udp_reuse_last_port`` function
to support **Queue Mode** in Anjay Lite. In LwM2M **Queue Mode**, the client may
temporarily disconnect and enter a low-power mode. To maintain the session and allow the
server to resume communication when the client reconnects, the client must reuse
the same local port number across connections. If the client changes the port,
the server may treat it as a new device, disrupting the ongoing session.

.. note::
    Reusing the same port is also important when operating behind a NAT (Network Address Translation) device.
    NAT devices map external connections to the internal IP address and port of the client.
    If the client changes its local port, the NAT mapping may be lost, and the LwM2M server may
    no longer be able to reach the device after it reconnects. Maintaining a consistent local
    port helps preserve the NAT binding and ensures reliable communication.

    However, NAT entries may expire after periods of inactivity. Even with port reuse enabled, the NAT
    device may assign a different external port. This behavior depends on the NAT implementation and
    is beyond the client's control.

.. note::
    This tutorial builds upon the Minimal Socket Implementation. The complete source
    code for this example can be found in the `examples/custom-network/reuse-port`
    directory.

The ``anj_udp_reuse_last_port`` function is called by Anjay Lite after closing an
existing connection and before establishing a new one. If the socket context has
not established any previous connection, the function returns an error.

You can find more information about Queue Mode and how to enable it in Anjay Lite
:doc:`here <../../AdvancedTopics/AT-QueueMode>`.

Prepare socket binding for port reuse
-------------------------------------

Update the network compatibility layer to support binding the socket to a specific
port. This is necessary for reusing the same port across multiple connections,
which is especially important for enabling **Queue Mode**.

Extend the ``net_ctx_posix_impl_t`` structure to store the last local port used
by the client:

.. highlight:: c
.. snippet-source:: examples/custom-network/reuse-port/src/net.c
    :emphasize-lines: 4-5

    typedef struct net_ctx_posix_impl {
        sockfd_t sockfd;
        anj_net_socket_state_t state;
        bool local_port_was_set;
        uint16_t port; // client side connection port number in network order
    } net_ctx_posix_impl_t;

**How it works**

    - ``local_port_was_set`` tracks whether a port has been assigned.
    - ``port`` stores the last used local port number.

These fields will later be used during socket creation to attempt binding to the
same port when reconnecting.

Add two helper functions: ``store_local_port_in_ctx`` and ``create_net_socket``.

.. highlight:: c
.. snippet-source:: examples/custom-network/reuse-port/src/net.c
    :emphasize-lines: 13,25-26

    static void store_local_port_in_ctx(net_ctx_posix_impl_t *ctx) {
        struct sockaddr addr;
        socklen_t addrlen = sizeof(addr);

        ctx->local_port_was_set = false;

        if (getsockname(ctx->sockfd, &addr, &addrlen)) {
            return;
        }

        /* assume IPv4 address family */
        struct sockaddr_in *addr_in = (struct sockaddr_in *) &addr;
        ctx->port = addr_in->sin_port;
        ctx->local_port_was_set = true;
    }

    static int create_net_socket(net_ctx_posix_impl_t *ctx) {
        ctx->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (ctx->sockfd < 0) {
            return NET_GENERAL_ERROR;
        }

        /* Always allow for reuse of address */
        int reuse_addr = 1;
        if (setsockopt(ctx->sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                       sizeof(reuse_addr))) {
            close(ctx->sockfd);
            ctx->sockfd = INVALID_SOCKET;
            return NET_GENERAL_ERROR;
        }

        return ANJ_NET_OK;
    }

**How it works**

    - ``store_local_port_in_ctx()`` saves the assigned local port after a successful connection.
    - ``create_net_socket()`` uses the ``socket()`` function to create a new socket descriptor.
      It also  ensures that the socket allows address reuse using
      the ``SO_REUSEADDR`` option. This allows the operating system to bind to a recently
      used ephemeral port, even if it hasn't fully released it yet ensuring smoother
      reconnections and better compatibility across different platforms.

Enhance the ``anj_udp_connect`` function to integrate the ``create_net_socket``
and ``store_local_port_in_ctx`` helper functions.

.. highlight:: c
.. snippet-source:: examples/custom-network/reuse-port/src/net.c
    :emphasize-lines: 1-6,15

    if (ctx->sockfd == INVALID_SOCKET) {
        if (create_net_socket(ctx)) {
            freeaddrinfo(serverinfo);
            return NET_GENERAL_ERROR;
        }
    }

    if (connect(ctx->sockfd, serverinfo->ai_addr, serverinfo->ai_addrlen)) {
        freeaddrinfo(serverinfo);
        return NET_GENERAL_ERROR;
    }
    set_socket_non_blocking(ctx->sockfd);
    ctx->state = ANJ_NET_SOCKET_STATE_CONNECTED;

    store_local_port_in_ctx(ctx);

**How it works**

    - If the current socket is invalid (``INVALID_SOCKET``), it creates a new one using ``create_net_socket``. 
    - After establishing a connection, it saves the assigned local port using ``store_local_port_in_ctx``.
      This ensures the client can reuse the same port during future reconnections.

Implement port reuse
--------------------

We are now ready to implement the ``anj_udp_reuse_last_port`` function.

Define a helper function ``net_bind_to_local_port`` that handles the socket
creation and bind operation:

.. highlight:: c
.. snippet-source:: examples/custom-network/reuse-port/src/net.c

    static int net_bind_to_local_port(net_ctx_posix_impl_t *ctx,
                                      struct addrinfo **serverinfo,
                                      const uint16_t port_in_net_order) {
        char port[ANJ_U16_STR_MAX_LEN + 1];
        if (!anj_uint16_to_string_value(port, ntohs(port_in_net_order))) {
            return NET_GENERAL_ERROR;
        }

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo("0.0.0.0", port, &hints, serverinfo)) {
            return NET_GENERAL_ERROR;
        }

        // Use the first entry in serverinfo
        struct addrinfo *local_addr = *serverinfo;
        if (!local_addr) {
            return NET_GENERAL_ERROR;
        }

        if (create_net_socket(ctx)) {
            return NET_GENERAL_ERROR;
        }

        if (bind(ctx->sockfd, local_addr->ai_addr, local_addr->ai_addrlen) < 0) {
            return NET_GENERAL_ERROR;
        }

        ctx->state = ANJ_NET_SOCKET_STATE_BOUND;
        return ANJ_NET_OK;
    }

**How it works**

    - It calls ``getaddrinfo()`` with the wildcard IPv4 address (0.0.0.0) and the
      port number we wish to use on the device side for the connection. This
      prepares a local address that can be used for binding.
    - After creating a new socket, it binds the socket to the first address returned
      in the serverinfo structure. If the bind operation succeeds, the socket
      state is updated to ``ANJ_NET_SOCKET_STATE_BOUND``.

Define the ``anj_udp_reuse_last_port`` function itself:

.. highlight:: c
.. snippet-source:: examples/custom-network/reuse-port/src/net.c

    int anj_udp_reuse_last_port(anj_net_ctx_t *ctx_) {
        net_ctx_posix_impl_t *ctx = (net_ctx_posix_impl_t *) ctx_;
        if (!ctx->local_port_was_set) {
            return NET_GENERAL_ERROR;
        }

        struct addrinfo *serverinfo = NULL;
        int ret = net_bind_to_local_port(ctx, &serverinfo, ctx->port);

        if (serverinfo) {
            freeaddrinfo(serverinfo);
        }

        return ret;
    }

**How it works**

    - Checks if a previous local port is available.
    - Binds the socket to the stored port for reuse.

Summary
-------

After completing this implementation, Anjay Lite will:

    - establish a connection with the LwM2M server,
    - enter **Queue Mode** after 5 seconds (as configured),
    - reuse the same local port for all subsequent connections.

By reusing the local port, the client can send an Update message directly after 
awaking from **Queue Mode**, without needing to perform a full registration.
This ensures session continuity and more efficient communication with the LwM2M
server.
