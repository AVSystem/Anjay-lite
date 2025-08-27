/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_NET_API_H
#    define ANJ_NET_API_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_SECURE_BINDINGS
// TODO: Remove references to avs_commons and include necessary
// structures in a header file inside Anjay Lite
#        include <avsystem/commons/avs_crypto_pki.h>
#        include <avsystem/commons/avs_crypto_psk.h>
#        include <avsystem/commons/avs_prng.h>
#    endif // ANJ_WITH_SECURE_BINDINGS

/**
 * A list of specific error codes recognized by Anjay. Any other generic error
 * must be indicated by a negative value.
 */

/**
 * Error code indicating success.
 */
#    define ANJ_NET_OK (0)

/**
 * Error code indicating that the operation would block. The caller should
 * retry the function with the same parameters.
 */
#    define ANJ_NET_EAGAIN (1)

/**
 * Message too long.
 */
#    define ANJ_NET_EMSGSIZE (2)

/**
 * Operation not supported. This indicates that the function is either not
 * implemented or that the provided arguments are not supported.
 */
#    define ANJ_NET_ENOTSUP (3)

typedef enum {
    ANJ_NET_BINDING_UDP = 0,
    ANJ_NET_BINDING_TCP,
    ANJ_NET_BINDING_DTLS,
    ANJ_NET_BINDING_TLS,
    ANJ_NET_BINDING_NON_IP
} anj_net_binding_type_t;

typedef enum {
    /**
     * Socket is either newly constructed, or it has been closed by calling
     * @ref anj_net_close.
     */
    ANJ_NET_SOCKET_STATE_CLOSED,

    /**
     * Socket was previously in either BOUND or CONNECTED state, but
     * @ref anj_net_shutdown was called.
     */
    ANJ_NET_SOCKET_STATE_SHUTDOWN,

    /**
     * @ref anj_net_reuse_last_port_t has been called. The socket is associated
     * with some port.
     */
    ANJ_NET_SOCKET_STATE_BOUND,

    /**
     * @ref anj_net_connect has been called. The socket is connected to
     * some concrete server. It is ready for @ref anj_net_send and
     * @ref anj_net_recv operations.
     */
    ANJ_NET_SOCKET_STATE_CONNECTED
} anj_net_socket_state_t;

#    ifdef ANJ_WITH_SECURE_BINDINGS
typedef struct {
    uint64_t min;
    uint64_t max;
} anj_net_dtls_handshake_timeouts_t;

typedef enum {
    ANJ_NET_SOCKET_DANE_CA_CONSTRAINT = 0,
    ANJ_NET_SOCKET_DANE_SERVICE_CERTIFICATE_CONSTRAINT = 1,
    ANJ_NET_SOCKET_DANE_TRUST_ANCHOR_ASSERTION = 2,
    ANJ_NET_SOCKET_DANE_DOMAIN_ISSUED_CERTIFICATE = 3
} anj_net_socket_dane_certificate_usage_t;

typedef enum {
    ANJ_NET_SOCKET_DANE_CERTIFICATE = 0,
    ANJ_NET_SOCKET_DANE_PUBLIC_KEY = 1
} anj_net_socket_dane_selector_t;

typedef enum {
    ANJ_NET_SOCKET_DANE_MATCH_FULL = 0,
    ANJ_NET_SOCKET_DANE_MATCH_SHA256 = 1,
    ANJ_NET_SOCKET_DANE_MATCH_SHA512 = 2
} anj_net_socket_dane_matching_type_t;

typedef struct {
    anj_net_socket_dane_certificate_usage_t certificate_usage;
    anj_net_socket_dane_selector_t selector;
    anj_net_socket_dane_matching_type_t matching_type;
    const void *association_data;
    size_t association_data_size;
} anj_net_socket_dane_tlsa_record_t;

typedef struct {
    const anj_net_socket_dane_tlsa_record_t *array_ptr;
    size_t array_element_count;
} anj_net_socket_dane_tlsa_array_t;
#    endif // ANJ_WITH_SECURE_BINDINGS

typedef enum {
    ANJ_NET_AF_SETTING_UNSPEC,
    ANJ_NET_AF_SETTING_FORCE_INET4,
    ANJ_NET_AF_SETTING_FORCE_INET6,
    ANJ_NET_AF_SETTING_PREFERRED_INET4,
    ANJ_NET_AF_SETTING_PREFERRED_INET6,
} anj_net_address_family_setting_t;

/**
 * Structure that contains additional configuration options for creating TCP and
 * UDP network sockets.
 */
typedef struct {
    /**
     * Sets the IP protocol version used for communication. Note that setting it
     * explicitly to <c>ANJ_NET_AF_SETTING_FORCE_IPV4</c> or
     * <c>ANJ_NET_AF_SETTING_FORCE_INET6</c> will result in limiting the socket
     * to support only addresses of that specific family. Using
     * <c>ANJ_NET_AF_SETTING_PREFERRED_INET4</c> or
     * <c>ANJ_NET_AF_SETTING_PREFERRED_INET6</c> may result in creating IPv4 or
     * IPv6 socket depending on the outcome of address resolution, while using
     * <c>ANJ_NET_AF_SETTING_UNSPEC</c> might be implementation specific.
     */
    anj_net_address_family_setting_t af_setting;
} anj_net_socket_configuration_t;

#    ifdef ANJ_WITH_SECURE_BINDINGS
/**
 * Available SSL versions that can be used by SSL sockets.
 */
typedef enum {
    ANJ_NET_SSL_VERSION_DEFAULT = 0,
    ANJ_NET_SSL_VERSION_TLSv1,
    ANJ_NET_SSL_VERSION_TLSv1_1,
    ANJ_NET_SSL_VERSION_TLSv1_2,
    ANJ_NET_SSL_VERSION_TLSv1_3
} anj_net_ssl_version_t;

typedef enum {
    ANJ_NET_SECURITY_DEFAULT = 0,
    ANJ_NET_SECURITY_PSK, /**< Pre-Shared Key */
    ANJ_NET_SECURITY_CERTIFICATE =
            ANJ_NET_SECURITY_DEFAULT /**< X509 Certificate + private key */
} anj_net_security_mode_t;

/**
 * A PSK/identity pair.
 */
typedef struct {
    avs_crypto_psk_key_info_t key;
    avs_crypto_psk_identity_info_t identity;
} anj_net_psk_info_t;

/**
 * Configuration for certificate-mode (D)TLS connection.
 */
typedef struct {
    /**
     * Enables validation of peer certificate chain. If disabled,
     * #ignore_system_trust_store and #trusted_certs are ignored.
     */
    bool server_cert_validation;

    /**
     * Setting this flag to true disables the usage of system-wide trust store
     * (e.g. <c>/etc/ssl/certs</c> on most Unix-like systems).
     *
     * NOTE: System-wide trust store is currently supported only by the OpenSSL
     * backend. This field is ignored by the Mbed TLS backend.
     */
    bool ignore_system_trust_store; // TODO: consider changing the default value

    /**
     * Enable use of DNS-based Authentication of Named Entities (DANE) if
     * possible.
     *
     * If this field is set to true, but #server_cert_validation is disabled,
     * "opportunistic DANE" is used.
     */
    bool dane;

    /**
     * Store of trust anchor certificates. This field is optional and can be
     * left zero-initialized. If used, it shall be initialized using one of the
     * <c>avs_crypto_certificate_chain_info_from_*</c> helper functions.
     */
    avs_crypto_certificate_chain_info_t trusted_certs;

    /**
     * Store of certificate revocation lists. This field is optional and can be
     * left zero-initialized. If used, it shall be initialized using one of the
     * <c>avs_crypto_cert_revocation_list_info_from_*</c> helper functions.
     */
    avs_crypto_cert_revocation_list_info_t cert_revocation_lists;

    /**
     * Local certificate chain to use for authenticating with the peer. This
     * field is optional and can be left zero-initialized. If used, it shall be
     * initialized using one of the
     * <c>avs_crypto_certificate_chain_info_from_*</c> helper functions.
     */
    avs_crypto_certificate_chain_info_t client_cert;

    /**
     * Private key matching #client_cert to use for authenticating with the
     * peer. This field is optional and can be left zero-initialized, unless
     * #client_cert is also specified. If used, it shall be initialized using
     * one of the <c>avs_crypto_private_key_info_from_*</c> helper functions.
     */
    avs_crypto_private_key_info_t client_key;

    /**
     * Enable rebuilding of client certificate chain based on certificates in
     * the trust store.
     *
     * If this field is set to <c>true</c>, and the last certificate in the
     * #client_cert chain is not self-signed, the library will attempt to find
     * its ancestors in #trusted_certs and append them to the chain presented
     * during handshake.
     */
    bool rebuild_client_cert_chain;
} anj_net_certificate_info_t;

typedef struct {
    anj_net_security_mode_t mode;
    union {
        anj_net_psk_info_t psk;
        anj_net_certificate_info_t cert;
    } data;
} anj_net_security_info_t;

typedef struct {
    /** Array of ciphersuite IDs, or NULL to enable all ciphers */
    uint32_t *ids;
    /** Number of elements in @ref anj_net_socket_tls_ciphersuites_t#ids */
    size_t num_ids;
} anj_net_socket_tls_ciphersuites_t;

typedef struct {
    /**
     * SSL/TLS version to use for communication.
     */
    anj_net_ssl_version_t version;

    /**
     * Security configuration (either PSK key or certificate information) to use
     * for communication.
     */
    anj_net_security_info_t security;

    /**
     * If non-NULL, can be used to customize DTLS handshake timeout limits.
     */
    const anj_net_dtls_handshake_timeouts_t *dtls_handshake_timeouts;

    /**
     * Buffer to use for (D)TLS session resumption (used if
     * <c>session_resumption_buffer_size</c> is non-zero).
     *
     * During @ref anj_net_connect, the library will attempt to load
     * session information from this buffer, and in case of success, will offer
     * that session to the server for resumption, allowing to maintain endpoint
     * association between connections.
     *
     * After a successful establishment, resumption or renegotiation of a
     * session, the buffer will be filled with the newly negotiated session
     * information.
     *
     * The buffer will also be always filled with zeroes in case of error, and
     * all the unused space will also be zeroed out after writing data, to allow
     * for e.g. size optimization when saving data to persistent storage.
     *
     * Session resumption support is currently only available through the mbed
     * TLS backend. Note that if support is added for other backends, the
     * session data format might not be compatible between backends. There is
     * rudimentary protection against attempting to read data in invalid format.
     */
    void *session_resumption_buffer;

    /**
     * Size of the buffer passed in <c>session_resumption_buffer</c>. Session
     * resumption support is enabled if nonzero. Must be zero if
     * <c>session_resumption_buffer</c> is NULL.
     *
     * Session data format used by the mbed TLS backend requires 112 bytes for
     * common data, and additional variable number of bytes for DER-formatted
     * X.509 peer certificate, if used.
     *
     * A buffer size of at least 1024 bytes is recommended to be able to store
     * most certificates.
     */
    size_t session_resumption_buffer_size;

    /**
     * An array of ciphersuite IDs, in big endian. For example,
     * TLS_PSK_WITH_AES_128_CCM_8 is represented as 0xC0A8.
     *
     * For a complete list of ciphersuites, see
     * https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml
     *
     * Note: cipher entries that are unsupported by the (D)TLS backend will be
     * silently ignored. An empty ciphersuite list (default) can be used to
     * enable all supported ciphersuites.
     *
     * A copy owned by the socket object is made, so it is not required for this
     * pointer to be valid after the call completes.
     */
    anj_net_socket_tls_ciphersuites_t ciphersuites;

    /**
     * Server Name Indication value to be used for certificate validation during
     * TLS handshake, or NULL if a default value shall be used (i.e. hostname to
     * which the connection is performed).
     *
     * The same value will also be used as DANE base domain if DANE is enabled.
     */
    const char *
            server_name_indication; // TODO: consider chaning the name of this
                                    // parameter, see CN/SAN:
                                    // https://confluence.avsystem.com/pages/viewpage.action?pageId=151233033

    /**
     * Enables / disables the use of DTLS connection_id extension (if
     * implemented by the backend). Note that it only works for DTLS sockets,
     * and has no effect on other socket types.
     */
    bool use_connection_id;

    /**
     * PRNG context to use. It must outlive the created socket. MUST NOT be
     * @p NULL if secure connection is used.
     */
    avs_crypto_prng_ctx_t *prng_ctx;
} anj_net_ssl_configuration_t;
#    endif // ANJ_WITH_SECURE_BINDINGS

/**
 * Structure that contains configuration for creating a network socket.
 *
 * A structure initialized with all zeroes (e.g. using <c>memset()</c>) is
 * a valid, default configuration - it is used when <c>NULL</c> is passed to
 * @ref anj_net_create_ctx , and may also be used as a starting point for
 * customizations.
 *
 * If \c secure_socket_config is initialized with all zeros the connection
 * will not use any kind of security.
 */
typedef struct {
    anj_net_socket_configuration_t raw_socket_config;
#    ifdef ANJ_WITH_SECURE_BINDINGS
    anj_net_ssl_configuration_t secure_socket_config;
#    endif // ANJ_WITH_SECURE_BINDINGS
} anj_net_config_t;

struct anj_net_ctx_struct;
typedef struct anj_net_ctx_struct anj_net_ctx_t;

static inline bool anj_net_is_ok(int res) {
    return ANJ_NET_OK == res;
}

static inline bool anj_net_is_again(int res) {
    return ANJ_NET_EAGAIN == res;
}

/**
 * Returns a pointer to the underlying system socket (e.g., for use with
 * functions like <c>select</c> or <c>poll</c>).
 *
 * Although Anjay does not call this function directly, it may be useful
 * for the end user. The caller should be aware of the system socket type
 * and cast it appropriately.
 *
 * If the system socket is not yet available or is invalid, this function
 * returns NULL.
 *
 * @code
 * const int *sock_fd = (const int*) anj_net_get_system_socket_t(ctx);
 * struct pollfd pfd;
 * pfd.fd = *sock_fd;
 * pfd.events = POLLIN;
 *
 * int ret = poll(&pfd, 1, timeout);
 * @endcode
 *
 * @param ctx Pointer to the socket context.
 *
 * @returns A pointer to the system socket on success, or NULL in case of an
 *          error.
 */
typedef const void *anj_net_get_system_socket_t(anj_net_ctx_t *ctx);

/**
 * Initializes a communication context for a connection.
 *
 * If a valid @p config pointer is supplied, it is used to configure the
 * context; otherwise, a NULL pointer is acceptable.
 *
 * This function never returns @ref ANJ_NET_EAGAIN.
 *
 * @param ctx     Output parameter to store the created socket context.
 * @param config  Optional configuration for initializing the socket context.
 *
 * @returns @ref ANJ_NET_OK on success, or a negative value in case of an error.
 */
typedef int anj_net_create_ctx_t(anj_net_ctx_t **ctx,
                                 const anj_net_config_t *config);

/**
 * Calls shutdown on the connection associated with @p ctx, cleans up the
 * @p ctx context, and sets it to NULL.
 *
 * If the function returns @ref ANJ_NET_EAGAIN, a subsequent call with the same
 * arguments resumes the operation. Note that such a call may not occur
 * immediately, as other operations may be performed first depending on internal
 * scheduling.
 *
 * @param[inout] ctx Connection context to clean up.
 *
 * @returns @ref ANJ_NET_OK on success.
 *          @ref ANJ_NET_EAGAIN if function would block.
 *          A negative value in case of an error.
 */
typedef int anj_net_cleanup_ctx_t(anj_net_ctx_t **ctx);

/**
 * This function connects to a server specified by the @p hostname and
 * @p port parameters. If the specific binding being used does not require
 * these parameters, the user should pass NULL instead.
 *
 * NOTE: The function is allowed to block during the connection attempt or
 * return immediately with @ref ANJ_NET_EAGAIN. The behavior of the function in
 * blocking or non-blocking mode will not be enforced, allowing flexibility in
 * implementation.
 *
 * If the function returns @ref ANJ_NET_EAGAIN, a subsequent call with the same
 * arguments resumes the operation. Note that such a call may not occur
 * immediately, as other operations may be performed first depending on internal
 * scheduling.
 *
 * @param ctx       Pointer to a socket context.
 * @param hostname  Target host name to connect to.
 * @param port      Target port of the server.
 *
 * @returns @ref ANJ_NET_OK on success.
 *          @ref ANJ_NET_EAGAIN if function would block.
 *          A negative value in case of an error.
 */
typedef int
anj_net_connect_t(anj_net_ctx_t *ctx, const char *hostname, const char *port);

/**
 * This function sends the provided data through the given connection context.
 *
 * If the operation would block and no data has been sent, the function returns
 * @ref ANJ_NET_EAGAIN. However, if all or partial data has been successfully
 * sent, the function returns @ref ANJ_NET_OK, and @p bytes_sent will indicate
 * the amount of data transmitted. The caller should then retry the operation
 * with the remaining data until all bytes are sent.
 *
 * If the function returns @ref ANJ_NET_EAGAIN, a subsequent call with the same
 * arguments resumes the operation. Note that such a call may not occur
 * immediately, as other operations may be performed first depending on internal
 * scheduling.
 *
 * NOTE: This function does not block.
 *
 * @param ctx         Pointer to a socket context.
 * @param bytes_sent  Output parameter indicating the number of bytes sent.
 * @param buf         Pointer to the message buffer.
 * @param length      Length of the data in the message buffer.
 *
 * @returns @ref ANJ_NET_OK if all data was sent or partial data was sent
 *          successfully.
 *          @ref ANJ_NET_EAGAIN if no data was sent and the operation would
 *          block.
 *          A negative value in case of an error.
 */
typedef int anj_net_send_t(anj_net_ctx_t *ctx,
                           size_t *bytes_sent,
                           const uint8_t *buf,
                           size_t length);

/**
 * This function receives data from the specified connection context. If data is
 * available, it is stored in the provided buffer, and @p bytes_received
 * indicates the number of bytes received.
 *
 * If the operation would block and no data has been received, the function
 * returns @ref ANJ_NET_EAGAIN. If the provided buffer is too small to hold the
 * full message, the function returns @ref ANJ_NET_EMSGSIZE.
 *
 * If the function returns @ref ANJ_NET_EAGAIN, a subsequent call with the same
 * arguments resumes the operation. Note that such a call may not occur
 * immediately, as other operations may be performed first depending on internal
 * scheduling.
 *
 * NOTE: This function does not block.
 *
 * @param ctx             Pointer to a socket context.
 * @param bytes_received  Output parameter indicating the number of bytes
 *                        received.
 * @param buf             Pointer to the message buffer.
 * @param length          Size of the provided buffer.
 *
 * @returns @ref ANJ_NET_OK on success.
 *          @ref ANJ_NET_EAGAIN if no data was received or the operation would
 *          block.
 *          @ref ANJ_NET_EMSGSIZE if the provided buffer is too small.
 *          Negative value in case of other errors.
 */
typedef int anj_net_recv_t(anj_net_ctx_t *ctx,
                           size_t *bytes_received,
                           uint8_t *buf,
                           size_t length);

/**
 * Binds a socket associated with @p ctx to the previous port number used by
 * this context.
 *
 * If bind operation is not supported the function returns @ref ANJ_NET_ENOTSUP.
 *
 * If the function returns @ref ANJ_NET_EAGAIN, a subsequent call with the same
 * arguments resumes the operation. Note that such a call may not occur
 * immediately, as other operations may be performed first depending on internal
 * scheduling.
 *
 * @param ctx Pointer to a socket context.
 *
 * @returns @ref ANJ_NET_OK on success.
 *          @ref ANJ_NET_EAGAIN if function would block.
 *          A negative value in case of an error.
 */
typedef int anj_net_reuse_last_port_t(anj_net_ctx_t *ctx);

/**
 * Shuts down the connection associated with @p ctx. No further communication is
 * allowed using this context. Any buffered but not yet processed data should
 * still be delivered. Performs the termination handshake if the protocol used
 * requires one.
 *
 * Data already received can still be read using @ref anj_net_recv. However, the
 * user must call @ref anj_net_close before reusing the context.
 *
 * If the function returns @ref ANJ_NET_EAGAIN, a subsequent call with the same
 * arguments resumes the operation. Note that such a call may not occur
 * immediately, as other operations may be performed first depending on internal
 * scheduling.
 *
 * @param ctx Communication context to shut down.
 *
 * @returns @ref ANJ_NET_OK on success.
 *          @ref ANJ_NET_EAGAIN if function would block.
 *          A negative value in case of an error.
 */
typedef int anj_net_shutdown_t(anj_net_ctx_t *ctx);

/**
 * Shuts down the connection associated with @p ctx. No further
 * communication is allowed using this context. Discards any buffered but not
 * yet processed data.
 *
 * If the function returns @ref ANJ_NET_EAGAIN, a subsequent call with the same
 * arguments resumes the operation. Note that such a call may not occur
 * immediately, as other operations may be performed first depending on internal
 * scheduling.
 *
 * @p ctx may later be reused by calling @ref anj_net_connect again.
 *
 * @param ctx Communication context to close.
 *
 * @returns @ref ANJ_NET_OK on success.
 *          @ref ANJ_NET_EAGAIN if function would block.
 *          A negative value in case of an error.
 */
typedef int anj_net_close_t(anj_net_ctx_t *ctx);

/**
 * Returns the current state of the socket context.
 *
 * @param ctx        Pointer to a socket context.
 * @param out_value  Variable to store the retrieved option value.
 *
 * @returns @ref ANJ_NET_OK on success, or a negative value in case of an error.
 */
typedef int anj_net_get_state_t(anj_net_ctx_t *ctx,
                                anj_net_socket_state_t *out_value);

/**
 * Returns the maximum size of a buffer that can be passed to
 * @ref anj_net_send and transmitted as a single packet.
 *
 * @param ctx        Pointer to a socket context.
 * @param out_value  Variable to store retrieved option value in.
 *
 * @returns @ref ANJ_NET_OK on success, or a negative value in case of an error.
 */
typedef int anj_net_get_inner_mtu_t(anj_net_ctx_t *ctx, int32_t *out_value);

/**
 * Returns the number of bytes sent. Does not include protocol overhead.
 *
 * @param ctx        Pointer to a socket context.
 * @param out_value  Variable to store the retrieved option value.
 *
 * @returns @ref ANJ_NET_OK on success, or a negative value in case of an error.
 */
typedef int anj_net_get_bytes_sent_t(anj_net_ctx_t *ctx, uint64_t *out_value);

/**
 * Returns the number of bytes received. Does not include protocol
 * overhead.
 *
 * @param ctx        Pointer to a socket context.
 * @param out_value  Variable to store the retrieved option value.
 *
 * @returns @ref ANJ_NET_OK on success, or a negative value in case of an error.
 */
typedef int anj_net_get_bytes_received_t(anj_net_ctx_t *ctx,
                                         uint64_t *out_value);

#    ifdef ANJ_WITH_SECURE_BINDINGS
// TODO To decide later if the following functions should return ANJ_NET_EAGAIN.
/**
 * Returns information whether the last (D)TLS handshake was a successful
 * session resumption - <c>true</c> if the session was resumed, or <c>false</c>
 * if it was a full handshake. If the socket is in any other state than
 * @ref AVS_NET_SOCKET_STATE_CONNECTED, the behaviour is undefined.
 *
 * If <c>session_resumption_buffer_size</c> field in
 * @ref anj_net_ssl_configuration_t is nonzero,
 * @ref anj_net_connect will attempt to resume the session that was previously
 * used before calling @ref anj_net_close. However, if it is not possible, a
 * normal handshake will be used instead and the whole call will still be
 * successful. This option makes it possible to check whether the session has
 * been resumed, or is a new unrelated one.
 *
 * @param ctx        Pointer to a socket context.
 * @param out_value  Variable to store retrieved option value in.
 *
 * @returns @ref ANJ_NET_OK on success, or a negative value in case of an error.
 */
typedef int anj_net_get_session_resumed_t(anj_net_ctx_t *ctx, bool *out_value);

/**
 * Sets an array of DANE TLSA records.
 *
 * The data is copied into the socket, and the value passed by the user may
 * be freed after a successful call.
 *
 * NOTE: Attempting to set this option on a socket that is not a (D)TLS
 * socket or is not configured to use DANE, will yield an error.
 *
 * @param ctx    Pointer to a socket context.
 * @param value  Pointer to a array of DANE TLSA records.
 *
 * @returns @ref ANJ_NET_OK on success, or a negative value in case of an error.
 */
typedef int
anj_net_set_dane_tlsa_array_t(anj_net_ctx_t *ctx,
                              anj_net_socket_dane_tlsa_array_t *value);

/**
 * Sets the timeouts for the DTLS handshake.
 *
 * @param ctx    Pointer to a socket context.
 * @param value  Pointer to a dtls timeout structure.
 *
 * @returns @ref ANJ_NET_OK on success, or a negative value in case of an error.
 */
typedef int
anj_net_set_dtls_handshake_timeouts_t(anj_net_ctx_t *ctx,
                                      anj_net_dtls_handshake_timeouts_t *value);

/**
 * Used to check whether the last call to @ref anj_net_connect
 * resulted in restoring a previously known DTLS Connection ID, instead of
 * performing a handshake. Sets the value to <c>true</c> if the Connection
 * ID was restored, or <c>false</c> if a handshake was performed. If the
 * socket is in any other state than @ref AVS_NET_SOCKET_STATE_CONNECTED,
 * the behaviour is undefined.
 *
 * @param ctx        Pointer to a socket context.
 * @param out_value  Variable to store retrieved option value in.
 *
 * @returns @ref ANJ_NET_OK on success, or a negative value in case of an error.
 */
typedef int anj_net_get_connection_id_resumed_t(anj_net_ctx_t *ctx,
                                                bool *out_value);

#    endif // ANJ_WITH_SECURE_BINDINGS

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_NET_API_H
