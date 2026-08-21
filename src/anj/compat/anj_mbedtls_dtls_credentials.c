/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "../init_internal.h"

#define ANJ_LOG_SOURCE_FILE_ID 66

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

#    include <mbedtls/error.h>
#    include <mbedtls/platform.h>
#    include <mbedtls/ssl.h>

#    ifdef ANJ_WITH_CERTIFICATES
#        include <mbedtls/pk.h>
#        include <mbedtls/x509_crt.h>
#    endif // ANJ_WITH_CERTIFICATES

#    include "anj_mbedtls_dtls_internal.h"

#    ifdef ANJ_WITH_CERTIFICATES

#        ifdef MBEDTLS_PEM_PARSE_C
static int append_null_sign_to_buffer(char *in_out,
                                      size_t *buff_len,
                                      size_t max_buff_len) {
    // Maybe it's a PEM format without a terminating '\0' that
    // mbedtls_x509_crt_parse() / mbedtls_pk_parse_key() requires - let's try
    // adding it.
    mbedtls_log(L_WARNING,
                "key/cert may be PEM without trailing '\\0', appending it");
    if (*buff_len == max_buff_len) {
        mbedtls_log(L_ERROR,
                    "Buffer too small to append terminating NULL sign");
        return -1;
    }
    in_out[*buff_len] = '\0';
    (*buff_len)++;
    return 0;
}
#        endif // MBEDTLS_PEM_PARSE_C

static int load_security_info_to_buffer(anj_net_ssl_configuration_t *config,
                                        const anj_crypto_security_info_t *info,
                                        char *out_buf,
                                        size_t out_buf_size,
                                        size_t *out_buf_len,
                                        const char *description) {
    (void) config;
    if (info->source == ANJ_CRYPTO_DATA_SOURCE_BUFFER) {
        if (info->info.buffer.data_size > out_buf_size) {
            mbedtls_log(L_ERROR, "%s size exceeds maximum allowed size",
                        description);
            return -1;
        }
        assert(info->info.buffer.data != NULL);
        memcpy(out_buf, info->info.buffer.data, info->info.buffer.data_size);
        *out_buf_len = info->info.buffer.data_size;
        return 0;
    }

#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    if (info->source == ANJ_CRYPTO_DATA_SOURCE_EXTERNAL) {
        *out_buf_len = 0;
        if (anj_crypto_storage_resolve_security_info(
                    config->crypto_ctx, &info->info.external, out_buf,
                    out_buf_size, out_buf_len)) {
            mbedtls_log(L_ERROR, "Failed to load %s from external storage",
                        description);
            return -1;
        }
        return 0;
    }
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

    mbedtls_log(L_ERROR, "%s source is not supported", description);
    return -1;
}

static int parse_crt_from_buffer(mbedtls_x509_crt *crt,
                                 char *buf,
                                 size_t buf_size,
                                 size_t buf_len,
                                 const char *description) {
    (void) buf_size;

    int res = mbedtls_x509_crt_parse(crt, (const unsigned char *) buf, buf_len);
    if (res != 0) {
#        ifdef MBEDTLS_PEM_PARSE_C
        // Maybe it's a PEM format without a terminating '\0' that
        // mbedtls_x509_crt_parse() requires - let's try adding it.
        if (res == MBEDTLS_ERR_X509_INVALID_FORMAT && buf_len > 0
                && ((const char *) buf)[buf_len - 1] != '\0') {
            if (append_null_sign_to_buffer(buf, &buf_len, buf_size)) {
                return -1;
            }
            res = mbedtls_x509_crt_parse(crt, (const unsigned char *) buf,
                                         buf_len);
        }
#        endif // MBEDTLS_PEM_PARSE_C
        if (res != 0) {
#        ifdef MBEDTLS_ERROR_C
            mbedtls_log(L_ERROR, "Failed to parse %s: %s", description,
                        mbedtls_high_level_strerr(res));
#        else  // MBEDTLS_ERROR_C
            mbedtls_log(L_ERROR, "Failed to parse %s with %d", description,
                        res);
#        endif // MBEDTLS_ERROR_C
            return -1;
        }
    }
    return 0;
}

#        ifndef ANJ_WITH_EXTERNAL_SIGNING
static int parse_key_from_buffer(mbedtls_pk_context *key,
                                 char *buff,
                                 size_t buff_size,
                                 size_t buff_len,
                                 const char *description) {
    (void) buff_size;

    int res = mbedtls_pk_parse_key(key, (const unsigned char *) buff, buff_len,
                                   NULL, 0, _anj_mbedtls_rng, NULL);
    if (res != 0) {
#            ifdef MBEDTLS_PEM_PARSE_C
        // Maybe it's a PEM format without a terminating '\0' that
        // mbedtls_pk_parse_key() requires - let's try adding it.
        if (res == MBEDTLS_ERR_PK_KEY_INVALID_FORMAT && buff_len > 0
                && ((const char *) buff)[buff_len - 1] != '\0') {
            if (append_null_sign_to_buffer(buff, &buff_len, buff_size)) {
                return -1;
            }
            res = mbedtls_pk_parse_key(key, (const unsigned char *) buff,
                                       buff_len, NULL, 0, _anj_mbedtls_rng,
                                       NULL);
        }
#            endif // MBEDTLS_PEM_PARSE_C
        if (res != 0) {
            mbedtls_log(L_ERROR, "Failed to parse %s with %d", description,
                        res);
            return -1;
        }
    }
    return 0;
}
#        endif // ANJ_WITH_EXTERNAL_SIGNING

static int parse_key_and_certs(mbedtls_x509_crt *client_cert,
                               mbedtls_pk_context *client_key,
                               mbedtls_x509_crt *server_cert,
                               mbedtls_x509_crt *trust_store,
                               anj_net_ssl_configuration_t *config) {
    int result = -1;
    // Make buffer big enough to hold either public key, private key, or server
    // certificate so we can reuse it.
    char buff[ANJ_MAX(
            ANJ_MAX(ANJ_MAX(ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE,
                            ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE),
                    ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE),
            ANJ_MBEDTLS_MAX_TRUST_STORE_CERTIFICATE_SIZE)];
    size_t buff_len = 0;

    anj_net_security_info_t *security = &config->security;
    anj_net_certificate_info_t *cert = &security->data.cert;

    if (load_security_info_to_buffer(config, &cert->client_cert, buff,
                                     sizeof(buff), &buff_len,
                                     "client certificate")
            || parse_crt_from_buffer(client_cert, buff, sizeof(buff), buff_len,
                                     "client certificate")) {
        goto cleanup;
    }

#        ifndef ANJ_WITH_EXTERNAL_SIGNING
    if (load_security_info_to_buffer(config, &cert->private_key, buff,
                                     sizeof(buff), &buff_len, "private key")
            || parse_key_from_buffer(client_key, buff, sizeof(buff), buff_len,
                                     "private key")) {
        goto cleanup;
    }
#        else  // ANJ_WITH_EXTERNAL_SIGNING
    assert(cert->private_key.source == ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    if (anj_crypto_storage_resolve_external_private_key_info(
                config->crypto_ctx, &cert->private_key.info.external,
                (void *) client_key)) {
        mbedtls_log(L_ERROR, "Failed to resolve private key info");
        goto cleanup;
    }
#        endif // ANJ_WITH_EXTERNAL_SIGNING

    if (cert->server_cert.source != ANJ_CRYPTO_DATA_SOURCE_EMPTY) {
        if (load_security_info_to_buffer(config, &cert->server_cert, buff,
                                         sizeof(buff), &buff_len,
                                         "server certificate")
                || parse_crt_from_buffer(server_cert, buff, sizeof(buff),
                                         buff_len, "server certificate")) {
            goto cleanup;
        }
        if (server_cert->next) {
            mbedtls_log(
                    L_ERROR,
                    "Server certificate must contain exactly one certificate");
            goto cleanup;
        }
    }
    if (cert->certificate_usage == ANJ_NET_CERTIFICATE_CA_CONSTRAINT
            || cert->certificate_usage
                           == ANJ_NET_CERTIFICATE_SERVICE_CERTIFICATE_CONSTRAINT) {
        for (size_t i = 0; i < cert->trust_store.ca_certs_count; i++) {
            if (load_security_info_to_buffer(
                        config, &cert->trust_store.ca_certs[i], buff,
                        sizeof(buff), &buff_len, "trust store certificate")
                    || parse_crt_from_buffer(trust_store, buff, sizeof(buff),
                                             buff_len,
                                             "trust store certificate")) {
                goto cleanup;
            }
        }
    }

    result = 0;
cleanup:
    mbedtls_platform_zeroize(buff, sizeof(buff));
    // In case of failure mbedtls_x509_crt_free() and mbedtls_pk_free() will be
    // called immediately after this function returns in internal_free()
    return result;
}

static int load_certificate(_anj_mbedtls_ssl_socket_t *secure_socket) {
    anj_net_security_info_t *security = &secure_socket->secure_config.security;
    anj_net_certificate_info_t *cert_info = &security->data.cert;

    if (cert_info->client_cert.source == ANJ_CRYPTO_DATA_SOURCE_EMPTY
#        ifndef ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP
            || cert_info->server_cert.source == ANJ_CRYPTO_DATA_SOURCE_EMPTY
#        endif // ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP
            || cert_info->private_key.source == ANJ_CRYPTO_DATA_SOURCE_EMPTY) {
        mbedtls_log(L_ERROR, "X.509 certificate or private key not set");
        return -1;
    }

    if (parse_key_and_certs(&secure_socket->client_cert,
                            &secure_socket->client_key,
                            &secure_socket->server_cert,
                            &secure_socket->ca_certs,
                            &secure_socket->secure_config)) {
        return -1;
    }

    int res = mbedtls_ssl_conf_own_cert(&secure_socket->ssl_conf,
                                        &secure_socket->client_cert,
                                        &secure_socket->client_key);
    if (res) {
        mbedtls_log(L_ERROR, "Failed to set own cert with %d", res);
        return -1;
    }
    return 0;
}
#    endif // ANJ_WITH_CERTIFICATES

static int load_psk(mbedtls_ssl_config *ssl_conf,
                    anj_net_ssl_configuration_t *config) {
    int result = -1;
    char psk_key_buff[ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE];
    size_t psk_key_len;
    char psk_identity_buff[ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE];
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
        if (psk_key->info.buffer.data_size > ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE) {
            mbedtls_log(L_ERROR, "PSK key size exceeds maximum allowed size");
            goto cleanup;
        }
        assert(psk_key->info.buffer.data != NULL);
        memcpy(psk_key_buff, psk_key->info.buffer.data,
               psk_key->info.buffer.data_size);
        psk_key_len = psk_key->info.buffer.data_size;
    } else {
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        assert(psk_key->source == ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
        if (anj_crypto_storage_resolve_security_info(
                    crypto_ctx, &psk_key->info.external, psk_key_buff,
                    ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE, &psk_key_len)) {
            goto cleanup;
        }
#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        mbedtls_log(L_ERROR, "Such PSK source is not supported");
        goto cleanup;
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    }

    if (psk_identity->source == ANJ_CRYPTO_DATA_SOURCE_BUFFER) {
        if (psk_identity->info.buffer.data_size
                > ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE) {
            mbedtls_log(L_ERROR,
                        "PSK identity size exceeds maximum allowed size");
            goto cleanup;
        }
        assert(psk_identity->info.buffer.data != NULL);
        memcpy(psk_identity_buff, psk_identity->info.buffer.data,
               psk_identity->info.buffer.data_size);
        psk_identity_len = psk_identity->info.buffer.data_size;
    } else {
#    ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        assert(psk_identity->source == ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
        if (anj_crypto_storage_resolve_security_info(
                    crypto_ctx, &psk_identity->info.external, psk_identity_buff,
                    ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE,
                    &psk_identity_len)) {
            goto cleanup;
        }
#    else  // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
        mbedtls_log(L_ERROR, "Such PSK source is not supported");
        goto cleanup;
#    endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    }

    result = mbedtls_ssl_conf_psk(ssl_conf,
                                  (const unsigned char *) psk_key_buff,
                                  psk_key_len,
                                  (const unsigned char *) psk_identity_buff,
                                  psk_identity_len);
    if (result) {
        mbedtls_log(L_ERROR, "Failed to set psk identity with %d", result);
        goto cleanup;
    }

    result = 0;
cleanup:
    mbedtls_platform_zeroize(psk_key_buff, sizeof(psk_key_buff));
    mbedtls_platform_zeroize(psk_identity_buff, sizeof(psk_identity_buff));
    return result;
}

int _anj_mbedtls_load_security_credentials(
        _anj_mbedtls_ssl_socket_t *secure_socket) {
#    ifdef ANJ_WITH_CERTIFICATES
    if (secure_socket->secure_config.security.mode
            == ANJ_NET_SECURITY_CERTIFICATE) {
        return load_certificate(secure_socket);
    }
#    endif // ANJ_WITH_CERTIFICATES

    return load_psk(&secure_socket->ssl_conf, &secure_socket->secure_config);
}

#endif // ANJ_WITH_MBEDTLS && ANJ_NET_WITH_DTLS
