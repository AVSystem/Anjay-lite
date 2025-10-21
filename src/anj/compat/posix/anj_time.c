/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define _GNU_SOURCE

#include <stdint.h>

#include <sys/types.h>

#include <anj/compat/time.h>
#include <anj/time.h>

#ifdef ANJ_WITH_TIME_POSIX_COMPAT

#    include <time.h>

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

anj_time_real_t anj_time_real_now(void) {
    return anj_time_real_new(get_time(CLOCK_REALTIME), ANJ_TIME_UNIT_US);
}

#endif // ANJ_WITH_TIME_POSIX_COMPAT
