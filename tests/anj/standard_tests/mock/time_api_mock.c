
/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdint.h>

#include <anj/time.h>

#include "time_api_mock.h"

// Its a shame that thanks to cpp compatibility we cant have ANJ_TIME_REAL_ZERO
// as a macro therefore it can't be used as initializer here
static anj_time_real_t mock_time_real = { { 0 } };
static anj_time_monotonic_t mock_time_monotonic = { { 0 } };

void mock_time_advance(anj_time_duration_t delta) {
    mock_time_real = anj_time_real_add(mock_time_real, delta);
    mock_time_monotonic = anj_time_monotonic_add(mock_time_monotonic, delta);
}

void mock_time_advance_monotonic(anj_time_duration_t delta) {
    mock_time_monotonic = anj_time_monotonic_add(mock_time_monotonic, delta);
}

void mock_time_advance_real(anj_time_duration_t delta) {
    mock_time_real = anj_time_real_add(mock_time_real, delta);
}

void mock_time_tick(anj_time_unit_t unit) {
    mock_time_real =
            anj_time_real_add(mock_time_real, anj_time_duration_new(1, unit));
    mock_time_monotonic =
            anj_time_monotonic_add(mock_time_monotonic,
                                   anj_time_duration_new(1, unit));
}

void mock_time_reset(void) {
    // simulate difference between clocks
    mock_time_real = anj_time_real_new(30 * 365, ANJ_TIME_UNIT_DAY);
    mock_time_monotonic = ANJ_TIME_MONOTONIC_ZERO;
}

anj_time_monotonic_t anj_time_monotonic_now(void) {
    return mock_time_monotonic;
}

anj_time_real_t anj_time_real_now(void) {
    return mock_time_real;
}
