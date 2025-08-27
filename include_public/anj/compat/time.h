/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_TIME_H
#    define ANJ_TIME_H

#    include <stdint.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * Special value that can be used to represent an undefined time.
 */
#    define ANJ_TIME_UNDEFINED UINT64_MAX

/**
 * @return The number of milliseconds that have elapsed since the system was
 * started. Should not return ANJ_TIME_UNDEFINED.
 */
uint64_t anj_time_now(void);

/**
 * @return The current system Unix time expressed in milliseconds. Should not
 * return ANJ_TIME_UNDEFINED.
 */
uint64_t anj_time_real_now(void);

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_TIME_H
