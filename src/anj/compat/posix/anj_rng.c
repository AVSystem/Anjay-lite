/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 57

#ifdef ANJ_WITH_RNG_POSIX_COMPAT

#    include <errno.h>
#    include <stdint.h>
#    include <string.h>

#    if defined(__linux__) || defined(__APPLE__)
#        include <sys/random.h>
#    else
#        include <unistd.h> // BSD
#    endif

#    include <anj/compat/rng.h>
#    include <anj/log.h>
#    include <anj/utils.h>

int anj_rng_generate(uint8_t *buffer, size_t size) {
    const size_t MAX_CHUNK = 256; // getentropy() limit per call

    while (size > 0) {
        size_t chunk = size > MAX_CHUNK ? MAX_CHUNK : size;
        if (getentropy(buffer, chunk) != 0) {
            anj_log(rng, L_ERROR, "getentropy failed: %s", strerror(errno));
            return -1;
        }
        buffer += chunk;
        size -= chunk;
    }
    return 0;
}

#endif // ANJ_WITH_RNG_POSIX_COMPAT
