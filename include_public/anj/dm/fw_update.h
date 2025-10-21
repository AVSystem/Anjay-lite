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
 * @brief Default implementation of the LwM2M Firmware Update Object (/5).
 *
 * Declares states, results, and handler callbacks for firmware update support.
 * Supports both Push (Package) and Pull (Package URI) delivery modes depending
 * on compile-time configuration.
 */

#ifndef ANJ_DM_FW_UPDATE_H
#    define ANJ_DM_FW_UPDATE_H

#    include <stddef.h>
#    include <stdint.h>

#    include <anj/core.h>
#    include <anj/dm/core.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_DEFAULT_FOTA_OBJ

/**
 * Numeric values of the Firmware Update Protocol Support Resource (/5/0/8).
 *
 * @note See @lwm2m_core &sect;E.6 for details.
 */
typedef enum {
    ANJ_DM_FW_UPDATE_STATE_IDLE = 0,
    ANJ_DM_FW_UPDATE_STATE_DOWNLOADING = 1,
    ANJ_DM_FW_UPDATE_STATE_DOWNLOADED = 2,
    ANJ_DM_FW_UPDATE_STATE_UPDATING = 3
} anj_dm_fw_update_state_t;

/**
 * Numeric values of the Update Result resource
 *
 * @note See @lwm2m_core &sect;E.6 for details.
 *
 * @warning The values of this enum are incorporated error codes defined
 *          in the LwM2M documentation, but actual code logic introduces a
 *          variation in the interpretation of the success code @ref
 *          ANJ_DM_FW_UPDATE_RESULT_SUCCESS. While the LwM2M documentation
 *          specifies it as a success code for the overall firmware update
 *          process (when paired with State Resource set to \em Idle), in this
 *          implementation, it is used in other contexts, mainly to signalize
 *          success at individual stages of the process. User should be mindful
 *          of this distinction and adhere to the return code descriptions
 *          provided for each function in this file.
 */
typedef enum {
    ANJ_DM_FW_UPDATE_RESULT_INITIAL = 0,
    ANJ_DM_FW_UPDATE_RESULT_SUCCESS = 1,
    ANJ_DM_FW_UPDATE_RESULT_NOT_ENOUGH_SPACE = 2,
    ANJ_DM_FW_UPDATE_RESULT_OUT_OF_MEMORY = 3,
    ANJ_DM_FW_UPDATE_RESULT_CONNECTION_LOST = 4,
    ANJ_DM_FW_UPDATE_RESULT_INTEGRITY_FAILURE = 5,
    ANJ_DM_FW_UPDATE_RESULT_UNSUPPORTED_PACKAGE_TYPE = 6,
    ANJ_DM_FW_UPDATE_RESULT_INVALID_URI = 7,
    ANJ_DM_FW_UPDATE_RESULT_FAILED = 8,
    ANJ_DM_FW_UPDATE_RESULT_UNSUPPORTED_PROTOCOL = 9
} anj_dm_fw_update_result_t;

/**
 * Initates the Push-mode download of a firmware package. The library calls this
 * function when a LwM2M Server performs a Write on Package Resource.
 *
 * If this callback returns @ref ANJ_DM_FW_UPDATE_RESULT_SUCCESS, it will be
 * followed by a series of calls to @ref anj_dm_fw_update_package_write_t that
 * pass the actual firmware package.
 *
 * @warning This handler must be implemented if @ref ANJ_FOTA_WITH_PUSH_METHOD
 *          is enabled.
 *
 * @param user_ptr Opaque pointer to user data, as passed to @ref
 *                 anj_dm_fw_update_object_install.
 *
 * @return The callback should return:
 *         - @ref ANJ_DM_FW_UPDATE_RESULT_INITIAL if the download process can be
 *           initiated,
 *         - other @ref anj_dm_fw_update_result_t value, accordingly to the
 *           reason of failure.
 */
typedef anj_dm_fw_update_result_t
anj_dm_fw_update_package_write_start_t(void *user_ptr);

/**
 * Passes parts of firmware package written by a LwM2M Server to the Package
 * Resource. This method is called for each chunk of data, in order.
 *
 * If this callback returns a value other than @ref
 * ANJ_DM_FW_UPDATE_RESULT_SUCCESS, the download process will be aborted.
 *
 * @warning This handler must be implemented if @ref ANJ_FOTA_WITH_PUSH_METHOD
 *          is enabled.
 *
 * @param user_ptr  Opaque pointer to user data, as passed to @ref
 *                  anj_dm_fw_update_object_install.
 * @param data      Pointer to the data chunk.
 * @param data_size Size of the data chunk.
 *
 * @return The callback should return:
 *         - @ref ANJ_DM_FW_UPDATE_RESULT_INITIAL if part of firmware package
 *           has been successfully processed,
 *         - other @ref anj_dm_fw_update_result_t value, accordingly to the
 *           reason of failure.
 */
typedef anj_dm_fw_update_result_t anj_dm_fw_update_package_write_t(
        void *user_ptr, const void *data, size_t data_size);

/**
 * Finalizes the process of writing firmware package chunks. This method is
 * called once, after the last chunk of data hsa been passed to @ref
 * anj_dm_fw_update_package_write_t.
 *
 * If this callback returns @ref ANJ_DM_FW_UPDATE_RESULT_SUCCESS, the State
 * Resource will be set to \em Downloaded and the library will wait for the
 * execution of Update Resource by the LwM2M Server to proceed with the actual
 * firmware update.
 *
 * @warning This handler must be implemented if @ref ANJ_FOTA_WITH_PUSH_METHOD
 *          is enabled.
 *
 * @param user_ptr Opaque pointer to user data, as passed to @ref
 *                 anj_dm_fw_update_object_install.
 *
 * @return The callback should return:
 *         - @ref ANJ_DM_FW_UPDATE_RESULT_INITIAL if the download process has
 *           been successfully finalized,
 *         - other @ref anj_dm_fw_update_result_t value, accordingly to the
 *           reason of failure.
 */
typedef anj_dm_fw_update_result_t
anj_dm_fw_update_package_write_finish_t(void *user_ptr);

/**
 * Initiates the Pull-mode download of a firmware package by providing the URI
 * written by a LwM2M Server to the Package URI Resource.
 *
 * User code should start the download of the firmware package from the given
 * URI. If this callback returns @ref ANJ_DM_FW_UPDATE_RESULT_SUCCESS, the State
 * Resource will be set to \em Downloading and the library will wait for the
 * call to @ref anj_dm_fw_update_object_set_download_result to proceed.
 *
 * @warning This handler must be implemented if @ref ANJ_FOTA_WITH_PULL_METHOD
 *          is enabled.
 *
 * @param user_ptr Opaque pointer to user data, as passed to @ref
 *                 anj_dm_fw_update_object_install.
 * @param uri      Null-terminated string containing the URI to the firmware
 *                 package.
 *
 * @return The callback should return:
 *         - @ref ANJ_DM_FW_UPDATE_RESULT_INITIAL if the download process can be
 *           initiated,
 *         - other @ref anj_dm_fw_update_result_t value, accordingly to the
 *           reason of failure.
 */
typedef anj_dm_fw_update_result_t anj_dm_fw_update_uri_write_t(void *user_ptr,
                                                               const char *uri);

/**
 * Schedules the actual upgrade with previously downloaded package.
 *
 * Will be called at request of the server, after a package has been downloaded
 * (by executing Update Resource when State Resource is \em Downloaded).
 *
 * User code should initiate the actual upgrade process. If this callback
 * returns @ref ANJ_DM_FW_UPDATE_RESULT_SUCCESS, the State Resource will be set
 * to \em Updating. The library will wait for the call to @ref
 * anj_dm_fw_update_object_set_update_result to set the State Resource back to
 * \em Idle and set the Result Resource accordingly, to notify the server about
 * the outcome of the upgrade.
 *
 * @warning Most users will want to implement firmware update in a way that
 *          involves a reboot. In such case, it is expected that this callback
 *          will only schedule the upgrade and return, while the upgrade process
 *          and reboot will be performed later. In such case, the user must
 *          ensure that @ref anj_dm_fw_update_object_set_update_result is called
 *          **IMMEDIATELY AFTER** installing the Firmware Update Object.
 *
 * @warning The Execute operation is a Confirmable CoAP request, so the LwM2M
 *          server expects an acknowledgement. Additionally, the Server might
 *          track State and Update Result Resources. It's recommended to delay
 *          the reboot until the response and Notifications are sent by the
 *          client.
 *
 * @param user_ptr Opaque pointer to user data, as passed to @ref
 *                 anj_dm_fw_update_object_install.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
typedef int anj_dm_fw_update_update_start_t(void *user_ptr);

/**
 * Returns the name of downloaded firmware package.
 *
 * The name will be exposed in the data model as the PkgName Resource. If this
 * callback returns @c NULL or is not implemented at all (with the corresponding
 * field set to @c NULL), PkgName Resource will contain an empty string.
 *
 * It only makes sense for this handler to return non-<c>NULL</c> values if
 * there is a valid package already downloaded. The library will not call this
 * handler in any state other than \em Downloaded.
 *
 * The library expects that the returned pointer will remain valid until the
 * next call to this handler, or until the State Resource changes to a value
 * other than \em Downloaded.
 *
 * @param user_ptr Opaque pointer to user data, as passed to @ref
 *                 anj_dm_fw_update_object_install.
 *
 * @return The callback shall return a pointer to a null-terminated string
 *         containing the package name, or @c NULL if it is not currently
 *         available.
 */
typedef const char *anj_dm_fw_update_get_name_t(void *user_ptr);

/**
 * Returns the version of downloaded firmware package.
 *
 * The version will be exposed in the data model as the PkgVersion Resource. If
 * this callback returns @c NULL or is not implemented at all (with the
 * corresponding field set to @c NULL), PkgVersion Resource will contain an
 * empty string.
 *
 * It only makes sense for this handler to return non-<c>NULL</c> values if
 * there is a valid package already downloaded. The library will not call this
 * handler in any state other than \em Downloaded.
 *
 * The library expects that the returned pointer will remain valid until the
 * next call to this handler, or until the State Resource changes to a value
 * other than \em Downloaded.
 *
 * @param user_ptr Opaque pointer to user data, as passed to @ref
 *                 anj_dm_fw_update_object_install.
 *
 * @return The callback shall return a pointer to a null-terminated string
 *         containing the package version, or @c NULL if it is not currently
 *         available.
 */
typedef const char *anj_dm_fw_update_get_version_t(void *user_ptr);

/**
 * Aborts any ongoing firmware package download process, either at the request
 * of the server (by writing an empty value to Package or Package URI
 * Resources), or after a failed download (indicated by returning an error value
 * by other callbacks involved in download process).
 *
 * The user should ensure that any temporary resources used during the
 * download are released, and that any ongoing download is aborted.
 *
 * @warning Note, that this function may be called without previously calling
 *          @ref anj_dm_fw_update_package_write_finish_t, so it shall also close
 *          the currently open download stream and free any temporary resources.
 *
 * @param user_ptr Opaque pointer to user data, as passed to @ref
 *                 anj_dm_fw_update_object_install.
 */
typedef void anj_dm_fw_update_reset_t(void *user_ptr);

/**
 * Collection of user‑provided callbacks used by the Firmware Update Object.
 *
 * @warning The user must ensure that this structure remains valid for the
 *          entire lifetime of @ref anj_t object or until the Object is removed
 *          using @ref anj_dm_remove_obj.
 */
typedef struct {
    /** Initiates Push‑mode download of the firmware package. */
    anj_dm_fw_update_package_write_start_t *package_write_start_handler;

    /** Writes a chunk of the firmware package during Push‑mode download. */
    anj_dm_fw_update_package_write_t *package_write_handler;

    /** Finalizes the Push‑mode download operation. */
    anj_dm_fw_update_package_write_finish_t *package_write_finish_handler;

    /** Handles Write to Package URI (starts Pull‑mode download). */
    anj_dm_fw_update_uri_write_t *uri_write_handler;

    /** Schedules execution of the actual firmware upgrade. */
    anj_dm_fw_update_update_start_t *update_start_handler;

    /** Returns the name of the downloaded firmware package. */
    anj_dm_fw_update_get_name_t *get_name;

    /** Returns the version of the downloaded firmware package. */
    anj_dm_fw_update_get_version_t *get_version;

    /** Aborts firmware download process and cleans up temporary resources. */
    anj_dm_fw_update_reset_t *reset_handler;
} anj_dm_fw_update_handlers_t;

/**
 * Internal state of Firmware Update Object.
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
    struct {
        int8_t state;
        int8_t result;
        anj_dm_fw_update_handlers_t *user_handlers;
        void *user_ptr;
#        if defined(ANJ_FOTA_WITH_PULL_METHOD)
        char uri[256];
#        endif // defined (ANJ_FOTA_WITH_PULL_METHOD)
#        if defined(ANJ_FOTA_WITH_PUSH_METHOD)
        bool write_start_called;
#        endif // defined (ANJ_FOTA_WITH_PUSH_METHOD)
    } repr;
} anj_dm_fw_update_entity_ctx_t;

/**
 * Installs Firmware Update Object in data model.
 *
 * @param anj        Anjay object.
 * @param entity_ctx Pointer to a variable that will hold the state of the
 *                   Object.
 * @param handlers   Pointer to a set of handler functions that handle
 *                   platform-specific parts of firmware update process.
 * @param user_ptr   Opaque pointer that will be passed to all callbacks.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_fw_update_object_install(anj_t *anj,
                                    anj_dm_fw_update_entity_ctx_t *entity_ctx,
                                    anj_dm_fw_update_handlers_t *handlers,
                                    void *user_ptr);

/**
 * Sets the result of upgrade process.
 *
 * For details on the expected usage of this function see description of @ref
 * anj_dm_fw_update_update_start_t callback.
 *
 * @warning This function MUST NOT be called in states other than @ref
 *          ANJ_DM_FW_UPDATE_STATE_IDLE or @ref ANJ_DM_FW_UPDATE_STATE_UPDATING.
 *          Calling this function in other states is an undefined behavior.
 *
 * @param anj        Anjay object.
 * @param entity_ctx Firmware Update Object state.
 * @param result     Result of the upgrade process. If the process is
 *                   successful, the value must be set to @ref
 *                   ANJ_DM_FW_UPDATE_RESULT_SUCCESS.
 */
void anj_dm_fw_update_object_set_update_result(
        anj_t *anj,
        anj_dm_fw_update_entity_ctx_t *entity_ctx,
        anj_dm_fw_update_result_t result);

/**
 * Sets the result of firmware download process in Pull mode.
 *
 * For details on the expected usage of this function see description of @ref
 * anj_dm_fw_update_uri_write_t callback.
 *
 * @warning Calling this function in states other than @ref
 *          ANJ_DM_FW_UPDATE_STATE_DOWNLOADING is an error.
 *
 * @param anj        Anjay object.
 * @param entity_ctx Firmware Update Object state.
 * @param result     Result of the download process. If the process is
 *                   successful, the value must be set to @ref
 *                   ANJ_DM_FW_UPDATE_RESULT_SUCCESS.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_fw_update_object_set_download_result(
        anj_t *anj,
        anj_dm_fw_update_entity_ctx_t *entity_ctx,
        anj_dm_fw_update_result_t result);

#    endif // ANJ_WITH_DEFAULT_FOTA_OBJ

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_FW_UPDATE_H
