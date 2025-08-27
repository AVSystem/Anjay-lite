/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_DM_SERVER_OBJ_H
#    define ANJ_DM_SERVER_OBJ_H

#    include <stddef.h>
#    include <stdint.h>

#    include <anj/core.h>
#    include <anj/defs.h>
#    include <anj/dm/core.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_DEFAULT_SERVER_OBJ

#        ifdef ANJ_WITH_LWM2M12
#            define ANJ_DM_SERVER_OBJ_BINDINGS "UMHTSN"
#        else // ANJ_WITH_LWM2M12
#            define ANJ_DM_SERVER_OBJ_BINDINGS "UTSN"
#        endif // ANJ_WITH_LWM2M12

/*
 * Server Object Instance context, used to store Instance specific data, don't
 * modify directly.
 */
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
} anj_server_instance_t;

/**
 * Representation of a single Instance of the Server Object.
 */
typedef struct {
    /** Resource: Short Server ID */
    uint16_t ssid;
    /** Resource: Lifetime */
    uint32_t lifetime;
    /** Resource: Default Minimum Period */
    uint32_t default_min_period;
    /** Resource: Default Maximum Period, value of 0, means pmax is ignored. */
    uint32_t default_max_period;
    /** Resource: Disable Timeout. If not set, default will be used. */
    uint32_t disable_timeout;
    /** Resource: Notification Storing When Disabled or Offline */
    bool notification_storing;
    /** Resource: Binding */
    const char *binding;
    /** Resource: Bootstrap on Registration Failure. True if not set. */
    const bool *bootstrap_on_registration_failure;
    /** Resource: Mute Send */
    bool mute_send;
    /** Instance ID. If not set, default value is used. */
    const anj_iid_t *iid;
    /** Communication Retry Resources. If not set, default value is used.*/
    anj_communication_retry_res_t *comm_retry_res;
    /*
     * Resource: Default Notification Mode.
     * 0 = NonConfirmable, 1 = Confirmable.
     */
    uint8_t default_notification_mode;
} anj_dm_server_instance_init_t;

/*
 * Complex structure of a whole Server Object entity context that holds the
 * Object and its Instances that are linked to Static Data Model.
 *
 * User is expected to instantiate a structure of this type and not modify it
 * directly throughout the LwM2M Client life.
 */
typedef struct {
    anj_dm_obj_t obj;
    anj_dm_obj_inst_t inst;
    anj_dm_obj_inst_t cache_inst;
    anj_server_instance_t server_instance;
    anj_server_instance_t cache_server_instance;
    bool installed;
} anj_dm_server_obj_t;

/**
 * Initializes the Server Object context. Call this function only once before
 * any other operation on the Server Object.
 *
 * @param server_obj_ctx  Context of the Server Object.
 */
void anj_dm_server_obj_init(anj_dm_server_obj_t *server_obj_ctx);

/**
 * Adds new Instance of Server Object. After calling @ref
 * anj_dm_server_obj_install, this function cannot be called.
 *
 * @param server_obj_ctx  Context of the Server Object.
 * @param instance        Server Instance to insert.
 *
 * @return 0 in case of success, negative value in case of error.
 */
int anj_dm_server_obj_add_instance(
        anj_dm_server_obj_t *server_obj_ctx,
        const anj_dm_server_instance_init_t *instance);

/**
 * Installs the Server Object into the Static Data Model.
 *
 * After calling this function, new Instance can be added only by LwM2M
 * Bootstrap Server.
 *
 * @param anj             Anjay object to operate on.
 * @param server_obj_ctx  Context of the Server Object.
 *
 * @return 0 in case of success, negative value in case of error.
 */
int anj_dm_server_obj_install(anj_t *anj, anj_dm_server_obj_t *server_obj_ctx);

#    endif // ANJ_WITH_DEFAULT_SERVER_OBJ

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_SERVER_OBJ_H
