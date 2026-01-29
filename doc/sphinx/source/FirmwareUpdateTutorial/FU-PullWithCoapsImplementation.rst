..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

FOTA Pull with CoAPs
====================

Overview
^^^^^^^^

This tutorial builds on the :doc:`FU-PullBasicImplementation` example and demonstrates
how to use **CoAPs** (CoAP over DTLS) as the transport for firmware downloads.
The overall flow is similar to the FOTA Pull with CoAP, but in this case
the firmware download session may require secure credentials.

In this example, the LwM2M Client connects to the Coiote server using PSK
credentials, as described in the :doc:`../BasicClient/BC-Security` tutorial.

Project structure
^^^^^^^^^^^^^^^^^

The project is structured similarly to the FOTA Pull with CoAP:

.. code::

    examples/tutorial/firmware-update-coaps-downloader/
    ├── CMakeLists.txt (changed)
    └── src
        ├── firmware_update.c (changed)
        ├── firmware_update.h
        └── main.c (changed)

Configure the library
^^^^^^^^^^^^^^^^^^^^^

Enable the following options in ``CMakeLists.txt`` or ``anj_config.h``:

* ``ANJ_WITH_DEFAULT_FOTA_OBJ=ON``:
  Enables the built-in Firmware Update Object.
* ``ANJ_FOTA_WITH_PULL_METHOD=ON``:
  Enables Pull mode support.
* ``ANJ_WITH_COAP_DOWNLOADER=ON``:
  Enables the CoAP Downloader used in Pull mode.
* ``ANJ_FOTA_WITH_COAP=ON`` and ``ANJ_FOTA_WITH_COAPS=ON``:
  Allows both CoAP and CoAPs download URIs.
* ``ANJ_WITH_SECURITY=ON``, ``ANJ_WITH_MBEDTLS=ON``, ``ANJ_NET_WITH_DTLS=ON``:
  Required for DTLS-based secure transport.

Function: ``fu_uri_write()``
----------------------------

The function below is the **only difference** between the plain FOTA Pull with CoAP
and the secure (CoAPs) variant.  

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-coaps-downloader/src/firmware_update.c
    :emphasize-lines: 4,6-18

    static anj_dm_fw_update_result_t fu_uri_write(void *user_ptr,
                                                const char *_uri) {
        firmware_update_t *fu = (firmware_update_t *) user_ptr;
        log(L_INFO, "fu_uri_write called with URI: %s", _uri);

        anj_net_config_t net_cfg;
        memset(&net_cfg, 0x00, sizeof(net_cfg));
        net_cfg.secure_socket_config.security.mode = ANJ_NET_SECURITY_PSK;
        if (anj_dm_security_obj_get_psk(
                    fu->anj,
                    false,
                    &net_cfg.secure_socket_config.security.data.psk.identity,
                    &net_cfg.secure_socket_config.security.data.psk.key)) {
            log(L_ERROR, "Failed to get PSK credentials from Security Object");
            return ANJ_DM_FW_UPDATE_RESULT_FAILED;
        }

        int res = anj_coap_downloader_start(&coap_downloader, _uri, &net_cfg);
        if (res == ANJ_COAP_DOWNLOADER_ERR_INVALID_URI) {
            return ANJ_DM_FW_UPDATE_RESULT_INVALID_URI;
        } else if (res) {
            return ANJ_DM_FW_UPDATE_RESULT_FAILED;
        }
        return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
    }

**Explanation**

- **PSK credentials** are retrieved from the Security Object using ``anj_dm_security_obj_get_psk()`` 
  and reused for the firmware download connection. This works with **Coiote DM**, which
  applies the same keys for both the management and the FOTA session,
  but may not apply to all servers or deployment models.  
  If your download server requires **separate keys**, you need to provide them
  here instead.  

- The security configuration is passed directly to
  ``anj_coap_downloader_start()``. This is done because at the time of
  initializing the module with ``anj_coap_downloader_init()``, the proper
  credentials may not yet be available — for example, the device may only
  have a bootstrap instance installed.  

- The downloader automatically distinguishes between **coap://** and
  **coaps://** URIs and sets up the corresponding transport (UDP or DTLS).
  The application does not need to care about this. If an unencrypted
  connection is used, any provided PSK credentials are simply ignored.  
