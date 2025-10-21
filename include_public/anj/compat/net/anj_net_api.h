/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Platform hooks for network transport integration.
 *
 * This header declares the minimal socket-like API that platform integrators
 * must implement to let Anjay Lite communicate over UDP, TCP, DTLS, TLS or some
 * Non-IP transport.
 *
 * The API covers:
 * - creating/cleaning up opaque connection contexts
 * - connecting, sending, receiving, shutting down and closing
 * - querying state and MTU
 * - handling Queue Mode RX hints
 *
 * Security (PSK / certificates) is supported if @c ANJ_WITH_SECURITY is
 * enabled. The functions are designed to be non-blocking, with retry signaled
 * by returning @ref ANJ_NET_EINPROGRESS.
 *
 * Implementations typically wrap system sockets, modem AT command layers,
 * or RTOS/BSP networking stacks.
 */

#ifndef ANJ_NET_API_H
#    define ANJ_NET_API_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_SECURITY
#        include <anj/compat/rng.h>
#        include <anj/crypto.h>
#    endif // ANJ_WITH_SECURITY

/** @name Network API error codes
 *  Specific error codes recognized by Anjay. Any other generic error must be
 *  indicated by a negative value.
 */
/**@{*/

/**
 * Error code indicating success.
 */
#    define ANJ_NET_OK (0)

/**
 * A code indicating that no data is currently available to be received. The
 * caller should retry the function with the same parameters. This does not need
 * to occur immediately, other operations might be executed first.
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

/**
 * A code indicating that the operation cannot be performed immediately. This
 * may occur when the underlying operation is still in progress (e.g., ongoing
 * communication with a cellular modem). To properly complete the operation, the
 * caller must retry the function with the same parameters.
 */
#    define ANJ_NET_EINPROGRESS (4)

/**@}*/

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
     * Socket was previously in @ref ANJ_NET_SOCKET_STATE_CONNECTED state, and
     * @ref anj_net_shutdown was called.
     */
    ANJ_NET_SOCKET_STATE_SHUTDOWN,

    /**
     * @ref anj_net_connect has been called. The socket is connected to some
     * specific server. It is ready for @ref anj_net_send and @ref anj_net_recv
     * operations.
     */
    ANJ_NET_SOCKET_STATE_CONNECTED
} anj_net_socket_state_t;

/**
 * Address family preference when creating a network socket.
 */
typedef enum {
    /**
     * No preference specified.
     * The actual address family is chosen by the implementation.
     */
    ANJ_NET_AF_SETTING_UNSPEC,

    /**
     * Force IPv4.
     * Only IPv4 addresses will be supported.
     */
    ANJ_NET_AF_SETTING_FORCE_INET4,

    /**
     * Force IPv6.
     * Only IPv6 addresses will be supported.
     */
    ANJ_NET_AF_SETTING_FORCE_INET6,

    /**
     * Prefer IPv4, but allow IPv6.
     * The final choice depends on address resolution and system behavior.
     */
    ANJ_NET_AF_SETTING_PREFERRED_INET4,

    /**
     * Prefer IPv6, but allow IPv4.
     * The final choice depends on address resolution and system behavior.
     */
    ANJ_NET_AF_SETTING_PREFERRED_INET6
} anj_net_address_family_setting_t;

/**
 * Additional configuration options for creating TCP or UDP sockets.
 */
typedef struct {
    /**
     * Address family selection policy. See
     * @ref anj_net_address_family_setting_t for details.
     */
    anj_net_address_family_setting_t af_setting;
} anj_net_socket_configuration_t;

#    ifdef ANJ_WITH_SECURITY
typedef enum {
    ANJ_NET_SECURITY_PSK,
    ANJ_NET_SECURITY_CERTIFICATE,
} anj_net_security_mode_t;

typedef struct {
    anj_crypto_security_info_t key;
    anj_crypto_security_info_t identity;
} anj_net_psk_info_t;

typedef struct {
    void *empty;
} anj_net_certificate_info_t;

typedef struct {
    anj_net_security_mode_t mode;
    union {
        anj_net_psk_info_t psk;
        anj_net_certificate_info_t cert;
    } data;
} anj_net_security_info_t;

typedef struct {
    /**
     * Security configuration (either PSK key or certificate information) to use
     * for communication.
     */
    anj_net_security_info_t security;

#        ifdef ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
    /**
     * Cryptographic context to use.
     */
    void *crypto_ctx;
#        endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
} anj_net_ssl_configuration_t;
#    endif // ANJ_WITH_SECURITY

/**
 * Structure that contains configuration for creating a network socket.
 *
 * A structure initialized with all zeroes (e.g. using @c memset()) is a valid,
 * default configuration - it is used when @c NULL is passed to
 * @ref anj_net_create_ctx, and may also be used as a starting point for
 * customizations.
 *
 * If @c secure_socket_config is initialized with all zeros the connection
 * will not use any kind of security.
 */
typedef struct {
    anj_net_socket_configuration_t raw_socket_config;
#    ifdef ANJ_WITH_SECURITY
    anj_net_ssl_configuration_t secure_socket_config;
#    endif // ANJ_WITH_SECURITY
} anj_net_config_t;

struct anj_net_ctx_struct;

/**
 * @brief Opaque network context handle.
 *
 * This type represents a network context object used internally by the
 * networking layer. Its definition is intentionally hidden and implementation-
 * specific. Applications should only work with pointers to @c anj_net_ctx_t
 * obtained from API functions such as @ref anj_net_create_ctx, and never
 * attempt to access or modify its contents directly.
 *
 * On the implementation side, @c anj_net_ctx_t is cast to a backend-specific
 * structure.
 */
typedef struct anj_net_ctx_struct anj_net_ctx_t;

/**
 * @brief Check if a network operation succeeded.
 *
 * Convenience function that tests whether the result code of a network
 * operation equals @c ANJ_NET_OK.
 *
 * @param res Result code returned by a networking API call.
 * @return @c true if @p res indicates success, @c false otherwise.
 */
static inline bool anj_net_is_ok(int res) {
    return ANJ_NET_OK == res;
}

/**
 * @brief Check if data was received.
 *
 * Convenience function that tests whether the result code of a network
 * operation equals @c ANJ_NET_EAGAIN.
 *
 * @param res Result code returned by a networking API call.
 * @return @c true if @p res equals ANJ_NET_EAGAIN, @c false
 * otherwise.
 */
static inline bool anj_net_is_again(int res) {
    return ANJ_NET_EAGAIN == res;
}

/**
 * @brief Check if a network operation is in progress.
 *
 * Convenience function that tests whether the result code of a network
 * operation equals @c ANJ_NET_EINPROGRESS.
 *
 * This indicates thst the operation requires a repeated call, without any other
 * anj_net_* calls being performed in between.
 *
 * @param res Result code returned by a networking API call.
 * @return @c true if @p res indicates a retryable condition, @c false
 * otherwise.
 */
static inline bool anj_net_is_inprogress(int res) {
    return ANJ_NET_EINPROGRESS == res;
}

/**
 * Initializes a communication context for a connection.
 *
 * If a valid @p config pointer is supplied, it is used to configure the
 * context; otherwise, a @c NULL pointer is acceptable.
 *
 * @note This function does not block.
 *
 * @param[out] ctx    Created socket context.
 * @param      config Optional configuration for initializing the socket
 *                    context.
 *
 * @return @ref ANJ_NET_OK on success.
 *         Other non-zero value in case of other errors.
 */
typedef int anj_net_create_ctx_t(anj_net_ctx_t **ctx,
                                 const anj_net_config_t *config);

/**
 * Calls shutdown on the connection associated with @p ctx, cleans up the
 * @p ctx context, and sets it to @c NULL.
 *
 * @note This function does not block.
 *
 * @param[inout] ctx Connection context to clean up.
 *
 * @return @ref ANJ_NET_OK          on success.
 *         @ref ANJ_NET_EINPROGRESS
 *         Other non-zero value in case of other errors.
 */
typedef int anj_net_cleanup_ctx_t(anj_net_ctx_t **ctx);

/**
 * This function connects to a server specified by the @p hostname and
 * @p port parameters. If the specific binding being used does not require these
 * parameters, the user should pass @c NULL instead.
 *
 * @warning The function is allowed to block during the connection attempt or
 *          return immediately with @ref ANJ_NET_EINPROGRESS. The behavior of
 *          the function in blocking or non-blocking mode will not be enforced,
 *          allowing flexibility in implementation.
 *
 * @param ctx      Pointer to a socket context.
 * @param hostname Target host name to connect to.
 * @param port     Target port of the server.
 *
 * @return @ref ANJ_NET_OK          on success.
 *         @ref ANJ_NET_EINPROGRESS
 *         Other non-zero value in case of an error.
 */
typedef int
anj_net_connect_t(anj_net_ctx_t *ctx, const char *hostname, const char *port);

/**
 * This function sends the provided data through the given connection context.
 *
 * If no data has been sent, the function returns @ref ANJ_NET_EINPROGRESS.
 * However, if some (all) data has been successfully sent, the function returns
 * @ref ANJ_NET_OK, and @p bytes_sent will indicate the amount of data
 * transmitted. The caller should then retry the operation with the remaining
 * data until all bytes are sent.
 *
 * @note This function does not block.
 *
 * @param      ctx         Pointer to a socket context.
 * @param[out] bytes_sent  Number of bytes sent.
 * @param      buf         Pointer to the message buffer.
 * @param      length      Length of the data in the message buffer.
 *
 * @return @ref ANJ_NET_OK if all data was sent or partial data was sent
 *         successfully.
 *         @ref ANJ_NET_EINPROGRESS
 *         Other non-zero value in case of an error.
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
 * @note This function does not block.
 *
 * @param      ctx            Pointer to a socket context.
 * @param[out] bytes_received Output parameter indicating the number of bytes
 *                            received.
 * @param[out] buf            Pointer to the message buffer.
 * @param      length         Size of the provided buffer.
 *
 * @return @ref ANJ_NET_OK          if data was received successfully.
 *         @ref ANJ_NET_EAGAIN
 *         @ref ANJ_NET_EMSGSIZE    if the provided buffer is too small.
 *         @ref ANJ_NET_EINPROGRESS
 *         Other non-zero value in case of other errors.
 */
typedef int anj_net_recv_t(anj_net_ctx_t *ctx,
                           size_t *bytes_received,
                           uint8_t *buf,
                           size_t length);

/**
 * Shuts down the connection associated with @p ctx. No further communication is
 * allowed using this context. Any buffered but not yet processed data should
 * still be delivered. Performs the termination handshake if the protocol used
 * requires one.
 *
 * Data already received can still be read using @ref anj_net_recv. However, the
 * user must call @ref anj_net_close before reusing the context.
 *
 * @note This function does not block.
 *
 * @param ctx Communication context to shut down.
 *
 * @return @ref ANJ_NET_OK          on success.
 *         @ref ANJ_NET_EINPROGRESS
 *         Other non-zero value in case of an error.
 */
typedef int anj_net_shutdown_t(anj_net_ctx_t *ctx);

/**
 * Shuts down the connection associated with @p ctx. No further communication is
 * allowed using this context. Discards any buffered but not yet processed data.
 *
 * @p ctx may later be reused by calling @ref anj_net_connect again.
 *
 * @note This function does not block.
 *
 * @param ctx Communication context to close.
 *
 * @return @ref ANJ_NET_OK          on success.
 *         @ref ANJ_NET_EINPROGRESS
 *         Other non-zero value in case of other errors.
 */
typedef int anj_net_close_t(anj_net_ctx_t *ctx);

/**
 * Returns the current state of the socket context.
 *
 * @note This function does not block.
 *
 * @param      ctx       Pointer to a socket context.
 * @param[out] out_value Retrieved option value.
 *
 * @return @ref ANJ_NET_OK on success.
 *         Other non-zero value in case of other errors.
 */
typedef int anj_net_get_state_t(anj_net_ctx_t *ctx,
                                anj_net_socket_state_t *out_value);

/**
 * Returns the maximum size of a buffer that can be passed to @ref anj_net_send
 * and transmitted as a single packet.
 *
 * @note This function does not block.
 *
 * @param      ctx       Pointer to a socket context.
 * @param[out] out_value Retrieved option value.
 *
 * @return @ref ANJ_NET_OK on success.
 *         Other non-zero value in case of other errors.
 */
typedef int anj_net_get_inner_mtu_t(anj_net_ctx_t *ctx, int32_t *out_value);

/**
 * Hints the transport that the client is entering LwM2M Queue Mode and
 * **will not need to receive** application data until it initiates the next
 * outgoing exchange. Implementations may use this to reduce power consumption
 * (e.g., disable RX path / radio) while keeping the connection context valid.
 *
 * @note This function does not block.
 *
 * Requirements:
 * - Must not invalidate @p ctx. Subsequent calls to @ref anj_net_send /
 *   @ref anj_net_recv and others must still be possible.
 * - Should be idempotent: calling it multiple times is allowed.
 * - Net compat implementation must implicitly re-enable RX whenever
 *   a subsequent net API call may require inbound traffic (e.g.,
 *   @ref anj_net_connect, @ref anj_net_send, @ref anj_net_recv,
 *   and @ref anj_net_close / @ref anj_net_shutdown for TCP/TLS).
 * - Once RX is re-enabled, it remains enabled for the connection until
 *   @ref anj_net_queue_mode_rx_off_t is called again.
 *
 * Non-blocking semantics:
 * - If disabling RX requires asynchronous work (e.g., modem operations),
 *   return @ref ANJ_NET_EINPROGRESS and complete the operation on subsequent
 *   calls.
 *
 * @param ctx Pointer to a transport context.
 *
 * @return @ref ANJ_NET_OK          if RX successfully disabled, or not
 *                                  applicable (no-op).
 *         @ref ANJ_NET_EINPROGRESS
 *         Other non-zero value in case of other errors.
 */
typedef int anj_net_queue_mode_rx_off_t(anj_net_ctx_t *ctx);

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_NET_API_H
