/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

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
#include "../utils.h"
#include "core_utils.h"

#define COAP_DEFAULT_PORT_STR "5683"
#define COAPS_DEFAULT_PORT_STR "5684"

#define COAP_DEFAULT_BOOTSTRAP_PORT_STR "5693"
#define COAPS_DEFAULT_BOOTSTRAP_PORT_STR "5694"

int _anj_server_get_resolved_server_uri(anj_t *anj) {
    anj_res_value_t res_val;
    const anj_uri_path_t path =
            ANJ_MAKE_RESOURCE_PATH(ANJ_OBJ_ID_SECURITY,
                                   anj->security_instance.iid,
                                   SECURITY_OBJ_SERVER_URI_RID);
    if (anj_dm_res_read(anj, &path, &res_val)) {
        return -1;
    }

    const char *uri_start = (const char *) res_val.bytes_or_string.data;
    if (strncmp(uri_start, "coap://", sizeof("coap://") - 1) == 0) {
        anj->security_instance.type = ANJ_NET_BINDING_UDP;
        uri_start += sizeof("coap://") - 1;
    } else if (strncmp(uri_start, "coaps://", sizeof("coaps://") - 1) == 0) {
        // HACK: DTLS is not supported yet
        anj->security_instance.type = ANJ_NET_BINDING_DTLS;
        uri_start += sizeof("coaps://") - 1;
    } // TODO: implement other schemes
    else {
        log(L_ERROR, "Unsupported URI scheme");
        return -1;
    }

    // find uri start
    // if uri contains IPv6 address, it contains many ':'
    // so we need to find first ':' after ']' to get port
    char *uri_end = strchr(uri_start, ']');
    if (uri_end) {
        uri_end = strchr(uri_end, ':');
    } else {
        uri_end = strchr(uri_start, ':');
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
        memcpy(anj->security_instance.port, uri_end + 1, port_len);
    } else {
        uri_end = strchr(uri_start, '/');
        // no port specified, use default
        switch (anj->server_state.conn_status) {
        case ANJ_CONN_STATUS_BOOTSTRAPPING:
            memcpy(anj->security_instance.port, COAP_DEFAULT_BOOTSTRAP_PORT_STR,
                   sizeof(COAP_DEFAULT_BOOTSTRAP_PORT_STR));
            break;
        case ANJ_CONN_STATUS_REGISTERING:
            memcpy(anj->security_instance.port, COAP_DEFAULT_PORT_STR,
                   sizeof(COAP_DEFAULT_PORT_STR));
            break;
        default:
            ANJ_UNREACHABLE("Invalid connection status");
            break;
        }
    }

    if (!uri_end) {
        log(L_ERROR, "Invalid URI");
        return -1;
    }

    const size_t uri_len = (size_t) (uri_end - uri_start);
    memset(anj->security_instance.server_uri, 0x00,
           sizeof(anj->security_instance.server_uri));
    memcpy(anj->security_instance.server_uri, uri_start, uri_len);
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
