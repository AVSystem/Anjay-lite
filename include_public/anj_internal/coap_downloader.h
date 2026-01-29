/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_COAP_DOWNLOADER_H
#define ANJ_INTERNAL_COAP_DOWNLOADER_H

#ifndef ANJ_INTERNAL_INCLUDE_COAP_DOWNLOADER
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_COAP_DOWNLOADER

#define ANJ_INTERNAL_INCLUDE_SERVER
#include <anj_internal/srv_conn.h>
#undef ANJ_INTERNAL_INCLUDE_SERVER

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ANJ_WITH_COAP_DOWNLOADER

/**
 * @anj_internal_api_do_not_use
 * CoAP downloader context structure.
 */
typedef struct _anj_coap_downloader_struct {
    _anj_server_connection_ctx_t connection_ctx;
    _anj_exchange_ctx_t exchange_ctx;
    size_t out_msg_len;

    anj_net_config_t net_socket_cfg;
    anj_coap_downloader_event_callback_t *event_cb;
    void *event_cb_arg;

    uint8_t msg_buffer[ANJ_COAP_DOWNLOADER_MAX_MSG_SIZE];

    int error_code;
    anj_coap_downloader_status_t status;
    anj_coap_downloader_status_t last_reported_status;

    const char *uri;
    const char *host;
    uint8_t host_len;
    const char *port;
    uint8_t port_len;
    anj_net_binding_type_t binding;
    _anj_etag_t etag;
} _anj_coap_downloader_t;

#endif // ANJ_WITH_COAP_DOWNLOADER

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_COAP_DOWNLOADER_H
