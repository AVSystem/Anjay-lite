/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_DM_CORE_H
#    define ANJ_DM_CORE_H

#    include <anj/defs.h>
#    include <anj/dm/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/** Error values that may be returned from data model handlers. @{ */
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
/** Target of the operation (Object/Instance/Resource) does not exist. */
#    define ANJ_DM_ERR_NOT_FOUND (-(int) ANJ_COAP_CODE_NOT_FOUND)
/**
 * Operation is not allowed in current device state or the attempted operation
 * is invalid for this target (Object/Instance/Resource)
 */
#    define ANJ_DM_ERR_METHOD_NOT_ALLOWED \
        (-(int) ANJ_COAP_CODE_METHOD_NOT_ALLOWED)
/** Unspecified error, no other error code was suitable. */
#    define ANJ_DM_ERR_INTERNAL (-(int) ANJ_COAP_CODE_INTERNAL_SERVER_ERROR)
/** Operation is not implemented by the LwM2M Client. */
#    define ANJ_DM_ERR_NOT_IMPLEMENTED (-(int) ANJ_COAP_CODE_NOT_IMPLEMENTED)
/**
 * LwM2M Client is busy processing some other request; LwM2M Server may retry
 * sending the same request after some delay.
 */
#    define ANJ_DM_ERR_SERVICE_UNAVAILABLE \
        (-(int) ANJ_COAP_CODE_SERVICE_UNAVAILABLE)
/** @} */

/**
 * Adds an Object to the data model and validates its structure.
 *
 * The library does **not** modify the contents of @p obj or any of the
 * structures it references. The ownership and lifetime of all referenced memory
 * remain the responsibility of the caller.
 *
 * @note Validation of the Object, its Instances, and Resources is performed
 *       only in debug builds (i.e., when the @p NDEBUG macro is **not**
 *       defined). In release builds, this function will skip internal
 *       consistency checks.
 *
 * @note Object Instances in the @p obj structure must be stored in ascending
 *       order by their ID value.
 *
 * @param anj  Anjay object to operate on.
 * @param obj  Pointer to the Object definition struct.
 *
 * @returns 0 on success, a negative value in case of error.
 */
int anj_dm_add_obj(anj_t *anj, const anj_dm_obj_t *obj);

/**
 * Removes Object from the data model.
 *
 * @param anj  Anjay object to operate on.
 * @param oid  ID number of the Object to be removed.
 *
 * @returns 0 on success, @p ANJ_DM_ERR_NOT_FOUND if the Object is not found.
 */
int anj_dm_remove_obj(anj_t *anj, anj_oid_t oid);

/**
 * Reads the value of the Resource or Resource Instance.
 *
 * @param      anj       Anjay object to operate on.
 * @param      path      Resource or Resource Instance path.
 * @param[out] out_value Returned Resource value.
 *
 * @returns 0 on success, a negative value in case of error.
 */
int anj_dm_res_read(anj_t *anj,
                    const anj_uri_path_t *path,
                    anj_res_value_t *out_value);

/**
 * Handles writing of a opaque data in the @ref anj_dm_res_write_t handler.
 *
 * This function is designed to assist in collecting binary data that may arrive
 * in multiple chunks during the LwM2M Write operation. It copies a single
 * chunk of data from the @ref anj_res_value_t structure into the specified
 * target buffer at the correct offset.
 *
 * The function is intended to be called repeatedly for the same persistent
 * buffer, as subsequent chunks of data are provided by the library.
 * The @ref anj_res_value_t structure contains all information necessary to
 * place each chunk at the correct offset within the buffer — the user does
 * **not** need to manually manage offsets or write positions.
 *
 * The user is expected to provide the same buffer across invocations during
 * the same Write operation. The buffer must be large enough to accommodate
 * the entire expected data.
 *
 * This function does **not** add a terminator — it is intended for binary
 * (non-null-terminated) content.
 *
 * This function is reentrant and safe to call repeatedly for the same Write
 * operation, as long as the user correctly manages the buffer across calls.
 *
 * @note This function must be used **only** for Resources of type
 *       @ref ANJ_DATA_TYPE_BYTES. For string values, use
 *       @ref anj_dm_write_string_chunked instead.
 *
 * @param      value             Resource value to be written.
 * @param      buffer            Target buffer to write into.
 * @param      buffer_len        Size of the target buffer.
 * @param[out] out_bytes_len     Set to total data length if the current chunk
 * is the last one.
 * @param[out] out_is_last_chunk Set to true if this is the last chunk of data.
 *                               Optional parameter.
 *
 * @returns 0 on success, or @p ANJ_DM_ERR_INTERNAL if the buffer is too
 *          small to accommodate the data.
 */
int anj_dm_write_bytes_chunked(const anj_res_value_t *value,
                               uint8_t *buffer,
                               size_t buffer_len,
                               size_t *out_bytes_len,
                               bool *out_is_last_chunk);

/**
 * Handles writing of a string value in the @ref anj_dm_res_write_t handler.
 *
 * This function assists with collecting a potentially fragmented string value
 * from the @ref anj_res_value_t structure, which may arrive in multiple chunks
 * during the LwM2M Write operation (e.g., for larger payloads).
 *
 * The function is intended to be called repeatedly for the same persistent
 * buffer, as subsequent chunks of data are provided by the library.
 * The @ref anj_res_value_t structure contains all information necessary to
 * place each chunk at the correct offset within the buffer — the user does
 * **not** need to manually manage offsets or write positions.
 *
 * The user is expected to provide the same buffer across invocations during
 * the same Write operation. The buffer must be large enough to accommodate
 * the entire expected data.
 *
 * When the final chunk is received (i.e., when
 * `offset + chunk_length == full_length_hint`), a null terminator (`'\0'`) is
 * appended at position `buffer[full_length_hint]`, completing the string.
 *
 * This function is reentrant and safe to call multiple times for the same Write
 * operation, as long as the buffer passed in is appropriately managed by the
 * user (i.e., zero-initialized or reused correctly).
 *
 * @note This function must be used **only** for Resources of type
 *       @ref ANJ_DATA_TYPE_STRING.
 *
 * @param      value             Resource value to be written.
 * @param      buffer            Target buffer to write the data into.
 * @param      buffer_len        Size of the target buffer (must include space
 *                               for the null terminator).
 * @param[out] out_is_last_chunk Set to true if this is the last chunk of data.
 *                               Optional parameter.
 *
 * @returns 0 on success, or @p ANJ_DM_ERR_INTERNAL if the buffer is too
 *          small to accommodate the data.
 */
int anj_dm_write_string_chunked(const anj_res_value_t *value,
                                char *buffer,
                                size_t buffer_len,
                                bool *out_is_last_chunk);

/**
 * Removes all instances of the Server and Security objects except those
 * associated with the Bootstrap-Server. It is recommended to call
 * this function in case of bootstrap failure.
 *
 * @param anj Anjay object to operate on.
 */
void anj_dm_bootstrap_cleanup(anj_t *anj);

#    define ANJ_INTERNAL_INCLUDE_DM_DEFS
#    include <anj_internal/dm/defs.h>
#    undef ANJ_INTERNAL_INCLUDE_DM_DEFS

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_CORE_H
