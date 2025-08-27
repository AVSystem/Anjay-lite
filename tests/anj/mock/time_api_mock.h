/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef TIME_API_MOCK_H
#define TIME_API_MOCK_H

#include <stdint.h>

// set time value in seconds
void set_mock_time(uint64_t time);

void set_mock_time_advance(uint64_t *inout_actual_time, uint64_t delta);

#endif /* TIME_API_MOCK_H */
