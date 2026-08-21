/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/compat/time.h>
#include <anj/time.h>

#include <time.h>
#include <unistd.h>

#include "commands.hpp"

static anj_time_duration_t monotonic_offset = { 0 };
static anj_time_duration_t real_offset = { 0 };

int add_monotonic_time_offset(int offset_ms) {
    monotonic_offset = anj_time_duration_add(
            monotonic_offset,
            anj_time_duration_new(offset_ms, ANJ_TIME_UNIT_MS));
    return 0;
}

int add_real_time_offset(int offset_ms) {
    real_offset = anj_time_duration_add(
            real_offset, anj_time_duration_new(offset_ms, ANJ_TIME_UNIT_MS));
    return 0;
}

/* Get the current time in microseconds from selected clock type */
static int64_t get_time(clockid_t clk_id) {
    timespec res{};
    if (clock_gettime(clk_id, &res)) {
        return 0;
    }
    return (int64_t) res.tv_sec * 1000 * 1000 + (int64_t) res.tv_nsec / 1000;
}

anj_time_monotonic_t anj_time_monotonic_now() {
    return anj_time_monotonic_add(
            anj_time_monotonic_new(get_time(CLOCK_MONOTONIC), ANJ_TIME_UNIT_US),
            monotonic_offset);
}

anj_time_real_t anj_time_real_now() {
    return anj_time_real_add(anj_time_real_new(get_time(CLOCK_REALTIME),
                                               ANJ_TIME_UNIT_US),
                             real_offset);
}
