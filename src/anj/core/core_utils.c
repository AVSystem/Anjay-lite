/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#include "../dm/dm_io.h"
#include "core_utils.h"

#define COAP_DEFAULT_PORT_STR "5683"
#define COAPS_DEFAULT_PORT_STR "5684"

#define COAP_DEFAULT_BOOTSTRAP_PORT_STR "5693"
#define COAPS_DEFAULT_BOOTSTRAP_PORT_STR "5694"

int _anj_parse_uri_components(const char *uri,
                              bool is_bootstrap,
                              _anj_uri_components_t *out_uri_components) {
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

int _anj_server_get_resolved_server_uri(anj_t *anj) {
    anj_res_value_t res_val;
    const anj_uri_path_t path =
            ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY,
                                   anj->security_instance.iid,
                                   SECURITY_OBJ_SERVER_URI_RID);
    if (anj_dm_res_read(anj, &path, &res_val)) {
        return -1;
    }

    _anj_uri_components_t anj_uri_components;
    if (_anj_parse_uri_components((const char *) res_val.bytes_or_string.data,
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

#ifndef NDEBUG
int _anj_validate_security_resource_types(anj_t *anj) {
    anj_uri_path_t path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY,
                                                 anj->security_instance.iid,
                                                 SECURITY_OBJ_SERVER_URI_RID);
    anj_data_type_t type;
    if (_anj_dm_get_resource_type(anj, &path, &type)
            || type != ANJ_DATA_TYPE_STRING) {
        log(L_ERROR, "Invalid URI type");
        return -1;
    }
    path = ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY,
                                  anj->security_instance.iid,
                                  SECURITY_OBJ_CLIENT_HOLD_OFF_TIME_RID);
    if (_anj_dm_get_resource_type(anj, &path, &type)
            || type != ANJ_DATA_TYPE_INT) {
        log(L_ERROR, "Invalid Client Hold Off Time type");
        return -1;
    }
    return 0;
}
#endif // NDEBUG
