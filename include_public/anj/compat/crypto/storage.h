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
 * @brief Platform hooks for external cryptographic storage.
 *
 * This header declares the API that platform integrators can implement
 * to back Anjay Lite's credential handling with a secure store or HSM.
 *
 * Typical responsibilities include:
 * - initializing and deinitializing a storage context
 * - creating and deleting key/certificate records
 * - writing data in chunks and finalizing records
 * - resolving identifiers into actual key/certificate bytes
 * - (optionally) serializing/deserializing persistence identifiers
 *
 * Which functions are required depends on build-time options such as
 * @c ANJ_WITH_EXTERNAL_CRYPTO_STORAGE and @c ANJ_WITH_PERSISTENCE.
 */

#ifndef ANJ_CRYPTO_STORAGE_H
#    define ANJ_CRYPTO_STORAGE_H

#    include <anj/crypto.h>
#    include <anj/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

#        ifdef ANJ_WITH_PERSISTENCE
/**
 * Maximum size of the persistence information used for storing cryptographic
 * data.
 */
#            define ANJ_CRYPTO_STORAGE_PERSISTENCE_INFO_MAX_SIZE 64
#        endif // ANJ_WITH_PERSISTENCE

/**
 * Called once in @ref anj_core_init to initialize the Cryptographic storage
 * module.
 *
 * @param[out] out_crypto_ctx Pointer to a pointer that will receive the
 *                            Cryptographic context. This can be NULL if the
 *                            Cryptographic module does not require a context.
 *
 * @return 0 on success, or non-zero error code on failure.
 */
int anj_crypto_storage_init(void **out_crypto_ctx);

/**
 * Creates a new security information record.
 *
 * This function is called to create a new security information record.
 * The implementation should allocate and initialize a new record on the
 * underlying storage side, which will then be populated with data in
 * subsequent calls to @ref anj_crypto_storage_store_data.
 *
 * The @p out_info->tag and @p out_info->source are set by the caller to
 * identify the type and source of the security information being stored.
 *
 * @param        crypto_ctx Cryptographic context.
 * @param[inout] out_info   Pointer to a structure that will receive the
 *                          newly created security information record.
 *
 * @return 0 on success, or a negative value on failure.
 */
int anj_crypto_storage_create_new_record(void *crypto_ctx,
                                         anj_crypto_security_info_t *out_info);

/**
 * Stores certificate or key data in the specified security record.
 *
 * This function is always called after a successful call to
 * @ref anj_crypto_storage_create_new_record for the same security record.
 *
 * This function is called to store a portion (or the entirety) of the
 * certificate or key data in the record identified by @p info. Data may be
 * provided in multiple calls, in which case the implementation should append
 * each chunk to the existing record until the final chunk is received.
 *
 * When @p last_chunk is true, the implementation should finalize the storage
 * process and make the record ready for use.
 *
 * @note In case of any error during the storage process, Anjay will always call
 *       @ref anj_crypto_storage_delete_record.
 *
 * @note Only one security record can be created at a time.
 *
 * @param crypto_ctx Cryptographic context.
 * @param info       Identifier of the target security record.
 * @param data       Pointer to the data to store.
 * @param data_size  Size of the provided data chunk in bytes.
 * @param last_chunk True if this is the final data chunk.
 *
 * @return 0 on success, or a negative value on failure.
 */
int anj_crypto_storage_store_data(void *crypto_ctx,
                                  const anj_crypto_security_info_t *info,
                                  const void *data,
                                  size_t data_size,
                                  bool last_chunk);

/**
 * Deletes a security record.
 *
 * This function removes the certificate, key, or other security information
 * identified by @p info. After successful deletion, the record should no longer
 * be retrievable through this API.
 *
 * @param crypto_ctx Cryptographic context.
 * @param info       Identifier of the security record to delete.
 *
 * @return 0 on success, or a negative value on failure.
 */
int anj_crypto_storage_delete_record(void *crypto_ctx,
                                     const anj_crypto_security_info_t *info);

/**
 * Retrieves the security information from the storage.
 *
 * This function is called to load the security information from the storage
 * into a buffer. The implementation should read the data associated with the
 * provided @p info and copy it into the @p out_buffer. The size of the
 * buffer is specified by @p out_buffer_size, and the actual size of the loaded
 * data should be returned in @p out_record_size.
 *
 * @param      crypto_ctx      Cryptographic context.
 * @param      info            Security record identifier.
 * @param[out] out_buffer      Buffer to store the loaded data.
 * @param      out_buffer_size Size of the output buffer.
 * @param[out] out_record_size Size of the loaded data.
 *
 * @return 0 on success, negative value on failure.
 */
int anj_crypto_storage_resolve_security_info(
        void *crypto_ctx,
        anj_crypto_security_info_external_t *info,
        char *out_buffer,
        size_t out_buffer_size,
        size_t *out_record_size);

/**
 * Deinitializes the Cryptographic storage context.
 *
 * This function is called once in @ref anj_core_shutdown to clean up the
 * resources associated with the Cryptographic storage context.
 *
 * @param out_crypto_ctx Cryptographic context.
 */
void anj_crypto_storage_deinit(void *out_crypto_ctx);

#        ifdef ANJ_WITH_PERSISTENCE
/**
 * Serializes key or certificate identification data from @ref
 * anj_crypto_security_info_external_t into the @ref ANJ_DATA_TYPE_BYTES format
 * for persistence.
 *
 * This function is called by the Security Object to convert the given @p info
 * into a byte array. The resulting data can be deserialized back into an @ref
 * anj_crypto_security_info_external_t using @ref
 * anj_crypto_storage_resolve_persistence_info.
 *
 * @note @p out_data_size can't exceed @ref
 *       ANJ_CRYPTO_STORAGE_PERSISTENCE_INFO_MAX_SIZE.
 *
 * @param      crypto_ctx    Cryptographic context.
 * @param      info          Key or certificate identifier used to retrieve the
 *                           data.
 * @param[out] out_data      Pointer to the data buffer.
 * @param[out] out_data_size Size of the retrieved data.
 *
 * @return 0 on success, or a negative value on failure.
 */
int anj_crypto_storage_get_persistence_info(
        void *crypto_ctx,
        const anj_crypto_security_info_external_t *info,
        void *out_data,
        size_t *out_data_size);

/**
 * This function is called by the Security Object to interpret the provided
 * @p data buffer and populate @p out_info with the corresponding key or
 * certificate identifier. It is the reverse operation of
 * @ref anj_crypto_storage_get_persistence_info.
 *
 * @note All data must be provided in single chunk.
 *
 * @param      crypto_ctx Cryptographic context.
 * @param      data       Pointer to the data buffer containing the certificate
 *                        or key identifier in @ref ANJ_DATA_TYPE_BYTES format.
 * @param      data_size  Size of the data buffer in bytes.
 * @param[out] out_info   Pointer to a structure that will be filled with the
 *                        resolved persistence information.
 *
 * @return 0 on success, or a negative value on failure.
 */
int anj_crypto_storage_resolve_persistence_info(
        void *crypto_ctx,
        const void *data,
        size_t data_size,
        anj_crypto_security_info_external_t *out_info);
#        endif // ANJ_WITH_PERSISTENCE

#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_CRYPTO_STORAGE_H
