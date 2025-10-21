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
 * @brief Core type and constant definitions for Anjay Lite.
 *
 * Defines LwM2M identifiers, CoAP codes, data types, and utility structures
 * shared across the library.
 */

#ifndef ANJ_DEFS_H
#    define ANJ_DEFS_H

#    include <assert.h>
#    include <math.h>
#    include <stdbool.h>
#    include <stdint.h>
#    include <stdlib.h>

#    include <anj/time.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    define ANJ_COAP_CODE_CLASS_MASK 0xE0
#    define ANJ_COAP_CODE_CLASS_SHIFT 5
#    define ANJ_COAP_CODE_DETAIL_MASK 0x1F
#    define ANJ_COAP_CODE_DETAIL_SHIFT 0

#    define ANJ_COAP_CODE(cls, detail)                                     \
        ((((cls) << ANJ_COAP_CODE_CLASS_SHIFT) & ANJ_COAP_CODE_CLASS_MASK) \
         | (((detail) << ANJ_COAP_CODE_DETAIL_SHIFT)                       \
            & ANJ_COAP_CODE_DETAIL_MASK))

/** @defgroup anj_coap_code_constants CoAP code constants
 * CoAP code values as defined in RFC 7252, RFC 7959 and related extensions.
 *
 * @see RFC7252 (CoAP), RFC7959 (Blockwise), RFC8132 (FETCH/PATCH/iPATCH),
 *      RFC8323 (CoAP over TCP/TLS, Signaling codes)
 * @{
 */

// clang-format off

#define ANJ_COAP_CODE_EMPTY  ANJ_COAP_CODE(0, 0)

#define ANJ_COAP_CODE_GET     ANJ_COAP_CODE(0, 1)
#define ANJ_COAP_CODE_POST    ANJ_COAP_CODE(0, 2)
#define ANJ_COAP_CODE_PUT     ANJ_COAP_CODE(0, 3)
#define ANJ_COAP_CODE_DELETE  ANJ_COAP_CODE(0, 4)
#define ANJ_COAP_CODE_FETCH   ANJ_COAP_CODE(0, 5)
#define ANJ_COAP_CODE_PATCH   ANJ_COAP_CODE(0, 6)
#define ANJ_COAP_CODE_IPATCH  ANJ_COAP_CODE(0, 7)

#define ANJ_COAP_CODE_CREATED   ANJ_COAP_CODE(2, 1)
#define ANJ_COAP_CODE_DELETED   ANJ_COAP_CODE(2, 2)
#define ANJ_COAP_CODE_VALID     ANJ_COAP_CODE(2, 3)
#define ANJ_COAP_CODE_CHANGED   ANJ_COAP_CODE(2, 4)
#define ANJ_COAP_CODE_CONTENT   ANJ_COAP_CODE(2, 5)
#define ANJ_COAP_CODE_CONTINUE  ANJ_COAP_CODE(2, 31)

#define ANJ_COAP_CODE_BAD_REQUEST                 ANJ_COAP_CODE(4, 0)
#define ANJ_COAP_CODE_UNAUTHORIZED                ANJ_COAP_CODE(4, 1)
#define ANJ_COAP_CODE_BAD_OPTION                  ANJ_COAP_CODE(4, 2)
#define ANJ_COAP_CODE_FORBIDDEN                   ANJ_COAP_CODE(4, 3)
#define ANJ_COAP_CODE_NOT_FOUND                   ANJ_COAP_CODE(4, 4)
#define ANJ_COAP_CODE_METHOD_NOT_ALLOWED          ANJ_COAP_CODE(4, 5)
#define ANJ_COAP_CODE_NOT_ACCEPTABLE              ANJ_COAP_CODE(4, 6)
#define ANJ_COAP_CODE_REQUEST_ENTITY_INCOMPLETE   ANJ_COAP_CODE(4, 8)
#define ANJ_COAP_CODE_PRECONDITION_FAILED         ANJ_COAP_CODE(4, 12)
#define ANJ_COAP_CODE_REQUEST_ENTITY_TOO_LARGE    ANJ_COAP_CODE(4, 13)
#define ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT  ANJ_COAP_CODE(4, 15)

#define ANJ_COAP_CODE_INTERNAL_SERVER_ERROR   ANJ_COAP_CODE(5, 0)
#define ANJ_COAP_CODE_NOT_IMPLEMENTED         ANJ_COAP_CODE(5, 1)
#define ANJ_COAP_CODE_BAD_GATEWAY             ANJ_COAP_CODE(5, 2)
#define ANJ_COAP_CODE_SERVICE_UNAVAILABLE     ANJ_COAP_CODE(5, 3)
#define ANJ_COAP_CODE_GATEWAY_TIMEOUT         ANJ_COAP_CODE(5, 4)
#define ANJ_COAP_CODE_PROXYING_NOT_SUPPORTED  ANJ_COAP_CODE(5, 5)

#define ANJ_COAP_CODE_CSM      ANJ_COAP_CODE(7, 1)
#define ANJ_COAP_CODE_PING     ANJ_COAP_CODE(7, 2)
#define ANJ_COAP_CODE_PONG     ANJ_COAP_CODE(7, 3)
#define ANJ_COAP_CODE_RELEASE  ANJ_COAP_CODE(7, 4)
#define ANJ_COAP_CODE_ABORT    ANJ_COAP_CODE(7, 5)

// clang-format on

/**@} */

#    define ANJ_OBJ_ID_SECURITY 0U
#    define ANJ_OBJ_ID_SERVER 1U
#    define ANJ_OBJ_ID_ACCESS_CONTROL 2U
#    define ANJ_OBJ_ID_DEVICE 3U
#    define ANJ_OBJ_ID_FIRMWARE_UPDATE 5U
#    define ANJ_OBJ_ID_OSCORE 21U

/** The values below do not include the terminating null character */
#    define ANJ_I64_STR_MAX_LEN (sizeof("-9223372036854775808") - 1)
#    define ANJ_U16_STR_MAX_LEN (sizeof("65535") - 1)
#    define ANJ_U32_STR_MAX_LEN (sizeof("4294967295") - 1)
#    define ANJ_U64_STR_MAX_LEN (sizeof("18446744073709551615") - 1)
#    define ANJ_DOUBLE_STR_MAX_LEN (sizeof("-2.2250738585072014E-308") - 1)

#    define ANJ_ATTR_UINT_NONE (UINT32_MAX)
#    define ANJ_ATTR_DOUBLE_NONE (NAN)

/**
 * Returned by @ref anj_get_external_data_t to inform that this callback should
 * be invoked again.
 */
#    define ANJ_IO_NEED_NEXT_CALL 4

/** Object ID */
typedef uint16_t anj_oid_t;

/** Object Instance ID */
typedef uint16_t anj_iid_t;

/** Resource ID */
typedef uint16_t anj_rid_t;

/** Resource Instance ID */
typedef uint16_t anj_riid_t;

typedef struct anj_struct anj_t;

#    ifdef ANJ_WITH_COAP_DOWNLOADER
typedef struct anj_coap_downloader_struct anj_coap_downloader_t;
#    endif // ANJ_WITH_COAP_DOWNLOADER

/**
 * LwM2M Server URI maximum size (including null terminator).
 *
 * @see @lwm2m_core &sect;E.1
 */
#    define ANJ_SERVER_URI_MAX_SIZE 256

/**
 * Default value for the Disable Timeout Resource in the Server Object.
 *
 * @see @lwm2m_core &sect;E.2
 */
#    define ANJ_DISABLE_TIMEOUT_DEFAULT_VALUE 86400

/**
 * Default values for the communication retry mechanism Resources.
 */
#    define ANJ_COMMUNICATION_RETRY_RES_DEFAULT \
        (anj_communication_retry_res_t) {       \
            .retry_count = 5,                   \
            .retry_timer = 60,                  \
            .seq_delay_timer = 24 * 60 * 60,    \
            .seq_retry_count = 1                \
        }

/**
 * Default time to wait for the next block of the LwM2M Server request. Can be
 * overridden in @ref anj_configuration_t::exchange_request_timeout.
 */
#    define ANJ_EXCHANGE_SERVER_REQUEST_TIMEOUT \
        anj_time_duration_new(50, ANJ_TIME_UNIT_S)

/**
 * Default CoAP transmission parameters as specified in RFC 7252.
 */
#    define ANJ_EXCHANGE_UDP_TX_PARAMS_DEFAULT                        \
        (anj_exchange_udp_tx_params_t) {                              \
            .ack_timeout = anj_time_duration_new(2, ANJ_TIME_UNIT_S), \
            .ack_random_factor = 1.5,                                 \
            .max_retransmit = 4                                       \
        }

/** Communication Retry mechanism resources from the Server Object (/1). */
typedef struct {
    /** Communication Retry Count Resource (/1/0/17) value. */
    uint16_t retry_count;

    /** Communication Retry Timer Resource (/1/0/18) value (in seconds). */
    uint32_t retry_timer;

    /** Communication Sequence Delay Timer Resource (/1/0/19) value (in
     * seconds).
     */
    uint32_t seq_delay_timer;

    /** Communication Sequence Retry Count Resource (/1/0/20) value. */
    uint16_t seq_retry_count;
} anj_communication_retry_res_t;

/**
 * CoAP transmission parameters (RFC 7252).
 *
 * These parameters control the timing and retransmission behavior of
 * confirmable CoAP messages sent over UDP.
 *
 * The initial retransmission timeout is chosen randomly between
 * @ref ack_timeout and @ref ack_timeout × @ref ack_random_factor.
 * With default values, this corresponds to a range of 2—3 seconds.
 * After each retransmission, the timeout value is doubled.
 *
 * Retransmissions continue until either a response is received or
 * @ref max_retransmit attempts have been made.
 */
typedef struct {
    /** Initial ACK_TIMEOUT value. */
    anj_time_duration_t ack_timeout;

    /** ACK_RANDOM_FACTOR multiplier applied to randomize the timeout. */
    double ack_random_factor;

    /** Maximum number of retransmissions before giving up. */
    uint16_t max_retransmit;
} anj_exchange_udp_tx_params_t;

/**
 * Identifiers used to index elements of @ref anj_uri_path_t::ids.
 *
 * These correspond to components of an LwM2M data model path:
 * - @ref ANJ_ID_OID  — Object ID
 * - @ref ANJ_ID_IID  — Object Instance ID
 * - @ref ANJ_ID_RID  — Resource ID
 * - @ref ANJ_ID_RIID — Resource Instance ID
 *
 * @ref ANJ_URI_PATH_MAX_LENGTH gives the size of the array that can store
 * all possible path components.
 */
typedef enum {
    ANJ_ID_OID,
    ANJ_ID_IID,
    ANJ_ID_RID,
    ANJ_ID_RIID,
    ANJ_URI_PATH_MAX_LENGTH
} anj_id_type_t;

/**
 * Representation of an LwM2M data model path.
 *
 * A path can point to any level of the data model hierarchy:
 * - the root,
 * - an Object,
 * - an Object Instance,
 * - a Resource,
 * - or a Resource Instance.
 *
 * The @ref ids array stores the identifiers for each level. It can be safely
 * indexed using values from @ref anj_id_type_t. The @ref uri_len field
 * indicates how many components of the path are in use.
 */
typedef struct {
    uint16_t ids[ANJ_URI_PATH_MAX_LENGTH];
    size_t uri_len;
} anj_uri_path_t;

/** @defgroup anj_data_types LwM2M data types
 * Data type identifiers used in the Anjay Lite data model.
 *
 * These values correspond to the types defined in @lwm2m_core &sect;C. They are
 * primarily used when parsing or encoding request/response payloads.
 *
 * Each constant indicates which field of @ref anj_res_value_t is used to
 * hold the value.
 * @{
 */

/**
 * LwM2M data type identifier. As this type is a bitmask, see @ref
 * anj_data_types for possible combinations.
 */
typedef uint16_t anj_data_type_t;

/**
 * Null data type.
 *
 * Indicates absence of a value. It is reported in the following cases:
 * - Parsing a Composite-Read request payload.
 * - Parsing a SenML-ETCH JSON/CBOR Write-Composite payload that contains
 *   an entry without a value (removal of a Resource Instance).
 * - Parsing a LwM2M CBOR or SenML-ETCH JSON/CBOR Write-Composite payload
 *   that contains an explicit NULL value (removal of a Resource Instance).
 * - Parsing a TLV or LwM2M CBOR payload where an aggregate (Object Instance
 *   or Multiple Instance Resource) contains zero nested elements.
 *
 * @note For this type, @ref anj_res_value_t is not used.
 */
#    define ANJ_DATA_TYPE_NULL ((anj_data_type_t) 0)

/**
 * Opaque data type.
 *
 * The @ref anj_res_value_t::bytes_or_string field holds the raw bytes.
 */
#    define ANJ_DATA_TYPE_BYTES ((anj_data_type_t) (1 << 0))

/**
 * String data type.
 *
 * The @ref anj_res_value_t::bytes_or_string field holds the string.
 *
 * @note May also be used to represent the "Corelnk" type, as both appear
 *       identical on the wire.
 */
#    define ANJ_DATA_TYPE_STRING ((anj_data_type_t) (1 << 1))

/**
 * Integer data type.
 *
 * The @ref anj_res_value_t::int_value field holds the number.
 */
#    define ANJ_DATA_TYPE_INT ((anj_data_type_t) (1 << 2))

/**
 * Floating-point data type.
 *
 * The @ref anj_res_value_t::double_value field holds the number.
 */
#    define ANJ_DATA_TYPE_DOUBLE ((anj_data_type_t) (1 << 3))

/**
 * Boolean data type.
 *
 * The @ref anj_res_value_t::bool_value field holds the value.
 */
#    define ANJ_DATA_TYPE_BOOL ((anj_data_type_t) (1 << 4))

/**
 * Object Link (Objlnk) data type.
 *
 * The @ref anj_res_value_t::objlnk field holds the value.
 */
#    define ANJ_DATA_TYPE_OBJLNK ((anj_data_type_t) (1 << 5))

/**
 * Unsigned Integer data type.
 *
 * The @ref anj_res_value_t::uint_value field holds the number.
 */
#    define ANJ_DATA_TYPE_UINT ((anj_data_type_t) (1 << 6))

/**
 * Time data type.
 *
 * The @ref anj_res_value_t::time_value field holds the value.
 */
#    define ANJ_DATA_TYPE_TIME ((anj_data_type_t) (1 << 7))

/**
 * Bitmask representing all supported data types.
 *
 * @note Does not include @ref ANJ_DATA_TYPE_FLAG_EXTERNAL.
 * @note @ref ANJ_DATA_TYPE_NULL is not a part of the bitmask.
 */
#    define ANJ_DATA_TYPE_ANY                                           \
        ((anj_data_type_t) (ANJ_DATA_TYPE_BYTES | ANJ_DATA_TYPE_STRING  \
                            | ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE  \
                            | ANJ_DATA_TYPE_BOOL | ANJ_DATA_TYPE_OBJLNK \
                            | ANJ_DATA_TYPE_UINT | ANJ_DATA_TYPE_TIME))

#    ifdef ANJ_WITH_EXTERNAL_DATA

/**
 * Flag indicating that data is supplied via an external callback.
 *
 * May be OR-ed with @ref ANJ_DATA_TYPE_BYTES or @ref ANJ_DATA_TYPE_STRING.
 * Valid only for output contexts.
 *
 * When this flag is set, the @ref anj_res_value_t::external_data field
 * must be used to provide the data.
 *
 * Typical use cases:
 * - Data is not directly available in memory and must be streamed
 *   (e.g., from external storage).
 * - Data size is unknown in advance; the external data callback
 *   interface does not require providing the total length beforehand.
 *
 * @note When encoding CBOR-based formats (LwM2M CBOR, SenML CBOR), data is
 *       encoded as an Indefinite-Length String, split into chunks of up to
 *       23 bytes.
 */
#        define ANJ_DATA_TYPE_FLAG_EXTERNAL ((anj_data_type_t) (1 << 15))

/**
 * Opaque data type supplied via external callback.
 *
 * The @ref anj_res_value_t::external_data field provides the data. Valid only
 * for output contexts.
 */
#        define ANJ_DATA_TYPE_EXTERNAL_BYTES        \
            ((anj_data_type_t) (ANJ_DATA_TYPE_BYTES \
                                | ANJ_DATA_TYPE_FLAG_EXTERNAL))

/**
 * String data type supplied via external callback.
 *
 * The @ref anj_res_value_t::external_data field provides the data. Valid only
 * for output contexts.
 */
#        define ANJ_DATA_TYPE_EXTERNAL_STRING        \
            ((anj_data_type_t) (ANJ_DATA_TYPE_STRING \
                                | ANJ_DATA_TYPE_FLAG_EXTERNAL))

/** @} */

/**
 * Callback to read a chunk of external data.
 *
 * Called by the library when encoding a resource whose type is
 * @ref ANJ_DATA_TYPE_EXTERNAL_BYTES or @ref ANJ_DATA_TYPE_EXTERNAL_STRING.
 * It may be invoked multiple times until the entire resource value
 * has been streamed.
 *
 * The library guarantees sequential calls with monotonically increasing
 * @p offset and no overlaps.
 *
 * @param buffer      Output buffer to be filled with data.
 * @param[in,out] inout_size
 *                    - On input: size of @p buffer in bytes.
 *                    - On output: number of bytes actually written.
 * @param offset      Absolute offset (in bytes) from the beginning of
 *                    the resource value.
 * @param user_args   Application-defined pointer passed unchanged to
 *                    every callback.
 *
 * @return
 * - 0 if the end of the resource was reached (all data provided),
 * - a negative value on error,
 * - @ref ANJ_IO_NEED_NEXT_CALL if more data remains.
 *   In this case, the implementation must have filled the entire buffer
 *   (i.e., left @p inout_size unchanged).
 */
typedef int anj_get_external_data_t(void *buffer,
                                    size_t *inout_size,
                                    size_t offset,
                                    void *user_args);

/**
 * Callback to initialize the external data source.
 *
 * Invoked once before the first call to @ref anj_get_external_data_t.
 * Can be used to open files, initialize peripherals, or allocate state.
 *
 * @param user_args  Application-defined pointer.
 *
 * @return
 * - 0 on success,
 * - a negative value if initialization failed (in which case
 *   @ref anj_close_external_data_t will not be called).
 */
typedef int anj_open_external_data_t(void *user_args);

/**
 * Callback to clean up the external data source.
 *
 * Invoked after reading completes (successfully or with error),
 * unless @ref anj_open_external_data_t failed.
 * Can be used to close file descriptors, release memory, or reset state.
 *
 * @param user_args  Application-defined pointer.
 */
typedef void anj_close_external_data_t(void *user_args);
#    endif // ANJ_WITH_EXTERNAL_DATA

/**
 * Representation of a string or opaque value (full or partial).
 *
 * Used when the resource type is @ref ANJ_DATA_TYPE_BYTES or
 * @ref ANJ_DATA_TYPE_STRING.
 */
typedef struct {
    /**
     * Pointer to the data buffer.
     *
     * - In output contexts, this points to the data to be sent to the server.
     * - In input contexts, this points to the data received from the server.
     */
    const void *data;

    /**
     * Absolute offset (in bytes) from the beginning of the resource value
     * represented by this chunk.
     *
     * - In output contexts, this must always be 0.
     * - In input contexts, this may be non-zero when the value is received
     *   in multiple fragments (e.g., blockwise transfer).
     */
    size_t offset;

    /**
     * Length (in bytes) of valid data at @p data.
     *
     * - In output contexts, if both @p chunk_length and @p full_length_hint
     *   are 0 and @p data is non-NULL, the buffer is assumed to contain a
     *   null-terminated string. The length will then be determined using
     *   @c strlen().
     */
    size_t chunk_length;

    /**
     * Full size (in bytes) of the complete resource value, if known.
     *
     * - If @p offset, @p chunk_length and @p full_length_hint are all 0,
     *   the resource is treated as empty.
     * - In output contexts, this must be either 0 or equal to @p chunk_length.
     *   Any other value is considered invalid.
     * - In input contexts, this is 0 when receiving formats without length
     *   metadata (e.g., Plain Text). When the final fragment is received,
     *   it will be set to @p offset + @p chunk_length.
     */
    size_t full_length_hint;
} anj_bytes_or_string_value_t;

/**
 * Object Link (Objlnk) value.
 */
typedef struct {
    /** Object ID. */
    anj_oid_t oid;

    /** Object Instance ID. */
    anj_iid_t iid;
} anj_objlnk_value_t;

/**
 * Union type holding the value of a resource, in one of the supported
 * LwM2M data types (see @lwm2m_core &sect;C).
 *
 * The active field depends on the resource's @ref anj_data_type_t.
 */
typedef union {
    /** String or Opaque value (see @ref ANJ_DATA_TYPE_BYTES, @ref
     * ANJ_DATA_TYPE_STRING). */
    anj_bytes_or_string_value_t bytes_or_string;

#    ifdef ANJ_WITH_EXTERNAL_DATA
    /**
     * Configuration for resources that use external data streaming
     * (see @ref ANJ_DATA_TYPE_EXTERNAL_BYTES, @ref
     * ANJ_DATA_TYPE_EXTERNAL_STRING).
     *
     * These fields are only valid in output contexts.
     */
    struct {
        /** Mandatory callback to stream chunks of data during encoding. */
        anj_get_external_data_t *get_external_data;

        /** Optional callback to initialize the external source before reading.
         */
        anj_open_external_data_t *open_external_data;

        /** Optional callback to finalize/clean up after reading is done or on
         * error. */
        anj_close_external_data_t *close_external_data;

        /** Application-defined pointer, passed unchanged to all callbacks. */
        void *user_args;
    } external_data;
#    endif // ANJ_WITH_EXTERNAL_DATA

    /** Integer value (see @ref ANJ_DATA_TYPE_INT). */
    int64_t int_value;

    /** Unsigned integer value (see @ref ANJ_DATA_TYPE_UINT). */
    uint64_t uint_value;

    /** Floating-point value (see @ref ANJ_DATA_TYPE_DOUBLE). */
    double double_value;

    /** Boolean value (see @ref ANJ_DATA_TYPE_BOOL). */
    bool bool_value;

    /** Object Link value (see @ref ANJ_DATA_TYPE_OBJLNK). */
    anj_objlnk_value_t objlnk;

    /** Time value as a UNIX timestamp (see @ref ANJ_DATA_TYPE_TIME). */
    int64_t time_value;
} anj_res_value_t;

/**
 * Data structure representing a single entry produced by the data model.
 */
typedef struct anj_io_out_entry_struct {
    /** Data type of this entry (see @ref anj_data_type_t). */
    anj_data_type_t type;

    /** Value of the entry (see @ref anj_res_value_t). */
    anj_res_value_t value;

    /** Path of the affected Resource (see @ref anj_uri_path_t). */
    anj_uri_path_t path;

    /**
     * Optional timestamp associated with this entry.
     *
     * This field is only used in Send and Notify operations, and only when
     * the message is encoded using a SenML-based content format
     * (SenML JSON or SenML CBOR).
     *
     * - If set to NaN (recommended default), no timestamp is included in
     *   the payload.
     * - If set to a non-NaN value:
     *   - A non-negative value represents an absolute Unix timestamp in
     *     seconds. For interoperability, values >= 2^28 seconds (per RFC8428)
     *     are interpreted as absolute time.
     *   - A negative value represents a relative time offset (in seconds)
     *     from the current time.
     *
     * For all other LwM2M operations and for non-SenML content formats,
     * this field is ignored.
     */
    double timestamp;
} anj_io_out_entry_t;

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DEFS_H
