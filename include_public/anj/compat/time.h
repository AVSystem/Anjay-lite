/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
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
 * - @ref anj_time_monotonic_now — monotonic time since boot
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
 * adjustments such as NTP sync, time zone changes, or manual edits.
 *
 * @return @ref anj_time_monotonic_t representing "now" on a monotonic clock.
 *
 * @note Resolution is platform-dependent. Implementations should ensure the
 *       returned value is nondecreasing across calls. Select the appropriate
 *       conversion unit when creating monotonic time structure from system time
 *       using @ref anj_time_duration_new.
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
 * The result is referenced to the Unix epoch
 * (1970-01-01T00:00:00Z) and **may** be affected by wall-clock adjustments
 * (NTP corrections, DST changes as reflected in local time, or manual edits).
 *
 * @return @ref anj_time_real_t representing "now" on the real/calendar clock.
 *
 * @warning Time synchronizations during client runtime may result in undefined
 * behavior and are not supported.
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
