/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "../init_internal.h"

#if defined(ANJ_WITH_MBEDTLS) && defined(ANJ_WITH_SECURITY)
#    include <mbedtls/platform_util.h>

#    include <anj/compat/crypto/zeroize.h>

void anj_crypto_zeroize(void *buf, size_t size) {
    mbedtls_platform_zeroize(buf, size);
}
#endif // ANJ_WITH_MBEDTLS && ANJ_WITH_SECURITY
