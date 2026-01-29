..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

FOTA Pull with CoAP
===================

Overview
^^^^^^^^

This tutorial shows how to implement firmware updates using Pull mode in Anjay Lite.
In this mode, the LwM2M Server writes a URI to the Package URI resource (`/5/0/1`).
The client then uses Anjay Lite's CoAP Downloader to download the firmware asynchronously.

Alternatively, the client can use an external HTTP(S) library or a cellular modem to retrieve the firmware.
Regardless of the download method, the client reports the download status through the
Firmware Update Object and uses the same Update trigger (`/5/0/2`).

.. admonition:: Key difference from Push mode
    
    Pull mode does not use the ``package_write_*`` handlers. Instead, it:

    * Uses ``uri_write_handler`` to start the download.
    * Requires periodic calls to ``anj_coap_downloader_step()`` in the main loop (for example, via ``fw_update_process()``).

Project structure
^^^^^^^^^^^^^^^^^

The tutorial source files are located in the ``examples/tutorial/firmware-update-coap-downloader``
directory. This example builds on the code from  :doc:`FU-PushBasicImplementation`:

.. code::

    examples/tutorial/firmware-update-coap-downloader/
    ├── CMakeLists.txt
    └── src
        ├── firmware_update.c (changed)
        ├── firmware_update.h (changed)
        └── main.c

Configure the library
^^^^^^^^^^^^^^^^^^^^^

Enable the following build options in either the ``CMakeLists.txt`` file or the ``anj_config.h`` file:

* ``ANJ_WITH_DEFAULT_FOTA_OBJ=ON``:
  Enables the built-in Firmware Update Object. Similar to other default objects such as Security or Server.

* ``ANJ_FOTA_WITH_PULL_METHOD=ON``:
  Enables Pull mode support in the built-in Firmware Update Object.

* ``ANJ_FOTA_WITH_COAP(COAPS/HTTP/HTTPS/COAP_TCP/COAPS_TCP)``:
  Specifies the supported download protocols.
  The client accepts URIs with these schemes written to the Package URI resource (`/5/0/1`).
  Supported protocols are advertised in the Supported Protocols resource (`/5/0/8`).

* ``ANJ_WITH_COAP_DOWNLOADER=ON``:
  Enables the built-in CoAP Downloader used to
  fetch firmware over CoAP or CoAPs in Pull mode.


Implement handlers and installation routine
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In Pull mode, the ``fw_update_object_install()`` function must initialize the **CoAP Downloader**
and register a callback that processes download progress.

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-coap-downloader/src/firmware_update.c
    :emphasize-lines: 2

    anj_coap_downloader_configuration_t coap_downloader_config = {
        .event_cb = coap_downloader_callback,
        .event_cb_arg = &firmware_update,
    };
    if (anj_coap_downloader_init(&coap_downloader, &coap_downloader_config)) {
        return -1;
    }

Pull mode requires a different set of handlers than Push mode. Use the following structure to register the necessary callbacks:

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-coap-downloader/src/firmware_update.c

    static anj_dm_fw_update_handlers_t fu_handlers = {
        .uri_write_handler = fu_uri_write,
        .update_start_handler = fu_update_start,
        .reset_handler = fu_reset,
        .get_version = fu_get_version,
    };

Write URI to the Package URI Resource (``/5/0/1``)
--------------------------------------------------

The update process begins when the server writes a URI to the Package URI resource (`/5/0/1`).
The ``fu_uri_write()`` handler starts the download using the provided URI.

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-coap-downloader/src/firmware_update.c
    :emphasize-lines: 5

    static anj_dm_fw_update_result_t fu_uri_write(void *user_ptr,
                                                  const char *_uri) {
        (void) user_ptr;
        log(L_INFO, "fu_uri_write called with URI: %s", _uri);
        int res = anj_coap_downloader_start(&coap_downloader, _uri, NULL);
        if (res == ANJ_COAP_DOWNLOADER_ERR_INVALID_URI) {
            return ANJ_DM_FW_UPDATE_RESULT_INVALID_URI;
        } else if (res) {
            return ANJ_DM_FW_UPDATE_RESULT_FAILED;
        }
        return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
    }

Process firmware download
-------------------------

The CoAP Downloader is a module in Anjay Lite that downloads files over a separate CoAP or CoAPs connection.
It operates independently from the main LwM2M session and handles firmware retrieval asynchronously.

To process ongoing downloads, call ``anj_coap_downloader_step()`` in the main loop.

After the server triggers an update by writing to the Update resource (`/5/0/2`), the ``fu_update_start()`` handler sets a ``waiting_for_reboot`` flag. 
The ``fw_update_process()`` helper function monitors this flag and performs the firmware installation and reboot:

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-coap-downloader/src/firmware_update.c
    :emphasize-lines: 2

    void fw_update_process(void) {
        anj_coap_downloader_step(&coap_downloader);
        if (firmware_update.waiting_for_reboot) {
            log(L_INFO, "Rebooting to apply new firmware");

            firmware_update.waiting_for_reboot = false;

            if (chmod(FW_IMAGE_PATH, 0700) == -1) {
                log(L_ERROR, "Failed to make firmware executable");
                return;
            }

            FILE *marker = fopen(FW_UPDATED_MARKER, "w");
            if (marker) {
                fclose(marker);
            } else {
                log(L_ERROR, "Failed to create update marker");
                return;
            }

            execl(FW_IMAGE_PATH, FW_IMAGE_PATH, firmware_update.endpoint_name,
                  NULL);
            log(L_ERROR, "execl() failed");

            unlink(FW_UPDATED_MARKER);
            exit(EXIT_FAILURE);
        }
    }

Handle download events
----------------------

The ``coap_downloader_callback()`` function processes events generated by the **CoAP Downloader** during firmware download.
This function is triggered at various stages of the transfer and reacts based on the current status.

**Status types**

The callback receives one of the following status values (``anj_coap_downloader_status_t``): 

+--------------------------------------------+-------------------------------------------------------------------------------------------+
| Status                                     | Description                                                                               |
+============================================+===========================================================================================+
| ``ANJ_COAP_DOWNLOADER_STATUS_STARTING``    | Connection is being established. Prepare the firmware file or memory buffer.              |
+--------------------------------------------+-------------------------------------------------------------------------------------------+
| ``ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING`` | A data chunk was received. Write it to the firmware buffer. May be called multiple times. |
+--------------------------------------------+-------------------------------------------------------------------------------------------+
| ``ANJ_COAP_DOWNLOADER_STATUS_FINISHING``   | Final packet received or error occurred. Close the firmware file.                         |
+--------------------------------------------+-------------------------------------------------------------------------------------------+
| ``ANJ_COAP_DOWNLOADER_STATUS_FINISHED``    | Download completed successfully. Report result as successful.                             |
+--------------------------------------------+-------------------------------------------------------------------------------------------+
| ``ANJ_COAP_DOWNLOADER_STATUS_FAILED``      | Download failed. Retrieve error with ``anj_coap_downloader_get_error()``.                 |
+--------------------------------------------+-------------------------------------------------------------------------------------------+

**Example download callback implementation**

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-coap-downloader/src/firmware_update.c
    :emphasize-lines: 46-47,50-53

    static void coap_downloader_callback(void *arg,
                                         anj_coap_downloader_t *downloader,
                                         anj_coap_downloader_status_t conn_status,
                                         const uint8_t *data,
                                         size_t data_len) {
        firmware_update_t *fu = (firmware_update_t *) arg;

        switch (conn_status) {
        case ANJ_COAP_DOWNLOADER_STATUS_STARTING: {
            assert(fu->firmware_file == NULL);
            // Ensure previous file is removed
            if (remove(FW_IMAGE_PATH) != 0 && errno != ENOENT) {
                log(L_ERROR, "Failed to remove existing firmware image");
                anj_coap_downloader_terminate(&coap_downloader);
                break;
            }
            fu->firmware_file = fopen(FW_IMAGE_PATH, "wb");
            if (!fu->firmware_file) {
                log(L_ERROR, "Failed to open firmware image for writing");
                anj_coap_downloader_terminate(&coap_downloader);
                break;
            }
            log(L_INFO, "Firmware download starting");
            break;
        }
        case ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING: {
            assert(fu->firmware_file != NULL);
            log(L_INFO, "Writing %lu bytes at offset %lu", data_len, fu->offset);
            fu->offset += data_len;
            if (fwrite(data, 1, data_len, fu->firmware_file) != data_len) {
                log(L_ERROR, "Failed to write firmware chunk");
                anj_coap_downloader_terminate(&coap_downloader);
            }
            break;
        }
        case ANJ_COAP_DOWNLOADER_STATUS_FINISHING: {
            if (fu->firmware_file && fclose(fu->firmware_file)) {
                log(L_ERROR, "Failed to close firmware file");
            }
            fu->firmware_file = NULL;
            fu->offset = 0;
            break;
        }
        case ANJ_COAP_DOWNLOADER_STATUS_FINISHED:
            log(L_INFO, "Firmware download finished successfully");
            anj_dm_fw_update_object_set_download_result(
                    fu->anj, &fu_entity, ANJ_DM_FW_UPDATE_RESULT_SUCCESS);
            break;
        case ANJ_COAP_DOWNLOADER_STATUS_FAILED:
            log(L_ERROR, "Firmware download failed with error: %d",
                anj_coap_downloader_get_error(downloader));
            anj_dm_fw_update_object_set_download_result(
                    fu->anj, &fu_entity, ANJ_DM_FW_UPDATE_RESULT_FAILED);
            break;
        default:
            log(L_WARNING, "Unknown firmware download status");
            break;
        }
    }

Implement additional handlers
-----------------------------

The remaining firmware update handlers support triggering the update, resetting the update state, and reporting the current firmware version.

* ``fu_update_start()`` sets a flag to indicate that the device should reboot and apply the downloaded firmware.

* ``fu_reset()`` cancels any ongoing download and removes any partially downloaded firmware file.

* ``fu_get_version()`` returns the currently installed firmware version.

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-coap-downloader/src/firmware_update.c
    :emphasize-lines: 12

    static int fu_update_start(void *user_ptr) {
        firmware_update_t *fu = (firmware_update_t *) user_ptr;
        log(L_INFO, "Firmware Update process started");
        fu->waiting_for_reboot = true;
        return 0;
    }

    static void fu_reset(void *user_ptr) {
        firmware_update_t *fu = (firmware_update_t *) user_ptr;

        fu->waiting_for_reboot = false;
        anj_coap_downloader_terminate(&coap_downloader);
        if (fu->firmware_file) {
            fclose(fu->firmware_file);
            fu->firmware_file = NULL;
        }
        (void) remove(FW_IMAGE_PATH);
    }

    static const char *fu_get_version(void *user_ptr) {
        firmware_update_t *fu = (firmware_update_t *) user_ptr;
        return fu->firmware_version;
    }
