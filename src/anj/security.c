/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "init_internal.h"

#define ANJ_LOG_SOURCE_FILE_ID 65

#include <assert.h>
#include <stdbool.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/log.h>
#include <anj/security.h>

#ifdef ANJ_WITH_SECURITY

#    include <anj/compat/net/anj_net_api.h>

#    define security_log(...) anj_log(security, __VA_ARGS__)

void anj_security_register_credential_handlers(
        anj_t *anj, const anj_security_credential_handlers_t *handlers) {
    assert(anj && handlers);
    anj->security_credential_handlers = handlers;
}

int anj_security_get_psk_info(const anj_t *anj,
                              bool bootstrap_credentials,
                              anj_net_psk_info_t *out_psk_info) {
    assert(anj && out_psk_info);

    if (anj->security_credential_handlers
            && anj->security_credential_handlers->get_psk_info) {
        return anj->security_credential_handlers->get_psk_info(
                anj, bootstrap_credentials, out_psk_info);
    }
    security_log(L_ERROR, "PSK credential handler not registered");
    return -1;
}

#    ifdef ANJ_WITH_CERTIFICATES
int anj_security_get_cert_info(const anj_t *anj,
                               bool bootstrap_credentials,
                               anj_net_certificate_info_t *out_cert_info) {
    assert(anj && out_cert_info);
    if (anj->security_credential_handlers
            && anj->security_credential_handlers->get_cert_info) {
        int ret = anj->security_credential_handlers->get_cert_info(
                anj, bootstrap_credentials, out_cert_info);
        out_cert_info->trust_store = anj->trust_store;
        return ret;
    }
    security_log(L_ERROR, "Certificate credential handler not registered");
    return -1;
}
#    endif // ANJ_WITH_CERTIFICATES

#endif // ANJ_WITH_SECURITY
