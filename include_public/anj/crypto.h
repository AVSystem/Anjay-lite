/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Cryptographic data descriptors used by Security Object and networking.
 *
 * Defines enums and structures for representing credentials such as
 * certificates, private keys, and PSK identity/keys. Data may be provided
 * inline in memory buffers or, if enabled, via opaque external references
 * (e.g. keystore identifiers).
 *
 * These types are typically used when configuring Security Object Instances
 * or transport security backends.
 */

#ifndef ANJ_CRYPTO_H
#    define ANJ_CRYPTO_H

#    include <anj/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_SECURITY

/**
 * Specifies where cryptographic data is obtained from.
 *
 * @note Most users will set this when populating @ref
 *       anj_dm_security_instance_init_t while adding a Security Object
 *       Instance.
 *
 * @see anj_crypto_security_info_t
 */
typedef enum {
    /**
     * No data set.
     */
    ANJ_CRYPTO_DATA_SOURCE_EMPTY = 0,

    /**
     * Data is provided inline in the structure.
     * The @ref anj_crypto_security_info_buffer_t::data field must point to a
     * valid memory region of size @ref
     * anj_crypto_security_info_buffer_t::data_size.
     *
     * @see anj_crypto_security_info_buffer_t
     */
    ANJ_CRYPTO_DATA_SOURCE_BUFFER,

#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    /**
     * Data is provided as an opaque external identifier (e.g., file path, alias
     * in a keystore).
     *
     * @see anj_crypto_security_info_external_t
     */
    ANJ_CRYPTO_DATA_SOURCE_EXTERNAL
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
} anj_crypto_data_source_t;

/**
 * Specifies the type of cryptographic security information.
 */
typedef enum {
    ANJ_CRYPTO_SECURITY_TAG_CERTIFICATE_CHAIN,
    ANJ_CRYPTO_SECURITY_TAG_PRIVATE_KEY,
    ANJ_CRYPTO_SECURITY_TAG_CERT_REVOCATION_LIST,
    ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY,
    ANJ_CRYPTO_SECURITY_TAG_PSK_KEY,
} anj_crypto_security_tag_t;

/** In-memory byte buffer for cryptographic data. */
typedef struct {
    const void *data;
    size_t data_size;
} anj_crypto_security_info_buffer_t;

#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
/** Opaque external identifier (e.g., file path, alias in a keystore). */
typedef struct {
    const char *identity;
} anj_crypto_security_info_external_t;

#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

/**
 * Tagged union describing a security info source.
 * The @ref source field selects which member of @ref info is valid.
 */
typedef struct {
    anj_crypto_security_tag_t tag;
    anj_crypto_data_source_t source;
    union {
        anj_crypto_security_info_buffer_t buffer;
#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        anj_crypto_security_info_external_t external;
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    } info;
} anj_crypto_security_info_t;

#    endif // ANJ_WITH_SECURITY

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_CRYPTO_H
