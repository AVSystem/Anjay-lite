/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_OBSERVE_H
#    define SRC_ANJ_OBSERVE_H

#    include <stdbool.h>
#    include <stdint.h>

#    include <anj/core.h>
#    include <anj/defs.h>

#    ifdef ANJ_WITH_OBSERVE

/** A constant that may be used to address all servers. */
#        define ANJ_OBSERVE_ANY_SERVER UINT16_MAX

/**
 * Contains information about the type of changes of the data model.
 * Used in @ref anj_observe_data_model_changed function.
 */
typedef enum {
    /** Resource or Resource Instance value changed. */
    ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED,
    /** Object, Object Instance or Resource Instance added. */
    ANJ_OBSERVE_CHANGE_TYPE_ADDED,
    /** Object, Object Instance or Resource Instance deleted. */
    ANJ_OBSERVE_CHANGE_TYPE_DELETED
} anj_observe_change_type_t;

/**
 * Initializes the observe module. Should be called once before any other
 * observe module function.
 *
 * NOTE: It is user responsibility to allocate the context.
 *
 * Example use anj_observe API (error checking omitted for brevity).
 * Below code show event request handling scenario:
 * @code
 * anj_t anj;
 * // Using of this API is strongly associated with exchange module.
 * _anj_exchange_ctx_t exchange_ctx;
 * _anj_exchange_handlers_t out_handlers;
 * _anj_exchange_init(&exchange_ctx, 0);
 * // Below handlers are provided by the user, refer to handlers documentation
 * // for more information.
 * _anj_observe_init(&anj);
 *
 * _anj_coap_msg_t inout_msg = {0};
 * // Example assumes that in place of this comment is a loop that receives
 * // LwM2M messages.
 * // The message then is received and decoded into _anj_coap_msg_t structure.
 *
 * _anj_observe_server_state_t srv = {
 *     .ssid = 1
 * };
 * uint8_t response_code;
 * char out_buff[100];
 * size_t out_buff_len = sizeof(out_buff);
 *
 * _anj_observe_new_request(&anj, &out_handlers, &srv,
 *                                  &inout_msg, &response_code);
 * _anj_exchange_new_server_request(&exchange_ctx, response_code, &inout_msg,
 *                             &out_handlers, out_buff, out_buff_len);
 * while(1) {
 *   // Do send a response
 *   // and call _anj_exchange_process again to inform
 *   // that message has been sent.
 *   if (_anj_exchange_process(&exchange_ctx, ANJ_EXCHANGE_EVENT_SEND_ACK,
 *       &inout_msg) == ANJ_EXCHANGE_STATE_FINISHED) {
 *     break;
 *   }
 *
 *   // Call _anj_exchange_process after receiving next message
 *   _anj_exchange_process(&exchange_ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
 *     &inout_msg);
 * }
 *
 * @endcode
 *
 * Below code show notification handling process scenario:
 * The code below has to be called periodically, with frequency not less than
 * 1Hz:
 * @code
 * _anj_coap_msg_t inout_msg = {0};
 * char out_buff[100];
 * size_t out_buff_len = sizeof(out_buff);
 * _anj_observe_server_state_t srv = {
 *     .ssid = 1,
 *     .is_server_online = true,
 * };
 *
 * _anj_observe_process(&anj, &out_handlers, &srv, &inout_msg);
 * if (inout_msg->operation == ANJ_OP_INF_CON_NOTIFY || inout_msg->operation ==
 *       ANJ_OP_INF_NON_CON_NOTIFY) {
 *   _anj_exchange_new_server_request(&exchange_ctx, response_code, &inout_msg,
 *                             &out_handlers, out_buff, out_buff_len);
 *   while(1) {
 *     // Do send a block of the payload
 *     // and call _anj_exchange_process again to inform
 *     // that message has been sent.
 *     if (_anj_exchange_process(&exchange_ctx, ANJ_EXCHANGE_EVENT_SEND_ACK,
 *         &inout_msg) == ANJ_EXCHANGE_STATE_FINISHED) {
 *       break;
 *     }
 *
 *     // Call _anj_exchange_process after receiving next message
 *     _anj_exchange_process(&exchange_ctx, ANJ_EXCHANGE_EVENT_NEW_MSG,
 *       &inout_msg);
 *   }
 * }
 * @endcode
 *
 * @param anj  Anjay object to operate on.
 */
void _anj_observe_init(anj_t *anj);

/**
 * This function should be called after receiving a request from the LwM2M
 * server related to information reporting interface:
 *      - ANJ_OP_DM_WRITE_ATTR,
 *      - ANJ_OP_INF_OBSERVE,
 *      - ANJ_OP_INF_OBSERVE_COMP,
 *      - ANJ_OP_INF_CANCEL_OBSERVE,
 *      - ANJ_OP_INF_CANCEL_OBSERVE_COMP.
 *
 * After calling this function, the user should use exchange API to prepare the
 * response and handle potential block transfer. Callbacks that should be used
 * when using the exchange API are set by the @p out_handlers argument.
 *
 * Function should be called independently for each LwM2M Server instance. The
 * operations for each server can be handled simultaneously. However, if a new
 * request begins to be processed then user cannot call this or @ref
 * _anj_observe_process function until @ref _anj_exchange_process returns
 * ANJ_EXCHANGE_STATE_FINISHED.
 *
 * NOTE: @ref _anj_observe_process and @ref _anj_observe_new_request cannot be
 * call if ongoing block transfer is preformed.
 *
 * NOTE: In current version composite observations do NOT support root path.
 *
 * @param       anj                 Anjay object to operate on.
 * @param[out]  out_handlers        Exchange handlers.
 * @param[in]   server_state        Information about server state; @ref
 *                                  _anj_observe_server_state_t.
 * @param[in]   request             Incoming message to be processed.
 * @param       out_response_code   Response code to be sent to the server.
 *
 * @returns 0 on success, a ANJ_COAP_CODE_* value in case of error.
 */
int _anj_observe_new_request(anj_t *anj,
                             _anj_exchange_handlers_t *out_handlers,
                             const _anj_observe_server_state_t *server_state,
                             const _anj_coap_msg_t *request,
                             uint8_t *out_response_code);

/**
 * This function checks if any notification should be sent. Function should be
 * called periodically in order to properly handle the notifications. If
 * <c>operation</c> field in @p out_msg is set to @ref ANJ_OP_INF_CON_NOTIFY,
 * then Confirmable Notification needs to be sent. For @ref
 * ANJ_OP_INF_NON_CON_NOTIFY value Non-Confirmable Notification need to be
 * sent. Otherwise, no message is ready to be sent.
 *
 * If <c>operation</c> field in @p out_msg indicates that there is a
 * notification to send then the user should use exchange API to prepare the
 * response and handle potential block transfer. Callbacks that should be used
 * when using the exchange API are set by the @p out_handlers argument.
 *
 * Function should be called independently for each LwM2M Server instance. The
 * notifications for each server can be handled simultaneously. However, if a
 * new notification begins to be processed then user cannot call this or @ref
 * _anj_observe_process function until @ref _anj_exchange_process returns
 * ANJ_EXCHANGE_STATE_FINISHED.
 *
 * NOTE: All parameters related to time (pmin, pmax, epmin, epmax) are specified
 * in seconds. For optimal performance, notification processing, carried out by
 * calling this function should be performed with a frequency of at least 1 Hz.
 *
 * NOTE: @ref _anj_observe_process and @ref _anj_observe_new_request cannot be
 * call if ongoing block transfer is preformed.
 *
 * @param      anj               Anjay object to operate on.
 * @param[out] out_handlers      Exchange handlers.
 * @param[in]  server_state      Information about server state; @ref
 *                               _anj_observe_server_state_t.
 * @param[out] out_msg           Outgoing message structure.
 *
 * @returns 0 on success, a ANJ_COAP_CODE_* value in case of error.
 */
int _anj_observe_process(anj_t *anj,
                         _anj_exchange_handlers_t *out_handlers,
                         const _anj_observe_server_state_t *server_state,
                         _anj_coap_msg_t *out_msg);

/**
 * Removes all observations for given server. Should be called when the
 * connection to a specific LwM2M server is finished.
 *
 * @param     anj    Anjay object to operate on.
 * @param[in] ssid   ID of the server for which all observations should be
 *                   removed. Use @ref ANJ_OBSERVE_ANY_SERVER to remove all
 *                   observations.
 */
void _anj_observe_remove_all_observations(anj_t *anj, uint16_t ssid);

/**
 * Removes all attribute storage records for given server. Might be called when
 * the connection to a specific LwM2M server is finished. Specification allows
 * the preservation of attributes between connection sessions.
 *
 * @param     anj    Anjay object to operate on.
 * @param[in] ssid   ID of the server for which all attributes should be
 *                   removed. Use @ref ANJ_OBSERVE_ANY_SERVER to remove all
 *                   records.
 */
void _anj_observe_remove_all_attr_storage(anj_t *anj, uint16_t ssid);

#        ifdef ANJ_WITH_DISCOVER_ATTR
/**
 * Retrieves the attribute storage record for the given server and path. This
 * function should be used for Discover operation processing.
 *
 * @param anj               Anjay object to operate on.
 * @param ssid              ID of the server for which the attribute storage
 *                          record should be retrieved.
 * @param with_parents_attr For the path from request @p out_attr should also
 *                          contain parents' attributes.
 * @param path              Pointer to the path for which the attribute storage
 *                          record should be retrieved.
 * @param[out] out_attr     Attribute storage record, will be set in case of
 *                          success. Must be not NULL.
 *
 * @returns 0 on success, @ref ANJ_COAP_CODE_NOT_FOUND if attribute storage is
 * not found.
 */
int _anj_observe_get_attr_storage(anj_t *anj,
                                  uint16_t ssid,
                                  bool with_parents_attr,
                                  const anj_uri_path_t *path,
                                  _anj_attr_notification_t *out_attr);
#        endif // ANJ_WITH_DISCOVER_ATTR

/**
 * The @p time_to_next_notification specifies the time after which the next
 * notification is likely to be returned from the @ref _anj_observe_process
 * function and will be ready be send. Despite this parameter, the @ref
 * _anj_observe_process should still be called periodically, as the @ref
 * anj_observe_data_model_changed function may be called in the meantime, or new
 * observations/attributes may be added/modified/deleted, which could cause the
 * previously obtained time value to become obsolete. The time returned by this
 * function may be useful in queue mode to determine how long the device can be
 * put to sleep.
 *
 * @param      anj                       Anjay object to operate on.
 * @param[in]  server_state              Information about server state; @ref
 *                                       _anj_observe_server_state_t.
 * @param[out] time_to_next_notification The time after which a notification
 *                                       will be ready to be sent in
 *                                       miliseconds.
 *
 * @returns 0 on success, a ANJ_COAP_CODE_* value in case of error.
 */
int anj_observe_time_to_next_notification(
        anj_t *anj,
        const _anj_observe_server_state_t *server_state,
        uint64_t *time_to_next_notification);

/**
 * Notifies the observe module that data model changed. Depending on the
 * @p change_type there are three possible scenarios:
 *  - @ref ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED - Resource or Resource Instance
 *    value changed. It may trigger preparation of a LwM2M Notify message for
 *    all observations attached above changed resource or resource instance.
 *  - @ref ANJ_OBSERVE_CHANGE_TYPE_ADDED - Object, Object Instance or Resource
 *    Instance has been added. It may trigger preparation of a LwM2M Notify
 *    message for all observations attached above (and under in case of
 *    composite observations) added Instance.
 *  - @ref ANJ_OBSERVE_CHANGE_TYPE_DELETED - Object Instance or
 *    Resource Instance has been deleted. The consequence will be the removal of
 *    all observations and attributes associated with this Instance.
 *
 * For correct handling of notifications, this function must be called for any
 * Resource or Resource Instance after changing its value, and each time an
 * Instance is added/removed.
 *
 * The SSID should be provided if the change was caused by the
 * server. Otherwise, the SSID should be set to 0.
 * When a non-zero value is provided, a resource change will not trigger a
 * notification for observations associated with the server with such SSID.
 *
 * @param     anj          Anjay object to operate on.t.
 * @param[in] path         Pointer to the path of the Resource that changed, or
 *                         the Instance that was added/removed.
 * @param     change_type  Type of change; @ref anj_observe_change_type_t.
 * @param     ssid         SSID of the server that caused the change. If the
 *                         change is not associated with any server, it should
 *                         be set to 0.
 *
 * @returns 0 on success, a ANJ_COAP_CODE_* value in case of error.
 */
int anj_observe_data_model_changed(anj_t *anj,
                                   const anj_uri_path_t *path,
                                   anj_observe_change_type_t change_type,
                                   uint16_t ssid);

#    endif // ANJ_WITH_OBSERVE

#endif // SRC_ANJ_OBSERVE_H
