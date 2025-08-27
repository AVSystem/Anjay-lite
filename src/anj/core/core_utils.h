/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_SRC_CORE_CORE_UTILS_H
#    define ANJ_SRC_CORE_CORE_UTILS_H

#    include <stdbool.h>
#    include <stdint.h>

#    include <anj/compat/net/anj_net_api.h>
#    include <anj/defs.h>
#    include <anj/log/log.h>

#    define SERVER_OBJ_LIFETIME_RID 1
#    define SERVER_OBJ_DEFAULT_PMIN_RID 2
#    define SERVER_OBJ_DEFAULT_PMAX_RID 3
#    define SERVER_OBJ_DISABLE_TIMEOUT 5
#    define SERVER_OBJ_NOTIFICATION_STORING_RID 6
#    define SERVER_OBJ_BOOTSTRAP_ON_REGISTRATION_FAILURE_RID 16
#    define SERVER_OBJ_COMMUNICATION_RETRY_COUNT_RID 17
#    define SERVER_OBJ_COMMUNICATION_RETRY_TIMER_RID 18
#    define SERVER_OBJ_COMMUNICATION_SEQUENCE_DELAY_TIMER_RID 19
#    define SERVER_OBJ_COMMUNICATION_SEQUENCE_RETRY_COUNT_RID 20
#    define SERVER_OBJ_MUTE_SEND_RID 23
#    define SERVER_OBJ_DEFAULT_NOTIFICATION_MODE_RID 26
#    define SECURITY_OBJ_SERVER_URI_RID 0
#    define SECURITY_OBJ_CLIENT_HOLD_OFF_TIME_RID 11

#    define log(...) anj_log(server, __VA_ARGS__)

#    define ANJ_CORE_LOG_COAP_ERROR(Error)                               \
        log(L_ERROR,                                                     \
            "CoAP decoding/ecoding error: %d, check coap.h for details", \
            Error)

typedef struct {
    anj_net_binding_type_t binding_type;
    const char *host;
    uint8_t host_len;
    const char *port;
    uint8_t port_len;
} _anj_uri_components_t;

/**
 * Possible URIs from CoAP specification: Appendix B. URI Examples
 * coap://example.net/
 * coap://eu.iot.avsystem.cloud:5683
 * coaps://[7777:d26:e8:7756:0:0:0:77]:5694
 * coap://example.net/.well-known/core
 * coap://198.51.100.1:61616//%2F//?%2F%2F&?%26
 * coap://example.org/path_1/path_2
 * coap://example.org:5663/path_1
 * coap://example.org:5663//path_2
 *
 * If port is not specified, default port is 5683 for coap and 5684 for coaps is
 * used.
 */
int _anj_parse_uri_components(const char *uri,
                              bool is_bootstrap,
                              _anj_uri_components_t *out_uri_components);

int _anj_server_get_resolved_server_uri(anj_t *anj);
#    ifndef NDEBUG
int _anj_validate_security_resource_types(anj_t *anj);
#    endif // NDEBUG

#endif // ANJ_SRC_CORE_CORE_UTILS_H
