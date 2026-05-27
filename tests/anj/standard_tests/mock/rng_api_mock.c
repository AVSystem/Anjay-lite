
/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdint.h>
#include <string.h>
#include <sys/random.h>

#include "rng_api_mock.h"

static mock_rng *g_rng;

void mock_rng_constructor(mock_rng *rng) {
    g_rng = rng;
}

int anj_rng_generate(uint8_t *buffer, size_t size) {
    if (g_rng) {
        return g_rng(buffer, size);
    }

    const size_t MAX_CHUNK = 256; // getentropy() limit per call
    while (size > 0) {
        size_t chunk = size > MAX_CHUNK ? MAX_CHUNK : size;
        if (getentropy(buffer, chunk) != 0) {
            return -1;
        }
        buffer += chunk;
        size -= chunk;
    }
    return 0;
}
