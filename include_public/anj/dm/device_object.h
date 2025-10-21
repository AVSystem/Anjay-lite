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
 * @brief Default implementation of the LwM2M Device Object (/3).
 *
 * Provides installation and basic metadata resources such as manufacturer,
 * model, serial number, firmware version, and reboot handling.
 */

#ifndef ANJ_DM_DEVICE_OBJECT_H
#    define ANJ_DM_DEVICE_OBJECT_H

#    include <anj/core.h>
#    include <anj/dm/core.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_DEFAULT_DEVICE_OBJ
/**
 * Callback function type for handling the Reboot Resource (/3/0/4).
 *
 * This function is invoked when the Reboot Resource is executed by the LwM2M
 * server. The user is expected to perform a system reboot after this callback
 * returns.
 *
 * @warning The Execute operation is a Confirmable CoAP request, so the LwM2M
 *          server expects an acknowledgement. It's recommended to delay the
 *          reboot until the response is successfully sent by the client.
 *
 * @param arg Opaque argument passed to the callback, as provided in @ref
 *            anj_dm_device_object_init_t::reboot_cb_arg.
 * @param anj Anjay object.
 */
typedef void anj_dm_reboot_callback_t(void *arg, anj_t *anj);

/**
 * Device Object initialization structure. Should be filled before passing to
 * @ref anj_dm_device_obj_install.
 *
 * @note Supported Binding and Modes Resource (/3/0/16) is defined by
 *       @ref ANJ_SUPPORTED_BINDING_MODES.
 *
 * @warning Strings passed to this structure are not copied internally when
 *          @ref anj_dm_device_obj_install is called. The user must ensure that
 *          they remain valid for the entire lifetime of @ref anj_t object or
 *          until the Object is removed using @ref anj_dm_remove_obj.
 */
typedef struct {
    /** Manufacturer Resource (/3/0/0) value. */
    const char *manufacturer;

    /** Model Number Resource (/3/0/1) value. */
    const char *model_number;

    /** Serial Number Resource (/3/0/2) value. */
    const char *serial_number;

    /** Firmware Version Resource (/3/0/3) value. */
    const char *firmware_version;

    /** Reboot Resource (/3/0/4) callback.
     *
     * @note If not set, Execute operation on this resource will fail.
     */
    anj_dm_reboot_callback_t *reboot_cb;

    /**
     * Argument passed to @ref reboot_cb when it is invoked.
     */
    void *reboot_cb_arg;
} anj_dm_device_object_init_t;

/**
 * Internal state of Device Object.
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
    anj_dm_reboot_callback_t *reboot_cb;
    void *reboot_cb_arg;
    const char *manufacturer;
    const char *model_number;
    const char *serial_number;
    const char *firmware_version;
    const char *binding_modes;
} anj_dm_device_obj_t;

/**
 * Installs Device Object (/3) in data model.
 *
 * Example usage:
 * @code
 * static int reboot_cb(void *arg, anj_t *anj) {
 *     // perform reboot
 * }
 *
 * // ...
 *
 * anj_dm_device_obj_t dev_obj;
 * static const anj_dm_device_object_init_t dev_obj_init = {
 *     .manufacturer = "manufacturer",
 *     .model_number = "model_number",
 *     .serial_number = "serial_number",
 *     .firmware_version = "firmware_version",
 *     .reboot_handler = reboot_cb
 * };
 * anj_dm_device_obj_install(&anj, &dev_obj, &dev_obj_init);
 * @endcode
 *
 * @param anj        Anjay object.
 * @param device_obj Pointer to a variable that will hold the state of the
 *                   Object.
 * @param obj_init   Pointer to Object's initialization structure.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_device_obj_install(anj_t *anj,
                              anj_dm_device_obj_t *device_obj,
                              const anj_dm_device_object_init_t *obj_init);

#    endif // ANJ_WITH_DEFAULT_DEVICE_OBJ

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_DEVICE_OBJECT_H
