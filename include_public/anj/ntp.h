/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief API for Network Time Protocol (NTP) client functionality.
 *
 * This module provides functions to synchronize the system time with NTP
 * servers and adds support for the standard LwM2M NTP object (OID 3415).
 * It allows initializing the NTP client, starting time synchronization, and
 * retrieving the current time. Logic is based on callbacks and state machine,
 * suitable for integration into the main application loop.
 */

#ifndef ANJ_NTP_H
#    define ANJ_NTP_H

#    include <anj/compat/net/anj_net_api.h>

#    include <anj/defs.h>

#    ifdef ANJ_WITH_PERSISTENCE
#        include <anj/persistence.h>
#    endif // ANJ_WITH_PERSISTENCE

/** @cond */
#    define ANJ_INTERNAL_INCLUDE_EXCHANGE
#    include <anj_internal/exchange.h>
#    undef ANJ_INTERNAL_INCLUDE_EXCHANGE
/** @endcond */

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_NTP

/** @defgroup anj_ntp_errors NTP client error codes.
 * @{
 **/

/**
 * Error code returned by @ref anj_ntp_start when a synchronization operation is
 * already in progress.
 *
 * To start a new synchronization, you must first wait until the ongoing one
 * completes.
 */
#        define ANJ_NTP_ERR_IN_PROGRESS -1

/**
 * Error code returned by @ref anj_ntp_init when the provided configuration is
 * invalid.
 */
#        define ANJ_NTP_ERR_CONFIGURATION -2

/**
 * Error code returned by @ref anj_ntp_init when the NTP object creation in
 * the Anjay instance failed.
 */
#        define ANJ_NTP_ERR_OBJECT_CREATION_FAILED -3

/**@}*/

/**
 * This enum represents the possible states of a NTP module.
 */
typedef enum {
    /**
     * Initial state of the module after startup.
     *
     * Start new NTP synchronization by calling @ref anj_ntp_start.
     */
    ANJ_NTP_STATUS_INITIAL,

    /**
     * The time since the last synchronization (successful or not) exceeds the
     * `NTP period` resource value. NTP synchronization should be started. Call
     * @ref anj_ntp_start to initiate synchronization.
     */
    ANJ_NTP_STATUS_PERIOD_EXCEEDED,

    /**
     * NTP synchronization is started and is currently in progress.
     */
    ANJ_NTP_STATUS_IN_PROGRESS,

    /**
     * NTP synchronization has finished successfully.
     * New time is provided through callback. `Time sync error` resource is set
     * to false. `Last time sync` resource is updated.
     */
    ANJ_NTP_STATUS_FINISHED_SUCCESSFULLY,

    /**
     * NTP synchronization has finished with error.
     * New time is not available. `Time sync error` resource is set to true.
     * `Last time sync` resource is updated.
     */
    ANJ_NTP_STATUS_FINISHED_WITH_ERROR,

#        ifdef ANJ_WITH_PERSISTENCE
    /**
     * NTP object state has been updated by LwM2M Server and should be
     * persisted.
     */
    ANJ_NTP_STATUS_OBJECT_UPDATED,
#        endif // ANJ_WITH_PERSISTENCE
} anj_ntp_status_t;

/**
 * Callback type for NTP events.
 *
 * This callback is invoked whenever the NTP module reports a new event,
 * such as a status change or the completion of a time synchronization
 * operation.
 *
 * When the status is @ref ANJ_NTP_STATUS_FINISHED_SUCCESSFULLY,
 * the callback will also provide the synchronized time via the
 * @p synchronized_time parameter.
 *
 * @param arg               Opaque user argument passed to the callback.
 * @param ntp               NTP object reporting the status change.
 * @param status            Current NTP status value; see @ref anj_ntp_status_t.
 * @param synchronized_time The synchronized time provided by the NTP module.
 *                          Valid only when status is @ref
 *                          ANJ_NTP_STATUS_FINISHED_SUCCESSFULLY.
 */
typedef void anj_ntp_event_callback_t(void *arg,
                                      anj_ntp_t *ntp,
                                      anj_ntp_status_t status,
                                      anj_time_real_t synchronized_time);

/**
 * NTP module configuration structure. Should be filled before passing to
 * @ref anj_ntp_init.
 */
typedef struct anj_ntp_configuration_struct {
    /**
     * Mandatory callback for monitoring NTP status changes.
     *
     * This callback will be invoked for each status change of the NTP
     * module.
     */
    anj_ntp_event_callback_t *event_cb;

    /**
     * Opaque argument that will be passed to the function configured in the
     * @ref event_cb field.
     */
    void *event_cb_arg;

    /**
     * NTP server address to synchronize time with. Must be a null-terminated
     * string.
     *
     * Corresponds to the `NTP server address` Resource (/3415/0/1) value in the
     * NTP object.
     *
     * String is copied internally, so the pointer can be freed after
     * this function returns. It must not exceed @ref
     * ANJ_NTP_SERVER_ADDR_MAX_LEN characters in length.
     */
    const char *ntp_server_address;

    /**
     * Fallback NTP server address to use if the primary server is unreachable.
     * Must be a null-terminated string. Optional - can be set to NULL.
     *
     * Corresponds to the `Backup NTP server address` Resource (/3415/0/2) in
     * the NTP object.
     *
     * String is copied internally, so the pointer can be freed after
     * this function returns. It must not exceed @ref
     * ANJ_NTP_SERVER_ADDR_MAX_LEN characters in length.
     */
    const char *backup_ntp_server_address;

    /**
     * Defines how often (in hours) the NTP synchronization should be
     * performed. If set to 0, periodic synchronization is disabled.
     *
     * Corresponds to the `NTP period` Resource (/3415/0/3) in the NTP object.
     *
     * If set @p event_cb will be called with status
     * @ref ANJ_NTP_STATUS_PERIOD_EXCEEDED when the period is exceeded.
     * The time is counted since the last synchronization attempt,
     * regardless of its result, or since module initialization.
     */
    uint32_t ntp_period_hours;

    /**
     * Defines how many NTP synchronization attempts will be made before
     * reporting failure. If not set, single attempt will be made.
     *
     * If @p backup_ntp_server_address is set, the same number of attempts will
     * be made for the backup server after all attempts for the primary server
     * are exhausted.
     */
    uint16_t attempts;

    /**
     * Timeout for waiting for an NTP server response, in seconds.
     *
     * This value applies to a single NTP request. If no response is received
     * within this period, the attempt is treated as failed.
     *
     * A smaller timeout reduces how long the client waits on an unresponsive
     * or unreachable server, but increases the risk of failures on slower
     * networks. Conversely, a very large timeout increases the chance of
     * eventually getting a reply, but the long round-trip time may cause the
     * resulting time adjustment to be less accurate (the clock is set based on
     * an older response).
     *
     * If not set, defaults to 5 seconds.
     */
    anj_time_duration_t response_timeout;

    /**
     * Network socket configuration to use for NTP communication.
     * Object pointed to by this pointer can be freed after
     * this function returns, as it is copied internally.
     */
    const anj_net_socket_configuration_t *net_socket_cfg;
} anj_ntp_configuration_t;

/**
 * Initializes NTP module internal state variable. Installs NTP object into
 * Anjay instance. `Last time sync` resource is initialized to the time of
 * initialization.
 *
 * @param anj       Anjay Lite instance.
 * @param ntp       Pointer to a variable that will hold the state of NTP
 *                  module.
 * @param config    Configuration structure for the NTP module.
 *                  Object pointed to by this pointer can be freed after
 *                  this function returns, as it is copied internally.
 *
 * @return 0 on success,
 *         @ref ANJ_NTP_ERR_CONFIGURATION if the provided configuration is
 *         invalid,
 *         @ref ANJ_NTP_ERR_OBJECT_CREATION_FAILED if NTP object creation in
 *         Anjay instance failed.
 */
int anj_ntp_init(anj_t *anj,
                 anj_ntp_t *ntp,
                 const anj_ntp_configuration_t *config);

/**
 * Main step function of the Anjay Lite NTP module.
 *
 * This function should be called regularly in the main application loop. It
 * drives the internal state machine and handles all scheduled operations
 * related to NTP time synchronization.
 *
 * This function is non-blocking, unless a custom network implementation
 * introduces blocking behavior.
 *
 * @param ntp       NTP module state.
 */
void anj_ntp_step(anj_ntp_t *ntp);

/**
 * Starts a new NTP time synchronization operation.
 *
 * @param ntp       NTP module state.
 *
 * @return 0 on success,
 *         @ref ANJ_NTP_ERR_IN_PROGRESS if a download is already in progress.
 */
int anj_ntp_start(anj_ntp_t *ntp);

/**
 * Terminates the current NTP time synchronization operation and cleans up the
 * NTP module.
 *
 * This function should be called when the synchronization is no longer needed
 * or when an error condition requires aborting the process. It has effect only
 * when the NTP module is in the @ref ANJ_NTP_STATUS_IN_PROGRESS. If called in
 * any other state, the function has no effect.
 *
 * After calling this function, the module's status will transition to
 * @ref ANJ_NTP_STATUS_FINISHED_WITH_ERROR.
 *
 * Subsequent calls to @ref anj_ntp_step will complete the termination sequence
 * by closing the UDP exchange and terminating the connection.
 *
 * @param ntp  NTP module state.
 */
void anj_ntp_terminate(anj_ntp_t *ntp);

#        ifdef ANJ_WITH_PERSISTENCE
/**
 * Serializes the current LwM2M NTP Object into the persistence stream.
 * Should be called before shutting down the application or when the NTP object
 * state changes (@ref ANJ_NTP_STATUS_OBJECT_UPDATED event).
 *
 * Writes NTP Object instance and its resources to the underlying medium via
 * @p ctx->write.
 *
 * @param ntp  NTP Object context to serialize.
 * @param ctx  Persistence context; must have @ref
 *             anj_persistence_context_t::direction set to
 *             ANJ_PERSISTENCE_STORE.
 *
 * @return 0 on success, negative value on error.
 */
int anj_ntp_obj_store(anj_ntp_t *ntp, const anj_persistence_context_t *ctx);

/**
 * Deserializes the LwM2M NTP Object from the persistence stream. Should be
 * called after successful initialization of NTP module.
 *
 * Reads NTP Object instance and its resources from the underlying medium via
 * @p ctx->read.
 *
 * @param ntp  NTP Object context to fill.
 * @param ctx  Persistence context; must have @ref
 *             anj_persistence_context_t::direction set to
 *             ANJ_PERSISTENCE_RESTORE.
 *
 * @return 0 on success, negative value on error.
 */
int anj_ntp_obj_restore(anj_ntp_t *ntp, const anj_persistence_context_t *ctx);
#        endif // ANJ_WITH_PERSISTENCE

/** @cond */
#        define ANJ_INTERNAL_INCLUDE_NTP
#        include <anj_internal/ntp.h>
#        undef ANJ_INTERNAL_INCLUDE_NTP
/** @endcond */

#    endif // ANJ_WITH_NTP

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_NTP_H
