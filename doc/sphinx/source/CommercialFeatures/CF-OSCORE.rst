..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

OSCORE
======

Overview
--------

OSCORE (Object Security for Constrained RESTful Environments) is a security
protocol defined in `RFC 8613 <https://datatracker.ietf.org/doc/html/rfc8613>`_
that provides end-to-end protection of CoAP messages at the application layer.

OSCORE does not require a full transport-layer security stack and the significant
memory overhead associated with (D)TLS. Instead, it achieves message protection
using CBOR Object Signing and Encryption (COSE). This results in a `significantly
smaller code footprint and lower RAM usage <https://www.sciencedirect.com/science/article/pii/S2542660520301645>`_, 
making OSCORE a good choice for memory-constrained IoT devices.

Additionally, because OSCORE operates at the application layer rather than the
transport layer, it ensures that message payloads and selected options remain
protected even when passing through intermediate proxies - providing true
end-to-end security that (D)TLS cannot guarantee in proxied environments.

In the context of LwM2M, OSCORE can be used as a lightweight alternative to (D)TLS for
securing communication between the LwM2M Client and the LwM2M Server.

In solutions where the security is top-priority, OSCORE can be used together with
(D)TLS to attain double encryption realized on different protocol stack layers.

.. note::
   Code related to this tutorial can be found under `examples/tutorial/AT-OSCORE`
   in the Anjay Lite source directory.

OSCORE configuration
--------------------

OSCORE is not enabled by default. To use it, set the CMake option ``ANJ_WITH_OSCORE``.
The ``ANJ_WITH_SECURITY`` option must also be enabled.

For Anjay Lite to work with OSCORE, an OSCORE Object implementation must be provided. The
default OSCORE Object implementation can be enabled by setting ``ANJ_WITH_DEFAULT_OSCORE_OBJ``.
Otherwise, the user must add the OSCORE Object themselves and implement the functions whose
declarations and descriptions are provided in ``include_public/anj/dm/oscore_object.h``.

.. note::
    You can read about adding a custom objects in the
    :doc:`../BasicClient/BC-BasicObjectImplementation` tutorial and in the
    :doc:`../AdvancedTopics` tutorial series.

OSCORE requires AEAD and HKDF algorithms to work. Anjay Lite does not provide implementations
of these algorithms on its own. If ``ANJ_WITH_MBEDTLS`` is disabled, then user has to
provide implementations of the functions declared in
``include_public/anj/compat/crypto/aead.h`` and ``include_public/anj/compat/crypto/hkdf.h``.

The example configuration used in this tutorial enables ``ANJ_WITH_DEFAULT_OSCORE_OBJ`` and
``ANJ_WITH_MBEDTLS``.

Other OSCORE configuration options:

- ``ANJ_OSCORE_MAX_MASTER_SECRET_SIZE`` - Maximum size of the OSCORE master secret in bytes.
- ``ANJ_OSCORE_MAX_MASTER_SALT_SIZE`` - Maximum size of the OSCORE master salt in bytes.
- ``ANJ_OSCORE_MAX_CONTEXT_ID_SIZE`` - Maximum size of the OSCORE context ID in bytes.

.. note::
   ``ANJ_OSCORE_MAX_CONTEXT_ID_SIZE`` affects the maximum allowed size of individual Context IDs
   generated during the B.2 procedure. For more information, see the ``anj_config.h.in`` file.

.. note::
   For more detailed descriptions of the above options, see the ``anj_config.h.in`` file.

Use OSCORE
----------

OSCORE can be used both with the LwM2M Bootstrap Server and the LwM2M Server.
Keying material stored in the OSCORE Object can only be set in the Bootstrap phase, that is,
during a Factory Bootstrap or by an LwM2M Bootstrap Server.

Factory Bootstrap
^^^^^^^^^^^^^^^^^

In the case where Anjay Lite does not connect to a Bootstrap Server all needed keying material
and related information of a LwM2M Client appropriate to access a specified LwM2M Server using
OSCORE must be provided via Anjay API:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-OSCORE/src/main.c

    static int install_oscore_obj(anj_t *anj, anj_dm_oscore_obj_t *oscore_obj) {
        static const uint8_t MASTER_SECRET[] = "M4$t3r $3cr3t";
        static const uint8_t MASTER_SALT[] = "M4$t3r $4lt";
        static const uint8_t SENDER_ID[] = "SenID";
        static const uint8_t RECIPIENT_ID[] = "RecID";
    
        anj_dm_oscore_instance_init_t oscore_inst = {
            .iid = OSCORE_IID,
            .master_secret = {
                .tag = ANJ_CRYPTO_SECURITY_TAG_OSCORE_MASTER_SECRET,
                .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
                {
                    .buffer = { MASTER_SECRET, sizeof(MASTER_SECRET) - 1 }
                }
            },
            .sender_id = SENDER_ID,
            .sender_id_len = sizeof(SENDER_ID) - 1,
            .recipient_id = RECIPIENT_ID,
            .recipient_id_len = sizeof(RECIPIENT_ID) - 1,
            .oscore_aead_algorithm = 0,
            .oscore_hmac_algorithm = 0,
            .oscore_master_salt = MASTER_SALT,
            .oscore_master_salt_len = sizeof(MASTER_SALT) - 1
        };
    
        anj_dm_oscore_obj_init(oscore_obj);
        if (anj_dm_oscore_obj_add_instance(oscore_obj, &oscore_inst)
                || anj_dm_oscore_obj_install(anj, oscore_obj)) {
            return -1;
        }
        return 0;
    }

.. note::
    Anjay Lite does not use the ``/21/x/6`` resource (``OSCORE Context ID``). This is because the
    library performs the procedure described in RFC 8613, Appendix B.2, as required by the
    LwM2M specification. The procedure is executed every time an OSCORE Object instance is
    created or modified, and it results in establishing a new ``Context ID``. As a consequence,
    the value stored in ``/21/x/6`` would always be overwritten and never used.

The ``OSCORE Security Mode`` resource must point to an OSCORE Object instance related to the
LwM2M server we want to connect to:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-OSCORE/src/main.c
   :emphasize-lines: 7

    static int install_security_obj(anj_t *anj,
                                    anj_dm_security_obj_t *security_obj) {
        anj_dm_security_instance_init_t security_inst = {
            .ssid = 1,
            .server_uri = "coap://eu.iot.avsystem.cloud:5683",
            .security_mode = ANJ_DM_SECURITY_NOSEC,
            .oscore_security_mode = { 21, OSCORE_IID },
        };
        anj_dm_security_obj_init(security_obj);
        if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
                || anj_dm_security_obj_install(anj, security_obj)) {
            return -1;
        }
        return 0;
    }

.. note::
   OSCORE can be combined with (D)TLS.

Finally, the OSCORE object has to be installed:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-OSCORE/src/main.c
   :emphasize-lines: 4

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj)
            || install_server_obj(&anj, &server_obj)
            || install_oscore_obj(&anj, &oscore_obj)) {
        return -1;
    }

LwM2M Bootstrap Server
^^^^^^^^^^^^^^^^^^^^^^

In a Bootstrap Server scenario, it is possible to establish the connection to the Bootstrap Server
using OSCORE. In that case, as in the example above, you need to provide the required OSCORE
related information yourself. However, if you don't want to use OSCORE for the connection to the
LwM2M Bootstrap Server but want to use it for the connection to the LwM2M Server, the only thing
you need to do is install the OSCORE Object (using ``anj_dm_oscore_obj_init`` and
``anj_dm_oscore_obj_install`` functions) without manually adding any instances. The OSCORE Object
instance will be added by the Bootstrap Server, provided the Bootstrap Server is configured
accordingly.

.. note::
    You can read more about Bootstrap and its configuration in the 
    :doc:`../AdvancedTopics/AT-Bootstrap` tutorial.

Limitations
^^^^^^^^^^^

OSCORE with Appendix B2 may trigger device re-registration when B2 negotiation occurs in the middle of
ongoing Block-wise transfer. Check :doc:`../KnownIssuesAndLimitations` to read more.
