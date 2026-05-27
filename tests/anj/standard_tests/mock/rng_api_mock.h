/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef RNG_API_MOCK_H
#define RNG_API_MOCK_H

#include <stddef.h>
#include <stdint.h>

typedef int mock_rng(uint8_t *buffer, size_t siz);

void mock_rng_constructor(mock_rng *rng);

#endif /* RNG_API_MOCK_H */
