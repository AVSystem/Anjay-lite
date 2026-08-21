..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

FOTA Pull with CoAPs (Certificates)
===================================

Overview
^^^^^^^^

This tutorial extends the :doc:`FU-PullWithCoapsImplementation` example and
demonstrates how to perform firmware downloads over **CoAPs (CoAP over DTLS)**
using **certificate-based authentication** instead of PSK.

The overall firmware update flow remains unchanged compared to the PSK-based
CoAPs variant. The key difference lies in how the client authenticates with the
firmware download server. In this version, the LwM2M Client uses X.509 certificates
provisioned in the Security Object, as described in the
:doc:`../AdvancedTopics/AT-Certificates` tutorial.

Function: ``fu_uri_write()``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The function below is the **only difference** between the FOTA Pull with CoAPs
using PSK and Certificates variant.

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-coaps-downloader-certificates/src/firmware_update.c
    :emphasize-lines: 8-12

    static anj_dm_fw_update_result_t fu_uri_write(void *user_ptr,
                                                  const char *_uri) {
        firmware_update_t *fu = (firmware_update_t *) user_ptr;
        log(L_INFO, "fu_uri_write called with URI: %s", _uri);

        anj_net_config_t net_cfg;
        memset(&net_cfg, 0x00, sizeof(net_cfg));
        net_cfg.secure_socket_config.security.mode = ANJ_NET_SECURITY_CERTIFICATE;
        if (anj_security_get_cert_info(
                    fu->anj,
                    false,
                    &net_cfg.secure_socket_config.security.data.cert)) {
            log(L_ERROR, "Failed to get CERT credentials from Security Object");
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

**Certificate credentials** are retrieved from the Security Object using
``anj_security_get_cert_info()`` and reused for the firmware download connection.
This works with **Coiote DM**, which allows the device to use the same key
and certificate for both the management and the FOTA session, but may not
apply to all servers or deployment models.
