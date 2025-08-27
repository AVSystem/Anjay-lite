/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_CORE_H
#    define ANJ_CORE_H

#    include <anj/compat/net/anj_net_api.h>
#    include <anj/defs.h>
#    include <anj/dm/core.h>

#    define ANJ_INTERNAL_INCLUDE_EXCHANGE
#    include <anj_internal/exchange.h> // IWYU pragma: export
#    undef ANJ_INTERNAL_INCLUDE_EXCHANGE

#    ifdef ANJ_WITH_BOOTSTRAP
#        define ANJ_INTERNAL_INCLUDE_BOOTSTRAP
#        include <anj_internal/bootstrap.h> // IWYU pragma: export
#        undef ANJ_INTERNAL_INCLUDE_BOOTSTRAP
#    endif // ANJ_WITH_BOOTSTRAP

#    ifdef ANJ_WITH_LWM2M_SEND
#        include <anj/lwm2m_send.h>
#    endif // ANJ_WITH_LWM2M_SEND

#    ifdef __cplusplus
extern "C" {
#    endif

#    if defined(ANJ_COAP_WITH_TCP) && defined(ANJ_COAP_WITH_UDP)
#        define ANJ_SUPPORTED_BINDING_MODES "UT"
#    elif defined(ANJ_COAP_WITH_TCP)
#        define ANJ_SUPPORTED_BINDING_MODES "T"
#    else
#        define ANJ_SUPPORTED_BINDING_MODES "U"
#    endif

/**
 * This enum represents the possible states of a server connection.
 */
typedef enum {
    /**
     * Initial state of the client after startup.
     *
     * Anjay Lite will automatically attempt to transition to either
     * @ref ANJ_CONN_STATUS_BOOTSTRAPPING or @ref ANJ_CONN_STATUS_REGISTERING,
     * depending on the configuration.
     *
     * If the provided configuration is invalid or incomplete, the client will
     * immediately transition to @ref ANJ_CONN_STATUS_INVALID.
     */
    ANJ_CONN_STATUS_INITIAL,
    /**
     * Provided configuration is invalid and a connection cannot be established.
     *
     * This state is transient — the client will immediately transition to
     * @ref ANJ_CONN_STATUS_FAILURE to indicate a permanent failure.
     */
    ANJ_CONN_STATUS_INVALID,
    /**
     * Indicates that bootstrap or registration has permanently failed
     * (i.e., all configured retry attempts have been exhausted).
     *
     * In this case, reinitialization of the Anjay Lite client is required
     * to attempt a new connection cycle. This can be done by calling
     * @ref anj_core_restart.
     */
    ANJ_CONN_STATUS_FAILURE,
    /**
     * Bootstrap process is ongoing.
     */
    ANJ_CONN_STATUS_BOOTSTRAPPING,
    /**
     * Bootstrapping process has finished successfully.
     */
    ANJ_CONN_STATUS_BOOTSTRAPPED,
    /**
     * Registering process is ongoing.
     */
    ANJ_CONN_STATUS_REGISTERING,
    /**
     * Registering/Updating process has finished successfully.
     */
    ANJ_CONN_STATUS_REGISTERED,
    /**
     * Connection is suspended. If the suspension was initiated by the server
     * the client will remain suspended until the Disable Timeout (resource
     * 1/x/5) expires. If the suspension was initiated by the client application
     * no action is taken until user decides to resume or timeout occurs.
     */
    ANJ_CONN_STATUS_SUSPENDED,
    /**
     * Client is entering queue mode.
     */
    ANJ_CONN_STATUS_ENTERING_QUEUE_MODE,
    /**
     * Client is in queue mode: new requests still can be sent to the server,
     * but no new messages are received.
     */
    ANJ_CONN_STATUS_QUEUE_MODE,
} anj_conn_status_t;

/**
 * Contains information about the type of changes of the data model.
 * Used in @ref anj_core_data_model_changed function.
 */
typedef enum {
    /** Resource or Resource Instance value changed. */
    ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED = 0,
    /** Object Instance or Resource Instance added. */
    ANJ_CORE_CHANGE_TYPE_ADDED = 1,
    /** Object Instance or Resource Instance deleted. */
    ANJ_CORE_CHANGE_TYPE_DELETED = 2
} anj_core_change_type_t;

/**
 * Callback type for connection status change notifications.
 *
 * This function is called whenever the connection state of the LwM2M client
 * changes — e.g., transitioning from bootstrapping to registered, or entering
 * queue mode.
 *
 * Users may use this callback to monitor client connectivity and act
 * accordingly (e.g., update UI, manage power states, or trigger other logic).
 *
 * @param arg          Opaque argument passed to the callback.
 * @param anj          Anjay object reporting the status change.
 * @param conn_status  New connection status value; see @ref anj_conn_status_t.
 */
typedef void anj_connection_status_callback_t(void *arg,
                                              anj_t *anj,
                                              anj_conn_status_t conn_status);

/**
 * Anjay Lite configuration. Provided in @ref anj_core_init() function.
 */
typedef struct anjay_configuration_struct {
    /**
     * Endpoint name as presented to the LwM2M server. Must be non-NULL.
     *
     * NOTE: Endpoint name must stay valid for the whole lifetime of the Anjay.
     */
    const char *endpoint_name;

    /**
     * Optional callback for monitoring connection status changes.
     *
     * If provided, this callback will be invoked by the library whenever the
     * client transitions between connection states, check @ref
     * anj_connection_status_callback_t for details.
     */
    anj_connection_status_callback_t *connection_status_cb;

    /**
     * Opaque argument that will be passed to the function configured in the
     * @ref connection_status_cb field.
     *
     * If @ref connection_status_cb is NULL, this field is ignored.
     */
    void *connection_status_cb_arg;

    /**
     * Enables Queue Mode — an LwM2M feature that allows the client to close
     * its transport connection (e.g., UDP socket) to reduce power consumption
     * in constrained environments.
     *
     * When enabled, the client enters offline mode after a configurable period
     * of inactivity — that is, when no message exchange with the server has
     * occurred for @ref queue_mode_timeout_ms time.
     *
     * The client exits offline mode only when sending a Registration Update,
     * a Send message, or a Notification.
     *
     * While the client is in offline mode, the LwM2M Server is expected to
     * refrain from sending any requests to the client.
     */
    bool queue_mode_enabled;

    /**
     * Specifies the timeout (in milliseconds) after which the client enters
     * offline mode in Queue Mode.
     *
     * @note If not set, the default is based on the CoAP MAX_TRANSMIT_WAIT,
     *       which is derived from CoAP transmission parameters
     *       (see @ref udp_tx_params).
     */
    uint64_t queue_mode_timeout_ms;

    /**
     * Network socket configuration.
     */
    const anj_net_config_t *net_socket_cfg;

    /**
     * UDP transmission parameters, for LwM2M client requests. If NULL, default
     * values will be used.
     */
    const anj_exchange_udp_tx_params_t *udp_tx_params;

    /**
     * Time to wait for the next block of the LwM2M Server request. If not set,
     * the default internal value of \c _ANJ_EXCHANGE_SERVER_REQUEST_TIMEOUT_MS
     * is used.
     */
    uint64_t exchange_request_timeout_ms;
#    ifdef ANJ_WITH_BOOTSTRAP

    /**
     * The number of successive communication attempts before which a
     * communication sequence to the Bootstrap Server is considered as failed.
     */
    uint16_t bootstrap_retry_count;

    /**
     * The delay, in seconds, between successive communication attempts in a
     * communication sequence to the Bootstrap Server. This value is multiplied
     * by two to the power of the communication retry attempt minus one
     * (2**(retry attempt-1)) to create an exponential back-off.
     */
    uint32_t bootstrap_retry_timeout;

    /**
     * Timeout (in seconds) for the Bootstrap process.
     *
     * This timeout defines the maximum period of inactivity allowed during
     * the Bootstrap phase. If no message is received from the Bootstrap Server
     * within this time, the bootstrap process will be considered failed.
     *
     * If not set, a default value of 247 seconds is used, which corresponds to
     * the CoAP EXCHANGE_LIFETIME value.
     */
    uint32_t bootstrap_timeout;
#    endif // ANJ_WITH_BOOTSTRAP
} anj_configuration_t;

/**
 * Initializes the core of the Anjay Lite library.
 * The @p anj object must be created and allocated by the user before calling
 * this function.
 *
 * @note It is recommended to ensure that the system time is set correctly
 *       before calling this function. Many internal operations — such as
 *       registration lifetimes, scheduling, and retransmissions — rely on
 *       time-based logic. If the system time is significantly adjusted
 *       (e.g., via NTP or manual update) after initialization, it may affect
 *       incorrect library behavior.
 *
 * @param anj    Anjay object to operate on.
 * @param config Configuration structure.
 *
 * @returns 0 on success, or a negative value in case of error.
 */
int anj_core_init(anj_t *anj, const anj_configuration_t *config);

/**
 * Main step function of the Anjay Lite library.
 *
 * This function should be called regularly in the main application loop. It
 * drives the internal state machine and handles all scheduled operations
 * related to LwM2M communication (e.g., bootstrap, registration, notifications,
 * queue mode transitions, etc.).
 *
 * This function is non-blocking, unless a custom network implementation
 * introduces blocking behavior.
 *
 * @param anj  Anjay object to operate on.
 */
void anj_core_step(anj_t *anj);

/**
 * Returns the time (in milliseconds) until the next call to @ref anj_core_step
 * is required.
 *
 * In most cases, the returned value will be 0, indicating that the main loop
 * should call @ref anj_core_step immediately.
 *
 * However, in certain low-activity states, this function may return a positive
 * value:
 * - If the client is in @ref ANJ_CONN_STATUS_SUSPENDED, the value indicates
 *   the remaining time until the scheduled wake-up.
 * - If the client is in @ref ANJ_CONN_STATUS_QUEUE_MODE, the value reflects
 *   the time remaining until the next scheduled Update or Notification.
 *
 * This function is useful for optimizing power consumption or sleeping,
 * allowing the main loop to wait the appropriate amount of time before calling
 * @ref anj_core_step again.
 *
 * @note For example, if the device is in queue mode and the next Update
 *       message is scheduled to be sent in 20 seconds and there is no active
 *       observations, this function will return 20*1000.
 *
 * @note Returned value might become outdated if @ref anj_core_change_type_t is
 * called after this function returns. In such case, the application should call
 * @ref anj_core_next_step_time again to get an updated value.
 *
 * This function allows the integration layer to optimize power consumption
 * or sleep scheduling by delaying calls to @ref anj_core_step until necessary.
 *
 * @param anj  Anjay object to operate on.
 *
 * @return Time in milliseconds until the next @ref anj_core_step is required.
 */
uint64_t anj_core_next_step_time(anj_t *anj);

/**
 * Checks if there is an ongoing exchange between the client and the server.
 * User must not process operations on the data model if this function returns
 * true.
 *
 * @param anj  Anjay object to operate on.
 *
 * @returns true if there is an ongoing operation, false otherwise.
 */
bool anj_core_ongoing_operation(anj_t *anj);

/**
 * Should be called when Disable resource of Server object (/1/x/4) is executed.
 *
 * @note The disable process will be scheduled but delayed until the currently
 *       processed LwM2M request is fully completed.
 *
 * @note De-Register message will be sent to the server before the connection is
 *       closed.
 *
 * @param anj     Anjay object to operate on.
 * @param timeout Timeout for server disable, i.e. time for which a server
 *                should be disabled before it's automatically enabled again, in
 *                seconds. It's taken from /1/x/5 Resource and if it's not set,
 *                default value of 86400 seconds (24h) should be used.
 */
void anj_core_server_obj_disable_executed(anj_t *anj, uint32_t timeout);

/**
 * Should be called when Registration Update Trigger resource of Server object
 * (/1/x/8) is executed.
 *
 * @note The Update operation will be scheduled but delayed until the currently
 *       processed LwM2M request is fully completed.
 *
 * @param anj   Anjay object to operate on.
 */
void anj_core_server_obj_registration_update_trigger_executed(anj_t *anj);

/**
 * Should be called when Bootstrap-Request Trigger resource of Server object
 * (/1/x/9) is executed.
 *
 * @note The bootstrap process will be scheduled but delayed until the currently
 *       processed LwM2M request is fully completed.
 *
 * @note De-Register message will be sent to the server before the connection
 *       with Management Server is closed and Bootstrap is started.
 *
 * @param anj   Anjay object to operate on.
 */
void anj_core_server_obj_bootstrap_request_trigger_executed(anj_t *anj);

/**
 * Notifies the library that data model changed. Depending on the
 * @p change_type there are three possible scenarios:
 *  - @ref ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED - Resource or Resource Instance
 *    value changed. It may trigger preparation of a LwM2M Notify message.
 *  - @ref ANJ_CORE_CHANGE_TYPE_ADDED - Object Instance or Resource Instance has
 *    been added. It may trigger preparation of a LwM2M Notify message and for
 *    new Object Instance also registration update.
 *  - @ref ANJ_CORE_CHANGE_TYPE_DELETED - Object Instance or Resource Instance
 *    has been deleted. The consequence will be the removal of all observations
 *    and attributes associated with this Instance. For removed Object Instance
 *    it will also trigger registration update.
 *
 * For correct handling of notifications, and registration updates, this
 * function must be called on any change in the data model.
 *
 * IMPORTANT: This function is to be used only for changes in the data model
 * that are caused by user actions. Changes caused by the LwM2M Server are
 * handled internally.
 *
 * @param anj          Anjay object to operate on.t.
 * @param path         Pointer to the path of the Resource that changed, or
 *                     the Instance that was added/removed.
 * @param change_type  Type of change; @ref anj_core_change_type_t.
 */
void anj_core_data_model_changed(anj_t *anj,
                                 const anj_uri_path_t *path,
                                 anj_core_change_type_t change_type);

/**
 * Disables the LwM2M Server connection for a specified timeout.
 * If the server is already disabled, this call will only update the timeout
 * value.
 *
 * @note If the client is currently registered, a De-Register message will
 *       be sent to the server before the connection is closed.
 *
 * @note To disable the server for infinite time, set @p timeout_ms to @ref
 *       ANJ_TIME_UNDEFINED.
 *
 * @param anj     Anjay object to operate on.
 * @param timeout_ms Timeout for server disable, i.e. time for which a server
 *                should be disabled before it's automatically enabled again, in
 *                miliseconds.
 */
void anj_core_disable_server(anj_t *anj, uint64_t timeout_ms);

/**
 * Forces the start of a Bootstrap sequence.
 *
 * This function immediately initiates a client-side Bootstrap procedure,
 * regardless of the current connection status.
 *
 * @note If a bootstrap session is already active, this function will have no
 *       effect.
 *
 * @param anj  Anjay object to operate on.
 */
void anj_core_request_bootstrap(anj_t *anj);

/**
 * Restarts the Anjay Lite client connection by resetting its internal state to
 * the initial state (@ref ANJ_CONN_STATUS_INITIAL).
 *
 * This function can be used to fully reinitialize the LwM2M connection cycle,
 * e.g., after a configuration change, error recovery, or user-triggered reset.
 *
 * The following operations are performed:
 * - If the client is currently registered with a LwM2M Server,
 *   a De-Register message is sent before tearing down the connection.
 * - If a Bootstrap or Registration process is in progress,
 *   it will be immediately aborted.
 *
 * @note If the client is currently in the @ref ANJ_CONN_STATUS_SUSPENDED state
 *       due to a server-triggered Disable operation (i.e., after calling
 *       @ref anj_core_server_obj_disable_executed), the user should **not**
 *       attempt to manually resume the connection (e.g., via
 *       @ref anj_core_restart or @ref anj_core_request_bootstrap).
 *
 *       In this case, the connection is suspended intentionally by the server,
 *       and the client must remain idle until the disable timeout expires.
 *
 * @param anj  Anjay object to operate on.
 */
void anj_core_restart(anj_t *anj);

/**
 * Forces to start a Registration Update sequence.
 *
 * @note If a registration session is not active, this function will have no
 *       effect.
 *
 * @param anj  Anjay object to operate on.
 */
void anj_core_request_update(anj_t *anj);

/**
 * Shuts down the Anjay Lite client instance.
 *
 * This function halts all active LwM2M operations and clears the client's
 * internal state. Once called, the @p anj object becomes unusable unless it is
 * reinitialized using @ref anj_core_init.
 *
 * The following cleanup steps are performed:
 * - All queued Send operations are canceled and removed.
 * - All observation and attribute storage entries are deleted.
 * - Any ongoing LwM2M exchanges (e.g., registration, update, notification,
 *   Server-initiated requests) are terminated.
 *
 * @note LwM2M Objects registered via @ref anj_dm_add_obj are user-managed. This
 *       function does not free or reset those objects; users are responsible
 *       for their manual deallocation if needed.
 *
 * Internally, this function invokes a sequence of operations to tear down
 * networking, including shutting down and closing sockets, and releasing
 * network context state. If any of these steps return @ref ANJ_NET_EAGAIN,
 * indicating that the operation is not yet complete, this function will
 * immediately return that value. In this case, the client is **not yet fully
 * shut down**, and the function should be called again later to complete the
 * process.
 *
 * If any networking-related call returns an error **other than**
 * @ref ANJ_NET_EAGAIN, the client is still fully shut down internally, but some
 * network resources (such as sockets or internal handles) may not have been
 * properly released.
 *
 * @warning If shutdown fails due to networking errors, there is a risk that
 *          sockets or system-level resources allocated by the client will not
 *          be completely cleaned up. This is especially important in
 *          long-running processes or test environments where resource leaks
 *          could accumulate.
 *
 * @note If blocking behavior is acceptable, the shutdown can be safely handled
 *       in a loop:
 *
 *       @code
 *       int ret;
 *       do {
 *           ret = anj_core_shutdown(&anj);
 *       } while (ret == ANJ_NET_EAGAIN);
 *       @endcode
 *
 * @param anj Pointer to the Anjay client instance to shut down.
 *
 * @returns 0 on success,
 *          @ref ANJ_NET_EAGAIN if shutdown is still in progress,
 *          or a different error code on failure.
 */
int anj_core_shutdown(anj_t *anj);

#    define ANJ_INTERNAL_INCLUDE_CORE
#    include <anj_internal/core.h> // IWYU pragma: export
#    undef ANJ_INTERNAL_INCLUDE_CORE

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_CORE_H
