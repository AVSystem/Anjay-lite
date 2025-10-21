/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifdef ANJ_WITH_RNG_POSIX_COMPAT

#    include <errno.h>
#    include <stdint.h>
#    include <string.h>
#    include <sys/random.h>
#    include <sys/types.h>

#    include <anj/compat/rng.h>
#    include <anj/log.h>
#    include <anj/utils.h>

int anj_rng_generate(uint8_t *buffer, size_t size) {
    ssize_t result = getrandom(buffer, size, 0);
    if (result > 0 && (size_t) result != size) {
        anj_log(rng, L_ERROR, "Failed to generate random bytes");
        return -1;
    } else if (result < 0) {
        anj_log(rng, L_ERROR, "getrandom failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

#endif // ANJ_WITH_RNG_POSIX_COMPAT
