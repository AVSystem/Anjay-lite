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
 * @brief Security credential access and provider registration API.
 *
 * Provides a public API for retrieving security credentials associated with
 * the LwM2M Security Object, as well as a mechanism for registering callbacks
 * used to supply such credentials.
 */

#ifndef ANJ_SECURITY_H
#    define ANJ_SECURITY_H

#    include <stdbool.h>

#    include <anj/compat/net/anj_net_api.h>
#    include <anj/crypto.h>
#    include <anj/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_SECURITY

/**
 * Retrieves the Pre-Shared Key (PSK) information for the specified
 * connection.
 *
 * The callback is expected to return credentials corresponding to the
 * Security Object instance used for the given connection.
 *
 * @note Anjay Lite supports only one non-Bootstrap Server LwM2M Server.
 *
 * @param      anj                   Anjay object.
 * @param      bootstrap_credentials If true, retrieves credentials for the
 *                                   Bootstrap Server, otherwise for the
 *                                   regular LwM2M Server.
 * @param[out] out_psk_info          Output parameter for the PSK
 *                                   information.
 *
 * @return 0 in case of success, negative value in case of error.
 */
typedef int
anj_security_get_psk_info_handler_t(const anj_t *anj,
                                    bool bootstrap_credentials,
                                    anj_net_psk_info_t *out_psk_info);

#        ifdef ANJ_WITH_CERTIFICATES
/**
 * Retrieves certificate-based security information for the specified
 * connection.
 *
 * The callback is expected to return credentials corresponding to the
 * Security Object instance used for the given connection.
 *
 * @note Anjay Lite supports only one non-Bootstrap Server LwM2M Server.
 *
 * @param      anj                   Anjay object.
 * @param      bootstrap_credentials If true, retrieves credentials for the
 *                                   Bootstrap Server, otherwise for the
 *                                   regular LwM2M Server.
 * @param[out] out_cert_info         Output parameter for certificate-based
 *                                   security information.
 *
 * @return 0 in case of success, negative value in case of error.
 */
typedef int
anj_security_get_cert_info_handler_t(const anj_t *anj,
                                     bool bootstrap_credentials,
                                     anj_net_certificate_info_t *out_cert_info);
#        endif // ANJ_WITH_CERTIFICATES

#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
/**
 * Moves newly configured security credentials (certificates and keys) from
 * Security Object instances to external storage and removes credentials that
 * are no longer used.
 *
 * This callback is called in two scenarios:
 * 1. After a successful Bootstrap Finish, i.e. only after the Bootstrap process
 *    has completed successfully and the newly provisioned Security Object state
 *    has been accepted.
 * 2. By @ref anj_dm_security_obj_install(), when using the default Security
 *    Object implementation, to move credentials to external storage immediately
 *    after the object is installed.
 *
 * The implementation is expected to move any new credentials to external
 * storage, unless they are already stored there, and to remove credentials from
 * external storage that have been overwritten or deleted from the Security
 * Object. Moving credentials to external storage may involve copying the
 * credential data and securely erasing the original in-memory buffers.
 *
 * @warning If moving credentials to external storage fails, the implementation
 *          MUST restore the affected credentials to the in-memory buffers, so
 *          that they remain available and the Security Object state can still
 *          be used safely.
 *
 * @param anj Anjay object used to access the Security Object and the crypto
 *            context.
 *
 * @return 0 in case of success, negative value in case of error.
 */
typedef int anj_security_offload_keys_and_certs_handler_t(anj_t *anj);
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

/**
 * Set of callbacks used to provide security credentials associated with the
 * LwM2M Security Object.
 */
typedef struct {
    anj_security_get_psk_info_handler_t *get_psk_info;
#        ifdef ANJ_WITH_CERTIFICATES
    anj_security_get_cert_info_handler_t *get_cert_info;
#        endif // ANJ_WITH_CERTIFICATES
#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    anj_security_offload_keys_and_certs_handler_t *offload_keys_and_certs;
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
} anj_security_credential_handlers_t;

/**
 * Registers callbacks used to access credentials associated with the LwM2M
 * Security Object.
 *
 * This function should be called during Security Object installation.
 *
 * If @ref ANJ_WITH_DEFAULT_SECURITY_OBJ is enabled, the default Security
 * Object integration registers appropriate callbacks automatically, so calling
 * this function is only necessary when providing a custom Security Object
 * implementation.
 *
 * The @p handlers structure is not copied. The library stores only a pointer
 * to it, so the user retains ownership and may update the registered callbacks
 * later by modifying that structure. The user MUST ensure that the structure
 * remains valid for as long as it may be used by the library.
 *
 * @param anj      Anjay object.
 * @param handlers Structure containing credential access callbacks.
 */
void anj_security_register_credential_handlers(
        anj_t *anj, const anj_security_credential_handlers_t *handlers);

/**
 * Retrieves the Pre-Shared Key (PSK) identity and key for the specified
 * connection.
 *
 * This function may be used both internally by the library and by user code
 * that needs access to credentials associated with the Security Object, for
 * example to configure transport security for firmware download.
 *
 * Credential data is provided by callbacks registered using @ref
 * anj_security_register_credential_handlers.
 *
 * @note Anjay Lite supports only one non-Bootstrap Server LwM2M Server.
 *
 * @param      anj                   Anjay object.
 * @param      bootstrap_credentials If true, retrieves credentials for the
 *                                   Bootstrap Server, otherwise for the regular
 *                                   LwM2M Server.
 * @param[out] out_psk_info          Output parameter for the PSK-based security
 *                                   information.
 *
 * @return 0 in case of success, negative value in case of error.
 */
int anj_security_get_psk_info(const anj_t *anj,
                              bool bootstrap_credentials,
                              anj_net_psk_info_t *out_psk_info);

#        ifdef ANJ_WITH_CERTIFICATES
/**
 * Retrieves certificate-based security information for the specified
 * connection.
 *
 * This function may be used both internally by the library and by user code
 * that needs access to credentials associated with the Security Object, for
 * example to configure transport security for firmware download.
 *
 * Credential data is provided by callbacks registered using @ref
 * anj_security_register_credential_handlers.
 *
 * @note Anjay Lite supports only one non-Bootstrap Server LwM2M Server.
 *
 * @param      anj                   Anjay object.
 * @param      bootstrap_credentials If true, retrieves credentials for the
 *                                   Bootstrap Server, otherwise for the regular
 *                                   LwM2M Server.
 * @param[out] out_cert_info         Output parameter for certificate-based
 *                                   security information.
 *
 * @return 0 in case of success, negative value in case of error.
 */
int anj_security_get_cert_info(const anj_t *anj,
                               bool bootstrap_credentials,
                               anj_net_certificate_info_t *out_cert_info);
#        endif // ANJ_WITH_CERTIFICATES

#    endif // ANJ_WITH_SECURITY

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_SECURITY_H
