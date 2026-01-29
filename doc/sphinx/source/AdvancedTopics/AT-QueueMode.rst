..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Queue Mode
==========

Overview
--------

Queue Mode is a special mode of operation in which the device is not required
to actively listen for incoming packets all the time. After sending any message
to the LwM2M Server, the LwM2M Client is only required to listen for incoming
packets for a limited period of time. The LwM2M Server does not immediately
send downlink requests, but instead waits until the LwM2M Client is online.
This mode can be particularly useful for devices that want to switch off their
transceiver and enter a power-saving state when they have nothing to transmit.
This mode is set during registration with the LwM2M Server, using one of the
parameters of the Register operation.

.. note::
    In LwM2M v1.0 Queue Mode is set by **Binding** resource in the LwM2M Server
    Object. Anjay Lite does not support it.

The LwM2M Client becomes active when it needs to send one of these operations:

- Update
- Send
- Notify

.. important::
    Some LwM2M specification diagrams suggest that an Update must precede
    a Send or Notification. Anjay Lite skips this step, reducing
    unnecessary network traffic.

.. tip::
    If you implement your own custom network compatibility layer, considered
    implementing the ``anj_net_queue_mode_rx_off_t`` API.

.. note::
    Code related to this tutorial can be found under
    `examples/tutorial/AT-QueueMode` in the Anjay Lite source directory and is
    based on `examples/tutorial/BC-MandatoryObjects` example.

Enable Queue Mode
-----------------

Update the configuration structure to enable Queue Mode:

.. note::
    ``queue_mode_timeout`` is set via Anjay Lite's Time API. For more details,
    see :doc:`Time API <../PortingGuideForNonPOSIXPlatforms/TimeAPI>`.

.. code-block:: c

    anj_configuration_t config = {
        .endpoint_name = argv[1],
        .queue_mode_enabled = true,
        .queue_mode_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S),
    };
    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

**How it works**

    - ``queue_mode_enabled = true`` — Enables Queue Mode.
    - ``queue_mode_timeout_ms = 5000`` — Specifies how long after last exchange
      with the LwM2M Server Anjay Lite stays online before switching to offline
      mode

It is recommended not to modify ``queue_mode_timeout_ms``. It is set to a lower
value here only to demonstrate faster transitions to offline mode. However,
setting this parameter can be advantageous when you are willing to trade
reliability for energy savings.

If not set manually, ``queue_mode_timeout_ms`` defaults to
``MAX_TRANSMIT_WAIT``, a value defined by the CoAP protocol. This parameter
represents the maximum time a sender waits for an acknowledgment or reset of a
confirmable CoAP message before giving up. Shortening the interval can cause
retransmissions — or, if reduced drastically, even the original messages — sent
by the LwM2M Server to never reach the LwM2M Client.

For the default transmission parameters, ``MAX_TRANSMIT_WAIT`` is equal to 93
seconds.

.. note::
    Another way to adjust the time after which Anjay Lite switches to offline
    mode is to modify the transmission parameters via the ``udp_tx_params``
    field from the ``anj_configuration_t`` structure.

Switch to power-saving mode
---------------------------

When Queue Mode is enabled, Anjay Lite automatically switches the client to an
offline state after a period of inactivity. During this time, the device can
enter a low-power mode to conserve energy.

.. note::
    This example uses the POSIX function ``clock_nanosleep`` to simulate
    low-power mode. This call does not reduce the device's actual power
    usage but illustrates how you can integrate real power management.

First, let's define the callback that will be called when the status of the
server connection changes:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-QueueMode/src/main.c

    static void connection_status_callback(void *arg,
                                           anj_t *anj,
                                           anj_conn_status_t conn_status) {
        (void) arg;

        if (conn_status == ANJ_CONN_STATUS_QUEUE_MODE) {
            anj_time_duration_t time = anj_core_next_step_time(anj);
            uint64_t time_ms =
                    (uint64_t) anj_time_duration_to_scalar(time, ANJ_TIME_UNIT_MS);

            // Simulate entering low power mode for period of time returned by
            // previous function
            struct timespec ts = {
                // Warning: unchecked cast
                .tv_sec = (time_t) (time_ms / 1000),
                .tv_nsec = (long) ((time_ms % 1000) * 1000000L)
            };
            clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
        }
    }

**How it works**

    - ``ANJ_CONN_STATUS_QUEUE_MODE`` — indicates that the client has switched
      to offline mode and will not receive any new messages.
    - ``anj_core_next_step_time()`` — returns the ``anj_time_duration_t`` until
      the next call to ``anj_core_step()`` is required. Use this value to
      determine how long the device can stay in power-saving mode.


.. note::
    If you call ``anj_observe_data_model_changed`` after putting the device
    into power-saving mode, the time previously returned by
    ``anj_core_next_step_time`` may no longer be valid; in that case, call the
    function again and use the updated time value.

Next, update the configuration:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-QueueMode/src/main.c
    :emphasize-lines: 5

    anj_configuration_t config = {
        .endpoint_name = argv[1],
        .queue_mode_enabled = true,
        .queue_mode_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S),
        .connection_status_cb = connection_status_callback
    };
    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }


NAT awareness
-------------

In typical deployments, clients do not have publicly routable IP addresses and
operate behind NAT devices.

Most NATs create short-lived bindings that map a client's internal (IP, port) to
an external (IP, port). Servers usually identify a peer by the 5-tuple (src
IP, src port, dst IP, dst port, transport). If inactivity lasts longer than the
NAT binding lifetime or if the client changes its local port, the NAT mapping
may be lost.

**Queue Mode implication:** if the NAT mapping is lost, the server can't match
uplink operations (Update, Send, Notify) with any registered client.
To enable long Queue Mode intervals, you need a mechanism that isn't tied to
the 5-tuple (e.g., DTLS Connection ID or an equivalent transport/session
identifier).
