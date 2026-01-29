..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

FOTA Direct Push
================

.. important::
   This tutorial describes the push-based firmware update method.
   Use this method only when strictly required. During the update,
   the client is fully blocked—it cannot handle notifications, send updates, or respond to any server requests.
   For most scenarios, the **pull-based** approach is recommended. See :doc:`FU-PullBasicImplementation`.

Project structure
^^^^^^^^^^^^^^^^^

This tutorial demonstrates a basic implementation of the Firmware Update process
with Anjay Lite using Push mode. This means that the firmware is uploaded 
directly to the Package Resource (`/5/0/0`) and typically 
uses block-wise transfer. The tutorial source files are located in the
``examples/tutorial/firmware-update`` directory. This example builds on the code
presented in the :doc:`../BasicClient/BC-MandatoryObjects` chapter:

.. code::

    examples/tutorial/firmware-update/
    ├── CMakeLists.txt
    └── src
        ├── firmware_update.c (new)
        ├── firmware_update.h (new)
        └── main.c

Configure the library
^^^^^^^^^^^^^^^^^^^^^

Enable the following build options in either the ``CMakeLists.txt`` file or the ``anj_config.h`` file:

* ``ANJ_WITH_DEFAULT_FOTA_OBJ=ON``: Enables the built-in Firmware Update Object.
  Similar to other default objects such as Security or Server.

* ``ANJ_FOTA_WITH_PUSH_METHOD=ON``: Enables Push mode in the Firmware Update functionality
  in builtin Firmware Update Object.

Install the Firmware Update Object
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To use the Firmware Update feature, you must install the Firmware Update Object
in the Anjay Lite client. Use the following API function:

.. highlight:: c
.. snippet-source:: include_public/anj/dm/fw_update.h

    int anj_dm_fw_update_object_install(anj_t *anj,
                                        anj_dm_fw_update_entity_ctx_t *entity_ctx,
                                        anj_dm_fw_update_handlers_t *handlers,
                                        void *user_ptr);

**The parameters description**

.. list-table::
   :header-rows: 1
   :widths: 15 65

   * - Parameter
     - Description
   * - ``anj``
     - A pointer to the initialized Anjay Lite instance.
   * - ``entity_ctx``
     - A pointer to the firmware update entity context.
   * - ``handlers``
     - A pointer to the handlers structure.
       See the :ref:`firmware-update-api` section.
   * - ``user_ptr``
     - Optional pointer to context-specific data passed to each handler call.
       Use it to store information required during firmware operations.
       Set to NULL if no data is needed.

Each mandatory input object referenced by a pointer must stay valid as long as
the Firmware Update Object remains active.

**Helper functions**

In the tutorial, the Firmware Update Object installation and handling is
abstracted through the functions declared in ``firmware_update.h``:

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update/src/firmware_update.h

    #ifndef _FIRMWARE_UPDATE_H_
    #define _FIRMWARE_UPDATE_H_

    #include <anj/defs.h>
    #include <anj/log.h>

    #define log(...) anj_log(fota_example_log, __VA_ARGS__)

    /**
    * Checks if a Firmware Update is pending and executes it if needed.
    * Should be called periodically in the main loop.
    */
    void fw_update_check(void);

    /**
    * Installs the Firmware Update Object on the LwM2M client instance.
    *
    * @param anj               Anjay Lite instance to operate on.
    * @param firmware_version  Version string of the current firmware.
    * @param endpoint_name     The endpoint name for register message.
    *
    * @return 0 on success, -1 on failure.
    */
    int fw_update_object_install(anj_t *anj,
                                const char *firmware_version,
                                const char *endpoint_name);

    #endif // _FIRMWARE_UPDATE_H_

The ``fw_update_object_install()`` function initializes the internal state of the Firmware Update Object.
It also checks if a firmware update has already been applied by looking for a marker file.
If the marker is present, it sets the Update Result Resource (`/5/0/5`) to
indicate a successful update using ``anj_dm_fw_update_object_set_update_result()``,
allowing the server to verify the update status.

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update/src/firmware_update.c
    :emphasize-lines: 16,17

    int fw_update_object_install(anj_t *anj,
                                 const char *firmware_version,
                                 const char *endpoint_name) {
        firmware_update.firmware_version = firmware_version;
        firmware_update.endpoint_name = endpoint_name;
        firmware_update.waiting_for_reboot = false;

        if (anj_dm_fw_update_object_install(anj, &fu_entity, &fu_handlers,
                                            &firmware_update)) {
            return -1;
        }

        if (access(FW_UPDATED_MARKER, F_OK) == 0) {
            log(L_INFO, "Firmware Updated successfully");
            unlink(FW_UPDATED_MARKER);
            (void) anj_dm_fw_update_object_set_update_result(
                    anj, &fu_entity, ANJ_DM_FW_UPDATE_RESULT_SUCCESS);
        }

        return 0;
    }

These are in turn called in the ``main.c``:

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update/src/main.c

    anj_res_value_t firmware_version;
    anj_dm_res_read(&anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3), &firmware_version);
    log(L_INFO, "Firmware version: %s",
                     (const char *) firmware_version.bytes_or_string.data);
    if (fw_update_object_install(
                &anj,
                (const char *) firmware_version.bytes_or_string.data,
                anj.endpoint_name)) {
        return -1;
    }

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update/src/main.c
    
    while (true) {
        anj_core_step(&anj);
        fw_update_check();
        usleep(50 * 1000);
    }

Implement handlers and installation routine
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This tutorial demonstrates a push-based firmware update workflow. The following
steps summarize the simplified process.

#. Create a temporary firmware file:

    .. highlight:: c
    .. snippet-source:: examples/tutorial/firmware-update/src/firmware_update.c

        static anj_dm_fw_update_result_t fu_write_start(void *user_ptr) {
            firmware_update_t *fu = (firmware_update_t *) user_ptr;
            assert(fu->firmware_file == NULL);

            // Ensure previous file is removed
            if (remove(FW_IMAGE_PATH) != 0 && errno != ENOENT) {
                log(L_ERROR, "Failed to remove existing firmware image");
                return ANJ_DM_FW_UPDATE_RESULT_FAILED;
            }

            fu->firmware_file = fopen(FW_IMAGE_PATH, "wb");
            if (!fu->firmware_file) {
                log(L_ERROR, "Failed to open firmware image for writing");
                return ANJ_DM_FW_UPDATE_RESULT_FAILED;
            }

            log(L_INFO, "Firmware Download started");
            return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
        }

#. Write the received firmware chunks to the file:

    .. highlight:: c
    .. snippet-source:: examples/tutorial/firmware-update/src/firmware_update.c

        static anj_dm_fw_update_result_t
        fu_write(void *user_ptr, const void *data, size_t data_size) {
            firmware_update_t *fu = (firmware_update_t *) user_ptr;
            assert(fu->firmware_file != NULL);

            log(L_INFO, "Writing %lu bytes at offset %lu", data_size,
                            fu->offset);
            fu->offset += data_size;

            if (fwrite(data, 1, data_size, fu->firmware_file) != data_size) {
                log(L_ERROR, "Failed to write firmware chunk");
                return ANJ_DM_FW_UPDATE_RESULT_FAILED;
            }
            return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
        }

    .. highlight:: c
    .. snippet-source:: examples/tutorial/firmware-update/src/firmware_update.c

        static anj_dm_fw_update_result_t fu_write_finish(void *user_ptr) {
            firmware_update_t *fu = (firmware_update_t *) user_ptr;
            assert(fu->firmware_file != NULL);

            if (fclose(fu->firmware_file)) {
                log(L_ERROR, "Failed to close firmware file");
                fu->firmware_file = NULL;
                return ANJ_DM_FW_UPDATE_RESULT_FAILED;
            }

            fu->firmware_file = NULL;
            fu->offset = 0;
            log(L_INFO, "Firmware Download finished");
            return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
        }


#. Reboot into the new firmware using the system call:

    .. highlight:: c
    .. snippet-source:: examples/tutorial/firmware-update/src/firmware_update.c

        static int fu_update_start(void *user_ptr) {
            firmware_update_t *fu = (firmware_update_t *) user_ptr;
            log(L_INFO, "Firmware Update process started");
            fu->waiting_for_reboot = true;
            return 0;
        }

    .. highlight:: c
    .. snippet-source:: examples/tutorial/firmware-update/src/firmware_update.c

        void fw_update_check(void) {
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

.. admonition:: Reminder

    The complete definition of the Firmware Update module's API, including all
    required callbacks, auxiliary functions, types, and macros, is available in
    the ``include_public/anj/dm/fw_update.h`` header file.

Firmware download is considered complete when a marker file is created. The
``fu_update_start()`` function only schedules the reboot. The actual reboot
check is performed by ``fw_update_check()``.

The firmware file used as the Firmware Update input in this example is the
executable binary created during compilation. To prepare a firmware image for
FOTA, change ``device_obj_conf.firmware_version`` in ``main.c`` and recompile
the application.

For additional guidance on managing Firmware Update process from the LwM2M
server perspective, refer to
`How to update firmware on a single device <https://eu.iot.avsystem.cloud/doc/user/basic-device-management/how-to-guides/update-firmware-on-a-single-device>`_.
This resource provides a practical walkthrough of initiating and managing
Firmware Update process through the AVSystem Coiote LwM2M Server, and may be
helpful when designing server-side logic or integration flows.

.. note::

    This example demonstrates a simplified Firmware Update process.
    For production environments, you may need to:

    - Verify firmware integrity before applying the update  
    - Store the firmware image in persistent memory (for example, flash)  
    - Use secure and reliable methods to verify and apply updates  
    - Address platform-specific behaviors appropriately
