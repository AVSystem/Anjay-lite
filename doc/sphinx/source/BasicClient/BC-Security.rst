..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Enabling Secure Communication
=============================

Anjay-Lite can be compiled with support for DTLS and linked with the mbedTLS
library. When enabled, the DTLS session is configured from the Resources of the
``LwM2M Security`` Object (ID ``/0``).

This article is based on the *BC-Security* example. The complete code is
available in the `examples/tutorial/BC-Security` directory of the Anjay-Lite
repository.

Prerequisites & build flags
---------------------------

To enable secure communication, you need to configure the appropriate build
flags. These can be set either in the CMake build system or in a custom
anjay_config.h file. A minimal example demonstrating the required flags can be
found in ``examples/tutorial/BC-Security/CMakeLists.txt`` and it looks like this:

.. highlight:: cmake
.. snippet-source:: examples/tutorial/BC-Security/CMakeLists.txt

    set(ANJ_WITH_MBEDTLS ON)
    set(ANJ_WITH_SECURITY ON)
    set(ANJ_NET_WITH_DTLS ON)

- ``ANJ_WITH_MBEDTLS`` — enable the bundled mbedTLS integration.
- ``ANJ_WITH_SECURITY`` — compiles-in code handling security features within
  Anjay-Lite.
- ``ANJ_NET_WITH_DTLS`` — enable DTLS transport support.

.. note::
   For more information about these flags, see the ``anjay_config.h.in`` file.

Anjay-Lite with Default mbedTLS Support
---------------------------------------

The build process of this example targets Linux platforms. For custom embedded
builds, we recommend creating a separate ``CMakeLists.txt`` file that adds all
source files for both Anjay-Lite and mbedTLS, similarly to what is presented in
:doc:`../Compile_client_applications`.

By providing the ``ANJ_WITH_MBEDTLS`` flag, two things happen:

- Anjay-Lite's CMake fetches the mbedTLS library and compiles it as part of the
  build.
- Anjay-Lite compiles with the bundled mbedTLS compatibility layer
  (``src/anj/compat/posix/anj_mbedtls_dtls_socket.c``).

The ``anj_mbedtls_dtls_socket.c`` compatibility layer depends on the
bundled POSIX UDP socket layer. Therefore, the ``ANJ_WITH_UDP`` option
(enabled by default) must remain enabled. For custom embedded platforms, you may
need to implement your own UDP socket compatibility layer.

.. important::
   Anjay-Lite supports mbedTLS version **3.6.4 and newer**.

.. note::
   When building for Linux with the CMake files shipped with Anjay-Lite,
   you can set your own ``MBEDTLS_VERSION`` or ``MBEDTLS_ROOT_DIR``.
   See ``cmake/anjay_lite_mbedtls.cmake`` for details.

Supported Security Modes
------------------------

The security mode is determined based on the *Security Mode* Resource in a given
instance of the Security Object (``/0/*/2``). Supported values are:

* ``0`` - **Pre-Shared Key (PSK) mode** — DTLS with PSK is used.
  Communication is symmetrically encrypted and authenticated using the same
  secret key, shared between the server and the client.

  * The TLS-PSK identity is stored in the *Public Key or Identity* Resource
    (``/0/*/3``). It is a string identifying the key being used, so that the
    server can uniquely determine which key to use for communication. This
    string shall be directly stored in this Resource.

  * The *Secret Key* (``/0/*/5``) Resource shall contain the secret pre-shared
    key itself, in an opaque binary format appropriate for the cipher suite used
    by the server.

* ``3`` - **NoSec mode** — In this mode, encryption and authentication are
  disabled completely and the CoAP messages are transmitted in plain text over
  the network. It must not be used in production environments, unless end-to-end
  security is provided at a lower layer (e.g. IPsec). It may be useful for
  development, testing, and debugging.

The *Raw Public Key*, *Certificate*, and *Certificate with EST* modes described
in the LwM2M specification are **not currently supported**.

**In this tutorial, we will focus on enabling security using the PSK mode.**

Provisioning Security Configuration
-----------------------------------

According to the LwM2M specification, the aforementioned Resources shall be
provisioned during the Bootstrap Phase. However, if *Bootstrap from Smartcard*
is not used, the Client must contain some factory defaults for connecting to a
LwM2M Server or a LwM2M Bootstrap Server. In this section, we will learn how to
implement such factory defaults for a DTLS connection.

Configuring Encryption Keys
^^^^^^^^^^^^^^^^^^^^^^^^^^^

PSK mode requires an Identity and a Pre-Shared Key in the Security Object. In
Anjay-Lite they are set via ``public_key_or_identity`` and ``secret_key`` fields
of ``anj_dm_security_instance_init_t``.

Code Changes
^^^^^^^^^^^^

Continuing the previous tutorial, we can modify the ``security_inst``
initialization code:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Security/src/main.c
    :emphasize-lines: 6-7, 11-22

    // Installs Security Object and adds an instance of it.
    // An instance of Security Object provides information needed to connect to
    // LwM2M server.
    static int install_security_obj(anj_t *anj,
                                    anj_dm_security_obj_t *security_obj) {
        static const char PSK_IDENTITY[] = "identity";
        static const char PSK_KEY[] = "P4s$w0rd";

        anj_dm_security_instance_init_t security_inst = {
            .ssid = 1,
            .server_uri = "coaps://eu.iot.avsystem.cloud:5684",
            .security_mode = ANJ_DM_SECURITY_PSK,
            .public_key_or_identity = {
                .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
                .info.buffer.data = PSK_IDENTITY,
                .info.buffer.data_size = strlen(PSK_IDENTITY)
            },
            .secret_key = {
                .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
                .info.buffer.data = PSK_KEY,
                .info.buffer.data_size = strlen(PSK_KEY)
            }
        };
        anj_dm_security_obj_init(security_obj);
        if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
                || anj_dm_security_obj_install(anj, security_obj)) {
            return -1;
        }
        return 0;
    }

.. note::
   Notice that the ``server_uri`` field has changed to use the ``coaps`` URI
   scheme and port ``5684`` (the default for secure CoAP).

All remaining activities related to establishing a secure communication channel
with the LwM2M Server are performed automatically by Anjay-Lite.

Operational notes
-----------------

Switching a device from **NoSec** to **PSK** while reusing the same endpoint
name often requires changes on the server side.

.. important::
    For many LwM2M Servers, including the
    `Coiote IoT Device Management platform
    <https://avsystem.com/coiote-iot-device-management-platform/>`_,
    you will need to change the server-side configuration if you previously used
    NoSec connectivity for the same endpoint name.

    The simplest solution might often be to remove the device entry completely
    and create it from scratch.
