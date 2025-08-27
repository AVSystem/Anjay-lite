..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Anjay Lite Beta Version Limitations and Workarounds
===================================================

This chapter outlines the current limitations of Anjay Lite Beta and presents recommended workarounds or implementation strategies.
**These limitations are expected to be addressed in version 1.0 of Anjay Lite, which aims to provide full support for the described features.**

.. warning::

   The proposed workarounds rely on internal behavior of library and may not be compatible with future releases.
   They are intended as temporary solutions and may require changes when upgrading to version 1.0 or later.

Lack of DTLS Security Layer Integration
---------------------------------------

Anjay Lite Beta does not yet support the DTLS security layer, which is required for secure LwM2M communication.
There are two possible workarounds for this limitation:

 - Combine the UDP transport layer with an external DTLS library such as MbedTLS.
   To enable this, you must modify the _anj_parse_uri_components() function to accept coaps:// URIs when using UDP.
 - Use the DTLS transport layer already present in Anjay Lite Beta. However, it is not yet integrated or officially tested.

.. note::

   In both approaches, you will need to implement the integration between the Security Object and the network layer yourself.
   This includes passing pre-shared keys, certificates, or identity information.
   Anjay Lite does not automatically propagate secure configuration from the Security Object to the transport layer.
   The default implementation of the Security Object in Anjay Lite Beta already provides the necessary data structures to store security information.

CoAP/HTTP Downloaders for FOTA Pull
-----------------------------------

The current release of Anjay Lite supports the Firmware Update Object (FOTA) using the Pull method.  
However, it does **not** include built-in HTTP or CoAP downloaders.

To use FOTA Pull over HTTP, follow these steps:

- Enable the `ANJ_FOTA_WITH_PULL_METHOD` and `ANJ_FOTA_WITH_HTTP` configuration options in your project.
- Implement the `uri_write_handler()` callback to handle the URI write operation.
- Download the firmware image using an external mechanism, since Anjay Lite Beta does not provide a built-in downloader:
  - On Linux systems, you can use tools such as `curl` or `wget`.
  - On embedded platforms, downloading can be performed using modem AT commands or other device-specific means.
- After successfully downloading the image, call `anj_dm_fw_update_object_set_download_result()` to notify Anjay
  Lite of the result and proceed with the update process.

A CoAP-based downloader is currently not available.  
You may implement it manually or wait for a future release that includes official support.

Lack of Persistence
-------------------

Anjay Lite Beta does not persist configuration received during the Bootstrap phase.
As a result, the client must repeat the entire Bootstrap procedure on each restart,
which may be inefficient or undesirable in production scenarios.

Recommended approach:

 - Implement logic to persist the contents of the Security and Server Objects.

   - You may use Anjay Lite's default implementations of these objects, or create your own.
   - You can serialize entire object instances or individual resources.

      - Serializing entire instances is simpler, but less robust to structural changes.
      - Reading/writing resources individually is more future-proof.

 - After a successful Bootstrap:

   - Detect completion using the `connection_status_cb` callback with `ANJ_CONN_STATUS_BOOTSTRAPPED`.
   - Save the Security Object and Server Object instances to a non-volatile storage (e.g., flash memory).

 - On client startup:

   - Load saved configuration from persistent storage.
   - Initialize the Security Object and Server Object with the loaded data.

Example implementation:

.. code-block:: c

   static void connection_status_callback(void *arg,
                                          anj_t *anj,
                                          anj_conn_status_t conn_status) {
      (void) arg;

      if (conn_status == ANJ_CONN_STATUS_BOOTSTRAPPED) {
         persist_server_obj_configuration();
         persist_security_obj_configuration();
      }
   }

   int main(int argc, char *argv[]) {
      restore_server_obj_configuration();
      restore_security_obj_configuration();
   
      anj_t anj;
      anj_configuration_t config = {
         .endpoint_name = endpoint_name,
         .connection_status_cb = connection_status_callback,
      };
      anj_core_init(&anj, &config);

      while (true) {
         anj_core_step(&anj);
      }
   }

Silent Observations drop when observed path disappears
------------------------------------------------------

When `anj_observe_data_model_changed()` is called with `ANJ_OBSERVE_CHANGE_TYPE_DELETED`
argument, the observation is dropped silently and no 4.04 Not Found is sent to the server.


.. note::
   We are actively working to address these limitations in the upcoming release.
   If you have questions, feedback, or specific technical requirements, please
   feel free to reach out to us directly via our `contact form <https://avsystem.com/contact>`_
