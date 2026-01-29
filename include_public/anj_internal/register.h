/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_REGISTER_H
#define ANJ_INTERNAL_REGISTER_H

#ifndef ANJ_INTERNAL_INCLUDE_REGISTER
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_REGISTER

#ifdef __cplusplus
extern "C" {
#endif

/** @anj_internal_api_do_not_use */
typedef struct _anj_register_ctx_struct {
    uint8_t internal_state;
    char location_path[ANJ_COAP_MAX_LOCATION_PATHS_NUMBER]
                      [ANJ_COAP_MAX_LOCATION_PATH_SIZE];
    size_t location_path_len[ANJ_COAP_MAX_LOCATION_PATHS_NUMBER];
    _anj_exchange_handlers_t dm_handlers;
    bool with_payload;
} _anj_register_ctx_t;

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_REGISTER_H
