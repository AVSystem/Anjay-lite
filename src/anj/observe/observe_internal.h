/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_OBSERVE_OBSERVE_CORE_H
#    define SRC_ANJ_OBSERVE_OBSERVE_CORE_H

#    include <string.h>

#    include <anj/defs.h>
#    include <anj/log.h>

#    include "../coap/coap.h"
#    include "observe.h"

#    ifdef ANJ_WITH_OBSERVE

#        define observe_log(...) anj_log(observe, __VA_ARGS__)

#        define MSG_TYPE_OBSERVE_RESPONSE 1
#        define MSG_TYPE_CANCEL_OBSERVE_RESPONSE 2
#        define MSG_TYPE_NOTIFY 3

/* Attributes */

bool _anj_observe_attribute_has_value_change_condition(
        const _anj_attr_notification_t *attr);

int _anj_observe_attributes_apply_condition(
        anj_t *anj,
        const anj_uri_path_t *path,
        const _anj_attr_notification_t *attr);

int _anj_observe_verify_attributes(const _anj_attr_notification_t *attr,
                                   const anj_uri_path_t *path,
                                   bool composite);

bool _anj_observe_is_empty_attr(const _anj_attr_notification_t *attr);

void _anj_observe_update_attr(_anj_attr_notification_t *attr,
                              const _anj_attr_notification_t *new_attr);

bool _anj_observe_compare_attr(const _anj_attr_notification_t *attr1,
                               const _anj_attr_notification_t *attr2);

_anj_observe_attr_storage_t *
_anj_observe_get_attr_from_path(_anj_observe_ctx_t *observe_ctx,
                                const anj_uri_path_t *path,
                                uint16_t ssid);

int _anj_observe_write_attr_handle(anj_t *anj,
                                   const _anj_coap_msg_t *inout_msg,
                                   uint16_t ssid);

/* Observations */

void _anj_observe_set_uri_paths_and_format(anj_t *anj);
void _anj_observe_remove_observation(_anj_observe_ctx_t *ctx);
uint8_t _anj_observe_build_message(void *arg_ptr,
                                   uint8_t *buff,
                                   size_t buff_len,
                                   _anj_exchange_read_result_t *out_params);
int _anj_observe_check_if_value_condition_attributes_should_be_disabled(
        anj_t *anj, _anj_observe_observation_t *observation);
void _anj_observe_write_anj_res_to_observe_val(
        _anj_observation_res_val_t *observe_val,
        const anj_res_value_t *res_value,
        const anj_data_type_t *type);
void _anj_observe_verify_effective_attributes(
        _anj_observe_observation_t *observation);

void _anj_observe_refresh_timestamp(_anj_observe_ctx_t *ctx,
                                    anj_time_real_t timestamp,
                                    bool confirmable);

#    endif // ANJ_WITH_OBSERVE

#endif // SRC_ANJ_OBSERVE_OBSERVE_CORE_H
