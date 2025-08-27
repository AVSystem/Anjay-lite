/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef _TEMPERATURE_OBJ_H_
#define _TEMPERATURE_OBJ_H_

#include <anj/core.h>
#include <anj/defs.h>

const anj_dm_obj_t *get_temperature_obj(void);
void temperature_obj_init(void);

/**
 * @brief Updates the sensor value and adjusts min/max tracked values.
 *
 * Simulates a new temperature reading for the given object by applying a small
 * random fluctuation to the current value. Also updates the minimum and maximum
 * recorded values based on the new reading.
 */
void update_temperature_obj_value(void);

#endif // _TEMPERATURE_OBJ_H_
