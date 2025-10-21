..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
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
- Must not be affected by wall-clock adjustments (NTP, manual edits, or time zone changes).
- **May** stop during system is suspend, depending on the clock source
  (e.g., ``CLOCK_MONOTONIC`` stops across suspend; ``CLOCK_BOOTTIME`` continues counting).

If your platform exposes a stable real-time clock that does not change during
runtime, you can use it as a monotonic source as a last resort.

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

``anj_time_real_now()``
^^^^^^^^^^^^^^^^^^^^^^^

The ``anj_time_real_now()`` function must return the current *real* (wall-clock)
time since the Unix epoch (00:00:00 UTC on 1970-01-01) as ``anj_time_real_t``.

Real time:

* Provides timestamps meaningful across reboots and devices.
* Reflects the actual current date and time.
* Requires synchronization (e.g., NTP) or correct RTC configuration.
* **Must** continue counting across suspend and power-down (typically via RTC).

.. important::
   The real-time clock **must not be adjusted while the Anjay Lite client is
   running**, as sudden time jumps can cause undefined behavior. If start-up
   synchronization is needed, perform it **before** the client starts.

In the POSIX reference implementation, this function wraps ``CLOCK_REALTIME``.

.. highlight:: c
.. snippet-source:: src/anj/compat/posix/anj_time.c

    anj_time_real_t anj_time_real_now(void) {
        return anj_time_real_new(get_time(CLOCK_REALTIME), ANJ_TIME_UNIT_US);
    }

Workarounds and limitations
---------------------------

If the platform lacks a dedicated monotonic clock, a stable real-time clock that
does not change during runtime **may** be used as a monotonic source. Be aware
of trade-offs:

- If ``anj_time_real_now()`` is backed by an unsynchronized or monotonic-like
  source, returned timestamps will not be meaningful as calendar time.
- LwM2M resources that require real time (e.g., ``Current Time (3/0/13)``)
  will not be useful without proper wall-clock synchronization.
