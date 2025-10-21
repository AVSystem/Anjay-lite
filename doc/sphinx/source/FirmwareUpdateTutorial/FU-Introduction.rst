..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Firmware Update concept
=======================

Overview
--------

Firmware over-the-air (FOTA) updates are one of the most important applications
of the LwM2M protocol. In real-world deployments, being able to remotely update
devices is essential for maintaining performance, reliability, and security.

The Firmware Update procedure is standardized in the LwM2M Specification and
implemented through the Firmware Update Object (**/5**). It supports the
following operations:

- Download
- Verification
- Upgrade

At the same time, it is flexible enough to allow custom logic to be introduced
(e.g., differential updates). There are no restrictions on the verification
method, image formats, and so on; these aspects depend entirely on the specific
implementation.

Firmware Update state machine
-----------------------------

The LwM2M Specification defines a state machine that governs the firmware
update process:

.. image:: https://www.openmobilealliance.org/release/LightweightM2M/V1_1-20180710-A/HTML-Version/OMA-TS-LightweightM2M_Core-V1_1-20180710-A_files/firmware_update_mechanisms.svg
   :alt: LwM2M Firmware Update State Machine

Each transition in the diagram is triggered by an event that may originate from
the Client or the Server. The state machine relies on two key resources:

- **State Resource (/5/0/3)** — indicates the current stage of the firmware update
  process.
- **Update Result Resource (/5/0/5)** — stores the result of the last download or
  update attempt.

Anjay Lite's Firmware Update API handles the protocol logic behind the state
machine. As a developer, you only need to implement the I/O operations,
verification, and firmware application logic.

Firmware Update process overview
--------------------------------

The update process typically follows these steps:

#. The Server initiates the firmware download.
#. The Client downloads the firmware and notifies the Server when it completes.
#. The Server instructs the Client to apply the update.
#. The Client attempts the update and reports the result to the Server.

.. important::

   Only the Server can initiate firmware download and update operations.
   The Client cannot start these actions independently. The Server either:

   - Provides a download URI using the `Package URI` resource (`/5/0/1`), or
   - Sends the firmware directly using the `Package` resource (`/5/0/0`).

   In both cases, the Client only acts in response to the Server's request.

Firmware Update modes
---------------------

The LwM2M protocol defines two Firmware Download modes:

- **Push mode** — The Server writes the firmware directly to the ``Package``
  Resource (``/5/0/0``). This typically uses block-wise transfer. In Anjay Lite
  clients, Push mode blocks all other LwM2M operations until the transfer finishes.

- **Pull mode** — The Server writes a URI to the ``Package URI`` Resource
  (``/5/0/1``). The Client downloads the firmware asynchronously from the given
  URI and can continue handling other LwM2M requests during the download.

Although the Server initiates the firmware download by writing to the
appropriate resource, it can only choose from the methods supported and reported
by the Client. If the Server attempts to use an unsupported method, the Client
will reject the operation.

.. admonition:: Recommendation

   Use **Pull mode** whenever possible. In Anjay Lite, Push mode significantly
   limits functionality by blocking the Client during the transfer.

.. _firmware-update-api:

Firmware update API in Anjay Lite
---------------------------------

Anjay Lite includes a built-in Firmware Update module that simplifies the
Firmware Update implementation for the user. At its core, the Firmware Update
module consists of various user-implemented callbacks. These are listed below to
illustrate the requirements for implementing the Firmware Update process:

.. highlight:: c
.. snippet-source:: include_public/anj/dm/fw_update.h

    /**
    * Collection of user‑provided callbacks used by the Firmware Update Object.
    *
    * @warning The user must ensure that this structure remains valid for the
    *          entire lifetime of @ref anj_t object or until the Object is removed
    *          using @ref anj_dm_remove_obj.
    */
    typedef struct {
        /** Initiates Push‑mode download of the firmware package. */
        anj_dm_fw_update_package_write_start_t *package_write_start_handler;

        /** Writes a chunk of the firmware package during Push‑mode download. */
        anj_dm_fw_update_package_write_t *package_write_handler;

        /** Finalizes the Push‑mode download operation. */
        anj_dm_fw_update_package_write_finish_t *package_write_finish_handler;

        /** Handles Write to Package URI (starts Pull‑mode download). */
        anj_dm_fw_update_uri_write_t *uri_write_handler;

        /** Schedules execution of the actual firmware upgrade. */
        anj_dm_fw_update_update_start_t *update_start_handler;

        /** Returns the name of the downloaded firmware package. */
        anj_dm_fw_update_get_name_t *get_name;

        /** Returns the version of the downloaded firmware package. */
        anj_dm_fw_update_get_version_t *get_version;

        /** Aborts firmware download process and cleans up temporary resources. */
        anj_dm_fw_update_reset_t *reset_handler;
    } anj_dm_fw_update_handlers_t;

Not all handlers are mandatory. The required set depends on the firmware update
mode:

.. list-table::
  :header-rows: 1
  :widths: 15 65

  * - Update mode
    - Mandatory handlers
  * - **Pull**
    - ``uri_write_handler``
  * - **Push**
    - ``package_write_start_handler``,
      ``package_write_handler``,
      ``package_write_finish_handler``
  * - **All modes**
    - ``update_start_handler``, ``reset_handler``

.. note::

	The complete definition of the Firmware Update module's API, including all
	required callbacks, auxiliary functions, types, and macros, is available in
	the ``include_public/anj/dm/fw_update.h`` header file.

The next chapter introduces the implementation of all these components from
scratch.
