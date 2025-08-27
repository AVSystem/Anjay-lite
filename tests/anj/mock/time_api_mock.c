
/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdint.h>

#include "time_api_mock.h"

static uint64_t mock_time_value = 0;

void set_mock_time(uint64_t time) {
    mock_time_value = time * 1000;
}

void set_mock_time_advance(uint64_t *inout_actual_time, uint64_t delta) {
    *inout_actual_time += delta;
    set_mock_time(*inout_actual_time);
}

uint64_t anj_time_now(void) {
    return mock_time_value;
}

uint64_t anj_time_real_now(void) {
    return mock_time_value;
}
