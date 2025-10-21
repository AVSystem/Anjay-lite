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
 * @brief API for downloading resources over CoAP/CoAPs.
 *
 * Provides a state machine—driven downloader that fetches resources via CoAP.
 * Applications configure it with callbacks, start a download from a URI, and
 * step it regularly in the main loop. Supports error reporting, termination,
 * and secure transport when enabled.
 */

#ifndef ANJ_COAP_DOWNLOADER_H
#    define ANJ_COAP_DOWNLOADER_H

#    include <anj/compat/net/anj_net_api.h>

#    include <anj/defs.h>

/** @cond */
#    define ANJ_INTERNAL_INCLUDE_EXCHANGE
#    include <anj_internal/exchange.h>
#    undef ANJ_INTERNAL_INCLUDE_EXCHANGE
/** @endcond */

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_COAP_DOWNLOADER

/** @defgroup anj_coap_downloader_errors CoAP downloader error codes.
 * Error values that may be returned from various callbacks and API of CoAP
 * downloader module.
 * @{
 **/

/**
 * Error code returned by @ref anj_coap_downloader_start and @ref
 * anj_coap_downloader_get_error when the provided URI is invalid.
 *
 * This error indicates that the URI format is incorrect or unsupported.
 */
#        define ANJ_COAP_DOWNLOADER_ERR_INVALID_URI -1

/**
 * Error code returned by @ref anj_coap_downloader_start when a download
 * operation is already in progress.
 *
 * To start a new download, you must first terminate the ongoing one using
 * @ref anj_coap_downloader_terminate, or wait until the current download
 * completes.
 */
#        define ANJ_COAP_DOWNLOADER_ERR_IN_PROGRESS -2

/**
 * Error code returned by @ref anj_coap_downloader_init when the provided
 * configuration is invalid or by @ref anj_coap_downloader_start when the URI
 * indicates CoAPs, and no @c net_config is provided.
 */
#        define ANJ_COAP_DOWNLOADER_ERR_INVALID_CONFIGURATION -3

/**
 * Error code returned by @ref anj_coap_downloader_get_error when the download
 * process was explicitly terminated by the user.
 */
#        define ANJ_COAP_DOWNLOADER_ERR_TERMINATED -4
/**
 * Error code returned by @ref anj_coap_downloader_get_error when a
 * network-related issue occurred during the download process.
 *
 * This may indicate a connection failure, unreachable server, or other
 * transport-layer error that prevented successful communication.
 */
#        define ANJ_COAP_DOWNLOADER_ERR_NETWORK -5

/**
 * Error code returned by @ref anj_coap_downloader_get_error when the server
 * response was invalid or unexpected.
 */
#        define ANJ_COAP_DOWNLOADER_ERR_INVALID_RESPONSE -6

/**
 * Error code returned by @ref anj_coap_downloader_get_error when the response
 * from the server was not received within the expected time window.
 */
#        define ANJ_COAP_DOWNLOADER_ERR_TIMEOUT -7

/**
 * Error code returned by @ref anj_coap_downloader_get_error or by
 * @ref anj_coap_downloader_init when an internal error occurred within the
 * CoAP downloader module.
 *
 * This may indicate a bug or unexpected condition that prevented the download
 * from completing successfully.
 */
#        define ANJ_COAP_DOWNLOADER_ERR_INTERNAL -8

/**
 * Error code returned by @ref anj_coap_downloader_get_error when the ETag
 * value in the server response does not match the expected value.
 *
 * It indicates that resource expired.
 */
#        define ANJ_COAP_DOWNLOADER_ERR_ETAG_MISMATCH -9

/**@}*/

/**
 * This enum represents the possible states of a CoAP Downloader module.
 */
typedef enum {
    /**
     * Initial state of the module after startup.
     *
     * Start new download by calling @ref anj_coap_downloader_start.
     */
    ANJ_COAP_DOWNLOADER_STATUS_INITIAL,

    /**
     * A new connection to the server is being established. The download will
     * begin once the connection is ready.
     */
    ANJ_COAP_DOWNLOADER_STATUS_STARTING,

    /**
     * A new chunk of data is being downloaded. This status will be reported via
     * callback for each received packet.
     */
    ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING,

    /**
     * Last packet has been received or error occurred. Connection is being
     * closed.
     */
    ANJ_COAP_DOWNLOADER_STATUS_FINISHING,

    /**
     * The download process has finished successfully. The last packet has been
     * received and the connection has been closed.
     */
    ANJ_COAP_DOWNLOADER_STATUS_FINISHED,

    /**
     * The download process has finished due to an error.
     *
     * The failure may be caused by a network issue, invalid server response,
     * or another unexpected condition. Call @ref anj_coap_downloader_get_error
     * to retrieve error details.
     */
    ANJ_COAP_DOWNLOADER_STATUS_FAILED,
} anj_coap_downloader_status_t;

/**
 * Callback type for CoAP downloader events.
 *
 * This callback is invoked whenever the CoAP downloader reports a new event,
 * such as a status change or the reception of a data packet.
 *
 * When the status is @ref ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING,
 * the callback will also provide the received data chunk.
 * For all other statuses, @p data will be @c NULL and @p data_len will be 0.
 *
 * @param arg             Opaque user argument passed to the callback.
 * @param coap_downloader CoAP downloader object reporting the status change.
 * @param conn_status     Current connection status value; see @ref
 *                        anj_coap_downloader_status_t.
 * @param data            Pointer to the received data, if applicable.
 * @param data_len        Length of the received data, if applicable.
 */
typedef void
anj_coap_downloader_event_callback_t(void *arg,
                                     anj_coap_downloader_t *coap_downloader,
                                     anj_coap_downloader_status_t conn_status,
                                     const uint8_t *data,
                                     size_t data_len);

/**
 * CoAP downloader configuration structure. Should be filled before passing to
 * @ref anj_coap_downloader_init.
 */
typedef struct anj_coap_downloader_configuration_struct {
    /**
     * Mandatory callback for monitoring CoAP downloader status changes.
     *
     * This callback will be invoked for each status change of the CoAP
     * downloader, and for each received packet.
     */
    anj_coap_downloader_event_callback_t *event_cb;

    /**
     * Opaque argument that will be passed to the function configured in the
     * @ref event_cb field.
     */
    void *event_cb_arg;

    /**
     * UDP transmission parameters, for LwM2M client requests. If @c NULL,
     * default values will be used.
     */
    const anj_exchange_udp_tx_params_t *udp_tx_params;
} anj_coap_downloader_configuration_t;

/**
 * Initializes CoAP downloader internal state variable.
 *
 * @param coap_downloader Pointer to a variable that will hold the state of CoAP
 *                        downloader.
 * @param config          Configuration structure for the CoAP downloader.
 *                        Object pointed to by this pointer can be freed after
 *                        this function returns, as it is copied internally.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int anj_coap_downloader_init(anj_coap_downloader_t *coap_downloader,
                             const anj_coap_downloader_configuration_t *config);

/**
 * Main step function of the Anjay Lite CoAP downloader module.
 *
 * This function should be called regularly in the main application loop. It
 * drives the internal state machine and handles all scheduled operations
 * related to downloading data via CoAP.
 *
 * This function is non-blocking, unless a custom network implementation
 * introduces blocking behavior.
 *
 * @param coap_downloader CoAP downloader state.
 */
void anj_coap_downloader_step(anj_coap_downloader_t *coap_downloader);

/**
 * Starts a new download operation.
 *
 * @warning The URI is not copied or stored internally by the CoAP downloader,
 *          so the pointer must remain valid throughout the entire download
 *          process.
 *
 * @param coap_downloader CoAP downloader state.
 * @param uri             URI of the resource to download.
 *                        The string must be null-terminated.
 * @param net_config      Optional network configuration to use for the
 *                        connection. Can be @c NULL for non-secure CoAP.
 *                        Object pointed to by this pointer can be freed after
 *                        this function returns, as it is copied internally.
 *
 * @return 0 on success,
 *         @ref ANJ_COAP_DOWNLOADER_ERR_INVALID_URI if the provided URI is
 *         invalid or unsupported.
 *         @ref ANJ_COAP_DOWNLOADER_ERR_IN_PROGRESS if a download is already
 *         in progress.
 *         @ref ANJ_COAP_DOWNLOADER_ERR_INVALID_CONFIGURATION if provided
 *         @p net_config is @c NULL while @p uri indicates secure CoAP.
 */
int anj_coap_downloader_start(anj_coap_downloader_t *coap_downloader,
                              const char *uri,
                              const anj_net_config_t *net_config);

/**
 * Terminates the current download operation and cleans up the CoAP downloader.
 *
 * This function should be called when the download is no longer needed
 * or when an error condition requires aborting the transfer.
 * It has effect only when the downloader is in the
 * @ref ANJ_COAP_DOWNLOADER_STATUS_STARTING or
 * @ref ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING state.
 * If called in any other state, the function has no effect.
 *
 * After calling this function, the downloader's status will be set to
 * @ref ANJ_COAP_DOWNLOADER_STATUS_FINISHING, and
 * @ref anj_coap_downloader_get_error will return
 * @ref ANJ_COAP_DOWNLOADER_ERR_TERMINATED.
 *
 * Subsequent calls to @ref anj_coap_downloader_step will complete
 * the termination sequence by closing the CoAP exchange and
 * terminating the connection. Once finished, the final status will be
 * @ref ANJ_COAP_DOWNLOADER_STATUS_FAILED.
 *
 * @param coap_downloader CoAP downloader state.
 */
void anj_coap_downloader_terminate(anj_coap_downloader_t *coap_downloader);

/**
 * Retrieves the error code from the last failed download operation.
 *
 * This function must be called only when the downloader's status is
 * @ref ANJ_COAP_DOWNLOADER_STATUS_FAILED. It returns a negative error code
 * indicating the reason for the failure. The behavior is undefined if this
 * function is called in any other state.
 *
 * @param coap_downloader CoAP downloader state.
 *
 * @return Error code indicating the reason for the failure. For possible return
 *         values, see @ref anj_coap_downloader_errors
 */
int anj_coap_downloader_get_error(anj_coap_downloader_t *coap_downloader);

/** @cond */
#        define ANJ_INTERNAL_INCLUDE_COAP_DOWNLOADER
#        include <anj_internal/coap_downloader.h>
#        undef ANJ_INTERNAL_INCLUDE_COAP_DOWNLOADER
/** @endcond */

#    endif // ANJ_WITH_COAP_DOWNLOADER

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_CORE_H
