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
 * @brief Data model definitions: resources, instances, objects, and handlers.
 *
 * Provides enums and structures that describe LwM2M Objects, as well as
 * handler prototypes for Read, Write, Execute, Create, Delete, and
 * transaction management.
 */

#ifndef ANJ_DM_DEFS_H
#    define ANJ_DM_DEFS_H

#    include <anj/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * Resource kinds: single/multiple instance, readable/writable/executable.
 */
typedef enum {
    /**
     * Read-only Single-Instance Resource.
     *
     * @note Bootstrap Server is allowed to write to such Resources during
     *       Bootstrap procedure, even though they are marked as read-only.
     *
     * @see anj_dm_res_write_t
     */
    ANJ_DM_RES_R,

    /**
     * Read-only Multiple-Instance Resource.
     *
     * @note Bootstrap Server is allowed to write to such Resources during
     *       Bootstrap procedure, even though they are marked as read-only.
     *
     * @see anj_dm_res_write_t
     */
    ANJ_DM_RES_RM,

    /** Write-only Single-Instance Resource. */
    ANJ_DM_RES_W,

    /** Write-only Multiple-Instance Resource. */
    ANJ_DM_RES_WM,

    /** Read/Write Single-Instance Resource. */
    ANJ_DM_RES_RW,

    /** Read/Write Multiple-Instance Resource. */
    ANJ_DM_RES_RWM,

    /** Executable Resource. */
    ANJ_DM_RES_E,
} anj_dm_res_kind_t;

/**
 * Result of a transactional operation.
 */
typedef enum {
    /** The operation completed successfully. */
    ANJ_DM_TRANSACTION_SUCCESS = 0,

    /**
     * The operation failed. The object must be restored to its previous state.
     */
    ANJ_DM_TRANSACTION_FAILURE = -1
} anj_dm_transaction_result_t;

/** Struct defining a Resource. */
typedef struct anj_dm_res_struct {
    /** Resource ID. */
    anj_rid_t rid;

    /** Resource data type. */
    anj_data_type_t type;

    /** Resource kind. */
    anj_dm_res_kind_t kind;

    /**
     * Pointer to the array of Resource Instance IDs.
     *
     * The array must have a size equal to @ref max_inst_count. Unused slots
     * must be explicitly set to @ref ANJ_ID_INVALID.
     *
     * This array is never modified by the library. If handlers such as
     * @ref anj_dm_res_inst_create_t or @ref anj_dm_res_inst_delete_t are
     * defined for this Resource, the user is responsible for updating the
     * contents of this array accordingly.
     *
     * When passed to @ref anj_dm_add_obj, all valid IDs must be sorted
     * in strictly ascending order. Gaps or duplicate values are not allowed —
     * the list must contain only unique, valid IDs packed at the beginning
     * of the array.
     *
     * For Single-Instance Resources, this field is ignored.
     * For Multi-Instance Resources, this field must be a valid pointer if
     * @ref max_inst_count is greater than 0.
     */
    const anj_riid_t *insts;

    /**
     * Maximum number of instances allowed for this Resource.
     * Ignored for Single-Instance Resources.
     */
    uint16_t max_inst_count;
} anj_dm_res_t;

/** Struct defining an Object Instance. */
typedef struct anj_dm_obj_inst_struct {
    /**
     * Object Instance ID.
     *
     * If the instance is not currently active (i.e., unused slot in the
     * instance array), this field must be set to @ref ANJ_ID_INVALID.
     */
    anj_iid_t iid;

    /**
     * Pointer to the array of Resources belonging to this Object Instance.
     *
     * If the Object does not define any multi-instance Resources, this array
     * may be shared across all Object Instances.
     *
     * Resources in this array must be sorted in ascending order by their ID
     * value. The Resource list must remain constant throughout the lifetime of
     * this Object Instance — dynamic addition or removal of Resources at
     * runtime is not supported.
     *
     * If @ref res_count is not equal to 0, this field must not be @c NULL.
     */
    const anj_dm_res_t *resources;

    /**
     * Number of Resources defined for this Object Instance.
     */
    uint16_t res_count;
} anj_dm_obj_inst_t;

typedef struct anj_dm_handlers_struct anj_dm_handlers_t;

/** Struct defining an Object. */
typedef struct anj_dm_obj_struct {
    /** Object ID. */
    anj_oid_t oid;

    /**
     * Object version.
     *
     * A string with static lifetime, formatted as two digits separated by a dot
     * (e.g., "1.1"). If set to NULL, the LwM2M client will omit the "ver="
     * attribute in Register and Discover messages, which:
     * - for Core Objects, implies the Object version contained in the supported
     *   release of LwM2M Enabler (e.g. Server Object has version 1.2 in
     *   LwM2M 1.2)
     * - for Non-core Objects implies version 1.0.
     */
    const char *version;

    /**
     * Pointer to the Object handlers. Must not be @c NULL.
     */
    const anj_dm_handlers_t *handlers;

    /**
     * Pointer to the array of Object Instances. For unused slots, @ref
     * anj_dm_obj_inst_t::iid must be set to @ref ANJ_ID_INVALID.
     *
     * The array must have a size equal to @ref max_inst_count. When calling
     * @ref anj_dm_add_obj, all instances in the array must be sorted in
     * ascending order by their Instance IDs. Gaps or duplicate values are not
     * allowed — the list must contain only unique, valid IDs packed at the
     * beginning of the array.
     *
     * This array is never modified by the library itself.
     * If @ref anj_dm_inst_create_t or @ref anj_dm_inst_delete_t handlers are
     * defined, the user is responsible for updating the contents of the @p
     * insts array accordingly.
     *
     * If @ref max_inst_count is not equal to 0, this field must not be NULL.
     */
    const anj_dm_obj_inst_t *insts;

    /** Maximum number of Object Instances allowed for this Object. */
    uint16_t max_inst_count;
} anj_dm_obj_t;

/**
 * A handler that reads the value of a Resource or Resource Instance.
 *
 * This callback may be called for Resources of the following kinds:
 * - @ref ANJ_DM_RES_R,
 * - @ref ANJ_DM_RES_RW,
 * - @ref ANJ_DM_RES_RM,
 * - @ref ANJ_DM_RES_RWM.
 *
 * For values of type @ref ANJ_DATA_TYPE_BYTES, you must set both the data
 * pointer and @ref anj_bytes_or_string_value_t::chunk_length in @p out_value.
 *
 * For values of type @ref ANJ_DATA_TYPE_STRING, do not modify any additional
 * fields in @p out_value — only the data pointer should be provided.
 *
 * For values of type @ref ANJ_DATA_TYPE_EXTERNAL_BYTES and @ref
 * ANJ_DATA_TYPE_EXTERNAL_STRING, you must set @ref
 * anj_res_value_t::get_external_data callback.
 * @ref anj_res_value_t::open_external_data and @ref
 * anj_res_value_t::close_external_data callbacks are optional. If the external
 * data source needs initialization, it should be performed in the @ref
 * anj_res_value_t::open_external_data callback. This handler should do nothing
 * more than assign the relevant addresses to the pointers in the @ref
 * anj_res_value_t::external_data structure.
 *
 * @param      anj       Anjay object.
 * @param      obj       Object definition pointer.
 * @param      iid       Object Instance ID.
 * @param      rid       Resource ID.
 * @param      riid      Resource Instance ID, or @ref ANJ_ID_INVALID in case of
 *                       a Single-Instance Resource.
 * @param[out] out_value Resource value.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_res_read_t(anj_t *anj,
                              const anj_dm_obj_t *obj,
                              anj_iid_t iid,
                              anj_rid_t rid,
                              anj_riid_t riid,
                              anj_res_value_t *out_value);

/**
 * A handler that writes the value of a Resource or Resource Instance.
 *
 * This function may be called for Resources of the following kinds:
 * - @ref ANJ_DM_RES_W,
 * - @ref ANJ_DM_RES_RW,
 * - @ref ANJ_DM_RES_WM,
 * - @ref ANJ_DM_RES_RWM.
 *
 * Additionally, this handler may called for Read-only Resources during
 * the Bootstrap procedure, that is to Resources of following kinds:
 * - @ref ANJ_DM_RES_R,
 * - @ref ANJ_DM_RES_RM.
 *
 * For values of type @ref ANJ_DATA_TYPE_BYTES and @ref ANJ_DATA_TYPE_STRING, in
 * case of a Block-Wise operation, the handler may be called several times, with
 * consecutive chunks of value. See @ref anj_dm_write_bytes_chunked and @ref
 * anj_dm_write_string_chunked functions that help with handling such case.
 *
 * @warning Values of type @ref ANJ_DATA_TYPE_STRING are not null-terminated,
 *          hence instead of @c strlen(), use the @ref
 *          anj_bytes_or_string_value_t::chunk_length field (of @ref
 *          anj_res_value_t::bytes_or_string) to determine the length
 *          or just use @ref anj_dm_write_string_chunked helper.
 *
 * @param anj   Anjay object.
 * @param obj   Object definition pointer.
 * @param iid   Object Instance ID.
 * @param rid   Resource ID.
 * @param riid  Resource Instance ID, or @ref ANJ_ID_INVALID in case of a
 *              Single-Instance Resource.
 * @param value Resource value.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_res_write_t(anj_t *anj,
                               const anj_dm_obj_t *obj,
                               anj_iid_t iid,
                               anj_rid_t rid,
                               anj_riid_t riid,
                               const anj_res_value_t *value);

/**
 * A handler that performs the Execute operation on given Resource.
 *
 * This function may be called only for Resources of @ref ANJ_DM_RES_E kind.
 *
 * @param anj             Anjay object.
 * @param obj             Object definition pointer.
 * @param iid             Object Instance ID.
 * @param rid             Resource ID.
 * @param execute_arg     Payload provided in Execute request, NULL if not
 *                        present.
 * @param execute_arg_len Execute payload length.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_res_execute_t(anj_t *anj,
                                 const anj_dm_obj_t *obj,
                                 anj_iid_t iid,
                                 anj_rid_t rid,
                                 const char *execute_arg,
                                 size_t execute_arg_len);

/**
 * A handler called to create a new Resource Instance within a Multiple-Instance
 * Resource.
 *
 * This function is called when a LwM2M Write operation requires creating a new
 * Resource Instance. The handler is responsible for initializing the new
 * instance with the specified @p riid and inserting its ID into the @ref
 * anj_dm_res_t::insts array. The array must remain sorted in ascending order of
 * Resource Instance IDs.
 *
 * @warning If the operation fails at a later stage (i.e., @ref
 *          anj_dm_transaction_end_t is called with a failure result), the user
 *          is responsible for removing the newly added instance and restoring
 *          the array to its previous state.
 *
 * @param anj  Anjay object.
 * @param obj  Pointer to the object definition.
 * @param iid  Object Instance ID.
 * @param rid  Resource ID.
 * @param riid Resource Instance ID of newly created Resource Instance.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_res_inst_create_t(anj_t *anj,
                                     const anj_dm_obj_t *obj,
                                     anj_iid_t iid,
                                     anj_rid_t rid,
                                     anj_riid_t riid);

/**
 * A handler called to delete a Resource Instance of a Multiple-Instance
 * Resource.
 *
 * The handler is responsible for removing the Resource Instance with the
 * specified @p riid from the @ref anj_dm_res_t::insts array. After the removal,
 * the array must remain in ascending order, and all unused slots must be set to
 * @ref ANJ_ID_INVALID.
 *
 * @warning If the operation fails at a later stage (i.e., @ref
 *          anj_dm_transaction_end_t is called with a failure result), the user
 *          is responsible for removing the newly added instance and restoring
 *          the array to its previous state.
 *
 * @param anj  Anjay object.
 * @param obj  Pointer to the object definition.
 * @param iid  Object Instance ID.
 * @param rid  Resource ID.
 * @param riid Resource Instance ID to be deleted.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_res_inst_delete_t(anj_t *anj,
                                     const anj_dm_obj_t *obj,
                                     anj_iid_t iid,
                                     anj_rid_t rid,
                                     anj_riid_t riid);

/**
 * A handler that creates a new Object Instance.
 *
 * This function is called when a LwM2M Create operation is requested for the
 * given Object. The handler is responsible for allocating and initializing a
 * new Object Instance with the specified @p iid and inserting it into the @ref
 * anj_dm_obj_t::insts array. The array must remain sorted in ascending order of
 * Object Instance IDs.
 *
 * @warning If the operation fails at a later stage (i.e., @ref
 *          anj_dm_transaction_end_t is called with a failure result), the user
 *          is responsible for removing the newly added instance and restoring
 *          the array to its previous state.
 *
 * @param anj Anjay object.
 * @param obj Object definition pointer.
 * @param iid New object Instance ID.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int
anj_dm_inst_create_t(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid);

/**
 * A handler that deletes an Object Instance.
 *
 * This function is called when a LwM2M Delete operation is requested for the
 * given Object. The handler is responsible for removing the Object Instance
 * with the specified @p iid from the @ref anj_dm_obj_t::insts array. After the
 * removal, the array must remain in ascending order, and all unused slots must
 * be set to @ref ANJ_ID_INVALID.
 *
 * If the operation fails later (i.e., @ref anj_dm_transaction_end_t is called
 * with a failure result), the user is responsible for restoring the deleted
 * instance and reinserting it into the Instances array at the correct position.
 *
 * @param anj Anjay object.
 * @param obj Pointer to the object definition.
 * @param iid Object Instance ID to be deleted.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int
anj_dm_inst_delete_t(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid);

/**
 * A handler that resets an Object Instance to its default (post-creation)
 * state.
 *
 * This handler is used during the LwM2M Write Replace operation. It should
 * remove all writable Resource Instances belonging to the specified Object
 * Instance. After the reset, new Resource values will be provided by subsequent
 * write calls.
 *
 * @param anj Anjay object.
 * @param obj Pointer to the object definition.
 * @param iid Object Instance ID to reset.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int
anj_dm_inst_reset_t(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid);

/**
 * A handler called at the beginning of a transactional operation that may
 * modify the Object.
 *
 * This function is invoked when the LwM2M server sends a request involving this
 * Object that may alter its state — specifically for Create, Write, or Delete
 * operations.
 *
 * User is expected to back up the current state of the Object in callback, so
 * that in case of a failure later in the transaction
 * (i.e., @ref anj_dm_transaction_end_t is called with a failure result), they
 * can restore the Object to its previous state.
 *
 * @param anj Anjay object.
 * @param obj Object definition pointer.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_transaction_begin_t(anj_t *anj, const anj_dm_obj_t *obj);

/**
 * A handler called after a transaction is finished, but before it is finalized.
 *
 * This function is used to validate whether the operation can be safely
 * completed. It is invoked for transactional operations that may modify the
 * Object (i.e., Create, Write, and Delete) after all handler calls have
 * completed, but before @ref anj_dm_transaction_end_t is called.
 *
 * @warning It is not guaranteed that this function will be called at some point
 *          after every call to @ref anj_dm_transaction_begin_t. If an error
 *          occurs during the validation of other objects involed in the same
 *          operation (e.g. during Write-Composite), this function may never be
 *          called.
 *
 * @param anj   Anjay object.
 * @param obj   Object definition pointer.
 *
 * @return This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the
 *   @ref anj_dm_errors "ANJ_DM_ERR_* constants", an appropriate CoAP error code
 *   will be used in the response. Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_transaction_validate_t(anj_t *anj, const anj_dm_obj_t *obj);

/**
 * A handler called at the end of a transactional operation.
 *
 * This function is invoked after handling a Create, Write, or Delete request
 * from the LwM2M server. If @p result is @ref ANJ_DM_TRANSACTION_FAILURE, the
 * user is responsible for restoring the Object to its previous state.
 *
 * @param anj     Anjay object.
 * @param obj     Object definition pointer.
 * @param result  Result of the operation.
 */
typedef void anj_dm_transaction_end_t(anj_t *anj,
                                      const anj_dm_obj_t *obj,
                                      anj_dm_transaction_result_t result);

/**
 * A struct containing function pointers to Object handlers.
 */
struct anj_dm_handlers_struct {
    /**
     * Creates an Object Instance.
     *
     * Required to support the LwM2M Create operation.
     */
    anj_dm_inst_create_t *inst_create;

    /**
     * Deletes an Object Instance.
     *
     * Required to support the LwM2M Delete operation targeting an Object
     * Instance.
     */
    anj_dm_inst_delete_t *inst_delete;

    /**
     * Resets an Object Instance to its default state.
     *
     * Required to support the LwM2M Write operation in Replace mode.
     * If not defined, Replace mode will fail.
     */
    anj_dm_inst_reset_t *inst_reset;

    /**
     * Called before any transactional LwM2M operation (Create, Write, Delete)
     * involving this Object.
     */
    anj_dm_transaction_begin_t *transaction_begin;

    /**
     * Called after a transactional operation that modifies this Object.
     *
     * The return value determines whether the Object's state is valid and the
     * operation can be completed.
     */
    anj_dm_transaction_validate_t *transaction_validate;

    /**
     * Called after any transactional operation is completed.
     *
     * Provides information about the result of the operation.
     */
    anj_dm_transaction_end_t *transaction_end;

    /**
     * Reads a Resource value.
     *
     * Required to support the LwM2M Read operation.
     */
    anj_dm_res_read_t *res_read;

    /**
     * Writes a Resource value.
     *
     * Required to support the LwM2M Write operation.
     */
    anj_dm_res_write_t *res_write;

    /**
     * Executes a Resource.
     *
     * Required to support the LwM2M Execute operation.
     */
    anj_dm_res_execute_t *res_execute;

    /**
     * Creates a Resource Instance in a Multiple-Instance Resource.
     *
     * Required to support the LwM2M Write operation when new instances must be
     * created.
     */
    anj_dm_res_inst_create_t *res_inst_create;

    /**
     * Deletes a Resource Instance from a Multiple-Instance Resource.
     *
     * Required to support:
     * - LwM2M Delete operations targeting individual Resource Instances,
     * - LwM2M Write Replace operations that remove all instances.
     */
    anj_dm_res_inst_delete_t *res_inst_delete;
};

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DM_DEFS_H
