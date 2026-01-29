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
 * @brief Default implementation of the LwM2M Server Object (/1).
 *
 * Provides initialization, instance management, and optional persistence of
 * Server Object state.
 */

#ifndef ANJ_DM_SERVER_OBJ_H
#    define ANJ_DM_SERVER_OBJ_H

#    include <stddef.h>
#    include <stdint.h>

#    include <anj/core.h>
#    include <anj/defs.h>
#    include <anj/dm/core.h>

#    ifdef ANJ_WITH_PERSISTENCE
#        include <anj/persistence.h>
#    endif // ANJ_WITH_PERSISTENCE

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_DEFAULT_SERVER_OBJ
/** @cond */
#        ifdef ANJ_WITH_LWM2M12
#            define ANJ_DM_SERVER_OBJ_BINDINGS "UMHTSN"
#        else // ANJ_WITH_LWM2M12
#            define ANJ_DM_SERVER_OBJ_BINDINGS "UTSN"
#        endif // ANJ_WITH_LWM2M12
/** @endcond */

/** @anj_internal_fields_do_not_use */
typedef struct {
    uint16_t ssid;
    uint32_t lifetime;
    uint32_t default_min_period;
    uint32_t default_max_period;
    uint32_t disable_timeout;
    uint8_t default_notification_mode;
    anj_communication_retry_res_t comm_retry_res;
    char binding[sizeof("UMHTSN")];
    bool bootstrap_on_registration_failure;
    bool mute_send;
    bool notification_storing;
} anj_dm_server_instance_t;

/**
 * Server Object Instance initialization structure. Should be filled before
 * passing to @ref anj_dm_server_obj_add_instance.
 */
typedef struct {
    /** Short Server ID Resource (/1/x/0) value. */
    uint16_t ssid;

    /** Lifetime Resource (/1/x/1) value. */
    uint32_t lifetime;

    /** Default Minimum Period Resource (/1/x/2) value. */
    uint32_t default_min_period;

    /**
     * Default Maximum Period Resource (/1/x/3) value. If set to 0, there's no
     * default @c pmax.
     */
    uint32_t default_max_period;

    /**
     * Disable Timeout Resource (/1/x/5) value. If not set, default of @ref
     * ANJ_DISABLE_TIMEOUT_DEFAULT_VALUE is used.
     */
    uint32_t disable_timeout;

    /**
     * Notification Storing When Disabled or Offline Resource (/1/x/6) value.
     */
    bool notification_storing;

    /** Binding Resource (/1/x/7) value. */
    const char *binding;

    /**
     * Bootstrap on Registration Failure Resource (/1/x/16) value. If @c NULL,
     * default of @c true is used.
     */
    const bool *bootstrap_on_registration_failure;

    /** Mute Send Resource (/1/x/23) value. */
    bool mute_send;

    /**
     * Optional Instance ID. If @c NULL, a new unique Instance ID will be
     * chosen automatically.
     */
    const anj_iid_t *iid;

    /**
     * Resources:
     * - Communication Retry Count (/1/x/17)
     * - Communication Retry Timer (/1/x/18)
     * - Communication Sequence Delay Timer (/1/x/19)
     * - Communication Sequence Retry Count (/1/x/20)
     * If @c NULL, @ref ANJ_COMMUNICATION_RETRY_RES_DEFAULT is used.
     */
    anj_communication_retry_res_t *comm_retry_res;

    /**
     * Default Notification Mode Resource (/1/x/26) value:
     * - 0: Non-Confirmable
     * - 1: Confirmable
     */
    uint8_t default_notification_mode;
} anj_dm_server_instance_init_t;

/**
 * Internal state of Server Object.
 *
 * @warning The user must ensure that this structure remains valid for the
 *          entire lifetime of @ref anj_t object or until the Object is removed
 *          using @ref anj_dm_remove_obj.
 *
 * @anj_internal_fields_do_not_use
 */
typedef struct {
    anj_dm_obj_t obj;
    anj_dm_obj_inst_t inst;
    anj_dm_obj_inst_t cache_inst;
    anj_dm_server_instance_t server_instance;
    anj_dm_server_instance_t cache_server_instance;
    bool installed;
} anj_dm_server_obj_t;

/**
 * Initializes Server Object internal state variable.
 *
 * This function must be called once, before adding any Instances.
 *
 * @param server_obj_ctx Pointer to a variable that will hold the state of the
 *                       Object.
 */
void anj_dm_server_obj_init(anj_dm_server_obj_t *server_obj_ctx);

/**
 * Adds new Instance of Server Object.
 *
 * @warning This function must not be called after @ref
 *          anj_dm_security_obj_install.
 *
 * @param server_obj_ctx Server Object state.
 * @param instance       Instance to insert.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_server_obj_add_instance(
        anj_dm_server_obj_t *server_obj_ctx,
        const anj_dm_server_instance_init_t *instance);

/**
 * Installs Server Object in data model.
 *
 * Call this function after adding all Instances using @ref
 * anj_dm_server_obj_add_instance. After calling this function, new Instances
 * can be added only by LwM2M Bootstrap Server.
 *
 * @param anj            Anjay object.
 * @param server_obj_ctx Server Object state.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_server_obj_install(anj_t *anj, anj_dm_server_obj_t *server_obj_ctx);

#        ifdef ANJ_WITH_PERSISTENCE
/**
 * Serializes the current LwM2M Server Object into the persistence stream.
 *
 * Writes Server Object instance and its resources to the underlying medium via
 * @p ctx->write.
 *
 * @param server_obj_ctx Server Object context to serialize.
 * @param ctx            Persistence context; must have
 *                       @ref anj_persistence_context_t::direction set to
 *                       ANJ_PERSISTENCE_STORE.
 *
 * @return 0 on success, negative value on error.
 */
int anj_dm_server_obj_store(anj_dm_server_obj_t *server_obj_ctx,
                            const anj_persistence_context_t *ctx);

/**
 * Deserializes the LwM2M Server Object from the persistence stream.
 *
 * Reads Server Object instance and its resources from the underlying medium via
 * @p ctx->read.
 *
 * @param server_obj_ctx Server Object context to fill.
 * @param ctx            Persistence context; must have
 *                       @ref anj_persistence_context_t::direction set to
 *                       ANJ_PERSISTENCE_RESTORE.
 *
 * @return 0 on success, negative value on error.
 */
int anj_dm_server_obj_restore(anj_dm_server_obj_t *server_obj_ctx,
                              const anj_persistence_context_t *ctx);
#        endif // ANJ_WITH_PERSISTENCE

#    endif // ANJ_WITH_DEFAULT_SERVER_OBJ

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_SERVER_OBJ_H
