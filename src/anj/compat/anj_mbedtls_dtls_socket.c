/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "../init_internal.h"

#define ANJ_LOG_SOURCE_FILE_ID 61

#include <assert.h>  // IWYU pragma: keep
#include <stdbool.h> // IWYU pragma: keep
#include <stdio.h>   // IWYU pragma: keep
#include <string.h>  // IWYU pragma: keep

#if defined(ANJ_WITH_MBEDTLS) && defined(ANJ_NET_WITH_DTLS)

#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
#        include <anj/compat/crypto/storage.h>
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

#    include <anj/compat/net/anj_dtls.h>
#    include <anj/compat/net/anj_net_api.h>
#    include <anj/compat/net/anj_net_wrapper.h>
#    include <anj/compat/rng.h>
#    include <anj/crypto.h>
#    include <anj/log.h>
#    include <anj/utils.h>

#    include <mbedtls/entropy.h>
#    include <mbedtls/error.h>
#    include <mbedtls/net_sockets.h>
#    include <mbedtls/platform.h>
#    include <mbedtls/ssl.h>
#    include <mbedtls/timing.h>

#    ifdef ANJ_WITH_CERTIFICATES
#        include <mbedtls/pk.h>
#        include <mbedtls/x509_crt.h>
#    endif // ANJ_WITH_CERTIFICATES

#    include "anj_mbedtls_dtls_internal.h"

int _anj_mbedtls_rng(void *p_rng, unsigned char *output, size_t out_len) {
    (void) p_rng;
    int ret = anj_rng_generate((uint8_t *) output, out_len);
    return ret == 0 ? 0 : MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

static bool is_retry_result(int result) {
    return result == MBEDTLS_ERR_SSL_WANT_READ
           || result == MBEDTLS_ERR_SSL_WANT_WRITE
           || result == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS
           || result == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS
           || result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
           || result == MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA;
}

static int
_anj_mbedtls_bio_send(void *ctx, const unsigned char *buf, size_t len) {
    _anj_mbedtls_ssl_socket_t *s = (_anj_mbedtls_ssl_socket_t *) ctx;
    size_t sent = 0;

    int ret = anj_net_send(ANJ_NET_BINDING_UDP, s->net, &sent, buf, len);

    if (anj_net_is_again(ret) || anj_net_is_inprogress(ret)) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    } else if (ret != ANJ_NET_OK) {
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return (int) sent;
}

static int _anj_mbedtls_bio_recv(void *ctx, unsigned char *buf, size_t len) {
    _anj_mbedtls_ssl_socket_t *s = (_anj_mbedtls_ssl_socket_t *) ctx;
    size_t got = 0;

    int ret = anj_net_recv(ANJ_NET_BINDING_UDP, s->net, &got, buf, len);
    // we have to different the ANJ_NET_EINPROGRESS from ANJ_NET_EAGAIN
    s->last_recv_err = ret;

    if (anj_net_is_again(ret) || anj_net_is_inprogress(ret)) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    } else if (ret != ANJ_NET_OK) {
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    return (int) got;
}

static void internal_init(_anj_mbedtls_ssl_socket_t *secure_socket) {
    mbedtls_ssl_init(&secure_socket->ssl_ctx);
    mbedtls_ssl_config_init(&secure_socket->ssl_conf);
#    ifdef ANJ_WITH_CERTIFICATES
    mbedtls_x509_crt_init(&secure_socket->client_cert);
    mbedtls_pk_init(&secure_socket->client_key);
    mbedtls_x509_crt_init(&secure_socket->server_cert);
    mbedtls_x509_crt_init(&secure_socket->ca_certs);
#    endif // ANJ_WITH_CERTIFICATES
    secure_socket->sm_state = _ANJ_MBEDTLS_STATE_INITIAL;
}

static void internal_free(_anj_mbedtls_ssl_socket_t *secure_socket) {
    mbedtls_ssl_free(&secure_socket->ssl_ctx);
    mbedtls_ssl_config_free(&secure_socket->ssl_conf);
#    ifdef ANJ_WITH_CERTIFICATES
    mbedtls_x509_crt_free(&secure_socket->client_cert);
    mbedtls_pk_free(&secure_socket->client_key);
    mbedtls_x509_crt_free(&secure_socket->server_cert);
    mbedtls_x509_crt_free(&secure_socket->ca_certs);
#    endif // ANJ_WITH_CERTIFICATES
}

#    ifdef ANJ_WITH_CERTIFICATES

static bool cert_raw_data_equal(const mbedtls_x509_crt *a,
                                const mbedtls_x509_crt *b) {
    return a && b && a->raw.p && b->raw.p && a->raw.len == b->raw.len
           && memcmp(a->raw.p, b->raw.p, a->raw.len) == 0;
}

static int verify_server_cert_usage_3(_anj_mbedtls_ssl_socket_t *secure_socket,
                                      mbedtls_x509_crt *crt,
                                      int depth,
                                      uint32_t *flags) {
    /*
     * Certificate Usage 3 (domain-issued certificate) does not use the
     * configured server certificate as a CA trust anchor. Instead, /0/x/4 is
     * expected to contain the exact server end-entity certificate.
     *
     * mbedTLS still performs its regular certificate verification before
     * calling this callback. Since no real PKIX trust chain is configured for
     * this mode, MBEDTLS_X509_BADCERT_NOT_TRUSTED is expected and handled here
     * by comparing the peer leaf certificate directly with the configured
     * certificate.
     *
     * Other verification errors, e.g. expired/not-yet-valid certificates or
     * hostname mismatch, are left unchanged and cause the handshake to fail.
     */
    if (*flags != 0 && *flags != MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
        mbedtls_log(L_ERROR,
                    "Server certificate verification failed: %" PRIu32,
                    *flags);
        return 0;
    }
    /*
     * mbedTLS calls this callback for every certificate in the peer chain. The
     * leaf certificate has depth 0. For usage 3, only the leaf is relevant; all
     * non-leaf verification results are ignored.
     */
    if (depth != 0) {
        *flags = 0;
        return 0;
    }

    if (cert_raw_data_equal(crt, &secure_socket->server_cert)) {
        *flags = 0;
        mbedtls_log(L_TRACE, "Server certificate verification successful");
        return 0;
    }
    mbedtls_log(L_ERROR, "Server certificate verification failed");
    *flags = MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    return 0;
}

static int verify_server_cert_usage_2(_anj_mbedtls_ssl_socket_t *secure_socket,
                                      mbedtls_x509_crt *crt,
                                      int depth,
                                      uint32_t *flags) {
    /*
     * Certificate Usage 2 (trust anchor assertion) treats /0/x/4 as a
     * per-server trust anchor.
     *
     * configure_server_cert_verification() passes the configured /0/x/4
     * certificate to mbedTLS as the CA chain. Therefore mbedTLS performs
     * regular PKIX path validation against that trust anchor.
     *
     * Unlike Certificate Usage 3, MBEDTLS_X509_BADCERT_NOT_TRUSTED is not
     * ignored here. If mbedTLS reports NOT_TRUSTED, the peer chain was not
     * successfully built to the configured trust anchor.
     *
     * The only additional check is the degenerate case in which the server
     * presents the configured trust anchor itself as the end-entity
     * certificate. In usage 2, /0/x/4 is a trust anchor, not the peer leaf.
     */
    if (depth == 0 && cert_raw_data_equal(crt, &secure_socket->server_cert)) {
        mbedtls_log(L_ERROR,
                    "Server presented the configured trust anchor as its "
                    "end-entity certificate");
        *flags |= MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    }

    if (depth == 0 && *flags != 0) {
        mbedtls_log(L_ERROR,
                    "Server certificate verification failed: %" PRIu32,
                    *flags);
    }

    return 0;
}

static int verify_server_cert_usage_1(_anj_mbedtls_ssl_socket_t *secure_socket,
                                      mbedtls_x509_crt *crt,
                                      int depth,
                                      uint32_t *flags) {
    if (*flags != 0) {
        mbedtls_log(L_ERROR, "Server certificate verification failed: %" PRIu32,
                    *flags);
        return 0;
    }
    /*
     * mbedTLS calls this callback for every certificate in the peer chain. The
     * leaf certificate has depth 0. For usage 1, the leaf must be equal to the
     * /0/x/4 certificate.
     */
    if (depth != 0) {
        return 0;
    }

    if (cert_raw_data_equal(crt, &secure_socket->server_cert)) {
        mbedtls_log(L_TRACE, "Server certificate verification successful");
        return 0;
    }
    mbedtls_log(L_ERROR, "Server certificate verification failed");
    *flags = MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    return 0;
}

static int verify_server_cert_usage_0(_anj_mbedtls_ssl_socket_t *secure_socket,
                                      mbedtls_x509_crt *crt,
                                      int depth,
                                      uint32_t *flags) {
    /*
     * Certificate Usage 0 (CA constraint) requires that the configured /0/x/4
     * certificate appears somewhere in the peer certificate chain.
     *
     * This function checks the entire chain by looking for a match at any depth
     * other than 0 (the leaf).
     */
    if (depth != 0 && cert_raw_data_equal(crt, &secure_socket->server_cert)) {
        secure_socket->dane_pkix_ta_passed = true;
    }

    if (depth == 0) {
        if (!secure_socket->dane_pkix_ta_passed) {
            mbedtls_log(L_ERROR, "Server certificate verification failed");
            *flags |= MBEDTLS_X509_BADCERT_NOT_TRUSTED;
        } else {
            mbedtls_log(L_TRACE, "Server certificate verification successful");
            secure_socket->dane_pkix_ta_passed = false;
        }
    }
    return 0;
}

static int verify_server_cert_cb(void *data,
                                 mbedtls_x509_crt *crt,
                                 int depth,
                                 uint32_t *flags) {
    assert(data && flags);

    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) data;
    anj_net_certificate_info_t *cert =
            &secure_socket->secure_config.security.data.cert;

    switch (cert->certificate_usage) {
    case ANJ_NET_CERTIFICATE_DOMAIN_ISSUED_CERTIFICATE:
        return verify_server_cert_usage_3(secure_socket, crt, depth, flags);
    case ANJ_NET_CERTIFICATE_TRUST_ANCHOR_ASSERTION:
        return verify_server_cert_usage_2(secure_socket, crt, depth, flags);
    case ANJ_NET_CERTIFICATE_SERVICE_CERTIFICATE_CONSTRAINT:
        return verify_server_cert_usage_1(secure_socket, crt, depth, flags);
    case ANJ_NET_CERTIFICATE_CA_CONSTRAINT:
        return verify_server_cert_usage_0(secure_socket, crt, depth, flags);
    default:
        ANJ_UNREACHABLE("Invalid state");
        *flags |= MBEDTLS_X509_BADCERT_NOT_TRUSTED;
        return 0;
    }
}

static int
configure_server_cert_verification(_anj_mbedtls_ssl_socket_t *secure_socket) {
    anj_net_certificate_info_t *cert =
            &secure_socket->secure_config.security.data.cert;

#        ifdef ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP
    mbedtls_log(L_WARNING,
                "Server certificate not set, skipping server cert "
                "verification (INSECURE)");
    mbedtls_ssl_conf_authmode(&secure_socket->ssl_conf,
                              MBEDTLS_SSL_VERIFY_NONE);
    return 0;
#        endif // ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP

    if ((cert->certificate_usage
                 == ANJ_NET_CERTIFICATE_SERVICE_CERTIFICATE_CONSTRAINT
         || cert->certificate_usage == ANJ_NET_CERTIFICATE_CA_CONSTRAINT)
            && cert->trust_store.ca_certs_count == 0) {
        mbedtls_log(L_ERROR, "In this mode trust store must be provided");
        return -1;
    }

    if (cert->server_cert.source == ANJ_CRYPTO_DATA_SOURCE_EMPTY) {
        mbedtls_log(L_ERROR, "Server certificate not set");
        return -1;
    }

    mbedtls_ssl_conf_authmode(&secure_socket->ssl_conf,
                              MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_verify(&secure_socket->ssl_conf, verify_server_cert_cb,
                            secure_socket);

    if (cert->certificate_usage
                    == ANJ_NET_CERTIFICATE_SERVICE_CERTIFICATE_CONSTRAINT
            || cert->certificate_usage == ANJ_NET_CERTIFICATE_CA_CONSTRAINT) {
        mbedtls_ssl_conf_ca_chain(&secure_socket->ssl_conf,
                                  &secure_socket->ca_certs, NULL);
    } else {
        /*
         * For certificate_usage == 3 (domain-issued certificate), the
         * configured server certificate is not used as a PKIX trust anchor. It
         * is passed as CA chain only to satisfy mbedTLS requirements; the
         * actual verification is an exact leaf certificate match in
         * verify_server_cert_cb().
         *
         * For certificate_usage == 2 (trust anchor assertion), the configured
         * server certificate is intentionally used as a per-server PKIX trust
         * anchor. mbedTLS validates the peer chain against it.
         */
        mbedtls_ssl_conf_ca_chain(&secure_socket->ssl_conf,
                                  &secure_socket->server_cert, NULL);
    }

    return 0;
}
#    endif // ANJ_WITH_CERTIFICATES

static const int psk_allowed_ciphersuites[] = {
    ANJ_MBEDTLS_ALLOWED_PSK_CIPHERSUITES, 0
};

#    ifdef ANJ_WITH_CERTIFICATES
static const int cert_allowed_ciphersuites[] = {
    ANJ_MBEDTLS_ALLOWED_CERT_CIPHERSUITES, 0
};
#    endif

static const int *get_allowed_ciphersuites(anj_net_security_info_t *config) {
    switch (config->mode) {
    case ANJ_NET_SECURITY_PSK:
        return psk_allowed_ciphersuites;
#    ifdef ANJ_WITH_CERTIFICATES
    case ANJ_NET_SECURITY_CERTIFICATE:
        return cert_allowed_ciphersuites;
#    endif
    default:
        return NULL;
    }
}

static int configure_dtls_client(_anj_mbedtls_ssl_socket_t *secure_socket) {
    int res = mbedtls_ssl_config_defaults(&secure_socket->ssl_conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT);
    if (res) {
        mbedtls_log(L_ERROR, "Failed to set SSL config defaults with %d", res);
        return -1;
    }

    // LwM2M 1.2 requires DTLS 1.2/1.3 support, but MbedTLS supports
    // only 1.2 so we set both min and max version to 1.2
    mbedtls_ssl_conf_min_tls_version(&secure_socket->ssl_conf,
                                     MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&secure_socket->ssl_conf,
                                     MBEDTLS_SSL_VERSION_TLS1_2);

    mbedtls_ssl_conf_ciphersuites(
            &secure_socket->ssl_conf,
            get_allowed_ciphersuites(&secure_socket->secure_config.security));

    mbedtls_ssl_conf_rng(&secure_socket->ssl_conf, _anj_mbedtls_rng, NULL);

    mbedtls_ssl_conf_handshake_timeout(&secure_socket->ssl_conf,
                                       ANJ_MBEDTLS_HS_INITIAL_TIMEOUT_VALUE_MS,
                                       ANJ_MBEDTLS_HS_MAXIMUM_TIMEOUT_VALUE_MS);

#    if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    res = mbedtls_ssl_conf_cid(&secure_socket->ssl_conf, 0,
                               MBEDTLS_SSL_UNEXPECTED_CID_IGNORE);
    if (res) {
        mbedtls_log(L_ERROR, "mbedtls conf cid failed with %d", res);
        return -1;
    }
#    endif // MBEDTLS_SSL_DTLS_CONNECTION_ID

#    ifdef ANJ_WITH_CERTIFICATES
    if (secure_socket->secure_config.security.mode
            == ANJ_NET_SECURITY_CERTIFICATE) {
        return configure_server_cert_verification(secure_socket);
    }
#    endif // ANJ_WITH_CERTIFICATES

    return 0;
}

static int setup_ssl_ctx(_anj_mbedtls_ssl_socket_t *secure_socket) {
    int res = mbedtls_ssl_setup(&secure_socket->ssl_ctx,
                                &secure_socket->ssl_conf);
    if (res) {
        mbedtls_log(L_ERROR, "Failed to setup SSL context with %d", res);
        return -1;
    }

#    if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    res = mbedtls_ssl_set_cid(&secure_socket->ssl_ctx, MBEDTLS_SSL_CID_ENABLED,
                              NULL, 0);
    if (res) {
        mbedtls_log(L_ERROR, "mbedtls set cid failed with %d", res);
        return -1;
    }
#    endif // MBEDTLS_SSL_DTLS_CONNECTION_ID

    // integration may provide a custom implementation of
    // mbedtls_timing_set/get_delay
    mbedtls_ssl_set_timer_cb(&secure_socket->ssl_ctx, &secure_socket->timer,
                             mbedtls_timing_set_delay,
                             mbedtls_timing_get_delay);
    // set bio callbacks for the SSL context to use our send/recv functions
    mbedtls_ssl_set_bio(&secure_socket->ssl_ctx, secure_socket,
                        _anj_mbedtls_bio_send, _anj_mbedtls_bio_recv, NULL);

    int32_t mtu = 0;
    if (anj_net_is_ok(anj_net_get_inner_mtu(ANJ_NET_BINDING_UDP,
                                            secure_socket->net, &mtu))
            && mtu > 0 && mtu <= UINT16_MAX) {
        mbedtls_ssl_set_mtu(&secure_socket->ssl_ctx, (uint16_t) mtu);
    }

    return 0;
}

#    ifdef MBEDTLS_X509_CRT_PARSE_C
static int configure_hostname(_anj_mbedtls_ssl_socket_t *secure_socket,
                              const char *hostname) {
    anj_net_security_info_t *config = &secure_socket->secure_config.security;
#        ifdef ANJ_WITH_CERTIFICATES
    const char *sni = config->mode == ANJ_NET_SECURITY_CERTIFICATE
                              ? config->data.cert.sni
                              : config->data.psk.sni;
#        else  // ANJ_WITH_CERTIFICATES
    const char *sni = config->data.psk.sni;
#        endif // ANJ_WITH_CERTIFICATES
    if (sni && sni[0] != '\0') {
        mbedtls_log(L_TRACE, "SNI: %s", sni);
    } else {
        // If SNI is not explicitly provided, use the hostname as SNI if
        // possible. Hostname may be IP address, which is not strictly compliant
        // with RFC 6066.
        sni = hostname;
        mbedtls_log(L_TRACE, "Using hostname as SNI");
    }

    // only MBEDTLS_ERR_SSL_ALLOC_FAILED is documented as a possible error code
    if (mbedtls_ssl_set_hostname(&secure_socket->ssl_ctx, sni)) {
        mbedtls_log(L_ERROR, "Failed to set SNI");
        return -1;
    }
    return 0;
}
#    endif // MBEDTLS_X509_CRT_PARSE_C

int anj_dtls_connect(anj_net_ctx_t *ctx_,
                     const char *hostname,
                     const char *port_str) {
    assert(ctx_ && hostname && port_str);
    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) ctx_;

    switch (secure_socket->sm_state) {
    case _ANJ_MBEDTLS_STATE_INITIAL: {
        if (_anj_mbedtls_load_security_credentials(secure_socket)
                || configure_dtls_client(secure_socket)
                || setup_ssl_ctx(secure_socket)
#    ifdef MBEDTLS_X509_CRT_PARSE_C
                || configure_hostname(secure_socket, hostname)
#    endif // MBEDTLS_X509_CRT_PARSE_C
        ) {
            goto reset_config_and_ctx;
        }

        secure_socket->sm_state = _ANJ_MBEDTLS_STATE_CONNECTING;
    }
        // fallthrough
    case _ANJ_MBEDTLS_STATE_CONNECTING: {
        // If Connection ID was not negotiated during the handshake and close()
        // has been called, a new handshake is required.
        int ret = anj_net_connect(ANJ_NET_BINDING_UDP, secure_socket->net,
                                  hostname, port_str);
        if (anj_net_is_inprogress(ret)) {
            return ret;
        } else if (!anj_net_is_ok(ret)) {
            return -1;
        }

        secure_socket->sm_state = _ANJ_MBEDTLS_STATE_HANDSHAKE_IN_PROGRESS;
    }
        // fallthrough
    case _ANJ_MBEDTLS_STATE_HANDSHAKE_IN_PROGRESS: {
        int result = mbedtls_ssl_handshake(&secure_socket->ssl_ctx);
        if (is_retry_result(result)) {
            mbedtls_log(L_TRACE, "Handshake in progress");
            return ANJ_NET_EINPROGRESS;
        }
        if (result != 0) {
#    ifdef MBEDTLS_ERROR_C
            mbedtls_log(L_ERROR, "Handshake failed: %s",
                        mbedtls_high_level_strerr(result));
#    else  // MBEDTLS_ERROR_C
            mbedtls_log(L_ERROR, "Handshake failed: %d", result);
#    endif // MBEDTLS_ERROR_C
            goto reset_config_and_ctx;
        }
        mbedtls_log(L_INFO, "DTLS handshake completed successfully");
        mbedtls_log(L_DEBUG, "Negotiated DTLS ciphersuite: %s",
                    mbedtls_ssl_get_ciphersuite(&secure_socket->ssl_ctx));
#    if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
        unsigned char peer_cid[MBEDTLS_SSL_CID_OUT_LEN_MAX];
        size_t peer_cid_len = 0;
        int enabled = 0;
        (void) mbedtls_ssl_get_peer_cid(&secure_socket->ssl_ctx, &enabled,
                                        peer_cid, &peer_cid_len);
        if (enabled) {
            char peer_cid_hex[2 * sizeof(peer_cid) + 1];
            if (!anj_hexlify(peer_cid_hex, sizeof(peer_cid_hex), peer_cid,
                             peer_cid_len)) {
                mbedtls_log(L_INFO, "negotiated CID = %s", peer_cid_hex);
            }
        }
#    endif // MBEDTLS_SSL_DTLS_CONNECTION_ID
        secure_socket->state = ANJ_NET_SOCKET_STATE_CONNECTED;
        secure_socket->sm_state = _ANJ_MBEDTLS_STATE_HANDSHAKE_DONE;
        return ANJ_NET_OK;
    }
    case _ANJ_MBEDTLS_STATE_HANDSHAKE_DONE: {
        // Post-handshake state: anj_dtls_cleanup_ctx() has NOT been called.
        // If Connection ID was negotiated during the handshake and close() has
        // been called, only the UDP socket has been closed. The next connect()
        // call should then only reopen UDP and reuse the existing DTLS session.
        //
        // Use anj_dtls_cleanup_ctx() if a full DTLS teardown is required.
        //
        // Integrators that do not want session reuse across reconnects may
        // remove this optimization and always recreate the DTLS state.
        if (secure_socket->state == ANJ_NET_SOCKET_STATE_CLOSED) {
            int ret = anj_net_connect(ANJ_NET_BINDING_UDP, secure_socket->net,
                                      hostname, port_str);
            if (anj_net_is_inprogress(ret)) {
                return ret;
            } else if (!anj_net_is_ok(ret)) {
                return -1;
            }
            secure_socket->state = ANJ_NET_SOCKET_STATE_CONNECTED;
        }
        return ANJ_NET_OK;
    }
    default:
        mbedtls_log(L_ERROR, "Invalid state: %d", secure_socket->sm_state);
        return -1;
    }

reset_config_and_ctx:
    internal_free(secure_socket);
    internal_init(secure_socket);
    return -1;
}

int anj_dtls_create_ctx(anj_net_ctx_t **ctx_, const anj_net_config_t *config) {
    assert(ctx_ && config);
    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) mbedtls_calloc(
                    1, sizeof(_anj_mbedtls_ssl_socket_t));
    if (!secure_socket) {
        return -1;
    }
    secure_socket->secure_config = config->secure_socket_config;
    secure_socket->close_notify_sent = false;

    // Create UDP transport for DTLS
    if (!anj_net_is_ok(anj_net_create_ctx(ANJ_NET_BINDING_UDP,
                                          &secure_socket->net, config))) {
        mbedtls_free(secure_socket);
        return -1;
    }

    internal_init(secure_socket);
    secure_socket->state = ANJ_NET_SOCKET_STATE_CLOSED;

    *ctx_ = (anj_net_ctx_t *) secure_socket;
    return ANJ_NET_OK;
}

int anj_dtls_send(anj_net_ctx_t *ctx_,
                  size_t *bytes_sent,
                  const uint8_t *buf,
                  size_t length) {
    assert(ctx_ && bytes_sent && buf);

    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) ctx_;

    size_t already_sent = 0;
    while (already_sent < length) {
        int result =
                mbedtls_ssl_write(&secure_socket->ssl_ctx, buf + already_sent,
                                  length - already_sent);
        if (result >= 0) {
            already_sent += (size_t) result;
            continue;
        }
        if (is_retry_result(result)) {
            // Some data was sent, but the transport is currently busy and
            // cannot accept more data. Return the amount of data that was sent.
            if (already_sent > 0) {
                *bytes_sent = already_sent;
                return ANJ_NET_OK;
            }
            mbedtls_log(L_TRACE, "Transport busy, need to retry send");
            return ANJ_NET_EINPROGRESS;
        }
        if (result < 0) {
            mbedtls_log(L_ERROR, "Failed to send data with error %d", result);
            return -1;
        }
    }
    *bytes_sent = already_sent;
    return ANJ_NET_OK;
}

int anj_dtls_recv(anj_net_ctx_t *ctx_,
                  size_t *bytes_received,
                  uint8_t *buf,
                  size_t length) {
    assert(ctx_);
    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) ctx_;

    secure_socket->last_recv_err = ANJ_NET_OK;

    int result = mbedtls_ssl_read(&secure_socket->ssl_ctx, buf, length);
    if (is_retry_result(result)) {
        /*
         * If mbedtls_ssl_read() reached the BIO receive callback and the
         * transport returned ANJ_NET_EAGAIN, there is currently no data
         * available to read and no read operation is pending at the transport
         * layer.
         * In all other retry cases, either the transport operation is still in
         * progress, or Mbed TLS needs the same read operation to be retried
         * later because it is still processing internal DTLS/TLS state.
         */
        return (secure_socket->last_recv_err == ANJ_NET_EAGAIN
                && result == MBEDTLS_ERR_SSL_WANT_READ)
                       ? ANJ_NET_EAGAIN
                       : ANJ_NET_EINPROGRESS;
    }
    if (result < 0) {
        mbedtls_log(L_ERROR, "Failed to receive data with error %d", result);
        return -1;
    }
    *bytes_received = (size_t) result;
    return ANJ_NET_OK;
}

int anj_dtls_close(anj_net_ctx_t *ctx_) {
    // DTLS seesion is not closed here, only the underlying UDP socket. The next
    // connect() should then only reopen UDP and reuse the existing DTLS
    // session. Connection ID must be supported and enabled for this
    // optimization to work.
    assert(ctx_);
    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) ctx_;

    int ret = anj_net_close(ANJ_NET_BINDING_UDP, secure_socket->net);
    if (anj_net_is_inprogress(ret)) {
        return ret;
    }
    secure_socket->state = ANJ_NET_SOCKET_STATE_CLOSED;
    if (!anj_net_is_ok(ret)) {
        return -1;
    }

#    if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    int cid_enabled = 0;
    (void) mbedtls_ssl_get_peer_cid(&secure_socket->ssl_ctx, &cid_enabled, NULL,
                                    NULL);
    if (!cid_enabled)
#    endif // MBEDTLS_SSL_DTLS_CONNECTION_ID
    {
        mbedtls_log(
                L_DEBUG,
                "Connection ID is not in use, new handshake will be required");

        mbedtls_ssl_session saved_session;
        mbedtls_ssl_session_init(&saved_session);

        int ret_get_session = mbedtls_ssl_get_session(&secure_socket->ssl_ctx,
                                                      &saved_session);
        ret = mbedtls_ssl_session_reset(&secure_socket->ssl_ctx);
        if (!ret_get_session) {
            (void) mbedtls_ssl_set_session(&secure_socket->ssl_ctx,
                                           &saved_session);
        }
        mbedtls_ssl_session_free(&saved_session);

        if (ret) {
            mbedtls_log(L_ERROR, "Failed to reset SSL session");
            return -1;
        }
        secure_socket->sm_state = _ANJ_MBEDTLS_STATE_CONNECTING;
    }
    return ANJ_NET_OK;
}

int anj_dtls_cleanup_ctx(anj_net_ctx_t **ctx_) {
    assert(ctx_ && *ctx_);
    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) *ctx_;
    secure_socket->sm_state = _ANJ_MBEDTLS_STATE_INITIAL;
    int ret;

    if (!secure_socket->close_notify_sent) {
        // Send close_notify and flush DTLS state
        ret = mbedtls_ssl_close_notify(&secure_socket->ssl_ctx);
        if (is_retry_result(ret)) {
            return ANJ_NET_EINPROGRESS;
        } else if (ret == 0) {
            secure_socket->close_notify_sent = true;
        }
    }

    ret = anj_net_cleanup_ctx(ANJ_NET_BINDING_UDP, &secure_socket->net);
    if (anj_net_is_inprogress(ret)) {
        return ret;
    }
    internal_free(secure_socket);
    mbedtls_free(secure_socket);
    *ctx_ = NULL;

    if (!anj_net_is_ok(ret)) {
        return -1;
    }
    return ANJ_NET_OK;
}

int anj_dtls_get_inner_mtu(anj_net_ctx_t *ctx, int32_t *out_value) {
    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) ctx;
    int ret = anj_net_get_inner_mtu(ANJ_NET_BINDING_UDP, secure_socket->net,
                                    out_value);
    if (!anj_net_is_ok(ret)) {
        return ret;
    }

#    if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
// When DTLS CID is enabled, overhead increases due to the CID field.
#        define ANJ_MBEDTLS_DTLS_MTU_OVERHEAD 21
#    else // MBEDTLS_SSL_DTLS_CONNECTION_ID
#        define ANJ_MBEDTLS_DTLS_MTU_OVERHEAD 13
#    endif // MBEDTLS_SSL_DTLS_CONNECTION_ID

    if (*out_value <= ANJ_MBEDTLS_DTLS_MTU_OVERHEAD) {
        return -1;
    }
    *out_value = *out_value - ANJ_MBEDTLS_DTLS_MTU_OVERHEAD;
    return ANJ_NET_OK;
}

int anj_dtls_get_state(anj_net_ctx_t *ctx_, anj_net_socket_state_t *out_value) {
    assert(ctx_ && out_value);
    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) ctx_;
    *out_value = secure_socket->state;
    return ANJ_NET_OK;
}

int anj_dtls_queue_mode_rx_off(anj_net_ctx_t *ctx_) {
    assert(ctx_);
    _anj_mbedtls_ssl_socket_t *secure_socket =
            (_anj_mbedtls_ssl_socket_t *) ctx_;
    return anj_net_queue_mode_rx_off(ANJ_NET_BINDING_UDP, secure_socket->net);
}

#endif // ANJ_WITH_MBEDTLS && ANJ_NET_WITH_DTLS
