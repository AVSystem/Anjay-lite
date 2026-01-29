..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

NTP-based time synchronization
==============================

Overview
--------

This example demonstrates how to use the Anjay Lite NTP module to synchronize the
device clock with an NTP server. It covers:

* enabling NTP support in the build configuration,
* configuring and initializing the NTP module,
* implementing an NTP event callback to handle synchronization events,
* synchronizing the application time.

.. note::
   Code related to this tutorial can be found under `examples/tutorial/AT-TimeSynchronization`
   in the Anjay Lite source directory.

Build configuration
-------------------

The example enables NTP support explicitly and disables the default POSIX time
compatibility layer:

.. highlight:: cmake
.. snippet-source:: examples/tutorial/AT-TimeSynchronization/CMakeLists.txt

    set(ANJ_WITH_NTP ON)
    set(ANJ_WITH_TIME_POSIX_COMPAT OFF)

.. note::
    With ``ANJ_WITH_TIME_POSIX_COMPAT`` disabled, the application must provide its
    own implementations of ``anj_time_monotonic_now`` and ``anj_time_real_now``.


NTP module initialization
-------------------------

The NTP module is initialized by preparing an ``anj_ntp_configuration_t``
structure and passing it to ``anj_ntp_init``. The configuration includes
the NTP server addresses, synchronization period, response timeout, and the
event callback function. The overall logic is based on the implementation of
the `Time Synchronization Object with ID 3415
<https://raw.githubusercontent.com/OpenMobileAlliance/lwm2m-registry/prod/version_history/3415-2_0.xml>`_.
This Object defines resources for configuring NTP parameters (server URIs,
synchronization period) and monitoring synchronization status.
As part of ``anj_ntp_init``, an instance of this Object is also
installed in the Anjay Lite's Data Model.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-TimeSynchronization/src/main.c

    anj_ntp_t ntp;
    anj_ntp_configuration_t ntp_config = {
        .event_cb = ntp_event_callback,
        .ntp_server_address = "pool.ntp.org",
        .backup_ntp_server_address = "time.google.com",
        .ntp_period_hours = 1,
        .response_timeout = anj_time_duration_new(3, ANJ_TIME_UNIT_S),
    };
    if (anj_ntp_init(&anj, &ntp, &ntp_config)) {
        log(L_ERROR, "Failed to initialize NTP module");
        return -1;
    }

NTP event callback
------------------

The NTP event callback function handles various NTP events, such as the start
of synchronization, successful completion, and errors. It can be used to log
the current status and update the application time accordingly. In the provided
example, ``anj_ntp_start`` is called to initiate synchronization in two cases:
when the NTP module is initialized and when the synchronization period has
elapsed.

In the case of successful synchronization, the callback updates the
``g_last_sync_real`` and ``g_last_sync_monotonic`` variables to reflect the
new synchronized time and the corresponding monotonic time. It also sets the
``g_time_synced`` flag to indicate that the time has been successfully
synchronized. For embedded applications, this can also be the place where the
RTC module or system clock is adjusted to the newly synchronized time.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-TimeSynchronization/src/main.c
    :emphasize-lines: 8-9,11,27-29

    static void ntp_event_callback(void *arg,
                                anj_ntp_t *ntp,
                                anj_ntp_status_t status,
                                anj_time_real_t synchronized_time) {
        (void) arg;

        switch (status) {
        case ANJ_NTP_STATUS_INITIAL:
        case ANJ_NTP_STATUS_PERIOD_EXCEEDED: {
            log(L_INFO, "NTP synchronization started");
            anj_ntp_start(ntp);
            break;
        }
        case ANJ_NTP_STATUS_FINISHED_WITH_ERROR: {
            log(L_ERROR, "NTP synchronization failed");
            break;
        }
        case ANJ_NTP_STATUS_FINISHED_SUCCESSFULLY: {
            log(L_INFO, "NTP synchronization succeeded");
            log(L_INFO, "\n   Old time: %s\n   New time: %s\n   Delta:    %f s",
                ANJ_TIME_REAL_AS_STRING(g_last_sync_real, ANJ_TIME_UNIT_MS),
                ANJ_TIME_REAL_AS_STRING(synchronized_time, ANJ_TIME_UNIT_MS),
                ((double) anj_time_duration_to_scalar(
                        anj_time_real_diff(synchronized_time, anj_time_real_now()),
                        ANJ_TIME_UNIT_MS))
                        / 1000.0);
            g_time_synced = true;
            g_last_sync_real = synchronized_time;
            g_last_sync_monotonic = anj_time_monotonic_now();
            break;
        }
        default:
            break;
        }
    }

Custom time functions
---------------------

With the default POSIX time compatibility layer disabled, the application
must provide its own implementations of ``anj_time_monotonic_now`` and
``anj_time_real_now``. These functions should return the current monotonic and
real time, respectively. In this example, ``clock_gettime`` is used to retrieve
the current monotonic time from the system clock.

The ``anj_time_real_now`` function calculates the current real time based on
the last synchronized real time and the elapsed monotonic duration since the
last synchronization.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-TimeSynchronization/src/main.c
    :emphasize-lines: 1-2,23-27

    static anj_time_real_t g_last_sync_real;
    static anj_time_monotonic_t g_last_sync_monotonic;
    static bool g_time_synced = false;

    static int64_t get_time(clockid_t clk_id) {
        struct timespec res;
        if (clock_gettime(clk_id, &res)) {
            return 0;
        }
        return (int64_t) res.tv_sec * 1000 * 1000 + (int64_t) res.tv_nsec / 1000;
    }

    anj_time_monotonic_t anj_time_monotonic_now(void) {
    #ifdef CLOCK_MONOTONIC
        return anj_time_monotonic_new(get_time(CLOCK_MONOTONIC), ANJ_TIME_UNIT_US);
    #else  /* CLOCK_MONOTONIC */
        log(L_ERROR, "CLOCK_MONOTONIC is not available on this platform");
        return anj_time_monotonic_new(0, ANJ_TIME_UNIT_US);
    #endif /* CLOCK_MONOTONIC */
    }

    anj_time_real_t anj_time_real_now(void) {
        anj_time_duration_t delta = anj_time_duration_sub(
                anj_time_monotonic_to_duration(anj_time_monotonic_now()),
                anj_time_monotonic_to_duration(g_last_sync_monotonic));

        return anj_time_real_add(g_last_sync_real, delta);
    }

Delaying normal operation until time is synchronized
----------------------------------------------------

In many deployments, it is desirable to start the main LwM2M loop only after
the device has a valid notion of real time (for example, to correctly handle
credential expiration or time-based application logic).

This example demonstrates a simple approach where the main loop first
periodically calls ``anj_ntp_step`` until the time is synchronized. Once
synchronization is confirmed (by checking the ``g_time_synced`` flag), normal
Anjay operation can proceed.

After the first synchronization, the NTP module will continue to
synchronize the time periodically based on the configured synchronization
period. This is why ``anj_ntp_step`` still needs to be called in each iteration
of the main loop.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-TimeSynchronization/src/main.c
    :emphasize-lines: 5

    while (true) {
        usleep(50 * 1000);
        anj_ntp_step(&ntp);

        if (g_time_synced) {
            // Once time is synchronized, we can proceed to normal Anjay
            // operation
            anj_core_step(&anj);
        }
    }

Summary
-------

This example illustrated how to integrate the Anjay Lite NTP module into an
application for time synchronization. By following the steps outlined above,
developers can ensure their devices maintain accurate time, which is crucial
for various embedded applications.
