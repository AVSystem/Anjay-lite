/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 54

#include <math.h>
#include <stdint.h>

#include <anj/time.h>
#include <anj/utils.h>

const anj_time_duration_t ANJ_TIME_DURATION_ZERO = { 0 };
const anj_time_real_t ANJ_TIME_REAL_ZERO = { 0 };
const anj_time_monotonic_t ANJ_TIME_MONOTONIC_ZERO = { 0 };

const anj_time_duration_t ANJ_TIME_DURATION_INVALID = { ANJ_TIME_US_MAX };
const anj_time_real_t ANJ_TIME_REAL_INVALID = { { ANJ_TIME_US_MAX } };
const anj_time_monotonic_t ANJ_TIME_MONOTONIC_INVALID = { { ANJ_TIME_US_MAX } };

static const int64_t TIME_UNIT_MULTIPLIERS[] = {
    /* The values will be calculated compile time anyway but the code clarity is
       better with multiplications */
    [ANJ_TIME_UNIT_DAY] = 24 * 60 * 60 * 1000 * 1000LL,
    [ANJ_TIME_UNIT_HOUR] = 60 * 60 * 1000 * 1000LL,
    [ANJ_TIME_UNIT_MIN] = 60 * 1000 * 1000LL,
    [ANJ_TIME_UNIT_S] = 1000 * 1000LL,
    [ANJ_TIME_UNIT_MS] = 1000LL,
    [ANJ_TIME_UNIT_US] = 1LL,
};

anj_time_duration_t anj_time_duration_new(const int64_t scalar,
                                          const anj_time_unit_t unit) {
    if (scalar == ANJ_TIME_US_MAX) {
        return ANJ_TIME_DURATION_INVALID;
    }

    return (anj_time_duration_t) {
        .us = scalar * TIME_UNIT_MULTIPLIERS[unit]
    };
}

anj_time_duration_t anj_time_duration_fnew(const double scalar,
                                           const anj_time_unit_t unit) {
    if (isinf(scalar) || isnan(scalar)) {
        return ANJ_TIME_DURATION_INVALID;
    }

    return (anj_time_duration_t) {
        .us = (int64_t) scalar * TIME_UNIT_MULTIPLIERS[unit]
    };
}

int64_t anj_time_duration_to_scalar(const anj_time_duration_t duration,
                                    const anj_time_unit_t unit) {
    return duration.us / TIME_UNIT_MULTIPLIERS[unit];
}

double anj_time_duration_to_fscalar(const anj_time_duration_t duration,
                                    const anj_time_unit_t unit) {
    return (double) duration.us / (double) TIME_UNIT_MULTIPLIERS[unit];
}

const char *_anj_time_duration_as_string_impl(
        const anj_time_duration_t duration,
        const anj_time_unit_t unit,
        char (*buffer)[ANJ_TIME_DURATION_AS_STRING_MAX_LEN]) {

    if (!anj_time_duration_is_valid(duration)) {
        return "<invalid>";
    }

    if (anj_int64_to_string_value(
                *buffer, anj_time_duration_to_scalar(duration, unit))) {
        return *buffer;
    }

    return "<error>";
}
