/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_DM_SECURITY_OBJECT_H
#    define ANJ_DM_SECURITY_OBJECT_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/defs.h>
#    include <anj/dm/core.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_DEFAULT_SECURITY_OBJ

#        ifdef ANJ_WITH_BOOTSTRAP
#            define ANJ_DM_SECURITY_OBJ_INSTANCES 2
#        else // ANJ_WITH_BOOTSTRAP
#            define ANJ_DM_SECURITY_OBJ_INSTANCES 1
#        endif // ANJ_WITH_BOOTSTRAP

/*
 * Security Object Instance context, used to store Instance specific data, don't
 * modify directly.
 */
typedef struct {
    char server_uri[ANJ_SERVER_URI_MAX_SIZE];
    bool bootstrap_server;
    int64_t security_mode;
    char public_key_or_identity[ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE];
    size_t public_key_or_identity_size;
    char server_public_key[ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE];
    size_t server_public_key_size;
    char secret_key[ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE];
    size_t secret_key_size;
    uint32_t client_hold_off_time;
    uint16_t ssid;
    anj_iid_t iid;
} anj_dm_security_instance_t;

/**
 * Possible values of the Security Mode Resource, as described in the Security
 * Object definition.
 */
typedef enum {
    ANJ_DM_SECURITY_PSK = 0,         // Pre-Shared Key mode
    ANJ_DM_SECURITY_RPK = 1,         // Raw Public Key mode
    ANJ_DM_SECURITY_CERTIFICATE = 2, // Certificate mode
    ANJ_DM_SECURITY_NOSEC = 3,       // NoSec mode
    ANJ_DM_SECURITY_EST = 4,         // Certificate mode with EST
} anj_dm_security_mode_t;

/**
 * Initial structure of a single Instance of the Security Object.
 */
typedef struct {
    /** Resource 0: LwM2M Server URI
     * This resource has to be provided for initialization */
    const char *server_uri;
    /** Resource 1: Bootstrap-Server */
    bool bootstrap_server;
    /** Resource 2: Security Mode */
    anj_dm_security_mode_t security_mode;
    /** Resource 3: Public Key or Identity */
    const char *public_key_or_identity;
    size_t public_key_or_identity_size;
    /** Resource 4: Server Public Key */
    const char *server_public_key;
    size_t server_public_key_size;
    /** Resource 5: Secret Key */
    const char *secret_key;
    size_t secret_key_size;
    /** Resource 10: Short Server ID, for Bootstrap-Server instance ignored.*/
    uint16_t ssid;
    /** Resource 11: Client Hold Off Time, valid only for Bootstrap-Server
     * instance */
    uint32_t client_hold_off_time;
    /** Instance ID. If not set, first non-negative, free integer value is used.
     */
    const anj_iid_t *iid;
} anj_dm_security_instance_init_t;

/*
 * Complex structure of a whole Security Object entity context that holds the
 * Object and its Instances that are linked to Static Data Model.
 *
 * User is expected to instantiate a structure of this type and not modify it
 * directly throughout the LwM2M Client life.
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
 * Initialize Security Object context. Call this function only once before
 * adding any Instances.
 *
 * @param security_obj_ctx  Security Object context to be initialized.
 */
void anj_dm_security_obj_init(anj_dm_security_obj_t *security_obj_ctx);

/**
 * Adds new Instance of Security Object.
 *
 * @param security_obj_ctx  Context of the Security Object.
 * @param instance          Security Instance to insert.
 *
 * @return 0 in case of success, negative value in case of error.
 */
int anj_dm_security_obj_add_instance(anj_dm_security_obj_t *security_obj_ctx,
                                     anj_dm_security_instance_init_t *instance);

/**
 * Installs the Security Object into the Static Data Model. Call this function
 * after adding all Instances using @ref anj_dm_security_obj_add_instance.
 *
 * After calling this function, new Instances can be added only by LwM2M
 * Bootstrap Server.
 *
 * @param anj               Anjay object to operate on.
 * @param security_obj_ctx  Context of the Security Object.
 *
 * @return 0 in case of success, negative value in case of error.
 */
int anj_dm_security_obj_install(anj_t *anj,
                                anj_dm_security_obj_t *security_obj_ctx);

#    endif // ANJ_WITH_DEFAULT_SECURITY_OBJ

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_SECURITY_OBJECT_H
