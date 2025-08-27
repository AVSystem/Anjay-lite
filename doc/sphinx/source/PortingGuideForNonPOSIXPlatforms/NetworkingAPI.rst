..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Network API
===========

Reference implementations
-------------------------

Anjay Lite provides a reference implementation of its network API, designed to
work with systems that support the BSD-style socket API. You can find the full
implementation in `src/anj/compat/posix/anj_socket.c`. If your target platform
does not support BSD-style sockets, you must implement a custom network
compatibility layer that Anjay Lite can use.

To help with this process, Anjay Lite includes tutorial examples featuring minimal and compact implementations:

.. toctree::
   :titlesonly:

   NetworkingAPI/MinimalSocketImplementation
   NetworkingAPI/ReuseLastPort

.. note::
   For practical guidance, see the :ref:`integrations` section, 
   which features example implementations of custom networking layers for various platforms. 
   Explore these sections to find platform-specific tips and ready-to-use code samples that can help accelerate your porting process.

Functions to implement
----------------------

When building a custom networking layer, you must provide implementations for a set of functions.
This tutorial assumes support for UDP connections, and therefore uses the ``anj_udp_*`` naming convention.

.. note::
   Currently Anjay Lite supports only UDP binding. We may add support for other
   bindings in the future.

If POSIX socket API is not available:

- Use ``ANJ_WITH_SOCKET_POSIX_COMPAT=OFF`` when running CMake on Anjay Lite,
- Implement the following mandatory functions:

+----------------------------+-----------------------------------------------------------------------------------+
| Function                   | Purpose                                                                           |
+============================+===================================================================================+
| ``anj_udp_create_ctx``     | Create and initialize a network context.                                          |
+----------------------------+-----------------------------------------------------------------------------------+
| ``anj_udp_connect``        | Connect the socket to a server address.                                           |
+----------------------------+-----------------------------------------------------------------------------------+
| ``anj_udp_send``           | Send data through the socket.                                                     |
+----------------------------+-----------------------------------------------------------------------------------+
| ``anj_udp_recv``           | Receive data from the socket.                                                     |
+----------------------------+-----------------------------------------------------------------------------------+
| ``anj_udp_shutdown``       | Shut down socket communication.                                                   |
+----------------------------+-----------------------------------------------------------------------------------+
| ``anj_udp_close``          | Close the socket.                                                                 |
+----------------------------+-----------------------------------------------------------------------------------+
| ``anj_udp_cleanup_ctx``    | Clean up and free the network context.                                            |
+----------------------------+-----------------------------------------------------------------------------------+
| ``anj_udp_get_inner_mtu``  | Returns the maximum size of a buffer that can be passed to ``anj_udp_send()``.    |
+----------------------------+-----------------------------------------------------------------------------------+
| ``anj_udp_get_state``      | Return the current state of the socket.                                           |
+----------------------------+-----------------------------------------------------------------------------------+

Optional functions
------------------

Implementing the following additional functions enables enhanced features:

+-------------------------------+---------------------------------------------------------------------------------------------+
| Function                      | Purpose                                                                                     |
+===============================+=============================================================================================+
| ``anj_udp_reuse_last_port``   | Required to support Queue Mode. If Queue Mode is not needed, implement a stub that returns  |
|                               | ``ANJ_NET_ENOTSUP``.                                                                        |
+-------------------------------+---------------------------------------------------------------------------------------------+
| ``anj_udp_get_bytes_received``| Not used directly by Anjay Lite. Useful for gathering statistics on incoming traffic.       |
+-------------------------------+---------------------------------------------------------------------------------------------+
| ``anj_udp_get_bytes_sent``    | Not used directly by Anjay Lite. Useful for gathering statistics on outgoing traffic.       |
+-------------------------------+---------------------------------------------------------------------------------------------+
| ``anj_udp_get_system_socket`` | Not used by Anjay Lite. Can be used to suspend the device until a packet is received.       |
+-------------------------------+---------------------------------------------------------------------------------------------+

.. note::
    For signatures and detailed description of listed functions, see
    `include_public/anj/compat/net/anj_net_api.h`
