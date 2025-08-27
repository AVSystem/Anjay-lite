/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_DM_IO_H
#    define ANJ_DM_IO_H

#    include <anj/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * There is no more data to read from data model.
 * This value can be returned by:
 *      - @ref _anj_dm_get_read_entry
 *      - @ref _anj_dm_get_register_record
 *      - @ref _anj_dm_get_discover_record
 *      - @ref _anj_dm_get_bootstrap_discover_record
 */
#    define _ANJ_DM_LAST_RECORD 1

/**
 * There is no data to read from data model.
 * This value can be returned by:
 *      - @ref _anj_dm_composite_next_path
 */
#    define _ANJ_DM_NO_RECORD 2

/**
 * A group of error codes resulting from incorrect API usage or memory
 * issues. If this occurs, the @ref ANJ_COAP_CODE_INTERNAL_SERVER_ERROR should
 * be returned in response.
 */

/** Invalid input arguments. */
#    define _ANJ_DM_ERR_INPUT_ARG (-1)
/** Not enough space in buffer or array. */
#    define _ANJ_DM_ERR_MEMORY (-2)
/** Invalid call. */
#    define _ANJ_DM_ERR_LOGIC (-3)

/**
 * Initializes the data model context.
 *
 * @param anj  Anjay object to operate on.
 */
void _anj_dm_initialize(anj_t *anj);

/**
 * Must be called at the beginning of each operation on the data model. It is to
 * be called only once, even if the message is divided into several blocks.
 * Data model operations are:
 *      - ANJ_OP_REGISTER,
 *      - ANJ_OP_UPDATE,
 *      - ANJ_OP_DM_READ,
 *      - ANJ_OP_DM_READ_COMP,
 *      - ANJ_OP_DM_DISCOVER,
 *      - ANJ_OP_DM_WRITE_REPLACE,
 *      - ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
 *      - ANJ_OP_DM_WRITE_COMP,
 *      - ANJ_OP_DM_EXECUTE,
 *      - ANJ_OP_DM_CREATE,
 *      - ANJ_OP_DM_DELETE.
 *
 * @param anj                   Anjay object to operate on.
 * @param operation             Data model operation type.
 * @param is_bootstrap_request  Indicate source of request.
 * @param path                  Path from the request, if not specified should
 *                              be NULL.
 *
 * @returns
 * - 0 on success,
 * - a negative value in case of error.
 */
int _anj_dm_operation_begin(anj_t *anj,
                            _anj_op_t operation,
                            bool is_bootstrap_request,
                            const anj_uri_path_t *path);

/**
 * Called at the end of each operation on the data model. If during
 * operation any function returns error value this function must be called
 * immediately.
 *
 * @param anj Anjay object to operate on.
 *
 * @returns
 * - 0 on success,
 * - a negative value in case of error.
 */
int _anj_dm_operation_end(anj_t *anj);

/**
 * Processes READ, READ-COMPOSITE and BOOTSTRAP-READ operation. Should be
 * repeatedly called until it returns the @ref _ANJ_DM_LAST_RECORD. Returns all
 * @ref ANJ_DM_RES_R, @ref ANJ_DM_RES_RW Resources/ Resource Instances from
 * path given in @ref _anj_dm_operation_begin for READ and BOOTSTRAP-READ
 * operation or given in @ref _anj_dm_composite_next_path for READ-COMPOSITE
 * operation.
 *
 * @param      anj         Anjay object to operate on.
 * @param[out] out_record  Resource or Resource Instance record, with defined
 *                         type, value and path.
 *
 * @returns
 * - 0 on success,
 * - @ref _ANJ_DM_LAST_RECORD when last record was read,
 * - a negative value in case of error.
 */
int _anj_dm_get_read_entry(anj_t *anj, anj_io_out_entry_t *out_record);

/**
 * Returns information about the number of Resources and Resource Instances that
 * can be read for the READ operation currently in progress.
 *
 * IMPORTANT: Call this function only after a successful @ref
 * _anj_dm_operation_begin call for @ref ANJ_OP_DM_READ operation.
 * If @p out_res_count is set to <c>0</c>, immediately call @ref
 * _anj_dm_operation_end.
 *
 * @param      anj           Anjay object to operate on.
 * @param[out] out_res_count Return number of the readable Resources.
 */
void _anj_dm_get_readable_res_count(anj_t *anj, size_t *out_res_count);

#    ifdef ANJ_WITH_COMPOSITE_OPERATIONS
/**
 * Provide new path for READ-COMPOSITE operation. Should be call before @ref
 * _anj_dm_get_read_entry. Do not call @ref _anj_dm_get_read_entry if function
 * returns @ref _ANJ_DM_NO_RECORD.
 *
 * @returns
 * - 0 on success,
 * - @ref _ANJ_DM_NO_RECORD when there is no records under provided path,
 * - a negative value in case of error.
 */
int _anj_dm_composite_next_path(anj_t *anj, const anj_uri_path_t *path);

/**
 * Returns information about the number of Resources and Resource Instances that
 * can be read from @p path. Use it in order to process READ-COMPOSITE
 * operation. While processing request on root path, disallowed Objects
 * are skipped.
 *
 * IMPORTANT: Call this function only after a successful @ref
 * _anj_dm_operation_begin call for @ref ANJ_OP_DM_READ_COMP operation.
 * If @p out_res_count is set to <c>0</c>, immediately call @ref
 * _anj_dm_operation_end or process the next record.
 *
 * @param      anj           Anjay object to operate on.
 * @param      path          Target uri path.
 * @param[out] out_res_count Return number of the readable Resources.
 *
 * @returns
 * - 0 on success,
 * - @ref ANJ_DM_ERR_UNAUTHORIZED when trying to access Security or OSCORE
 * objects
 * - @ref ANJ_DM_ERR_NOT_FOUND if path doesn't exist.
 */
int _anj_dm_count_readable_res_if_allowed(anj_t *anj,
                                          const anj_uri_path_t *path,
                                          size_t *out_res_count);
#    endif // ANJ_WITH_COMPOSITE_OPERATIONS

/**
 * Creates a new instance of the object. Call this function only after a
 * successful @ref _anj_dm_operation_begin call for @ref ANJ_OP_DM_CREATE
 * operation and before any @ref _anj_dm_write_entry call.
 *
 * @param anj  Anjay object to operate on.
 * @param iid  New instance ID. Set to ANJ_ID_INVALID if the value was not
 *             specified in the LwM2M server request, in which case the first
 *             free value will be selected.
 *
 * @returns
 * - 0 on success,
 * - a negative value in case of error.
 */
int _anj_dm_create_object_instance(anj_t *anj, anj_iid_t iid);

/**
 * Adds another record during any kind of WRITE and CREATE operation. Depending
 * on the value of @ref _anj_op_t we specified when calling @ref
 * _anj_dm_operation_begin this function can be used to handle the following
 * operations:
 *      - ANJ_OP_DM_WRITE_REPLACE,
 *      - ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
 *      - ANJ_OP_DM_WRITE_COMP,
 *      - ANJ_OP_DM_CREATE.
 *
 * @param anj     Anjay object to operate on.
 * @param record  Resource or Resource Instance record, with defined type, value
 *                and path.
 *
 * @returns
 * - 0 on success,
 * - a negative value in case of error.
 */
int _anj_dm_write_entry(anj_t *anj, const anj_io_out_entry_t *record);

/**
 * Returns information Resource value type, might be useful when payload format
 * does not contain information about the type of data.
 *
 * @param      anj       Anjay object to operate on.
 * @param      path      Resource or Resource Instance path.
 * @param[out] out_type  Resource or Resource Instance value type.
 *
 * @returns
 * - 0 on success,
 * - a negative value in case of error.
 */
int _anj_dm_get_resource_type(anj_t *anj,
                              const anj_uri_path_t *path,
                              anj_data_type_t *out_type);

/**
 * Processes REGISTER operation. Should be repeatedly called until it returns
 * the @ref _ANJ_DM_LAST_RECORD. Provides information about Objects and Object
 * Instances of the data model.
 *
 * @param      anj         Anjay object to operate on.
 * @param[out] out_path    Object or Object Instance path.
 * @param[out] out_version Object version, provided for Object @p out_path, set
 *                         to NULL if not present.
 *
 * @returns
 * - 0 on success,
 * - @ref _ANJ_DM_LAST_RECORD when last record was read,
 * - a negative value in case of error.
 */
int _anj_dm_get_register_record(anj_t *anj,
                                anj_uri_path_t *out_path,
                                const char **out_version);

#    ifdef ANJ_WITH_DISCOVER
/**
 * Processes DISCOVER operation. Should be repeatedly called until it returns
 * the @ref _ANJ_DM_LAST_RECORD. Provides all elements of the data model
 * included in path specified in @ref _anj_dm_operation_begin.
 *
 * @param      anj         Anjay object to operate on.
 * @param[out] out_path    Object, Object Instance, Resource or Resource
 *                         Instance path.
 * @param[out] out_version Object version, provided for Object @p out_path, set
 *                         to NULL if not present.
 * @param[out] out_dim     Relevant only if @p out_path is Resource, contains
 *                         the number of the Resource Instances. For
 *                         Single-Instance Resources set to NULL.
 *
 * @returns
 * - 0 on success,
 * - @ref _ANJ_DM_LAST_RECORD when last record was read,
 * - a negative value in case of error.
 */
int _anj_dm_get_discover_record(anj_t *anj,
                                anj_uri_path_t *out_path,
                                const char **out_version,
                                const uint16_t **out_dim);
#    endif // ANJ_WITH_DISCOVER

#    ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
/**
 * Processes BOOTSTRAP-DISCOVER operation. Should be repeatedly called until it
 * returns the @ref _ANJ_DM_LAST_RECORD. Provides all elements of the data model
 * included in path specified in @ref _anj_dm_operation_begin.
 *
 * @param      anj         Anjay object to operate on.
 * @param[out] out_path    Object, Object Instance, Resource or Resource
 *                         Instance path.
 * @param[out] out_version Object version, provided for Object @p out_path, set
 *                         to NULL if not present.
 * @param[out] out_ssid    Short server ID of Object Instance, relevant for
 *                         Security, OSCORE and Servers Object Instances. Set to
 *                         NULL if not present.
 * @param[out] out_uri     Server URI relevant for Security Object Instances.
 *                         Set to NULL if not present.
 *
 * @returns
 * - 0 on success,
 * - @ref _ANJ_DM_LAST_RECORD when last record was read,
 * - a negative value in case of error.
 */
int _anj_dm_get_bootstrap_discover_record(anj_t *anj,
                                          anj_uri_path_t *out_path,
                                          const char **out_version,
                                          const uint16_t **out_ssid,
                                          const char **out_uri);
#    endif // ANJ_WITH_BOOTSTRAP_DISCOVER

/**
 * Processes EXECUTE operation, on the Resource pointed to by path specified in
 * @ref _anj_dm_operation_begin. If there is a payload in the request then
 * pass it through @p execute_arg.
 *
 * @param anj             Anjay object to operate on.
 * @param execute_arg     Payload provided in EXECUTE request, set to NULL if
 *                        not present in COAP request.
 * @param execute_arg_len Execute payload length.
 *
 * @returns
 * - 0 on success,
 * - a negative value in case of error.
 */
int _anj_dm_execute(anj_t *anj,
                    const char *execute_arg,
                    size_t execute_arg_len);

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_IO_H
