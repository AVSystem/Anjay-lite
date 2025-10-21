..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Install mandatory Objects
=========================

To connect to a LwM2M server and handle incoming packets, the client must support the following mandatory LwM2M Objects:
  
  - `LwM2M Security <https://www.openmobilealliance.org/tech/profiles/LWM2M_Security-v1_0.xml>`_ (``/0``)
  - `LwM2M Server <https://www.openmobilealliance.org/tech/profiles/LWM2M_Server-v1_0.xml>`_ (``/1``)
  - `LwM2M Device <https://www.openmobilealliance.org/tech/profiles/LWM2M_Device-v1_0.xml>`_ (``/3``)

Anjay Lite provides pre-implemented modules for all three objects, making setup
straightforward.

.. note::
    Users can still provide their own implementation of these Objects if needed
    — Anjay Lite remains fully flexible.

When Anjay Lite is first instantiated (as in our previous :ref:`hello world
<anjay-lite-hello-world>` example), it has no knowledge about the Data Model,
i.e., no LwM2M Objects are registered within it. You must explicitly install
the required Objects, as shown below.

Install Objects
^^^^^^^^^^^^^^^

Use the following functions to install the Objects:

  - ``anj_dm_security_obj_install()``
  - ``anj_dm_server_obj_install()``
  - ``anj_dm_device_obj_install()``

.. important::

    To use these function you must include the following headers:

      - ``anj/dm/security_object.h``
      - ``anj/dm/server_object.h``
      - ``anj/dm/device_object.h``

Set up Security, Server and Device Objects
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This section shows how to implement and register the mandatory Objects:
Security, Server, and Device. It builds upon the setup from the
:ref:`previous tutorial <anjay-lite-hello-world>`.

Security Object
---------------

The Security Object holds connection parameters for the LwM2M server. In this
example, we configure a non-secure connection to the Coiote IoT Device
Management platform. A secure connection setup will be described in a later
section.

To use Coiote:

  - Create an account at avsystem.com/coiote-iot-device-management-platform.
  - Add your device entry in the Coiote interface using the following URI for
    the connection: ``coap://eu.iot.avsystem.cloud:5683``

If you are using another server, replace the URI with your target address.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-MandatoryObjects/src/main.c

    // Installs Security Object and adds an instance of it.
    // An instance of Security Object provides information needed to connect to
    // LwM2M server.
    static int install_security_obj(anj_t *anj,
                                    anj_dm_security_obj_t *security_obj) {
        anj_dm_security_instance_init_t security_inst = {
            .ssid = 1,
            .server_uri = "coap://eu.iot.avsystem.cloud:5683",
            .security_mode = ANJ_DM_SECURITY_NOSEC
        };
        anj_dm_security_obj_init(security_obj);
        if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
                || anj_dm_security_obj_install(anj, security_obj)) {
            return -1;
        }
        return 0;
    }

Server Object
-------------

The Server Object defines registration parameters like lifetime and binding
mode.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-MandatoryObjects/src/main.c

    // Installs Server Object and adds an instance of it.
    // An instance of Server Object provides the data related to a LwM2M Server.
    static int install_server_obj(anj_t *anj, anj_dm_server_obj_t *server_obj) {
        anj_dm_server_instance_init_t server_inst = {
            .ssid = 1,
            .lifetime = 50,
            .binding = "U",
            .bootstrap_on_registration_failure = &(bool) { false },
        };
        anj_dm_server_obj_init(server_obj);
        if (anj_dm_server_obj_add_instance(server_obj, &server_inst)
                || anj_dm_server_obj_install(anj, server_obj)) {
            return -1;
        }
        return 0;
    }

Both Security and Server instances are linked together by the Short Server ID
Resource (``ssid``). That is why the ssid value must match between the Security
and Server instances.

Device Object
-------------

The Device Object provides metadata about the device.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-MandatoryObjects/src/main.c

    // Installs Device Object and adds an instance of it.
    // An instance of Device Object provides the data related to a device.
    static int install_device_obj(anj_t *anj, anj_dm_device_obj_t *device_obj) {
        anj_dm_device_object_init_t device_obj_conf = {
            .firmware_version = "0.1"
        };
        return anj_dm_device_obj_install(anj, device_obj, &device_obj_conf);
    }

Integrate Object Installation
-----------------------------

Once the installation functions are implemented, call them from your ``main()``
function:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-MandatoryObjects/src/main.c
    :emphasize-lines: 20-24

    int main(int argc, char *argv[]) {
        if (argc != 2) {
            log(L_ERROR, "No endpoint name given");
            return -1;
        }

        anj_t anj;
        anj_dm_device_obj_t device_obj;
        anj_dm_server_obj_t server_obj;
        anj_dm_security_obj_t security_obj;

        anj_configuration_t config = {
            .endpoint_name = argv[1]
        };
        if (anj_core_init(&anj, &config)) {
            log(L_ERROR, "Failed to initialize Anjay Lite");
            return -1;
        }

        if (install_device_obj(&anj, &device_obj)
                || install_security_obj(&anj, &security_obj)
                || install_server_obj(&anj, &server_obj)) {
            return -1;
        }

        while (true) {
            anj_core_step(&anj);
            usleep(50 * 1000);
        }
        return 0;
    }

.. note::

    Complete code of this example can be found in
    `examples/tutorial/BC-MandatoryObjects` subdirectory of main Anjay Lite
    project repository.

Logs example
~~~~~~~~~~~~

After running the client, you should see ``registration successful, location =
/rd/<server-dependent identifier>`` once and ``registration successfully
updated`` every 25 seconds in logs. It means, that the client has connected to
the server and successfully sends Update messages. You can now perform
operations like Read from the server side.

Application events
^^^^^^^^^^^^^^^^^^

The example code shown above covers events managed internally by the Anjay Lite
library. However, most real-world applications also need to handle their own
logic. How to implement application-specific functionality will be explained
in the following sections.


Use Coiote DM to view objects
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can now log in to **Coiote IoT Device Management** and open the
**Device Center** for your registered device to explore platform functionality.
Check the **Data model** tab to view which LwM2M objects are exposed.

You'll notice that the **Server** and **Device** Objects are visible, but the
**Security** Object isn't. This is expected behavior — according to the LwM2M
specification, the Security Object is neither readable nor discoverable from
the device to protect sensitive configuration data.
