/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_DM_DEFS_H
#    define ANJ_DM_DEFS_H

#    include <anj/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * Resource operation types.
 */
typedef enum {
    /**
     * Read-only Single-Instance Resource. Bootstrap Server might attempt to
     * write to it anyway.
     */
    ANJ_DM_RES_R,
    /**
     * Read-only Multiple Instance Resource. Bootstrap Server might attempt to
     * write to it anyway.
     */
    ANJ_DM_RES_RM,
    /** Write-only Single-Instance Resource. */
    ANJ_DM_RES_W,
    /** Write-only Multiple Instance Resource. */
    ANJ_DM_RES_WM,
    /** Read/Write Single-Instance Resource. */
    ANJ_DM_RES_RW,
    /** Read/Write Multiple Instance Resource. */
    ANJ_DM_RES_RWM,
    /** Executable Resource. */
    ANJ_DM_RES_E,
} anj_dm_res_operation_t;

/** Main Resource struct. */
typedef struct anj_dm_res_struct {
    /** Resource ID. */
    anj_rid_t rid;
    /** Resource data type as defined in Appendix C of the LwM2M spec. */
    anj_data_type_t type;
    /** Operation supported by this Resource. */
    anj_dm_res_operation_t operation;
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
     * For single-instance Resources, this field must be set to NULL.
     * For multi-instance Resources, this field must be a valid pointer if
     * @ref max_inst_count is greater than 0.
     */
    const anj_riid_t *insts;
    /**
     * Maximum number of instances allowed for this Resource.
     * Ignored for single-instance Resources.
     */
    uint16_t max_inst_count;
} anj_dm_res_t;

/** A struct defining an Object Instance. */
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
     * value. The resource list must remain constant throughout the lifetime of
     * this Object Instance — dynamic addition or removal of Resources at
     * runtime is not supported.
     *
     * If @ref res_count is not equal to 0, this field must not be NULL.
     */
    const anj_dm_res_t *resources;
    /**
     * Number of Resources defined for this Object Instance.
     */
    uint16_t res_count;
} anj_dm_obj_inst_t;

typedef struct anj_dm_handlers_struct anj_dm_handlers_t;
/** A struct defining an Object. */
typedef struct anj_dm_obj_struct {
    /** Object ID. */
    anj_oid_t oid;
    /**
     * Object version.
     *
     * A string with static lifetime, formatted as two digits separated by a dot
     * (e.g., "1.1"). If set to NULL, the LwM2M client will omit the "ver="
     * attribute in Register and Discover messages, which implies version 1.0.
     */
    const char *version;
    /**
     * Pointer to the object handlers. Must not be NULL.
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
 * This function is called only for Resources of the following types:
 * @ref ANJ_DM_RES_R, @ref ANJ_DM_RES_RW, @ref ANJ_DM_RES_RM, or @ref
 * ANJ_DM_RES_RWM.
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
 * @param      anj        Anjay object to operate on.
 * @param      obj        Object definition pointer.
 * @param      iid        Object Instance ID.
 * @param      rid        Resource ID.
 * @param      riid       Resource Instance ID, or @ref ANJ_ID_INVALID in case
 *                        of a Single Resource.
 * @param[out] out_value  Returned Resource value.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_res_read_t(anj_t *anj,
                              const anj_dm_obj_t *obj,
                              anj_iid_t iid,
                              anj_rid_t rid,
                              anj_riid_t riid,
                              anj_res_value_t *out_value);

/**
 * A handler that writes the Resource or Resource Instance value, called only if
 * the Resource or Resource Instance is PRESENT and is one of the @ref
 * ANJ_DM_RES_W, @ref ANJ_DM_RES_RW, @ref ANJ_DM_RES_WM, @ref
 * ANJ_DM_RES_RWM.
 *
 * For values of type @ref ANJ_DATA_TYPE_BYTES and @ref
 * ANJ_DATA_TYPE_STRING, in case of the block operation, handler can be
 * called several times, with consecutive chunks of value - offset value in
 * @ref anj_bytes_or_string_value_t will be changing.
 *
 * IMPORTANT: For value of type @ref ANJ_DATA_TYPE_STRING always use
 * <c>chunk_length</c> to determine the length of the string, never use the
 * <c>strlen()</c> function - pointer to string data points directly to CoAP
 * message payload.
 *
 * @param anj      Anjay object to operate on.
 * @param obj      Object definition pointer.
 * @param iid      Object Instance ID.
 * @param rid      Resource ID.
 * @param riid     Resource Instance ID, or @ref ANJ_ID_INVALID in case
 *                 of a Single Resource.
 * @param value    Resource value.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_res_write_t(anj_t *anj,
                               const anj_dm_obj_t *obj,
                               anj_iid_t iid,
                               anj_rid_t rid,
                               anj_riid_t riid,
                               const anj_res_value_t *value);

/**
 * A handler that performs the Execute action on given Resource, called only if
 * the Resource is @ref ANJ_DM_RES_E kind.
 *
 * @param anj             Anjay object to operate on.
 * @param obj             Object definition pointer.
 * @param iid             Object Instance ID.
 * @param rid             Resource ID.
 * @param execute_arg     Payload provided in Execute request, NULL if not
 *                        present.
 * @param execute_arg_len Execute payload length.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_res_execute_t(anj_t *anj,
                                 const anj_dm_obj_t *obj,
                                 anj_iid_t iid,
                                 anj_rid_t rid,
                                 const char *execute_arg,
                                 size_t execute_arg_len);

/**
 * A handler called to create a new Resource Instance within a multi-instance
 * Resource.
 *
 * This function is called when a LwM2M Write operation requires creating a new
 * Resource Instance. The handler is responsible for initializing the new
 * instance with the specified @p riid and inserting its ID into the @p
 * res->insts array. The array must remain sorted in ascending order of Resource
 * Instance IDs.
 *
 * If the operation fails later (i.e., @ref anj_dm_transaction_end_t is called
 * with a failure result), the user is responsible for removing the newly added
 * instance and restoring the array to its previous state.
 *
 * @param anj   Anjay object to operate on.
 * @param obj   Pointer to the object definition.
 * @param iid   Object Instance ID.
 * @param rid   Resource ID.
 * @param riid  New Resource Instance ID.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_res_inst_create_t(anj_t *anj,
                                     const anj_dm_obj_t *obj,
                                     anj_iid_t iid,
                                     anj_rid_t rid,
                                     anj_riid_t riid);

/**
 * A handler called to delete a Resource Instance from a multi-instance
 * Resource.
 *
 * The handler is responsible for removing the Resource Instance with the
 * specified @p riid from the @p res->insts array and marking its ID as
 * @ref ANJ_ID_INVALID. The array must remain sorted in ascending order of
 * Resource Instance IDs after the removal.
 *
 * If the deletion succeeds but the overall transaction is later marked as
 * failed by @ref anj_dm_transaction_end_t, the user is responsible for
 * restoring the removed Resource Instance to its previous state.
 *
 * @param anj   Anjay object to operate on.
 * @param obj   Pointer to the object definition.
 * @param iid   Object Instance ID.
 * @param rid   Resource ID.
 * @param riid  Resource Instance ID to be deleted.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
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
 * new Object Instance with the specified @p iid and inserting it into the @p
 * obj->insts array. The array must remain sorted in ascending order of instance
 * IDs after insertion.
 *
 * If the operation fails later (i.e., @ref anj_dm_transaction_end_t is called
 * with a failure result), the user is responsible for restoring the previous
 * state of the Instances array by removing the newly added instance.
 *
 * @param anj   Anjay object to operate on.
 * @param obj   Object definition pointer.
 * @param iid   New object Instance ID.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int
anj_dm_inst_create_t(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid);

/**
 * A handler that deletes an Object Instance.
 *
 * This function is called when a LwM2M Delete operation is requested for the
 * given Object. The handler is responsible for removing the Object Instance
 * with the specified @p iid from the @p obj->insts array. The array must remain
 * sorted in ascending order of instance IDs after the removal.
 *
 * If the operation fails later (i.e., @ref anj_dm_transaction_end_t is called
 * with a failure result), the user is responsible for restoring the deleted
 * instance and reinserting it into the Instances array at the correct position.
 *
 * @param anj   Anjay object to operate on.
 * @param obj   Pointer to the object definition.
 * @param iid   Object Instance ID to be deleted.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
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
 * @param anj   Anjay object to operate on.
 * @param obj   Pointer to the object definition.
 * @param iid   Object Instance ID to reset.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
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
 * operations. It marks the beginning of a transaction.
 *
 * @param anj        Anjay object to operate on.
 * @param obj        Object definition pointer.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
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
 * @param anj   Anjay object to operate on.
 * @param obj   Object definition pointer.
 *
 * @returns This handler should return:
 * - 0 on success,
 * - a negative value on error. If the error matches one of the @p ANJ_DM_ERR_*
 *   constants, an appropriate CoAP error code will be used in the response.
 *   Otherwise, the device will respond with @ref
 *   ANJ_COAP_CODE_INTERNAL_SERVER_ERROR.
 */
typedef int anj_dm_transaction_validate_t(anj_t *anj, const anj_dm_obj_t *obj);

/**
 * A handler called at the end of a transactional operation.
 *
 * This function is invoked after handling a Create, Write, or Delete request
 * from the LwM2M server. If @p result is non-zero value, the user is
 * responsible for restoring the Object to its previous state.
 *
 * @param anj     Anjay object to operate on.
 * @param obj     Object definition pointer.
 * @param result  Result of the operation. Non-zero value indicates failure.
 *                In case of failure, this value will either be:
 *                - One of the @p ANJ_DM_ERR_* constants,
 *                - Or a custom error code returned by the user from one of the
 *                  callbacks (e.g. @ref anj_dm_res_write_t).
 */
typedef void
anj_dm_transaction_end_t(anj_t *anj, const anj_dm_obj_t *obj, int result);

/**
 * A struct containing function pointers to Object-level and Resource-level
 * operation handlers used by the DM (Data Model) interface.
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
     * Required to support the LwM2M Delete operation.
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
     * The return value determines whether the Object's state is valid and
     * whether the operation can be completed.
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
     * Creates a Resource Instance in a multi-instance Resource.
     *
     * Required to support the LwM2M Write operation when new instances
     * must be created. If not defined, and creation is required,
     * an error will be returned to the LwM2M server.
     */
    anj_dm_res_inst_create_t *res_inst_create;

    /**
     * Deletes a Resource Instance from a multi-instance Resource.
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
