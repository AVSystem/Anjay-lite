..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Time API
========

Overview
--------

Anjay Lite uses a POSIX-compatible time implementation based on
``clock_gettime()`` by default. If your platform doesn't support the POSIX time
API, you need to disable POSIX compatibility and provide your own time functions.

List of functions to implement
------------------------------

To disable POSIX compatibility and use a custom implementation:

1. Configure CMake with ``-DANJ_WITH_TIME_POSIX_COMPAT=OFF``.
2. Implement the following functions:

+-------------------------------+-----------------------------------------------+
| Function                      | Purpose                                       |
+===============================+===============================================+
| ``anj_time_monotonic_now()``  | Returns the current monotonic time.           |
+-------------------------------+-----------------------------------------------+
| ``anj_time_real_now()``       | Returns the current real time (Unix epoch).   |
+-------------------------------+-----------------------------------------------+

.. note::
   For function signatures and detailed descriptions, see
   ``include_public/anj/compat/time.h``.

.. important::
   For information about time types, units, arithmetic, and string helpers used
   throughout Anjay Lite, see Doxygen comments in
   ``include_public/anj/time.h``.

Reference implementation
------------------------

The default implementation in ``src/anj/compat/posix/anj_time.c`` uses the POSIX
``clock_gettime()`` API and can be used as a reference for your integration.

``anj_time_monotonic_now()``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``anj_time_monotonic_now()`` function must return the current *monotonic*
time as ``anj_time_monotonic_t``.

Monotonic time:

- Represents time elapsed since an arbitrary epoch (typically system boot).
- Must not be affected by wall-clock adjustments (NTP synchronization, manual edits,
  or time zone changes).
- Must never stop and must never go backwards. It needs to be a continuously
  advancing time base even if the system enters sleep/suspend.

If the platform's default monotonic clock stops across suspend, use a clock
source that continues counting, or implement compensation using the platform's
power-management hooks.

In the POSIX reference implementation, ``anj_time_monotonic_now()`` prefers
``CLOCK_MONOTONIC`` if available; otherwise it falls back to ``CLOCK_REALTIME``.

.. highlight:: c
.. snippet-source:: src/anj/compat/posix/anj_time.c

    /* Get the current time in microseconds from selected clock type */
    static int64_t get_time(clockid_t clk_id) {
        struct timespec res;
        if (clock_gettime(clk_id, &res)) {
            return 0;
        }
        return (int64_t) res.tv_sec * 1000 * 1000 + (int64_t) res.tv_nsec / 1000;
    }

    anj_time_monotonic_t anj_time_monotonic_now(void) {
    #    ifdef CLOCK_MONOTONIC
        return anj_time_monotonic_new(get_time(CLOCK_MONOTONIC), ANJ_TIME_UNIT_US);
    #    else  /* CLOCK_MONOTONIC */
        return anj_time_monotonic_new(get_time(CLOCK_REALTIME), ANJ_TIME_UNIT_US);
    #    endif /* CLOCK_MONOTONIC */
    }

Resolution, precision, and drift
""""""""""""""""""""""""""""""""

The resolution and precision of the monotonic clock are platform-dependent.
All internal scheduling in Anjay Lite (notifications, retransmissions, registration
lifetime handling, etc.) relies on monotonic time.

Your integration should therefore:

- provide sufficient resolution for your use case (millisecond-level resolution is
  typically sufficient),
- keep clock drift under control and take it into account especially for long
  intervals (e.g., operations scheduled once per day), where small errors may
  accumulate over time.

``anj_time_real_now()``
^^^^^^^^^^^^^^^^^^^^^^^

The ``anj_time_real_now()`` function must return the current *real* (wall-clock)
time since the Unix epoch (00:00:00 UTC on 1970-01-01) as ``anj_time_real_t``.

.. important::
   Within the Anjay Lite library, the real-time clock is used for: the LwM2M Send
   Operation timestamping and X.509 certificate validity period checks.

   Integrations that do not use these features and do not require real-time
   functionality may return a constant value (e.g., zero) from this function,
   or map it to the monotonic clock.

In the POSIX reference implementation, this function wraps ``CLOCK_REALTIME``.

.. highlight:: c
.. snippet-source:: src/anj/compat/posix/anj_time.c

    anj_time_real_t anj_time_real_now(void) {
        return anj_time_real_new(get_time(CLOCK_REALTIME), ANJ_TIME_UNIT_US);
    }

.. note::
    Real-time clock can be synchronized by Anjay Lite's Time Synchronization
    module. Check the :doc:`../AdvancedTopics/AT-TimeSynchronization` for details.
