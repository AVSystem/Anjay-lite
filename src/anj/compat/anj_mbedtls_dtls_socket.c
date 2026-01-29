/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

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

#    include <mbedtls/entropy.h>
#    include <mbedtls/error.h>
#    include <mbedtls/net_sockets.h>
#    include <mbedtls/ssl.h>
#    include <mbedtls/timing.h>

#    define mbedtls_log(level, ...) anj_log(mbedtls, level, __VA_ARGS__)

typedef enum {
    SOCKET_STATE_INITIAL,
    SOCKET_STATE_HANDSHAKE_IN_PROGRESS,
    SOCKET_STATE_HANDSHAKE_DONE,
} state_machine_t;

typedef struct {
    mbedtls_ssl_context ssl_ctx;
    mbedtls_ssl_config ssl_conf;
    mbedtls_timing_delay_context timer;

    anj_net_ctx_t *net;
    anj_net_ssl_configuration_t secure_config;
    anj_net_socket_state_t state;
    state_machine_t sm_state;

    int last_recv_err;
    int last_send_err;
    bool close_notify_sent;
} ssl_socket_t;

static int
_anj_mbedtls_rng(void *p_rng, unsigned char *output, size_t out_len) {
    (void) p_rng;
    int ret = anj_rng_generate((uint8_t *) output, out_len);
    return ret == 0 ? 0 : MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

static bool is_retry_result(int result) {
    return result == MBEDTLS_ERR_SSL_WANT_READ
           || result == MBEDTLS_ERR_SSL_WANT_WRITE;
}

static bool is_write_error(int result) {
    return result < 0 && result != MBEDTLS_ERR_SSL_WANT_WRITE
           && result != MBEDTLS_ERR_SSL_WANT_READ
           && result != MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS
           && result != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS
           && result != MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
           && result != MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA;
}

static int
_anj_mbedtls_bio_send(void *ctx, const unsigned char *buf, size_t len) {
    ssl_socket_t *s = (ssl_socket_t *) ctx;
    size_t sent = 0;

    int ret = anj_net_send(ANJ_NET_BINDING_UDP, s->net, &sent, buf, len);
    s->last_send_err = ret;

    if (anj_net_is_again(ret) || anj_net_is_inprogress(ret)) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    } else if (ret != ANJ_NET_OK) {
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return (int) sent;
}

static int _anj_mbedtls_bio_recv(void *ctx, unsigned char *buf, size_t len) {
    ssl_socket_t *s = (ssl_socket_t *) ctx;
    size_t got = 0;

    int ret = anj_net_recv(ANJ_NET_BINDING_UDP, s->net, &got, buf, len);
    s->last_recv_err = ret;

    if (anj_net_is_again(ret) || anj_net_is_inprogress(ret)) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    } else if (ret != ANJ_NET_OK) {
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    return (int) got;
}

static int load_psk(mbedtls_ssl_config *ssl_conf,
                    anj_net_ssl_configuration_t *config) {
    char psk_key_buff[MBEDTLS_PSK_MAX_LEN];
    size_t psk_key_len;
    char psk_identity_buff[ANJ_MBEDTLS_PSK_IDENTITY_MAX_LEN];
    size_t psk_identity_len;

    anj_net_security_info_t *security = &config->security;
    anj_crypto_security_info_t *psk_key = &security->data.psk.key;
    anj_crypto_security_info_t *psk_identity = &security->data.psk.identity;
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    void *crypto_ctx = config->crypto_ctx;
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

    if (psk_key->source == ANJ_CRYPTO_DATA_SOURCE_EMPTY
            || psk_identity->source == ANJ_CRYPTO_DATA_SOURCE_EMPTY) {
        mbedtls_log(L_ERROR, "PSK key or identity is not set");
        return -1;
    }

    // get psk key
    if (psk_key->source == ANJ_CRYPTO_DATA_SOURCE_BUFFER) {
        if (psk_key->info.buffer.data_size > MBEDTLS_PSK_MAX_LEN) {
            mbedtls_log(L_ERROR, "PSK key size exceeds maximum allowed size");
            return -1;
        }
        memcpy(psk_key_buff, psk_key->info.buffer.data,
               psk_key->info.buffer.data_size);
        psk_key_len = psk_key->info.buffer.data_size;
    } else {
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        if (anj_crypto_storage_resolve_security_info(
                    crypto_ctx, &psk_key->info.external, psk_key_buff,
                    MBEDTLS_PSK_MAX_LEN, &psk_key_len)) {
            return -1;
        }
#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        mbedtls_log(L_ERROR, "Such PSK source is not supported");
        return -1;
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    }

    if (psk_identity->source == ANJ_CRYPTO_DATA_SOURCE_BUFFER) {
        if (psk_identity->info.buffer.data_size > MBEDTLS_PSK_MAX_LEN) {
            mbedtls_log(L_ERROR,
                        "PSK identity size exceeds maximum allowed size");
            return -1;
        }
        memcpy(psk_identity_buff, psk_identity->info.buffer.data,
               psk_identity->info.buffer.data_size);
        psk_identity_len = psk_identity->info.buffer.data_size;
    } else {
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        if (anj_crypto_storage_resolve_security_info(
                    crypto_ctx, &psk_identity->info.external, psk_identity_buff,
                    MBEDTLS_PSK_MAX_LEN, &psk_identity_len)) {
            return -1;
        }
#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        mbedtls_log(L_ERROR, "Such PSK source is not supported");
        return -1;
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    }

    int res = mbedtls_ssl_conf_psk(ssl_conf,
                                   (const unsigned char *) psk_key_buff,
                                   psk_key_len,
                                   (const unsigned char *) psk_identity_buff,
                                   psk_identity_len);
    if (res) {
        mbedtls_log(L_ERROR, "Failed to set psk identity with %d", res);
        return -1;
    }

    static const int psk_suites[] = { ANJ_MBEDTLS_ALLOWED_CIPHERSUITES, 0 };
    mbedtls_ssl_conf_ciphersuites(ssl_conf, psk_suites);
    return 0;
}

static void internal_init(ssl_socket_t *secure_socket) {
    mbedtls_ssl_init(&secure_socket->ssl_ctx);
    mbedtls_ssl_config_init(&secure_socket->ssl_conf);
    secure_socket->sm_state = SOCKET_STATE_INITIAL;
}

static void internal_free(ssl_socket_t *secure_socket) {
    mbedtls_ssl_free(&secure_socket->ssl_ctx);
    mbedtls_ssl_config_free(&secure_socket->ssl_conf);
}

int anj_dtls_connect(anj_net_ctx_t *ctx_,
                     const char *hostname,
                     const char *port_str) {
    assert(ctx_ && hostname && port_str);
    ssl_socket_t *secure_socket = (ssl_socket_t *) ctx_;

    switch (secure_socket->sm_state) {
    case SOCKET_STATE_INITIAL: {
        int ret = anj_net_connect(ANJ_NET_BINDING_UDP, secure_socket->net,
                                  hostname, port_str);
        if (anj_net_is_inprogress(ret)) {
            return ret;
        } else if (!anj_net_is_ok(ret)) {
            return -1;
        }

        int res = mbedtls_ssl_config_defaults(&secure_socket->ssl_conf,
                                              MBEDTLS_SSL_IS_CLIENT,
                                              MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                              MBEDTLS_SSL_PRESET_DEFAULT);
        if (res) {
            mbedtls_log(L_ERROR, "Failed to set SSL config defaults with %d",
                        res);
            goto reset_config_and_ctx;
        }

        // we do not allow version negotiation, set it to the version we want
        mbedtls_ssl_conf_min_tls_version(&secure_socket->ssl_conf,
                                         ANJ_MBEDTLS_TLS_VERSION);
        mbedtls_ssl_conf_max_tls_version(&secure_socket->ssl_conf,
                                         ANJ_MBEDTLS_TLS_VERSION);
        if (load_psk(&secure_socket->ssl_conf, &secure_socket->secure_config)) {
            goto reset_config_and_ctx;
        }
        mbedtls_ssl_conf_rng(&secure_socket->ssl_conf, _anj_mbedtls_rng, NULL);

        mbedtls_ssl_conf_handshake_timeout(
                &secure_socket->ssl_conf,
                ANJ_MBEDTLS_HS_INITIAL_TIMEOUT_VALUE_MS,
                ANJ_MBEDTLS_HS_MAXIMUM_TIMEOUT_VALUE_MS);
#    if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
        if ((res = mbedtls_ssl_conf_cid(&secure_socket->ssl_conf, 0,
                                        MBEDTLS_SSL_UNEXPECTED_CID_IGNORE))) {
            mbedtls_log(L_ERROR, "mbedtls conf cid failed with %d", res);
            goto reset_config_and_ctx;
        }
#    endif // MBEDTLS_SSL_DTLS_CONNECTION_ID
        res = mbedtls_ssl_setup(&secure_socket->ssl_ctx,
                                &secure_socket->ssl_conf);
        if (res) {
            mbedtls_log(L_ERROR, "Failed to setup SSL context with %d", res);
            goto reset_config_and_ctx;
        }
#    if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
        if ((res = mbedtls_ssl_set_cid(&secure_socket->ssl_ctx,
                                       MBEDTLS_SSL_CID_ENABLED, NULL, 0))) {
            mbedtls_log(L_ERROR, "mbedtls set cid failed with %d", res);
            goto reset_config_and_ctx;
        }
#    endif // MBEDTLS_SSL_DTLS_CONNECTION_ID
        // integration may provide a custom implementation of
        // mbedtls_timing_set/get_delay
        mbedtls_ssl_set_timer_cb(&secure_socket->ssl_ctx, &secure_socket->timer,
                                 mbedtls_timing_set_delay,
                                 mbedtls_timing_get_delay);
        mbedtls_ssl_set_bio(&secure_socket->ssl_ctx, secure_socket,
                            _anj_mbedtls_bio_send, _anj_mbedtls_bio_recv, NULL);
        secure_socket->sm_state = SOCKET_STATE_HANDSHAKE_IN_PROGRESS;

        int32_t mtu = 0;
        if (anj_net_is_ok(anj_net_get_inner_mtu(ANJ_NET_BINDING_UDP,
                                                secure_socket->net, &mtu))
                && mtu > 0 && mtu <= UINT16_MAX) {
            mbedtls_ssl_set_mtu(&secure_socket->ssl_ctx, (uint16_t) mtu);
        }
    }
        // fallthrough
    case SOCKET_STATE_HANDSHAKE_IN_PROGRESS: {
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
            char peer_cid_hex[2 * sizeof(peer_cid) + 1] = "";
            for (size_t i = 0; i < peer_cid_len; ++i) {
                snprintf(peer_cid_hex + i * 2, 3, "%02X",
                         (unsigned) peer_cid[i]);
            }
            mbedtls_log(L_INFO, "negotiated CID = %s", peer_cid_hex);
        }
#    endif // MBEDTLS_SSL_DTLS_CONNECTION_ID
        secure_socket->state = ANJ_NET_SOCKET_STATE_CONNECTED;
        secure_socket->sm_state = SOCKET_STATE_HANDSHAKE_DONE;
        return ANJ_NET_OK;
    }
    case SOCKET_STATE_HANDSHAKE_DONE: {
        // Post-handshake state: anj_dtls_cleanup_ctx() has NOT been called.
        // If the close() was called, only UDP socket was closed, the next
        // connect() should then only reopen UDP and reuse the existing DTLS
        // session. Use anj_dtls_cleanup_ctx() if a full DTLS teardown is
        // required.
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
    ssl_socket_t *secure_socket = (ssl_socket_t *) malloc(sizeof(ssl_socket_t));
    if (!secure_socket) {
        return -1;
    }
    secure_socket->secure_config = config->secure_socket_config;
    secure_socket->last_recv_err = ANJ_NET_OK;
    secure_socket->last_send_err = ANJ_NET_OK;
    secure_socket->close_notify_sent = false;

    // Create UDP transport for DTLS
    if (!anj_net_is_ok(anj_net_create_ctx(ANJ_NET_BINDING_UDP,
                                          &secure_socket->net, config))) {
        free(secure_socket);
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

    ssl_socket_t *secure_socket = (ssl_socket_t *) ctx_;

    size_t already_sent = 0;
    while (already_sent < length) {
        int result =
                mbedtls_ssl_write(&secure_socket->ssl_ctx, buf + already_sent,
                                  length - already_sent);
        if (is_write_error(result)) {
            mbedtls_log(L_ERROR, "Failed to send data with error %d", result);
            return -1;
        }
        if (result > 0) {
            already_sent += (size_t) result;
            continue;
        }
        if (is_retry_result(result) && already_sent == 0) {
            mbedtls_log(L_DEBUG, "Transport busy, need to retry send");
            return secure_socket->last_send_err;
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
    ssl_socket_t *secure_socket = (ssl_socket_t *) ctx_;

    int result = mbedtls_ssl_read(&secure_socket->ssl_ctx, buf, length);
    if (is_retry_result(result)) {
        return secure_socket->last_recv_err;
    }
    if (result < 0) {
        mbedtls_log(L_ERROR, "Failed to receive data with error %d", result);
        return -1;
    }
    *bytes_received = (size_t) result;
    return ANJ_NET_OK;
}

int anj_dtls_close(anj_net_ctx_t *ctx_) {
    assert(ctx_);
    ssl_socket_t *secure_socket = (ssl_socket_t *) ctx_;

    int ret = anj_net_close(ANJ_NET_BINDING_UDP, secure_socket->net);
    if (anj_net_is_inprogress(ret)) {
        return ret;
    }
    secure_socket->state = ANJ_NET_SOCKET_STATE_CLOSED;
    if (!anj_net_is_ok(ret)) {
        return -1;
    }
    return ANJ_NET_OK;
}

int anj_dtls_cleanup_ctx(anj_net_ctx_t **ctx_) {
    assert(ctx_ && *ctx_);
    ssl_socket_t *secure_socket = (ssl_socket_t *) *ctx_;
    secure_socket->sm_state = SOCKET_STATE_INITIAL;
    int ret;

    if (!secure_socket->close_notify_sent) {
        // Send close_notify and flush DTLS state
        ret = mbedtls_ssl_close_notify(&secure_socket->ssl_ctx);
        if (is_retry_result(ret)) {
            return secure_socket->last_send_err;
        } else if (ret == 0) {
            secure_socket->close_notify_sent = true;
        }
    }

    ret = anj_net_cleanup_ctx(ANJ_NET_BINDING_UDP, &secure_socket->net);
    if (anj_net_is_inprogress(ret)) {
        return ret;
    }
    internal_free(secure_socket);
    free(secure_socket);
    *ctx_ = NULL;

    if (!anj_net_is_ok(ret)) {
        return -1;
    }
    return ANJ_NET_OK;
}

int anj_dtls_get_inner_mtu(anj_net_ctx_t *ctx, int32_t *out_value) {
    ssl_socket_t *secure_socket = (ssl_socket_t *) ctx;
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
    ssl_socket_t *secure_socket = (ssl_socket_t *) ctx_;
    *out_value = secure_socket->state;
    return ANJ_NET_OK;
}

int anj_dtls_queue_mode_rx_off(anj_net_ctx_t *ctx_) {
    assert(ctx_);
    ssl_socket_t *secure_socket = (ssl_socket_t *) ctx_;
    return anj_net_queue_mode_rx_off(ANJ_NET_BINDING_UDP, secure_socket->net);
}

#endif // ANJ_WITH_MBEDTLS && ANJ_NET_WITH_DTLS
