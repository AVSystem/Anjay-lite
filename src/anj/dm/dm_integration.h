/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_DM_IMPL_H
#    define ANJ_DM_IMPL_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/core.h>
#    include <anj/defs.h>

/**
 * Processes all LwM2M Server requests related to the data model, call it after
 * @ref anj_coap_decode_udp call. This function is
 * compliant with the anj_exchange API. Supported data model operations are:
 *      - ANJ_OP_DM_READ,
 *      - ANJ_OP_DM_READ_COMP,
 *      - ANJ_OP_DM_DISCOVER,
 *      - ANJ_OP_DM_WRITE_REPLACE,
 *      - ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
 *      - ANJ_OP_DM_EXECUTE,
 *      - ANJ_OP_DM_CREATE,
 *      - ANJ_OP_DM_DELETE.
 *
 * @param anj                      Anjay object to operate on.
 * @param request                  LwM2M Server/Client request.
 * @param ssid                     SSID, of the server that sent the request.
 *                                 For bootstrap server should be set to
 *                                 @ref _ANJ_SSID_BOOTSTRAP.
 * @param[out] out_response_code   Response code to be sent to the server.
 * @param[out] out_handlers        Handlers compliant with anj_exchange API
 *                                 specifications.
 */
void _anj_dm_process_request(anj_t *anj,
                             const _anj_coap_msg_t *request,
                             uint16_t ssid,
                             uint8_t *out_response_code,
                             _anj_exchange_handlers_t *out_handlers);

/**
 * Prepares the Register or Update operations message payload. This function is
 * compliant with the anj_exchange API.
 *
 * @param anj          Anjay object to operate on.
 * @param out_handlers Handlers compliant with anj_exchange API specifications.
 */
void _anj_dm_process_register_update_payload(
        anj_t *anj, _anj_exchange_handlers_t *out_handlers);

/**
 * Checks if there is at least one readable Resource under the @p path.
 *
 * NOTE: If the observe-composite operation is enabled then this function
 * returns ANJ_COAP_CODE_NOT_FOUND in the case where @p path is not found in the
 * data model.
 *
 * @param anj   Anjay object to operate on.
 * @param path  Pointer to a variable with path to check.
 *
 * @returns
 *  - 0 on success,
 *  - a ANJ_COAP_CODE_* code in case of error, especially:
 *    - ANJ_COAP_CODE_NOT_FOUND if @p path of operation not found in data
 *      model,
 *    - ANJ_COAP_CODE_METHOD_NOT_ALLOWED if there is no readable resources for
 *      given @p path
 */
int _anj_dm_observe_is_any_resource_readable(anj_t *anj,
                                             const anj_uri_path_t *path);

/**
 * Function called by observe module to read a Resource value and type from the
 * data model. If @p out_value is NULL, only the type of the Resource is
 * returned. If @p out_type is NULL, this function returns only the value of the
 * Resource. This function should be called for a value only for numerical
 * types. If @p out_value is not NULL and @p res_path points to a Resource and
 * Resource is Multi-Instance, the success is returned and @p out_multi_res is
 * set to true.
 *
 * @param      anj           Anjay object to operate on.
 * @param[out] out_value     Pointer to a variable to write a read resource.
 * @param[out] out_type      Pointer to a variable to write resulted type.
 * @param[out] out_multi_res Pointer to a variable to set flag if the Resource
 *                           is Multi-Instance.
 * @param      res_path      Pointer to a @ref anj_uri_path_t object with path
 *                           to read. This argument will always point to a
 *                           Resource or Resource Instance.
 *
 * @returns
 *  - 0 on success,
 *  - a ANJ_COAP_CODE_* code in case of error.
 */
int _anj_dm_observe_read_resource(anj_t *anj,
                                  anj_res_value_t *out_value,
                                  anj_data_type_t *out_type,
                                  bool *out_multi_res,
                                  const anj_uri_path_t *res_path);

/**
 * A function called by observe module to build the Observe response,
 * Cancel-Observe response or the Notify operation. CoAP header is prepared by
 * the observe module. This function is responsible only for payload
 * preparation.
 *
 * If provided payload buffer is too short to fit all data, the @ref
 * _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED code is returned. The subsequent call is
 * needed to prepare the next part of the payload.
 *
 * When building a payload for a composite observation the @p already_processed
 * argument is incremented after each processed path. If this function is
 * called again due to a block-wise transfer, the user should use @p
 * already_processed argument value to determine which path it should now
 * process.
 *
 * For composite observation @p paths contains only paths that have been found
 * in the data model. This means that not necessarily all stored paths
 * associated with particular composite observation will be passed. If there are
 * no existing paths then @p path_count is set to 0, such a situation is
 * completely legal for composite observation.
 *
 * @param        anj                Anjay object to operate on.
 * @param        paths              Paths to build the payload. For
 *                                  non-composite observations there will always
 *                                  be one path.
 * @param        path_count         Path count. Ignored for non-composite
 *                                  observations.
 * @param[inout] already_processed  Indicates how many paths have already been
 *                                  processed. Ignored for non-composite
 *                                  observations.
 * @param[out]   out_buff           Payload buffer.
 * @param[out]   out_len            Payload size.
 * @param        buff_len           Length of the payload buffer.
 * @param[inout] inout_format       Required payload format. If set to @ref
 *                                  _ANJ_COAP_FORMAT_NOT_DEFINED function
 *                                  returns information about the format used.
 * @param        composite          Indicates whether there is a composite
 *                                  observation.
 *
 * @returns
 *  - 0 on success when all data was written to @p out_buff,
 *  - @ref _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED if there is more data to write to
 *    @p out_buff and the block transfer is needed,
 *  - a ANJ_COAP_CODE_* code in case of error.
 */
int _anj_dm_observe_build_msg(anj_t *anj,
                              const anj_uri_path_t *const *paths,
                              const size_t path_count,
                              size_t *already_processed,
                              uint8_t *out_buff,
                              size_t *out_len,
                              size_t buff_len,
                              uint16_t *inout_format,
                              bool composite);

/**
 * Informs the data model that operation has been ended. Data model
 * should end the operation on its side.
 *
 * @param anj          Anjay object to operate on.
 * @param result       Result of the operation.
 */
void _anj_dm_observe_finalize_operation(anj_t *anj, int result);

/**
 * Called by the bootstrap API during Bootstrap-Finish handling, checks if there
 * is at least one instance of the Server object and one non-bootstrap instance
 * of the Security object in the data model.
 *
 * IMPORTANT: The instance contents are checked during the Write/Create
 * operations.
 *
 * @param anj Anjay object to operate on.
 *
 * @returns 0 if the data model is valid, a non-zero value otherwise.
 */
int _anj_dm_bootstrap_validation(anj_t *anj);

/**
 * Finds existing Server Object Instance and returns its SSID and IID. @p
 * out_ssid can be used in @ref _anj_dm_get_security_obj_instance_iid to find
 * the corresponding Security Object Instance.
 *
 * If the Server Object Instance is not found, @p out_ssid and @p out_iid are
 * set to @ref ANJ_ID_INVALID and function returns success.
 *
 * IMPORTANT: Anjay Lite supports only one Server Object Instance.
 *
 * @param      anj      Anjay object to operate on.
 * @param[out] out_ssid SSID of the Server Object Instance.
 * @param[out] out_iid  ID of the Server Object Instance.
 *
 * @returns 0 on success, a non-zero value otherwise.
 */
int _anj_dm_get_server_obj_instance_data(anj_t *anj,
                                         uint16_t *out_ssid,
                                         anj_iid_t *out_iid);

/**
 * Finds existing Security Object Instance and returns its IID. Set @p ssid to
 * @ref _ANJ_SSID_BOOTSTRAP to get Security Object Instance related to the
 * Bootstrap Server.
 *
 * @param      anj      Anjay object to operate on.
 * @param      ssid     SSID of the Security Object Instance.
 * @param[out] out_iid  ID of the Security Object Instance.
 *
 * @returns 0 on success, a non-zero value otherwise.
 */
int _anj_dm_get_security_obj_instance_iid(anj_t *anj,
                                          uint16_t ssid,
                                          anj_iid_t *out_iid);

#endif // ANJ_DM_IMPL_H
