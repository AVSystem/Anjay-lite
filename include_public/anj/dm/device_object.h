/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_DM_DEVICE_OBJECT_H
#    define ANJ_DM_DEVICE_OBJECT_H

#    include <anj/core.h>
#    include <anj/dm/core.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_DEFAULT_DEVICE_OBJ

/*
 * HACK: error handling is not supported so in order to comply with the
 * definition of the object only one instance of Error Code resource is
 * defined with the value set to 0 which means "no errors".
 */
#        define ANJ_DM_DEVICE_ERR_CODE_RES_INST_MAX_COUNT 1

/**
 * Callback function type for handling the LwM2M Reboot resource (/3/0/4).
 *
 * This function is invoked when the Reboot resource is executed by the LwM2M
 * server. The user is expected to perform a system reboot after this callback
 * returns.
 *
 * @note The Execute operation may be sent as a Confirmable message,
 *       so it is recommended to delay the reboot until the response is
 *       successfully sent.
 *
 * @param arg  Opaque argument passed to the callback.
 * @param anj  Anjay object reporting the status change.
 */
typedef void anj_dm_reboot_callback_t(void *arg, anj_t *anj);

/**
 * Device object initialization structure. Should be filled before passing to
 * @ref anj_dm_device_obj_install .
 *
 * NOTE: when passing this structure to @ref anj_dm_device_obj_install, its
 * fields ARE NOT copied internally to DM, which means all dynamically
 * allocated strings MUST NOT be freed before removing the device object from
 * the DM.
 *
 * NOTE: Supported binding and modes resource (/3/0/16) is defined by
 * @ref ANJ_SUPPORTED_BINDING_MODES
 */
typedef struct {
    // /3/0/0 resource value
    const char *manufacturer;
    // /3/0/1 resource value
    const char *model_number;
    // /3/0/2 resource value
    const char *serial_number;
    // /3/0/3 resource value
    const char *firmware_version;
    // /3/0/4 resource callback
    // if not set, execute on /3/0/4 (reboot) resource will fail
    anj_dm_reboot_callback_t *reboot_cb;
    // argument passed to the reboot callback
    void *reboot_handler_arg;
} anj_dm_device_object_init_t;

/**
 * Complex structure of a whole Device Object entity context that holds the
 * Object and its Instance that are linked to Static Data Model.
 *
 * User is expected to instantiate a structure of this type and not modify it
 * directly throughout the LwM2M Client life.
 */
typedef struct {
    anj_dm_obj_t obj;
    anj_dm_obj_inst_t inst;
    anj_dm_reboot_callback_t *reboot_cb;
    void *reboot_handler_arg;
    const char *manufacturer;
    const char *model_number;
    const char *serial_number;
    const char *firmware_version;
    const char *binding_modes;
} anj_dm_device_obj_t;

/**
 * Installs device object (/3) in DM.
 *
 * Example usage:
 * @code
 * static int reboot_cb(void *arg, anj_t *anj) {
 *     // perform reboot
 * }
 * ...
 * anj_dm_device_obj_t dev_obj;
 * anj_dm_device_object_init_t dev_obj_init = {
 *     .manufacturer = "manufacturer",
 *     .model_number = "model_number",
 *     .serial_number = "serial_number",
 *     .firmware_version = "firmware_version",
 *     .reboot_handler = reboot_cb
 * };
 * anj_dm_device_obj_install(&anj, &dev_obj, &dev_obj_init);
 * @endcode
 *
 * @param anj        Anjay object to operate on.
 * @param device_obj Pointer to a device object structure. Must be non-NULL.
 * @param obj_init   Pointer to a device object's initialization structure. Must
 *                   be non-NULL.
 *
 * @returns non-zero value on error (i.e. object is already installed),
 *          0 otherwise.
 */
int anj_dm_device_obj_install(anj_t *anj,
                              anj_dm_device_obj_t *device_obj,
                              anj_dm_device_object_init_t *obj_init);

#    endif // ANJ_WITH_DEFAULT_DEVICE_OBJ

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_DEVICE_OBJECT_H
