..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Network API
===========

Overview
--------

Anjay Lite provides a reference implementation of its network API, designed for
systems that support the BSD-style socket API. You can find the full
implementation in `src/anj/compat/posix/anj_socket.c`.

If your target platform does not support BSD-style sockets, you'll need to
implement a custom network compatibility layer for Anjay Lite.

To help with this process, Anjay Lite includes a tutorial that demonstrates a
minimal network implementation:

.. toctree::
   :titlesonly:

   NetworkingAPI/MinimalSocketImplementation

.. note::
   For practical guidance, see the :ref:`integrations` section, 
   which features example implementations of custom networking layers for various platforms. 
   Explore these sections to find platform-specific tips and ready-to-use code samples that can help accelerate your porting process.

Functions to implement
----------------------

When building a custom networking layer, you must provide implementations for a set of functions.
This tutorial assumes support for UDP connections, and therefore uses the ``anj_udp_*`` naming convention.

.. note::
   Anjay Lite currently supports only UDP and DTLS binding.
   Support for additional bindings may be added later.

If POSIX socket API is not available:

- Use ``ANJ_WITH_SOCKET_POSIX_COMPAT=OFF`` when running CMake on Anjay Lite.
- Implement the following mandatory functions:

+-------------------------------+---------------------------------------------------------------------------------------+
| Function                      | Purpose                                                                               |
+===============================+=======================================================================================+
| ``anj_udp_create_ctx``        | Create and initialize a network context.                                              |
+-------------------------------+---------------------------------------------------------------------------------------+
| ``anj_udp_connect``           | Connect the socket to a server address.                                               |
+-------------------------------+---------------------------------------------------------------------------------------+
| ``anj_udp_send``              | Send data through the socket.                                                         |
+-------------------------------+---------------------------------------------------------------------------------------+
| ``anj_udp_recv``              | Receive data from the socket.                                                         |
+-------------------------------+---------------------------------------------------------------------------------------+
| ``anj_udp_shutdown``          | Shut down socket communication.                                                       |
+-------------------------------+---------------------------------------------------------------------------------------+
| ``anj_udp_close``             | Close the socket.                                                                     |
+-------------------------------+---------------------------------------------------------------------------------------+
| ``anj_udp_cleanup_ctx``       | Clean up and free the network context.                                                |
+-------------------------------+---------------------------------------------------------------------------------------+
| ``anj_udp_get_inner_mtu``     | Returns the maximum size of a buffer that can be passed to ``anj_udp_send()``.        |
+-------------------------------+---------------------------------------------------------------------------------------+
| ``anj_udp_get_state``         | Return the current socket state.                                                      |
+-------------------------------+---------------------------------------------------------------------------------------+
| ``anj_net_queue_mode_rx_off`` | Used by Anjay Lite when entering Queue Mode. Can return ``ANJ_NET_OK`` if unused.     |
+-------------------------------+---------------------------------------------------------------------------------------+

.. attention::
    Some API functions are allowed to return ``ANJ_NET_EINPROGRESS``. Note that using this behaviour
    may cause library to stuck in continues calling of the function, instead of proceeding with
    fallback procedure.

.. note::
    For function signatures and detailed description of listed functions, see
    ``include_public/anj/compat/net/anj_net_api.h``.
