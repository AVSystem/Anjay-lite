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

#include <anj/time.h>

void mock_time_advance(anj_time_duration_t delta);
void mock_time_advance_monotonic(anj_time_duration_t delta);
void mock_time_advance_real(anj_time_duration_t delta);
void mock_time_tick(anj_time_unit_t unit);
anj_time_monotonic_t anj_time_monotonic_now(void);
anj_time_real_t anj_time_real_now(void);
void mock_time_reset(void);

#endif /* TIME_API_MOCK_H */
