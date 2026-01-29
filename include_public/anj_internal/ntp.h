/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_NTP_H
#define ANJ_INTERNAL_NTP_H

#ifndef ANJ_INTERNAL_INCLUDE_NTP
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_NTP

#define ANJ_INTERNAL_INCLUDE_SERVER
#include <anj_internal/srv_conn.h>
#undef ANJ_INTERNAL_INCLUDE_SERVER

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ANJ_WITH_NTP

/**
 * Size of NTP message in bytes.
 */
#    define _ANJ_NTP_MSG_SIZE 48
/**
 * Size of NTP buffer in bytes.
 * Added 1 byte to avoid ANJ_NET_EMSGSIZE error in case of UDP sockets.
 */
#    define _ANJ_NTP_BUFF_SIZE (_ANJ_NTP_MSG_SIZE + 1)

/**
 * @anj_internal_api_do_not_use
 * NTP module context structure.
 */
typedef struct _anj_ntp_struct {
    anj_dm_obj_t ntp_object;
    _anj_server_connection_ctx_t connection_ctx;
    // ntp module configuration
    anj_net_socket_configuration_t net_socket_cfg;
    anj_ntp_event_callback_t *event_cb;
    void *event_cb_arg;
    uint16_t max_attempts;
    anj_time_duration_t response_timeout;
    // resource values
    char server_address[ANJ_NTP_SERVER_ADDR_MAX_LEN];
    char backup_server_address[ANJ_NTP_SERVER_ADDR_MAX_LEN];
    uint32_t period_hours;
    bool time_sync_error;
    // cached values
    uint32_t cache_period_hours;
    char cache_server_address[ANJ_NTP_SERVER_ADDR_MAX_LEN];
    char cache_backup_server_address[ANJ_NTP_SERVER_ADDR_MAX_LEN];
    // internal state
    uint8_t internal_state;
    uint16_t attempts;
    anj_time_monotonic_t last_sync_time;
    anj_time_monotonic_t last_successful_sync;
    anj_time_monotonic_t recv_wait_start_time;
    bool using_backup_server;
    bool synchronized;
    // NTP message buffer - can't be on stack because send reguest may be
    // repeated
    uint8_t ntp_msg[_ANJ_NTP_BUFF_SIZE];
} _anj_ntp_t;

#endif // ANJ_WITH_NTP

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_NTP_H
