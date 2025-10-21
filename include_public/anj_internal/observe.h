/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_OBSERVE_H
#define ANJ_INTERNAL_OBSERVE_H

#define ANJ_INTERNAL_INCLUDE_COAP
#include <anj_internal/coap.h> // IWYU pragma: export
#undef ANJ_INTERNAL_INCLUDE_COAP

#ifndef ANJ_INTERNAL_INCLUDE_OBSERVE
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_OBSERVE

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ANJ_WITH_OBSERVE

/**
 * @anj_internal_api_do_not_use
 * A constant that may be used to address all servers.
 */
#    define ANJ_OBSERVE_ANY_SERVER UINT16_MAX

/** @anj_internal_api_do_not_use */
typedef union {
    int64_t int_value;
    uint64_t uint_value;
    double double_value;
    bool bool_value;
} _anj_observation_res_val_t;

/** @anj_internal_api_do_not_use */
typedef struct _anj_observe_observation_struct _anj_observe_observation_t;
struct _anj_observe_observation_struct {
    // ssid set to 0 means that the observation is not used
    uint16_t ssid;
    anj_uri_path_t path;
    _anj_coap_token_t token;
    uint32_t observe_number;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    // These two fields are needed in case we receive a composite observation
    // with the same token again; we then need to compare if all the CoAP
    // options match.
    uint16_t accept_opt;
    uint16_t content_format_opt;
#    endif // ANJ_WITH_OBSERVE_COMPOSITE

#    ifdef ANJ_WITH_LWM2M12
    _anj_attr_notification_t observation_attr;
#    endif // ANJ_WITH_LWM2M12
    _anj_attr_notification_t effective_attr;
    // if effective_attr are not valid, the observation is not active
    bool observe_active;

    anj_time_real_t last_notify_timestamp;
    anj_time_real_t next_conf_notify_timestamp;

    /* This field is used for the purpose of the "Change Value Conditions"
     * attributes handling. This value is written from data model when:
     * 1. @ref _anj_observe_process detects that a notification is to be sent,
     * and observation has the relevant attributes.
     * 2. Server adds new observation which has the relevant attributes.
     * 3. Server performs the write-attributes operation, which affects an
     * existing observation, and in consequence this observation has the
     * relevant attributes (which it did not have before). In this case,
     * last_sent_value will store the current value at the time of entering
     * attributes, NOT the last sent value.
     */
    _anj_observation_res_val_t last_sent_value;

#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    _anj_observe_observation_t *prev;
#    endif // ANJ_WITH_OBSERVE_COMPOSITE

    bool notification_to_send;
};

/** @anj_internal_api_do_not_use */
typedef struct {
    uint16_t ssid;
    anj_uri_path_t path;
    _anj_attr_notification_t attr;
} _anj_observe_attr_storage_t;

/**
 * @anj_internal_api_do_not_use
 * Information about server state. Should be filled by the user and passed to
 * the observe module just before the call to @ref _anj_observe_process.
 */
typedef struct anj_observe_server_state_struct {
    /** Indicates if the corresponding server is online. */
    bool is_server_online;

    /** Short Server Id. */
    uint16_t ssid;

    /** Value of Resource 2 of corresponding Server Object (/1/x/2) should
     * be used here. If this Resource doesn't exist, then use 0, which is the
     * default value.
     *
     * Time unit: seconds
     */
    uint32_t default_min_period;

    /** Value of Resource 3 of corresponding Server Object (/1/x/3) should
     * be used here. If this Resource doesn't exist, then use 0, which is
     * the default value, meaning that pmax must be ignored.
     *
     * Time unit: seconds
     */
    uint32_t default_max_period;

    /** Indicates if notification storage should be used. Value of Resource 6 of
     * corresponding Server Object (/1/x/6) should be used here. */
    bool notify_store;

#    ifdef ANJ_WITH_LWM2M12
    /** Default notification mode. Value of Resource 26 of corresponding Server
     * Object (/1/x/26) should be used here. */
    uint32_t default_con;
#    endif // ANJ_WITH_LWM2M12
} _anj_observe_server_state_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    _anj_observe_observation_t
            observations[ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER];
    _anj_observe_attr_storage_t
            attributes_storage[ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER];

    /* Fields related to currently process operation */
    int in_progress_type;
    _anj_observe_observation_t *processing_observation;
    const _anj_coap_token_t *token;
    bool observation_exists;
    size_t already_processed;
    size_t uri_count;
    uint16_t format;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    /* Field required for adding observations from exchange module callback */
    uint16_t ssid;
    uint16_t accept;
    const anj_uri_path_t *uri_paths[ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER];
#        ifdef ANJ_WITH_LWM2M12
    _anj_attr_notification_t notification_attr;
#        endif // ANJ_WITH_LWM2M12
    _anj_observe_observation_t *first_observation;
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
} _anj_observe_ctx_t;

#endif // ANJ_WITH_OBSERVE

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_OBSERVE_H
