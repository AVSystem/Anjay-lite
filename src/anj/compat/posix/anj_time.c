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

#ifdef ANJ_WITH_TIME_POSIX_COMPAT

#    include <time.h>

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

uint64_t anj_time_real_now(void) {
    return get_time(CLOCK_REALTIME);
}

#endif // ANJ_WITH_TIME_POSIX_COMPAT
