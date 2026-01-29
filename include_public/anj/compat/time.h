/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Platform compatibility hooks for obtaining current time.
 *
 * This header declares a minimal API that platform integrators (or end users)
 * can implement to provide current time to Anjay Lite:
 * - @ref anj_time_monotonic_now — monotonic time point (from an arbitrary
 *                                 epoch, typically boot)
 * - @ref anj_time_real_now      — real/calendar time since Unix epoch
 *
 * The concrete implementation is platform-specific and may live in the
 * application, BSP, or OS abstraction layer. The returned values use the
 * same types and semantics as defined in @ref anj/time.h.
 */

#ifndef ANJ_COMPAT_TIME_H
#    define ANJ_COMPAT_TIME_H

#    include <anj/time.h>
#    include <stdint.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * @brief Returns the current monotonic time point.
 *
 * The result is a time point measured from an arbitrary monotonic epoch
 * (typically system boot) and **must not** be affected by wall-clock
 * adjustments such as NTP synchronization, time zone changes, or manual edits.
 * Resolution is platform-dependent.
 *
 * The monotonic clock used by Anjay Lite must never stop or go backwards: it
 * needs to be a continuously advancing time base even if the system enters
 * sleep/suspend. If the platform's default monotonic clock stops across
 * suspend, use a clock source that continues counting (or implement sleep-time
 * compensation).
 *
 * @return @ref anj_time_monotonic_t representing "now" on a monotonic clock.
 *
 * @note Since clock precision and drift affect retransmission timers,
 *       registration lifetime handling and other timeouts, integrations should
 *       take drift into account, especially for operations scheduled at long
 *       intervals (e.g., sending an UPDATE once per day) where small errors can
 *       accumulate over time.
 *
 * @see anj_time_monotonic_t
 * @see anj_time_duration_t
 * @see anj_time_monotonic_new
 * @see anj_time_duration_new
 */
anj_time_monotonic_t anj_time_monotonic_now(void);

/**
 * @brief Returns the current real (calendar) time point.
 *
 * The result is referenced to the Unix epoch (00:00:00 UTC on 1970-01-01)) and
 * **may** be affected by wall-clock adjustments (NTP corrections or manual
 * edits).
 *
 * @return @ref anj_time_real_t representing "now" on the real/calendar clock.
 *
 * @note In Anjay Lite, the real-time clock is used for:
 *       - the LwM2M Send Operation timestamping, and
 *       - X.509 certificate validity period checks.
 *       Integrations that do not use these features may return a constant value
 *       (e.g., zero) from this function, or map it to the monotonic clock.
 *
 * @see anj_time_real_t
 * @see anj_time_duration_t
 * @see anj_time_real_new
 * @see anj_time_duration_new
 */
anj_time_real_t anj_time_real_now(void);

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_COMPAT_TIME_H
