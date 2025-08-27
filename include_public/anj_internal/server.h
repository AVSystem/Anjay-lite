/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_SERVER_H
#define ANJ_INTERNAL_SERVER_H

#ifndef ANJ_INTERNAL_INCLUDE_SERVER
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_SERVER

#ifdef __cplusplus
extern "C" {
#endif

/** @anj_internal_api_do_not_use */
typedef struct anj_server_connection_ctx_struct {
    anj_net_ctx_t *net_ctx;
    int32_t mtu;
    size_t bytes_sent;
    anj_net_binding_type_t type;
    bool send_in_progress;
} _anj_server_connection_ctx_t;

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_SERVER_H
