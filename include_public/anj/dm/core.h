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
 * @brief Core data model API: object management, read/write helpers, bootstrap.
 *
 * Provides error codes for data model handlers, functions to add/remove
 * objects, read resources, handle chunked writes, and perform bootstrap
 * cleanup.
 */

#ifndef ANJ_DM_CORE_H
#    define ANJ_DM_CORE_H

#    include <anj/defs.h>
#    include <anj/dm/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/** @defgroup anj_dm_errors Data model handlers error codes
 * Error values that may be returned from data model handlers.
 * @{
 **/

/**
 * Request sent by the LwM2M Server was malformed or contained an invalid
 * value.
 */
#    define ANJ_DM_ERR_BAD_REQUEST (-(int) ANJ_COAP_CODE_BAD_REQUEST)

/**
 * LwM2M Server is not allowed to perform the operation due to lack of
 * necessary access rights.
 */
#    define ANJ_DM_ERR_UNAUTHORIZED (-(int) ANJ_COAP_CODE_UNAUTHORIZED)

/**
 * Target of the operation (Object/Instance/Resource) does not exist.
 */
#    define ANJ_DM_ERR_NOT_FOUND (-(int) ANJ_COAP_CODE_NOT_FOUND)

/**
 * Operation is not allowed in current device state or the attempted operation
 * is invalid for this target (Object/Instance/Resource)
 */
#    define ANJ_DM_ERR_METHOD_NOT_ALLOWED \
        (-(int) ANJ_COAP_CODE_METHOD_NOT_ALLOWED)

/**
 * Unspecified error, no other error code was suitable.
 */
#    define ANJ_DM_ERR_INTERNAL (-(int) ANJ_COAP_CODE_INTERNAL_SERVER_ERROR)

/**
 * Operation is not implemented by the LwM2M Client.
 */
#    define ANJ_DM_ERR_NOT_IMPLEMENTED (-(int) ANJ_COAP_CODE_NOT_IMPLEMENTED)

/**
 * LwM2M Client is busy processing some other request; LwM2M Server may retry
 * sending the same request after some delay.
 */
#    define ANJ_DM_ERR_SERVICE_UNAVAILABLE \
        (-(int) ANJ_COAP_CODE_SERVICE_UNAVAILABLE)

/**@}*/

/**
 * Adds an Object to the data model and validates its structure.
 *
 * @p obj parameter is not copied; the user must ensure that it remains valid
 * for the entire lifetime of the @p anj object or until it is removed using
 * @ref anj_dm_remove_obj.
 *
 * @note Validation of the Object, its Instances, and Resources is performed
 *       only in debug builds (i.e., when the @c NDEBUG macro is **not**
 *       defined). In release builds, internal consistency checks are skipped.
 *
 * @warning Object Instances in the @p obj structure must be stored in ascending
 *          order by their @c oid.
 *
 * @param anj Anjay object.
 * @param obj Pointer to the Object definition struct.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_add_obj(anj_t *anj, const anj_dm_obj_t *obj);

/**
 * Removes Object from the data model.
 *
 * @param anj Anjay object.
 * @param oid ID number of the Object to be removed.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_remove_obj(anj_t *anj, anj_oid_t oid);

/**
 * Reads the value of the Resource or Resource Instance.
 *
 * @param  anj       Anjay object.
 * @param  path      Resource or Resource Instance path.
 * @param[out] out_value Resource value.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_res_read(anj_t *anj,
                    const anj_uri_path_t *path,
                    anj_res_value_t *out_value);

/**
 * Handles writing of a opaque data in the @ref anj_dm_res_write_t handler.
 *
 * This function assists in collecting binary data that may arrive in multiple
 * chunks during the LwM2M Write operation. It copies a single chunk of data
 * from the @ref anj_res_value_t structure into the specified target @p buffer
 * at the automatically tracked offset - for this reason, @p buffer is expected
 * to be the same across multiple invocations of this function.
 *
 * This function does **not** add a terminator — it is intended for binary
 * (non-null-terminated) content.
 *
 * @note This function must be used **only** for Resources of type
 *       @ref ANJ_DATA_TYPE_BYTES. For string values, use
 *       @ref anj_dm_write_string_chunked instead.
 *
 * @param      value             Resource value to be written.
 * @param[out] buffer            Target buffer to write into.
 * @param      buffer_len        Size of the target buffer.
 * @param[out] out_bytes_len     Set to total data length if the current chunk
 *                               is the last one.
 * @param[out] out_is_last_chunk Optional parameter. Set to @c true if this is
 *                               the last chunk of data.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_write_bytes_chunked(const anj_res_value_t *value,
                               uint8_t *buffer,
                               size_t buffer_len,
                               size_t *out_bytes_len,
                               bool *out_is_last_chunk);

/**
 * Handles writing of a string value in the @ref anj_dm_res_write_t handler.
 *
 * This function assists in collecting strings that may arrive in multiple
 * chunks during the LwM2M Write operation. It copies a single chunk of data
 * from the @ref anj_res_value_t structure into the specified target @p buffer
 * at the automatically tracked offset - for this reason, @p buffer is expected
 * to be the same across multiple invocations of this function.
 *
 * When the final chunk is received, a null terminator (@c '\0') is additionally
 * written into the @p buffer.
 *
 * @note This function must be used **only** for Resources of type
 *       @ref ANJ_DATA_TYPE_STRING. For binary values, use
 *       @ref anj_dm_write_bytes_chunked instead.
 *
 * @param      value             Resource value to be written.
 * @param[out] buffer            Target buffer to write the data into.
 * @param      buffer_len        Size of the target buffer (must include space
 *                               for the null terminator).
 * @param[out] out_is_last_chunk Optional parameter. Set to @c true if this is
 *                               the last chunk of data.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_dm_write_string_chunked(const anj_res_value_t *value,
                                char *buffer,
                                size_t buffer_len,
                                bool *out_is_last_chunk);

/** @cond */
#    define ANJ_INTERNAL_INCLUDE_DM_DEFS
#    include <anj_internal/dm/defs.h>
#    undef ANJ_INTERNAL_INCLUDE_DM_DEFS
/** @endcond */

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_CORE_H
