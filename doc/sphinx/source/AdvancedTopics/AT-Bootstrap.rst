..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Bootstrap
=========

Overview
--------

The LwM2M Protocol Specification defines the Bootstrap Interface, whose primary
role is to provision LwM2M-enabled devices with the necessary configuration and
credentials required to establish a connection with the LwM2M Server.

The most common use case of this interface, and the one covered in this example,
involves delivering the LwM2M Server Object Instance together with appropriate
security credentials. However, the bootstrap process is far more versatile.

**LwM2M Bootstrap Server**

A LwM2M Bootstrap Server is a special entity in the LwM2M architecture, as it is
allowed to modify object instances and resources that are otherwise inaccessible
to regular LwM2M Servers, ignoring the Read-Only property.

Security Object instance that is related to the connection with LwM2M Bootstrap
Server (has ``Bootstrap-Server Resource`` set to true, as well as URI and security
credentials for LwM2M Bootstrap Server) is often called a
**LwM2M Bootstrap-Server Account**. LwM2M Bootstrap Server connection requires
only ``/0`` Security Object instance, without a corresponding ``/1`` Server
Object instance (with matching SSID).

**Key Operations**

- ``Bootstrap-Delete /0``: Removes all Security Object instances except the one related to the Bootstrap Server.

- ``Bootstrap-Discover``: Identifies the Security Object instance ID for the Bootstrap Server.

- ``Bootstrap-Write``: Updates server URI or credentials.

Bootstrap Interface support is enabled with ``ANJ_WITH_BOOTSTRAP`` configuration
flag.


Add a Bootstrap Account in Anjay Lite
-------------------------------------

To inform Anjay Lite that a Security Instance is a Bootstrap Account, use
``Bootstrap Server`` Resource in Security Object instance by setting
**anj_dm_security_instance_init_t::bootstrap_server** flag.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Bootstrap/src/main.c
    :emphasize-lines: 8

    // Installs Security Object and adds an instance of it.
    // This instance of Security Object provides information needed to connect to
    // LwM2M Bootstrap Server.
    static int install_security_obj(anj_t *anj,
                                    anj_dm_security_obj_t *security_obj) {
        anj_dm_security_instance_init_t security_inst = {
            .server_uri = "coap://eu.iot.avsystem.cloud:5693",
            .bootstrap_server = true,
            .security_mode = ANJ_DM_SECURITY_NOSEC
        };
        anj_dm_security_obj_init(security_obj);
        if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
                || anj_dm_security_obj_install(anj, security_obj)) {
            return -1;
        }
        return 0;
    }

The LwM2M Bootstrap Server doesn't have a /1 Server Object instance. However, you must still install the Server Object in Anjay Lite data model to allow the Bootstrap Server to create the Server Object dynamically.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Bootstrap/src/main.c

    // Installs Server Object and DOES NOT add an instance of it.
    // An instance of Server Object will be provided by the LwM2M Bootstrap Server.
    static int install_server_obj(anj_t *anj, anj_dm_server_obj_t *server_obj) {
        anj_dm_server_obj_init(server_obj);
        if (anj_dm_server_obj_install(anj, server_obj)) {
            return -1;
        }
        return 0;
    }

Once LwM2M Client connects to a LwM2M Bootstrap Server and sends Bootstrap Request,
the server will perform a series of bootstrap operations (**Bootstrap-Write**,
**Bootstrap-Read**, **Bootstrap-Discover**, **Bootstrap-Delete**), finished with 
a **Bootstrap-Finish** to provision the device.

.. note::

    Complete code of this example can be found in
    `examples/tutorial/AT-Bootstrap` subdirectory of main Anjay Lite
    project repository.


Configure bootstrap
-------------------

**Bootstrap Procedure in Anjay Lite**

Anjay Lite attempts Bootstrap in the following cases:

- No LwM2M Server is defined in the data model.
- Connection to the LwM2M Server fails.

If the Bootstrap Server doesn't send a ``Bootstrap-Finish`` operation within a timeout period, the procedure is considered failed.

**Configure the timeout and retries**

You can configure the timeout using the ``bootstrap_timeout`` field in the ``anj_configuration_t`` structure passed to ``anj_core_init()``.

If the timeout is not explicitly set, the default value ``CoAP EXCHANGE_LIFETIME`` is used, as recommended by the LwM2M specification.

If the initial bootstrap attempt fails (for example, due to a timeout or network error), Anjay Lite can retry the process automatically.

Use the following configuration fields:

+---------------------------+----------------------------------------------------------------------------------------------------------+
| Field                     | Description                                                                                              |
+===========================+==========================================================================================================+
| `bootstrap_retry_count`   | Number of retry attempts.                                                                                |
+---------------------------+----------------------------------------------------------------------------------------------------------+
| `bootstrap_retry_timeout` | Base delay between retries. This delay grows exponentially: `2^(attempt - 1) * bootstrap_retry_timeout`. |
+---------------------------+----------------------------------------------------------------------------------------------------------+

.. note::

    `Client Hold Off Time` resource in the Security Object delays only the first attempt to connect to the LwM2M Bootstrap Server.

**Bootstrap-Discover Support**

In addition to the ANJ_WITH_BOOTSTRAP flag, you can enable the ANJ_WITH_BOOTSTRAP_DISCOVER configuration flag to support the Bootstrap-Discover operation.

This feature is useful in advanced setups where the LwM2M Bootstrap Server needs to inspect the device's data model. If not required, you can disable this flag to reduce Anjay Lite's flash memory usage.

**Handling Bootstrap Operations**

Bootstrap Interface operations that target data model are routed to the same
handlers in objects implementation. If The LwM2M Bootstrap Server performs, for example,
a Bootstrap Write, it will be handled in the `anj_dm_obj_struct::handlers`.


Coiote LwM2M Server
-------------------

To Bootstrap your device using AVSystem Coiote LwM2M Server, refer to
`Add device via the Bootstrap server guide <https://eu.iot.avsystem.cloud/doc/user/getting-started/add-devices/#add-device-via-the-bootstrap-server>`_ 
in the Coiote documentation.
