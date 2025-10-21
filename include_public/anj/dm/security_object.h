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
 * @brief Default implementation of the LwM2M Security Object (/0).
 *
 * Provides initialization, instance management, optional persistence, and
 * access to stored security credentials.
 */

#ifndef ANJ_DM_SECURITY_OBJECT_H
#    define ANJ_DM_SECURITY_OBJECT_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/defs.h>
#    include <anj/dm/core.h>

#    ifdef ANJ_WITH_PERSISTENCE
#        include <anj/persistence.h>
#    endif // ANJ_WITH_PERSISTENCE

#    ifdef ANJ_WITH_SECURITY
#        include <anj/crypto.h>
#    endif // ANJ_WITH_SECURITY

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * Possible values of the Security Mode Resource, as described in the Security
 * Object definition. For details, see @lwm2m_core &sect;E.1.
 */
typedef enum {
    /** Pre-Shared Key mode */
    ANJ_DM_SECURITY_PSK = 0,

    /** Raw Public Key mode */
    ANJ_DM_SECURITY_RPK = 1,

    /** Certificate mode */
    ANJ_DM_SECURITY_CERTIFICATE = 2,

    /** NoSec mode */
    ANJ_DM_SECURITY_NOSEC = 3,

    /** Certificate mode with EST */
    ANJ_DM_SECURITY_EST = 4
} anj_dm_security_mode_t;

#    ifdef ANJ_WITH_DEFAULT_SECURITY_OBJ

/** @cond */
#        ifdef ANJ_WITH_BOOTSTRAP
#            define ANJ_DM_SECURITY_OBJ_INSTANCES 2
#        else // ANJ_WITH_BOOTSTRAP
#            define ANJ_DM_SECURITY_OBJ_INSTANCES 1
#        endif // ANJ_WITH_BOOTSTRAP
/** @endcond */

/** @anj_internal_fields_do_not_use */
typedef struct {
    char server_uri[ANJ_SERVER_URI_MAX_SIZE];
    bool bootstrap_server;
    anj_dm_security_mode_t security_mode;
    uint32_t client_hold_off_time;
    uint16_t ssid;
    anj_iid_t iid;

#        ifdef ANJ_WITH_SECURITY
    uint8_t public_key_or_identity_buff
            [ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE];
    uint8_t server_public_key_buff[ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE];
    uint8_t secret_key_buff[ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE];
    anj_crypto_security_info_t public_key_or_identity;
    anj_crypto_security_info_t server_public_key;
    anj_crypto_security_info_t secret_key;
#        endif // ANJ_WITH_SECURITY
} anj_dm_security_instance_t;

/**
 * Security Object Instance initialization structure. Should be filled before
 * passing to @ref anj_dm_security_obj_add_instance.
 */
typedef struct {
    /** LwM2M Server URI Resource (/0/x/0) value. */
    const char *server_uri;

    /** Bootstrap-Server Resource (/0/x/1) value. */
    bool bootstrap_server;

    /** Security Mode Resource (/0/x/2) value. */
    anj_dm_security_mode_t security_mode;

#        ifdef ANJ_WITH_SECURITY
    /** Public Key or Identity Resource (/0/x/3) value. */
    anj_crypto_security_info_t public_key_or_identity;

    /** Server Public Key Resource (/0/x/4) value. */
    anj_crypto_security_info_t server_public_key;

    /** Secret Key Resource (/0/x/5) value. */
    anj_crypto_security_info_t secret_key;
#        endif // ANJ_WITH_SECURITY

    /**
     * Short Server ID Resource (/0/x/10) value. This Resource is ignored for
     * Bootstrap Servers.
     */
    uint16_t ssid;

    /**
     * Client Hold Off Time Resource (/0/x/11) value. This Resource is ignored
     * for Management Servers.
     */
    uint32_t client_hold_off_time;

    /**
     * Optional Instance ID. If @c NULL, a new unique Instance ID will be
     * chosen automatically.
     */
    const anj_iid_t *iid;
} anj_dm_security_instance_init_t;

/**
 * Internal state of Security Object.
 *
 * @warning The user must ensure that this structure remains valid for the
 *          entire lifetime of @ref anj_t object or until the Object is removed
 *          using @ref anj_dm_remove_obj.
 *
 * @anj_internal_fields_do_not_use
 */
typedef struct {
    anj_dm_obj_t obj;
    anj_dm_obj_inst_t inst[ANJ_DM_SECURITY_OBJ_INSTANCES];
    anj_dm_obj_inst_t cache_inst[ANJ_DM_SECURITY_OBJ_INSTANCES];
    anj_dm_security_instance_t
            security_instances[ANJ_DM_SECURITY_OBJ_INSTANCES];
    anj_dm_security_instance_t
            cache_security_instances[ANJ_DM_SECURITY_OBJ_INSTANCES];
    bool installed;
    anj_iid_t new_instance_iid;
} anj_dm_security_obj_t;

/**
 * Initializes Security Object internal state variable.
 *
 * This function must be called once, before adding any Instances.
 *
 * @param security_obj_ctx Pointer to a variable that will hold the state of the
 *                         Object.
 */
void anj_dm_security_obj_init(anj_dm_security_obj_t *security_obj_ctx);

/**
 * Adds new Instance of Security Object.
 *
 * @note All data from @p instance is copied, so the caller can free it.
 *
 * @warning This function must not be called after @ref
 *          anj_dm_security_obj_install.
 *
 * @param security_obj_ctx Security Object state.
 * @param instance         Instance to insert.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_security_obj_add_instance(
        anj_dm_security_obj_t *security_obj_ctx,
        const anj_dm_security_instance_init_t *instance);

/**
 * Installs Security Object in data model.
 *
 * Call this function after adding all Instances using @ref
 * anj_dm_security_obj_add_instance. After calling this function, new Instances
 * can be added only by LwM2M Bootstrap Server.
 *
 * @param anj              Anjay object.
 * @param security_obj_ctx Security Object state.
 *
 * @return 0 in case of success, negative value in case of error.
 */
int anj_dm_security_obj_install(anj_t *anj,
                                anj_dm_security_obj_t *security_obj_ctx);

#        ifdef ANJ_WITH_SECURITY

/**
 * Retrieves the Pre-Shared Key (PSK) identity and key for the specified
 * connection.
 *
 * @note Anjay Lite supports only one non-Bootstrap Server LwM2M Server.
 *
 * @param      anj                   Anjay object to take the Security Object
 *                                   from.
 * @param      bootstrap_credentials If true, retrieves credentials for the
 *                                   Bootstrap Server, otherwise for the regular
 *                                   LwM2M Server.
 * @param[out] out_psk_identity      Output parameter for the PSK identity.
 * @param[out] out_psk_key           Output parameter for the PSK key.
 *
 * @return 0 in case of success, @ref ANJ_DM_ERR_NOT_FOUND if instance not
 *         found.
 */
int anj_dm_security_obj_get_psk(const anj_t *anj,
                                bool bootstrap_credentials,
                                anj_crypto_security_info_t *out_psk_identity,
                                anj_crypto_security_info_t *out_psk_key);
#        endif // ANJ_WITH_SECURITY

#        ifdef ANJ_WITH_PERSISTENCE
/**
 * Serializes the current LwM2M Security Object into the persistence stream.
 *
 * Writes all present Security Object instances and their resources to the
 * underlying medium via @p ctx->write.
 *
 * @param anj              Initialized Anjay-Lite handle.
 * @param security_obj_ctx Security Object context to serialize.
 * @param ctx              Persistence context; must have
 *                         @ref anj_persistence_context_t::direction set to
 *                         ANJ_PERSISTENCE_STORE.
 *
 * @return 0 on success, negative value on error.
 */
int anj_dm_security_obj_store(anj_t *anj,
                              anj_dm_security_obj_t *security_obj_ctx,
                              const anj_persistence_context_t *ctx);

/**
 * Deserializes the LwM2M Security Object from the persistence stream.
 *
 * Reads Security Object instances and their resources from the underlying
 * medium via @p ctx->read.
 *
 * @param anj              Initialized Anjay-Lite handle.
 * @param security_obj_ctx Security Object context to fill.
 * @param ctx              Persistence context; must have
 *                         @ref anj_persistence_context_t::direction set to
 *                         ANJ_PERSISTENCE_RESTORE.
 *
 * @return 0 on success, negative value on error.
 */
int anj_dm_security_obj_restore(anj_t *anj,
                                anj_dm_security_obj_t *security_obj_ctx,
                                const anj_persistence_context_t *ctx);
#        endif // ANJ_WITH_PERSISTENCE

#    endif // ANJ_WITH_DEFAULT_SECURITY_OBJ

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_SECURITY_OBJECT_H
