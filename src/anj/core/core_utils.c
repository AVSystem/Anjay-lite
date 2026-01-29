/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 9

#include <ctype.h>
#include <inttypes.h>
#include <string.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/security_object.h>
#include <anj/log.h>
#include <anj/utils.h>

#include "../dm/dm_io.h"
#include "core_utils.h"

#define COAP_DEFAULT_PORT_STR "5683"
#define COAPS_DEFAULT_PORT_STR "5684"

#define COAP_DEFAULT_BOOTSTRAP_PORT_STR "5693"
#define COAPS_DEFAULT_BOOTSTRAP_PORT_STR "5694"

int _anj_core_utils_parse_uri_components(
        const char *uri,
        bool is_bootstrap,
        _anj_core_utils_uri_components_t *out_uri_components) {
    if (strncmp(uri, "coap://", sizeof("coap://") - 1) == 0) {
        out_uri_components->binding_type = ANJ_NET_BINDING_UDP;
        uri += sizeof("coap://") - 1;
    } else if (strncmp(uri, "coaps://", sizeof("coaps://") - 1) == 0) {
        out_uri_components->binding_type = ANJ_NET_BINDING_DTLS;
        uri += sizeof("coaps://") - 1;
    } else {
        log(L_ERROR, "Unsupported URI scheme");
        return -1;
    }
    // find uri start
    // if uri contains IPv6 address, it contains many ':'
    // so we need to find first ':' after ']' to get port
    const char *uri_end = strchr(uri, ']');
    if (uri_end) {
        uri_end = strchr(uri_end, ':');
    } else {
        uri_end = strchr(uri, ':');
    }
    if (uri_end) {
        // get port
        uint8_t port_len = 0;
        // server_uri can't be >= than ANJ_SERVER_URI_MAX_SIZE so on last field
        // of array there is always '\0' - that's why overflow is not possible
        // in this loop
        for (uint8_t i = 1; i <= ANJ_U16_STR_MAX_LEN; i++) {
            if (!isdigit(uri_end[i])) {
                break;
            }
            port_len++;
        }
        if (port_len == 0) {
            log(L_ERROR, "Invalid URI");
            return -1;
        }
        out_uri_components->port = uri_end + 1;
        out_uri_components->port_len = port_len;
    } else {
        uri_end = strchr(uri, '/');
        if (is_bootstrap) {
            out_uri_components->port =
                    (out_uri_components->binding_type == ANJ_NET_BINDING_UDP)
                            ? COAP_DEFAULT_BOOTSTRAP_PORT_STR
                            : COAPS_DEFAULT_BOOTSTRAP_PORT_STR;
        } else {
            out_uri_components->port =
                    (out_uri_components->binding_type == ANJ_NET_BINDING_UDP)
                            ? COAP_DEFAULT_PORT_STR
                            : COAPS_DEFAULT_PORT_STR;
        }
        out_uri_components->port_len = 4; // Default port length for CoAP/CoAPS
    }

    if (!uri_end || uri_end == uri) {
        log(L_ERROR, "Invalid URI");
        return -1;
    }
    out_uri_components->host = uri;
    out_uri_components->host_len = (uint8_t) (uri_end - uri);
    return 0;
}

int _anj_core_utils_server_get_resolved_server_uri(anj_t *anj) {
    anj_res_value_t res_val;
    const anj_uri_path_t path =
            ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY,
                                   anj->security_instance.iid,
                                   SECURITY_OBJ_SERVER_URI_RID);
    if (anj_dm_res_read(anj, &path, &res_val)) {
        return -1;
    }

    _anj_core_utils_uri_components_t anj_uri_components;
    if (_anj_core_utils_parse_uri_components(
                (const char *) res_val.bytes_or_string.data,
                (anj->server_state.conn_status
                 == ANJ_CONN_STATUS_BOOTSTRAPPING),
                &anj_uri_components)) {
        return -1;
    }
    anj->security_instance.type = anj_uri_components.binding_type;
    memcpy(anj->security_instance.port,
           anj_uri_components.port,
           anj_uri_components.port_len);
    anj->security_instance.port[anj_uri_components.port_len] = '\0';
    memcpy(anj->security_instance.server_uri,
           anj_uri_components.host,
           anj_uri_components.host_len);
    anj->security_instance.server_uri[anj_uri_components.host_len] = '\0';
    return 0;
}

#ifdef ANJ_WITH_SECURITY
int _anj_core_utils_get_security_info(
        anj_t *anj,
        bool bootstrap_credentials,
        anj_net_security_info_t *out_security_info) {
    // TODO: only PSK is supported for now
#    ifdef ANJ_WITH_CERTIFICATES
#        error "Certificates support is not implemented yet"
#    else  // ANJ_WITH_CERTIFICATES
    out_security_info->mode = ANJ_NET_SECURITY_PSK;
    return anj_dm_security_obj_get_psk(anj,
                                       bootstrap_credentials,
                                       &out_security_info->data.psk.identity,
                                       &out_security_info->data.psk.key);
#    endif // ANJ_WITH_CERTIFICATES
}
#endif // ANJ_WITH_SECURITY

#ifndef NDEBUG
typedef struct {
    uint16_t rid;
    anj_data_type_t type;
} _resource_type_check_t;

int _anj_core_utils_validate_server_resource_types(anj_t *anj) {
    // if resource is not present and it is mandatory, it will be handled later
    // in the code
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SERVER,
                                                 anj->server_instance.iid, 0);
    anj_data_type_t type;
    // clang-format off
    _resource_type_check_t server_obj_check[] = {
        {.rid = SERVER_OBJ_LIFETIME_RID,                          .type = ANJ_DATA_TYPE_INT},
        {.rid = SERVER_OBJ_DEFAULT_PMIN_RID,                      .type = ANJ_DATA_TYPE_INT},
        {.rid = SERVER_OBJ_DEFAULT_PMAX_RID,                      .type = ANJ_DATA_TYPE_INT},
        {.rid = SERVER_OBJ_DISABLE_TIMEOUT,                       .type = ANJ_DATA_TYPE_INT},
        {.rid = SERVER_OBJ_NOTIFICATION_STORING_RID,              .type = ANJ_DATA_TYPE_BOOL},
        {.rid = SERVER_OBJ_BOOTSTRAP_ON_REGISTRATION_FAILURE_RID, .type = ANJ_DATA_TYPE_BOOL},
        {.rid = SERVER_OBJ_COMMUNICATION_RETRY_COUNT_RID,         .type = ANJ_DATA_TYPE_UINT},
        {.rid = SERVER_OBJ_COMMUNICATION_RETRY_TIMER_RID,         .type = ANJ_DATA_TYPE_UINT},
        {.rid = SERVER_OBJ_COMMUNICATION_SEQUENCE_DELAY_TIMER_RID,.type = ANJ_DATA_TYPE_UINT},
        {.rid = SERVER_OBJ_COMMUNICATION_SEQUENCE_RETRY_COUNT_RID,.type = ANJ_DATA_TYPE_UINT},
#    ifdef ANJ_WITH_LWM2M12
        {.rid = SERVER_OBJ_DEFAULT_NOTIFICATION_MODE_RID,         .type = ANJ_DATA_TYPE_INT},
#    endif // ANJ_WITH_LWM2M12
    };
    // clang-format on
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(server_obj_check); i++) {
        path.ids[ANJ_ID_RID] = server_obj_check[i].rid;
        if (!_anj_dm_get_resource_type(anj, &path, &type)
                && type != server_obj_check[i].type) {
            log(L_ERROR, "Invalid resource type, for %" PRIu16 " RID",
                server_obj_check[i].rid);
            return -1;
        }
    }
    return 0;
}

int _anj_core_utils_validate_security_resource_types(anj_t *anj) {
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY,
                                                 anj->security_instance.iid, 0);
    anj_data_type_t type;
    // clang-format off
    _resource_type_check_t security_obj_check[] = {
        {.rid = SECURITY_OBJ_SERVER_URI_RID,           .type = ANJ_DATA_TYPE_STRING},
        {.rid = SECURITY_OBJ_SERVER_SECURITY_MODE_RID, .type = ANJ_DATA_TYPE_INT},
        {.rid = SECURITY_OBJ_PUBLIC_KEY_OR_IDENTITY_RID, .type = ANJ_DATA_TYPE_BYTES},
        {.rid = SECURITY_OBJ_SERVER_PUBLIC_KEY_RID,    .type = ANJ_DATA_TYPE_BYTES},
        {.rid = SECURITY_OBJ_SECRET_KEY_RID,           .type = ANJ_DATA_TYPE_BYTES},
        {.rid = SECURITY_OBJ_CLIENT_HOLD_OFF_TIME_RID, .type = ANJ_DATA_TYPE_INT}
    };
    // clang-format on
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(security_obj_check); i++) {
        path.ids[ANJ_ID_RID] = security_obj_check[i].rid;
        if (!_anj_dm_get_resource_type(anj, &path, &type)
                && type != security_obj_check[i].type) {
            log(L_ERROR, "Invalid resource type, for %" PRIu16 " RID",
                security_obj_check[i].rid);
            return -1;
        }
    }
    return 0;
}
#endif // NDEBUG
