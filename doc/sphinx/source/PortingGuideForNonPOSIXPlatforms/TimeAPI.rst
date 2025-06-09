..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Time API
========

List of functions to implement
------------------------------

By default, Anjay Lite uses a POSIX-compatible implementation based on ``clock_gettime()``.
If the POSIX time API is not available:

- Use ``ANJ_WITH_TIME_POSIX_COMPAT=OFF`` when running CMake on Anjay Lite,
- Implement the following functions:

+-------------------+-----------------------------------------------------------+
| Function          | Purpose                                                   |
+===================+===========================================================+
| anj_time_now      | Returns the current monotonic time.                       |
+-------------------+-----------------------------------------------------------+
| anj_time_real_now | Returns the current real time (Unix epoch time).          |
+-------------------+-----------------------------------------------------------+

.. note::
    For signatures and detailed description of listed functions, see
    `include_public/anj/compat/time.h`

Reference implementation
------------------------

The default `src/anj/compat/posix/anj_time.c` implementation that uses POSIX
``clock_gettime()`` API can be used as a reference for writing your own integration layer.

anj_time_now()
^^^^^^^^^^^^^^

The ``anj_time_now`` function should return the current *monotonic* time, expressed
in milliseconds.

Monotonic time:

   -  Represents the time elapsed since an arbitrary point (the epoch), typically the system boot time.
   -  Must remain stable throughout the lifetime of the process. Should not be affected by system
      clock changes (for example, changes to the real-time clock).

If the system provides a stable real-time clock that does not change during runtime,
it may also be used as a monotonic clock.

In the POSIX reference implementation, ``anj_time_now`` uses ``CLOCK_MONOTONIC`` if
available. If ``CLOCK_MONOTONIC`` is unavailable, the function falls back to using
``CLOCK_REALTIME``.

.. highlight:: c
.. snippet-source:: src/anj/compat/posix/anj_time.c

    static uint64_t get_time(clockid_t clk_id) {
        struct timespec res;
        if (clock_gettime(clk_id, &res)) {
            return 0;
        }
        return (uint64_t) res.tv_sec * 1000
               + (uint64_t) res.tv_nsec / (1000 * 1000);
    }

    uint64_t anj_time_now(void) {
    #    ifdef CLOCK_MONOTONIC
        return get_time(CLOCK_MONOTONIC);
    #    else  // CLOCK_MONOTONIC
        return get_time(CLOCK_REALTIME);
    #    endif // CLOCK_MONOTONIC
    }

anj_time_real_now()
^^^^^^^^^^^^^^^^^^^

The ``anj_time_real_now`` function should return the current *real* time,
measured as the number of milliseconds since the Unix epoch
(00:00:00 UTC on 1 January 1970).

Real time:

    - Provides timestamps that are meaningful across reboots and across devices.
    - Should reflect the actual current date and time.

In the reference implementation, this function is a wrapper for the
``CLOCK_REALTIME`` clock.

.. highlight:: c
.. snippet-source:: src/anj/compat/posix/anj_time.c

    uint64_t anj_time_real_now(void) {
        return get_time(CLOCK_REALTIME);
    }
