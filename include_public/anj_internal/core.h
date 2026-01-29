/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_CORE_H
#define ANJ_INTERNAL_CORE_H

#ifndef ANJ_INTERNAL_INCLUDE_CORE
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_CORE

#ifdef __cplusplus
extern "C" {
#endif

#define ANJ_INTERNAL_INCLUDE_DM_DEFS
#include <anj_internal/dm/defs.h> // IWYU pragma: export
#undef ANJ_INTERNAL_INCLUDE_DM_DEFS

#define ANJ_INTERNAL_INCLUDE_REGISTER
#include <anj_internal/register.h> // IWYU pragma: export
#undef ANJ_INTERNAL_INCLUDE_REGISTER

#define ANJ_INTERNAL_INCLUDE_OBSERVE
#include <anj_internal/observe.h> // IWYU pragma: export
#undef ANJ_INTERNAL_INCLUDE_OBSERVE

#define ANJ_INTERNAL_INCLUDE_IO_CTX
#include <anj_internal/io_ctx.h> // IWYU pragma: export
#undef ANJ_INTERNAL_INCLUDE_IO_CTX

#define ANJ_INTERNAL_INCLUDE_SERVER
#include <anj_internal/srv_conn.h>
#undef ANJ_INTERNAL_INCLUDE_SERVER

/**
 * @anj_internal_api_do_not_use
 * An SSID value reserved to refer to the Bootstrap Server.
 */
#define _ANJ_SSID_BOOTSTRAP 0

/** @anj_internal_api_do_not_use */
#ifdef ANJ_WITH_LWM2M12
#    define _ANJ_LWM2M_VERSION_STR "1.2"
#else
#    define _ANJ_LWM2M_VERSION_STR "1.1"
#endif // ANJ_WITH_LWM2M12

/**
 * @anj_internal_api_do_not_use
 * Anjay object containing all information required for LwM2M communication.
 */
typedef struct anj_struct {
    _anj_dm_data_model_t dm;
    _anj_register_ctx_t register_ctx;
    _anj_server_connection_ctx_t connection_ctx;
#ifdef ANJ_WITH_SECURITY
    void *crypto_ctx;
#endif // ANJ_WITH_SECURITY

    anj_net_config_t net_socket_cfg;
    const char *endpoint_name;
    bool queue_mode_enabled;
    anj_time_duration_t queue_mode_timeout;
    anj_connection_status_callback_t *conn_status_cb;
    void *conn_status_cb_arg;

#ifdef ANJ_WITH_BOOTSTRAP
    _anj_bootstrap_ctx_t bootstrap_ctx;
    uint16_t bootstrap_retry_count;
    anj_time_duration_t bootstrap_retry_timeout;
#endif // ANJ_WITH_BOOTSTRAP

#ifdef ANJ_WITH_OBSERVE
    _anj_observe_ctx_t observe_ctx;
#endif // ANJ_WITH_OBSERVE

#ifdef ANJ_WITH_LWM2M_SEND
    _anj_send_ctx_t send_ctx;
#endif // ANJ_WITH_LWM2M_SEND

    union {
        /** Used to prepare outgoing message payload. */
        _anj_io_out_ctx_t out_ctx;
        /** Used in handling of incoming message payload. */
        _anj_io_in_ctx_t in_ctx;
        /**
         * Used to prepare outgoing message payload for @ref ANJ_OP_REGISTER
         * operation.
         */
        _anj_io_register_ctx_t register_ctx;
#ifdef ANJ_WITH_DISCOVER
        /**
         * Used to prepare outgoing message payload for @ref ANJ_OP_DM_DISCOVER
         * operation.
         */
        _anj_io_discover_ctx_t discover_ctx;
#endif // ANJ_WITH_DISCOVER
#ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
        /**
         * Used to prepare outgoing message payload for @ref ANJ_OP_DM_DISCOVER
         * for Bootstrap Server connection.
         */
        _anj_io_bootstrap_discover_ctx_t bootstrap_discover_ctx;
#endif // ANJ_WITH_BOOTSTRAP_DISCOVER
    } anj_io;

    struct {
        bool disable_triggered;
        anj_time_monotonic_t enable_time;
        anj_time_monotonic_t enable_time_user_triggered;
        bool registration_update_triggered;
        bool bootstrap_request_triggered;
        bool restart_triggered;
        anj_conn_status_t conn_status;
        union {
#ifdef ANJ_WITH_BOOTSTRAP
            struct {
                uint8_t bootstrap_state;
                uint16_t bootstrap_retry_attempt;
                anj_time_monotonic_t bootstrap_timeout;
            } bootstrap;
#endif // ANJ_WITH_BOOTSTRAP
            struct {
                uint16_t retry_count;
                uint16_t retry_seq_count;
                anj_time_monotonic_t retry_timeout;
                uint8_t registration_state;
            } registration;
            struct {
                anj_time_monotonic_t next_update_time;
                anj_time_monotonic_t queue_start_time;
                bool update_with_lifetime;
                bool update_with_payload;
                uint8_t internal_state;
                bool transition_forced;
            } registered;
        } details;
    } server_state;

    struct {
        uint16_t ssid;
        anj_iid_t iid;
        /* lifetime in seconds */
        anj_time_duration_t lifetime;
        anj_communication_retry_res_t retry_res;
        bool bootstrap_on_registration_failure;
#ifdef ANJ_WITH_LWM2M_SEND
        bool mute_send;
#endif // ANJ_WITH_LWM2M_SEND
#ifdef ANJ_WITH_OBSERVE
        _anj_observe_server_state_t observe_state;
#endif // ANJ_WITH_OBSERVE
    } server_instance;

    struct {
        anj_iid_t iid;
        char server_uri[ANJ_SERVER_URI_MAX_SIZE];
        char port[ANJ_U16_STR_MAX_LEN + 1];
        anj_net_binding_type_t type;
#ifdef ANJ_WITH_BOOTSTRAP
        anj_time_duration_t client_hold_off_time;
#endif // ANJ_WITH_BOOTSTRAP
    } security_instance;

    uint8_t in_buffer[ANJ_IN_MSG_BUFFER_SIZE];
    uint8_t out_buffer[ANJ_OUT_MSG_BUFFER_SIZE];
    uint8_t payload_buffer[ANJ_OUT_PAYLOAD_BUFFER_SIZE];
    _anj_exchange_ctx_t exchange_ctx;
#ifdef ANJ_WITH_CACHE
    _anj_exchange_cache_t exchange_cache;
#endif // ANJ_WITH_CACHE
    size_t out_msg_len;
} _anj_t;

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_CORE_H
