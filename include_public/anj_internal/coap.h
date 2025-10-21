/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_COAP_H
#define ANJ_INTERNAL_COAP_H

#ifndef ANJ_INTERNAL_INCLUDE_COAP
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_COAP

#include <anj/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @anj_internal_api_do_not_use
 * CoAP block option type.
 */
typedef enum {
    ANJ_OPTION_BLOCK_NOT_DEFINED,
    ANJ_OPTION_BLOCK_1,
    ANJ_OPTION_BLOCK_2,
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    // Used only in @ref anj_coap_encode_udp / @ref _anj_coap_encode_tcp for
    // composite operations with block-wise transfer in both directions. BLOCK2
    // option is always encoded with number=0 and more_flag=1, BLOCK1 option is
    // always encoded with more_flag=0. Both options size is set to the same
    // value.
    ANJ_OPTION_BLOCK_BOTH
#endif // ANJ_WITH_COMPOSITE_OPERATIONS
} _anj_block_option_t;

/** @anj_internal_api_do_not_use */
typedef enum {
    ANJ_OP_NONE,
    // Bootstrap Interface
    ANJ_OP_BOOTSTRAP_REQ,
    ANJ_OP_BOOTSTRAP_FINISH,
    ANJ_OP_BOOTSTRAP_PACK_REQ,
    // Registration Interface
    ANJ_OP_REGISTER,
    ANJ_OP_UPDATE,
    ANJ_OP_DEREGISTER,
    // DM Interface,
    ANJ_OP_DM_READ,
    ANJ_OP_DM_READ_COMP,
    ANJ_OP_DM_DISCOVER,
    ANJ_OP_DM_WRITE_REPLACE,
    ANJ_OP_DM_WRITE_PARTIAL_UPDATE,
    ANJ_OP_DM_WRITE_ATTR,
    ANJ_OP_DM_WRITE_COMP,
    ANJ_OP_DM_EXECUTE,
    ANJ_OP_DM_CREATE,
    ANJ_OP_DM_DELETE,
    // Information reporting interface
    ANJ_OP_INF_OBSERVE,
    ANJ_OP_INF_OBSERVE_COMP,
    ANJ_OP_INF_CANCEL_OBSERVE,
    ANJ_OP_INF_CANCEL_OBSERVE_COMP,
    ANJ_OP_INF_INITIAL_NOTIFY,
    ANJ_OP_INF_CON_NOTIFY,
    ANJ_OP_INF_NON_CON_NOTIFY,
    ANJ_OP_INF_CON_SEND,
    ANJ_OP_INF_NON_CON_SEND,
    // CoAP Downloader Interface
    ANJ_OP_COAP_DOWNLOADER_GET,
    // client/server response - piggybacked/non-con/con
    ANJ_OP_RESPONSE,
    // CoAP related messages
    ANJ_OP_COAP_RESET,
    ANJ_OP_COAP_PING_UDP,
    ANJ_OP_COAP_EMPTY_MSG,
    // Signaling
    ANJ_OP_COAP_CSM,
    ANJ_OP_COAP_PING,
    ANJ_OP_COAP_PONG,
    ANJ_OP_COAP_RELEASE,
    ANJ_OP_COAP_ABORT
} _anj_op_t;

/**
 * @anj_internal_api_do_not_use
 * CoAP block option struct.
 */
typedef struct {
    _anj_block_option_t block_type;
    bool more_flag;
    uint32_t number;
    uint16_t size;
} _anj_block_t;

/**
 * @anj_internal_api_do_not_use
 * Notification attributes.
 */
typedef struct {
    bool has_min_period;
    bool has_max_period;
    bool has_greater_than;
    bool has_less_than;
    bool has_step;
    bool has_min_eval_period;
    bool has_max_eval_period;
    // if value is set to @ref ANJ_ATTR_UINT_NONE or @ref
    // ANJ_ATTR_DOUBLE_NONE, attribute is present but not set
    // and must be removed from the list of active parameters
    uint32_t min_period;
    uint32_t max_period;
    double greater_than;
    double less_than;
    double step;
    uint32_t min_eval_period;
    uint32_t max_eval_period;

#ifdef ANJ_WITH_LWM2M12
    bool has_edge;
    bool has_con;
    bool has_hqmax;
    uint32_t edge;
    uint32_t con;
    uint32_t hqmax;
#endif // ANJ_WITH_LWM2M12
} _anj_attr_notification_t;

/**
 * @anj_internal_api_do_not_use
 * DISCOVER operation attribute - depth parameter.
 */
typedef struct {
    bool has_depth;
    uint32_t depth;
} _anj_attr_discover_t;

/**
 * @anj_internal_api_do_not_use
 * REGISTER operation attributes.
 */
typedef struct {
    bool has_Q;
    bool has_endpoint;
    bool has_lifetime;
    bool has_lwm2m_ver;
    bool has_binding;
    bool has_sms_number;

    const char *endpoint;
    anj_time_duration_t lifetime;
    const char *lwm2m_ver;
    const char *binding;
    const char *sms_number;
} _anj_attr_register_t;

/**
 * @anj_internal_api_do_not_use
 * BOOTSTRAP-REQUEST operation attributes.
 */
typedef struct {
    bool has_endpoint;
    bool has_preferred_content_format;

    const char *endpoint;
    uint16_t preferred_content_format;
} _anj_attr_bootstrap_t;

/**
 * CREATE response attributes.
 */
typedef struct {
    bool has_uri;
    uint16_t oid;
    uint16_t iid;
} _anj_attr_create_ack_t;

/**
 * @anj_internal_api_do_not_use
 * Location-Path from REGISTER operation response. If the number of
 * Location-Paths exceeds @ref ANJ_COAP_MAX_LOCATION_PATHS_NUMBER then
 * @ref anj_coap_decode_udp / @ref _anj_coap_decode_tcp returns a @ref
 * _ANJ_ERR_LOCATION_PATHS_NUMBER error. For every @ref anj_coap_encode_udp /
 * @ref _anj_coap_encode_tcp calls for UPDATE and DEREGISTER operations, this
 * structure must be filled. After @ref anj_coap_encode_udp /
 * @ref _anj_coap_encode_tcp @p location points to message buffer, so they have
 * to be copied into user memory.
 */
typedef struct {
    const char *location[ANJ_COAP_MAX_LOCATION_PATHS_NUMBER];
    size_t location_len[ANJ_COAP_MAX_LOCATION_PATHS_NUMBER];
    size_t location_count;
} _anj_location_path_t;

/**
 * @anj_internal_api_do_not_use
 * Maximum size of ETag option, as defined in RFC7252.
 */
#define _ANJ_MAX_ETAG_LENGTH 8

/**
 * @anj_internal_api_do_not_use
 * CoAP ETag option.
 */
typedef struct {
    uint8_t size;
    uint8_t bytes[_ANJ_MAX_ETAG_LENGTH];
} _anj_etag_t;

/** @anj_internal_api_do_not_use */
#define _ANJ_COAP_MAX_TOKEN_LENGTH 8

/** @anj_internal_api_do_not_use
 * CoAP token object.
 */
typedef struct {
    uint8_t size;
    char bytes[_ANJ_COAP_MAX_TOKEN_LENGTH];
} _anj_coap_token_t;

/**
 * @anj_internal_api_do_not_use
 * CoAP message type, as defined in RFC 7252.
 */
typedef enum {
    ANJ_COAP_UDP_TYPE_CONFIRMABLE,
    ANJ_COAP_UDP_TYPE_NON_CONFIRMABLE,
    ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT,
    ANJ_COAP_UDP_TYPE_RESET
} _anj_coap_udp_type_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    uint16_t message_id;
    _anj_coap_udp_type_t type;
} _anj_coap_binding_data_udp_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    uint8_t msg_len;
    uint32_t extended_length;
} _anj_coap_binding_data_tcp_t;

/** @anj_internal_api_do_not_use */
typedef union {
    _anj_coap_binding_data_udp_t udp;
    _anj_coap_binding_data_tcp_t tcp;
} _anj_coap_binding_data_t;

#ifdef ANJ_WITH_COAP_DOWNLOADER
/** @anj_internal_api_do_not_use
 * Represents attributes for the downloader operation.
 */
typedef struct {
    /** Stores the value of ETag option. */
    _anj_etag_t etag;
    /** Stores the value of Size2 option - total size of the resource. */
    uint32_t total_size;
    /** Stores the value of Uri-Path options for GET request */
    const char *path[ANJ_COAP_DOWNLOADER_MAX_PATHS_NUMBER];
    uint16_t path_len[ANJ_COAP_DOWNLOADER_MAX_PATHS_NUMBER];
    /** Number of paths in the path array. */
    uint16_t paths_count;
} _anj_attr_downloader_t;
#endif // ANJ_WITH_COAP_DOWNLOADER

/**
 * @anj_internal_api_do_not_use
 * Represents a complete CoAP LwM2M message in a decoded, structured form.
 *
 * This structure serves as the central representation of CoAP messages within
 * the library. It is populated by the decode functions:
 * @ref anj_coap_decode_udp and @ref _anj_coap_decode_tcp, which parse raw CoAP
 * message buffers into this structured form.
 *
 * Conversely, the encode functions — @ref anj_coap_encode_udp and
 * @ref _anj_coap_encode_tcp — serialize the contents of this structure into a
 * raw CoAP message buffer for transmission over the network.
 *
 * As a result, other parts of the library operate solely on this structure,
 * without needing to directly parse or construct CoAP messages.
 *
 * Only the fields relevant to the given @p operation will be used during
 * message preparation.
 */
typedef struct {
    /**
     * ANJ operation type, which indicates LwM2M operation or specific CoAP
     * message. Must be defined before @ref anj_coap_encode_udp /
     * @ref _anj_coap_encode_tcp call.
     */
    _anj_op_t operation;

    /**
     * Pointer to CoAP msg payload. Set in @ref anj_coap_decode_udp / @ref
     * _anj_coap_decode_tcp, @ref anj_coap_encode_udp / @ref
     * _anj_coap_encode_tcp copies payload directly to message buffer.
     *
     * IMPORTANT: Payload is not encoded or decoded by ANJ functions, use
     * ANJ_IO API to achieve this.
     */
    uint8_t *payload;

    /** Payload length. */
    size_t payload_size;

    /**
     * Stores the value of Content Format option. If payload is present it
     * describes its format. In @ref anj_coap_decode_udp / @ref
     * _anj_coap_decode_tcp set to _ANJ_COAP_FORMAT_NOT_DEFINED if not present.
     * If message contains payload, must be set before @ref anj_coap_encode_udp
     * / @ref _anj_coap_encode_tcp call.
     */
    uint16_t content_format;

    /**
     * Stores the value of Accept option. It describes response payload
     * preferred format. Set to _ANJ_COAP_FORMAT_NOT_DEFINED if not present.
     */
    uint16_t accept;

    /**
     * Observation number. Have to be incremented with every Notify message.
     */
    uint32_t observe_number;

    /**
     * Stores the value of Uri Path options. Contains information about data
     * model path.
     */
    anj_uri_path_t uri;

    /**
     * Stores the value of Block option. If block type is defined
     * @ref anj_coap_encode_udp / @ref _anj_coap_encode_tcp will add block
     * option to the message.
     */
    _anj_block_t block;

    /**
     * Location path is send in respone to the REGISTER message and then have to
     * be used in UPDATE and DEREGISTER requests.
     */
    _anj_location_path_t location_path;

#ifdef ANJ_COAP_WITH_TCP
    /**
     * Stores CoAP signalling options.
     */
    union {
        struct {
            uint32_t max_msg_size;
            bool block_wise_transfer_capable;
        } csm;

        struct {
            bool custody;
        } ping_pong;
        // ANJ doesn't handle alternative_address, hold_off and bad_csm_option
        // options
    } signalling_opts;
#endif // ANJ_COAP_WITH_TCP

    /**
     * Each attribute is related to a specific operation.
     */
    union {
        _anj_attr_notification_t notification_attr;
        _anj_attr_discover_t discover_attr;
        _anj_attr_register_t register_attr;
        _anj_attr_bootstrap_t bootstrap_attr;
        _anj_attr_create_ack_t create_attr;
#ifdef ANJ_WITH_COAP_DOWNLOADER
        _anj_attr_downloader_t downloader_attr;
#endif // ANJ_WITH_COAP_DOWNLOADER
    } attr;

    /**
     * Coap msg code. Must be set before @ref anj_coap_encode_udp / @ref
     * _anj_coap_encode_tcp call if message is any kind of response.
     */
    uint8_t msg_code;

    /**
     * Contains communication channel dependend informations that allows to
     * prepare or identify response.
     */
    _anj_coap_binding_data_t coap_binding_data;

    /**
     * Token used in CoAP message. Unique for every exchange.
     */
    _anj_coap_token_t token;
} _anj_coap_msg_t;

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_COAP_H
