..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Handling Connectivity Errors
============================

Overview
^^^^^^^^

This guide shows how to configure Anjay Lite so that the client can continue to
operate reliably even on poor-quality networks, avoids generating excessive retry
traffic during temporary connectivity problems, and is able to recover cleanly
when the connection is eventually lost and the client enters the Failure state.

The example used in this tutorial is available in the
``examples/tutorial/AT-ConnectivityErrors`` directory of the Anjay Lite source
repository.

This example uses a factory provisioned LwM2M Server configuration and does
not perform Bootstrap. For Bootstrap-specific behavior and configuration, see
the `Bootstrap <AT-Bootstrap.html>`_
tutorial. For a broader description of Anjay Lite client states, retry rules
and failure transitions, see `LwM2M Client Logic <AT-ClientLogic.html>`_.

Configuring registration retries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Registration and Registration Update retry behavior is controlled by the
Communication Retry resources in the Server Object. These resources allow you
to control how often the client retries communication after network failures
or when the server rejects a registration attempt.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ConnectivityErrors/src/main.c

    static int install_server_obj(anj_t *anj, anj_dm_server_obj_t *server_obj) {
        anj_communication_retry_res_t comm_retry_res = {
            .retry_count = 2,
            // 10 minutes
            .retry_timer = 60 * 10,
            .seq_retry_count = 2,
            // 24 hours
            .seq_delay_timer = 60 * 60 * 24,
        };

        anj_dm_server_instance_init_t server_inst = {
            .ssid = 1,
            .lifetime = 50,
            .binding = "U",
            .bootstrap_on_registration_failure = &(bool) { false },
            .comm_retry_res = &comm_retry_res,
        };

        anj_dm_server_obj_init(server_obj);
        if (anj_dm_server_obj_add_instance(server_obj, &server_inst)
                || anj_dm_server_obj_install(anj, server_obj)) {
            return -1;
        }
        return 0;
    }

The four Communication Retry resources configure retry behavior as follows:

* ``retry_count`` limits the number of attempts in a single retry sequence,
* ``retry_timer`` defines the delay between attempts in that sequence,
* ``seq_retry_count`` allows additional retry sequences,
* ``seq_delay_timer`` defines the delay between consecutive sequences.

If Communication Retry resources are not explicitly configured by the user,
Anjay Lite uses the default values recommended by the LwM2M specification:

+---------------------+---------------+
| Resource            | Default value |
+=====================+===============+
| ``retry_count``     | ``5``         |
+---------------------+---------------+
| ``retry_timer``     | ``1 minute``  |
+---------------------+---------------+
| ``seq_retry_count`` | ``1``         |
+---------------------+---------------+
| ``seq_delay_timer`` | ``24 hours``  |
+---------------------+---------------+

These values correspond to the default ``ANJ_COMMUNICATION_RETRY_RES_DEFAULT``
configuration.

In this example, the values are intentionally conservative. After a registration-related
failure, the client waits 10 minutes before making another attempt. If that attempt also
fails, the client waits 24 hours before starting the second retry sequence, which again
consists of two attempts.

.. important::

   For secure connections using the default MbedTLS integration, the DTLS
   session is not closed within a single retry sequence. This approach requires an
   active Connection ID. If Connection ID cannot be used, it is best to set
   ``retry_count`` to ``1`` and rely on subsequent retry sequences instead. In that
   case, the DTLS handshake is repeated before each retry attempt.

Notice that ``bootstrap_on_registration_failure`` is explicitly set to
``false``. If all registration-related retries are exhausted, the client enters
the Failure state instead of switching to Bootstrap. If your deployment relies
on Bootstrap, the rules for bootstrap fallback are described in the
`Bootstrap <AT-Bootstrap.html>`_ tutorial.

.. note::

   The retry values in this example were chosen to avoid excessive
   communication attempts. This may be a good fit for battery-powered devices
   or deployments where energy efficiency matters. If your device should also
   spend more time offline between exchanges, see
   `Queue Mode <AT-QueueMode.html>`_.

Recovering from the Failure state
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

According to the Anjay Lite client logic, the client enters the Failure state
only after all configured automatic recovery attempts relevant to the current
connection flow have been exhausted. Depending on the situation, this may
include retries related to registration or, if Bootstrap is used, retries
related to the Bootstrap procedure as well.

Once in the Failure state, the client remains there until the application
initiates recovery through the ``anj_core`` API. A practical way to handle this
is to observe connection state changes and react when the Failure state is
reported. Depending on the application needs, the recovery action may be as
simple as calling `anj_core_restart() <../api/api_generated/function_core_8h_1ad1e69158ce6936a504e628b284fa1610.html>`_
to restart the connection flow, or `anj_core_request_bootstrap() <../api/api_generated/function_core_8h_1aaf0ad676d081647d607d4ebbc8ef2330.html>`_
to force a new Bootstrap attempt.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ConnectivityErrors/src/main.c

    static void connection_status_callback(void *arg,
                                           anj_t *anj,
                                           anj_conn_status_t conn_status) {
        (void) arg;

        if (conn_status == ANJ_CONN_STATUS_FAILURE) {
            log(L_ERROR,
                "All connection attempts have been exhausted. Restarting the "
                "client...");
            anj_core_restart(anj);
        }
    }

The callback is then passed in ``anj_configuration_t``:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ConnectivityErrors/src/main.c
    :emphasize-lines: 5

    anj_configuration_t config = {
        .endpoint_name = argv[1],
        .udp_tx_params = &udp_tx_params,
        .exchange_request_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S),
        .connection_status_cb = connection_status_callback,
    };

.. note::

   In a production system, you may want to perform additional recovery steps
   before calling ``anj_core_restart()``, for example resetting the modem,
   reinitializing the network interface or collecting diagnostics.

Configuring CoAP exchange parameters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In addition to LwM2M-level retry control, it is often useful to tune the CoAP
exchange parameters. These are configured through
`anj_configuration_t::udp_tx_params <../api/api_generated/structanj__exchange__udp__tx__params__t.html#_CPPv428anj_exchange_udp_tx_params_t>`_.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ConnectivityErrors/src/main.c
    :emphasize-lines: 9-10

    anj_exchange_udp_tx_params_t udp_tx_params = {
        .ack_timeout = anj_time_duration_new(2, ANJ_TIME_UNIT_S),
        .ack_random_factor = 1.5,
        .max_retransmit = 1
    };

    anj_configuration_t config = {
        .endpoint_name = argv[1],
        .udp_tx_params = &udp_tx_params,
        .exchange_request_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S),
        .connection_status_cb = connection_status_callback,
    };

The ``udp_tx_params`` structure controls confirmable CoAP exchanges:

* ``ack_timeout`` sets the initial response timeout,
* ``ack_random_factor`` randomizes that timeout,
* ``max_retransmit`` limits how many CoAP retransmissions are attempted before
  the exchange is considered failed.

If these parameters are not explicitly configured by the user, Anjay Lite uses
the default values defined by CoAP:

+-----------------------+---------------+
| Field                 | Default value |
+=======================+===============+
| ``ack_timeout``       | ``2 s``       |
+-----------------------+---------------+
| ``ack_random_factor`` | ``1.5``       |
+-----------------------+---------------+
| ``max_retransmit``    | ``4``         |
+-----------------------+---------------+

In this example ``max_retransmit`` is set to ``1``. This reduces the number of
UDP retransmissions and may shorten the time spent retrying a single exchange,
which can be useful when minimizing active time is more important, for example
on battery-powered devices. However, lowering this value also reduces exchange
reliability, especially in poor network conditions. Choice of CoAP retransmission
parameters should be made carefully, considering the requirements of the target deployment.

The example also sets ``exchange_request_timeout`` to 5 seconds. This limits
how long the client waits for the next block of a server request. Lower values
help the client stop waiting sooner when communication stalls, but they should
still be chosen carefully if the server is expected to send larger block-wise
requests over a slow link.
