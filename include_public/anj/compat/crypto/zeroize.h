/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Platform-independent API for securely clearing sensitive buffers.
 *
 * This header declares a minimal interface for securely erasing memory
 * contents used to store sensitive data. It defines:
 * - @ref anj_crypto_zeroize - securely overwrite a buffer
 *
 * The use of this API helps prevent leakage of sensitive information such as
 * cryptographic material or credentials.
 */

#ifndef ANJ_CRYPTO_ZEROIZE_H
#    define ANJ_CRYPTO_ZEROIZE_H

#    include <stddef.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_SECURITY
/**
 * Securely clears the contents of a memory buffer.
 *
 * This function overwrites the memory region pointed to by @p buf to ensure
 * that sensitive data such as cryptographic keys, passwords, or temporary
 * plaintext is not left in memory after it is no longer needed.
 *
 * The actual implementation of this operation is determined by the crypto
 * backend in use, and is designed to prevent the compiler from optimizing
 * the memory wiping away.
 *
 * @param buf   Pointer to the buffer to be cleared.
 * @param size  Size of the buffer in bytes.
 */
void anj_crypto_zeroize(void *buf, size_t size);
#    endif // ANJ_WITH_SECURITY

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_CRYPTO_ZEROIZE_H
