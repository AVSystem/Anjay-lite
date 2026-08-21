/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "../init_internal.h"

#ifndef SRC_ANJ_COMPAT_MBEDTLS_DTLS_INTERNAL_H
#    define SRC_ANJ_COMPAT_MBEDTLS_DTLS_INTERNAL_H

#    include <anj/compat/net/anj_net_api.h>
#    include <anj/defs.h>
#    include <anj/log.h>

#    include <mbedtls/ssl.h>
#    include <mbedtls/timing.h>

#    ifdef ANJ_WITH_CERTIFICATES
#        include <mbedtls/pk.h>
#        include <mbedtls/x509_crt.h>
#    endif // ANJ_WITH_CERTIFICATES

#    if defined(ANJ_WITH_MBEDTLS) && defined(ANJ_NET_WITH_DTLS)

typedef enum {
    _ANJ_MBEDTLS_STATE_INITIAL,
    _ANJ_MBEDTLS_STATE_CONNECTING,
    _ANJ_MBEDTLS_STATE_HANDSHAKE_IN_PROGRESS,
    _ANJ_MBEDTLS_STATE_HANDSHAKE_DONE,
} _anj_mbedtls_state_machine_t;

typedef struct {
    mbedtls_ssl_context ssl_ctx;
    mbedtls_ssl_config ssl_conf;
    mbedtls_timing_delay_context timer;

    anj_net_ctx_t *net;
    anj_net_ssl_configuration_t secure_config;
    anj_net_socket_state_t state;
    _anj_mbedtls_state_machine_t sm_state;

#        ifdef ANJ_WITH_CERTIFICATES
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_key;
    mbedtls_x509_crt server_cert;
    mbedtls_x509_crt ca_certs;
#        endif // ANJ_WITH_CERTIFICATES

    int last_recv_err;
    bool close_notify_sent;

    bool dane_pkix_ta_passed;
} _anj_mbedtls_ssl_socket_t;

#        define mbedtls_log(level, ...) anj_log(mbedtls, level, __VA_ARGS__)

int _anj_mbedtls_load_security_credentials(
        _anj_mbedtls_ssl_socket_t *secure_socket);

int _anj_mbedtls_rng(void *p_rng, unsigned char *output, size_t out_len);

#    endif // defined(ANJ_WITH_MBEDTLS) && defined(ANJ_NET_WITH_DTLS)

#endif // SRC_ANJ_COMPAT_MBEDTLS_DTLS_INTERNAL_H
