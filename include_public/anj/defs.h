/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_DEFS_H
#    define ANJ_DEFS_H

#    include <assert.h>
#    include <math.h>
#    include <stdbool.h>
#    include <stdint.h>
#    include <stdlib.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * Macros used for constructing/parsing CoAP codes.
 *
 */
#    define ANJ_COAP_CODE_CLASS_MASK 0xE0
#    define ANJ_COAP_CODE_CLASS_SHIFT 5
#    define ANJ_COAP_CODE_DETAIL_MASK 0x1F
#    define ANJ_COAP_CODE_DETAIL_SHIFT 0

#    define ANJ_COAP_CODE(cls, detail)                                     \
        ((((cls) << ANJ_COAP_CODE_CLASS_SHIFT) & ANJ_COAP_CODE_CLASS_MASK) \
         | (((detail) << ANJ_COAP_CODE_DETAIL_SHIFT)                       \
            & ANJ_COAP_CODE_DETAIL_MASK))

/**
 * @anchor anj_coap_code_constants
 * @name anj_coap_code_constants
 *
 * CoAP code constants, as defined in RFC7252/RFC7959.
 *
 * For detailed description of their semantics, refer to appropriate RFCs.
 *
 */
// clang-format off
#define ANJ_COAP_CODE_EMPTY  ANJ_COAP_CODE(0, 0)

#define ANJ_COAP_CODE_GET    ANJ_COAP_CODE(0, 1)
#define ANJ_COAP_CODE_POST   ANJ_COAP_CODE(0, 2)
#define ANJ_COAP_CODE_PUT    ANJ_COAP_CODE(0, 3)
#define ANJ_COAP_CODE_DELETE ANJ_COAP_CODE(0, 4)
/** https://tools.ietf.org/html/rfc8132#section-4 */
#define ANJ_COAP_CODE_FETCH  ANJ_COAP_CODE(0, 5)
#define ANJ_COAP_CODE_PATCH  ANJ_COAP_CODE(0, 6)
#define ANJ_COAP_CODE_IPATCH ANJ_COAP_CODE(0, 7)

#define ANJ_COAP_CODE_CREATED  ANJ_COAP_CODE(2, 1)
#define ANJ_COAP_CODE_DELETED  ANJ_COAP_CODE(2, 2)
#define ANJ_COAP_CODE_VALID    ANJ_COAP_CODE(2, 3)
#define ANJ_COAP_CODE_CHANGED  ANJ_COAP_CODE(2, 4)
#define ANJ_COAP_CODE_CONTENT  ANJ_COAP_CODE(2, 5)
#define ANJ_COAP_CODE_CONTINUE ANJ_COAP_CODE(2, 31)

#define ANJ_COAP_CODE_BAD_REQUEST                ANJ_COAP_CODE(4, 0)
#define ANJ_COAP_CODE_UNAUTHORIZED               ANJ_COAP_CODE(4, 1)
#define ANJ_COAP_CODE_BAD_OPTION                 ANJ_COAP_CODE(4, 2)
#define ANJ_COAP_CODE_FORBIDDEN                  ANJ_COAP_CODE(4, 3)
#define ANJ_COAP_CODE_NOT_FOUND                  ANJ_COAP_CODE(4, 4)
#define ANJ_COAP_CODE_METHOD_NOT_ALLOWED         ANJ_COAP_CODE(4, 5)
#define ANJ_COAP_CODE_NOT_ACCEPTABLE             ANJ_COAP_CODE(4, 6)
#define ANJ_COAP_CODE_REQUEST_ENTITY_INCOMPLETE  ANJ_COAP_CODE(4, 8)
#define ANJ_COAP_CODE_PRECONDITION_FAILED        ANJ_COAP_CODE(4, 12)
#define ANJ_COAP_CODE_REQUEST_ENTITY_TOO_LARGE   ANJ_COAP_CODE(4, 13)
#define ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT ANJ_COAP_CODE(4, 15)

#define ANJ_COAP_CODE_INTERNAL_SERVER_ERROR  ANJ_COAP_CODE(5, 0)
#define ANJ_COAP_CODE_NOT_IMPLEMENTED        ANJ_COAP_CODE(5, 1)
#define ANJ_COAP_CODE_BAD_GATEWAY            ANJ_COAP_CODE(5, 2)
#define ANJ_COAP_CODE_SERVICE_UNAVAILABLE    ANJ_COAP_CODE(5, 3)
#define ANJ_COAP_CODE_GATEWAY_TIMEOUT        ANJ_COAP_CODE(5, 4)
#define ANJ_COAP_CODE_PROXYING_NOT_SUPPORTED ANJ_COAP_CODE(5, 5)

#define ANJ_COAP_CODE_CSM      ANJ_COAP_CODE(7, 1)
#define ANJ_COAP_CODE_PING     ANJ_COAP_CODE(7, 2)
#define ANJ_COAP_CODE_PONG     ANJ_COAP_CODE(7, 3)
#define ANJ_COAP_CODE_RELEASE  ANJ_COAP_CODE(7, 4)
#define ANJ_COAP_CODE_ABORT    ANJ_COAP_CODE(7, 5)
// clang-format on

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
 * Can be returned by @ref anj_get_external_data_t to inform the library that
 * this callback should be invoked again; it is also used internally - do not
 * modify this value!
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

/**
 * Forward declaration of anj_struct, which represents an object containing all
 * statically allocated memory used by the Anjay Lite library.
 */
typedef struct anj_struct anj_t;

#    ifdef ANJ_WITH_COAP_DOWNLOADER
/**
 * Forward declaration of anj_coap_downloader_struct, which represents an object
 * containing all statically allocated memory used by the Anjay Lite CoAP
 * downloader module.
 */
typedef struct anj_coap_downloader_struct anj_coap_downloader_t;
#    endif // ANJ_WITH_COAP_DOWNLOADER

/**
 * LWM2M Server URI maximum size - as defined in LwM2M spec
 */
#    define ANJ_SERVER_URI_MAX_SIZE 255

/**
 * Default value for the Disable Timeout resource in the Server Object
 */
#    define ANJ_DISABLE_TIMEOUT_DEFAULT_VALUE 86400

/**
 * Default values for the communication retry mechanism resources.
 */
#    define ANJ_COMMUNICATION_RETRY_RES_DEFAULT \
        (anj_communication_retry_res_t) {       \
            .retry_count = 5,                   \
            .retry_timer = 60,                  \
            .seq_delay_timer = 24 * 60 * 60,    \
            .seq_retry_count = 1                \
        }

/** Communication retry mechanism resources from Server Object */
typedef struct {
    /** Communication Retry Count: RID=17 */
    uint16_t retry_count;
    /** Communication Retry Timer: RID=18 */
    uint32_t retry_timer;
    /** Communication Sequence Delay Timer: RID=19 */
    uint32_t seq_delay_timer;
    /** Communication Sequence Retry Count: RID=20 */
    uint16_t seq_retry_count;
} anj_communication_retry_res_t;

/**
 * CoAP transmission params object.
 *
 * For LwM2M client requests, the timeout is random duration between
 * ack_timeout_ms and ack_timeout_ms * ack_random_factor. For default values it
 * is range from 2 to 3 seconds. Each retransmission doubles the timeout value.
 */
typedef struct {
    /** RFC 7252: ACK_TIMEOUT */
    uint64_t ack_timeout_ms;
    /** RFC 7252: ACK_RANDOM_FACTOR */
    double ack_random_factor;
    /** RFC 7252: MAX_RETRANSMIT */
    uint16_t max_retransmit;
} anj_exchange_udp_tx_params_t;

/**
 * Enumeration of identifiers used to index the @ref anj_uri_path_t.ids
 */
typedef enum {
    ANJ_ID_OID,
    ANJ_ID_IID,
    ANJ_ID_RID,
    ANJ_ID_RIID,
    ANJ_URI_PATH_MAX_LENGTH
} anj_id_type_t;

/**
 * A data type that represents a data model path.
 *
 * It may represent a root path, an Object path, an Object Instance path, a
 * Resource path, or a Resource Instance path.
 *
 * The <c>ids</c> array is designed to be safely and meaningfully indexed by
 * @ref anj_id_type_t values.
 */
typedef struct {
    uint16_t ids[ANJ_URI_PATH_MAX_LENGTH];
    size_t uri_len;
} anj_uri_path_t;

/** defines entry type */
typedef uint16_t anj_data_type_t;

/**
 * Null data type. It will be returned by the input context in the following
 * situations:
 *
 * - when parsing a Composite-Read request payload
 * - when parsing a SenML-ETCH JSON/CBOR payload for a Write-Composite operation
 *   and an entry without a value, requesting a removal of a specific Resource
 *   Instance, is encountered
 * - when parsing a LwM2M CBOR or SenML-ETCH JSON/CBOR payload for a
 *   Write-Composite operation and an entry with NULL value, requesting a
 *   removal of a specific Resource Instance, is encountered
 * - when parsing a TLV or LwM2M CBOR payload and an aggregate (e.g. Object
 *   Instance or a multi-instance Resource) with zero nested elements is
 *   encountered
 *
 * @ref anj_res_value_t is not used for null data.
 */
#    define ANJ_DATA_TYPE_NULL ((anj_data_type_t) 0)

/**
 * "Opaque" data type, as defined in Appendix C of the LwM2M spec.
 *
 * The <c>bytes_or_string</c> field of @ref anj_res_value_t is used to pass the
 * actual data.
 */
#    define ANJ_DATA_TYPE_BYTES ((anj_data_type_t) (1 << 0))

/**
 * "String" data type, as defined in Appendix C of the LwM2M spec.
 *
 * May also be used to represent the "Corelnk" type, as those two are
 * indistinguishable on the wire.
 *
 * The <c>bytes_or_string</c> field of @ref anj_res_value_t is used to pass the
 * actual data.
 */
#    define ANJ_DATA_TYPE_STRING ((anj_data_type_t) (1 << 1))

/**
 * "Integer" data type, as defined in Appendix C of the LwM2M spec.
 *
 * The <c>int_value</c> field of @ref anj_res_value_t is used to pass the
 * actual data.
 */
#    define ANJ_DATA_TYPE_INT ((anj_data_type_t) (1 << 2))

/**
 * "Float" data type, as defined in Appendix C of the LwM2M spec.
 *
 * The <c>double_value</c> field of @ref anj_res_value_t is used to pass the
 * actual data.
 */
#    define ANJ_DATA_TYPE_DOUBLE ((anj_data_type_t) (1 << 3))

/**
 * "Boolean" data type, as defined in Appendix C of the LwM2M spec.
 *
 * The <c>bool_value</c> field of @ref anj_res_value_t is used to pass the
 * actual data.
 */
#    define ANJ_DATA_TYPE_BOOL ((anj_data_type_t) (1 << 4))

/**
 * "Objlnk" data type, as defined in Appendix C of the LwM2M spec.
 *
 * The <c>objlnk</c> field of @ref anj_res_value_t is used to pass the actual
 * data.
 */
#    define ANJ_DATA_TYPE_OBJLNK ((anj_data_type_t) (1 << 5))

/**
 * "Unsigned Integer" data type, as defined in Appendix C of the LwM2M spec.
 *
 * The <c>uint_value</c> field of @ref anj_res_value_t is used to pass the
 * actual data.
 */
#    define ANJ_DATA_TYPE_UINT ((anj_data_type_t) (1 << 6))

/**
 * "Time" data type, as defined in Appendix C of the LwM2M spec.
 *
 * The <c>time_value</c> field of @ref anj_res_value_t is used to pass the
 * actual data.
 */
#    define ANJ_DATA_TYPE_TIME ((anj_data_type_t) (1 << 7))

/**
 * When a bit mask of data types is applicable, this constant can be used to
 * specify all supported data types.
 *
 * Note that it does <strong>NOT</strong> include @ref
 * ANJ_DATA_TYPE_FLAG_EXTERNAL, and that @ref ANJ_DATA_TYPE_NULL, having
 * a numeric value of 0, does not participate in bit masks.
 */
#    define ANJ_DATA_TYPE_ANY                                           \
        ((anj_data_type_t) (ANJ_DATA_TYPE_BYTES | ANJ_DATA_TYPE_STRING  \
                            | ANJ_DATA_TYPE_INT | ANJ_DATA_TYPE_DOUBLE  \
                            | ANJ_DATA_TYPE_BOOL | ANJ_DATA_TYPE_OBJLNK \
                            | ANJ_DATA_TYPE_UINT | ANJ_DATA_TYPE_TIME))

#    ifdef ANJ_WITH_EXTERNAL_DATA
/**
 * A flag that can be OR-ed with either @ref ANJ_DATA_TYPE_BYTES or
 * @ref ANJ_DATA_TYPE_STRING to indicate that the data is provided
 * via an external callback. Valid only for output contexts.
 *
 * When this flag is set, the @p external_data field of
 * @ref anj_res_value_t must be used to provide the actual data.
 *
 * This mechanism is intended for scenarios where:
 * - the data is not directly available in memory (e.g., must be read from
 *   external storage),
 * - the total size of the data is not known in advance — the @p external_data
 *   callback interface does not require specifying the complete length
 *   beforehand.
 *
 * @note When encoding content using CBOR-based Content-Formats (such as LwM2M
 * CBOR or SenML CBOR), data is automatically encoded as an Indefinite-Length
 * String, split into chunks of up to 23 bytes.
 */
#        define ANJ_DATA_TYPE_FLAG_EXTERNAL ((anj_data_type_t) (1 << 15))

/**
 * "Opaque" data type, as defined in Appendix C of the LwM2M specification,
 * provided via an external callback. Valid only for output contexts.
 *
 * The @p external_data field of @ref anj_res_value_t is used to supply the
 * data.
 */
#        define ANJ_DATA_TYPE_EXTERNAL_BYTES        \
            ((anj_data_type_t) (ANJ_DATA_TYPE_BYTES \
                                | ANJ_DATA_TYPE_FLAG_EXTERNAL))

/**
 * "String" data type, as defined in Appendix C of the LwM2M specification,
 * provided via an external callback. Valid only for output contexts.
 *
 * The @p external_data field of @ref anj_res_value_t is used to supply the
 * data.
 */
#        define ANJ_DATA_TYPE_EXTERNAL_STRING        \
            ((anj_data_type_t) (ANJ_DATA_TYPE_STRING \
                                | ANJ_DATA_TYPE_FLAG_EXTERNAL))

/**
 * A handler used to retrieve string or binary data from an external source.
 *
 * This function is called when the resource's data type is set to
 * @ref ANJ_DATA_TYPE_EXTERNAL_BYTES or @ref ANJ_DATA_TYPE_EXTERNAL_STRING.
 * It may be called multiple times to retrieve subsequent data chunks.
 *
 * @note If this function returns @ref ANJ_IO_NEED_NEXT_CALL, the entire buffer
 *       is considered filled. In that case, the value of @p inout_size must
 *       remains unchanged.
 *
 * @note The @p offset parameter indicates the absolute position (in bytes)
 *       from the beginning of the resource data. The implementation must ensure
 *       that the copied data chunk corresponds to this offset, i.e., write
 *       exactly @p *inout_size bytes from position @p offset. The library
 *       guarantees sequential calls with increasing offsets and no overlaps.
 *
 * @param        buffer       Pointer to the buffer where data should be copied.
 * @param[inout] inout_size   On input: size of the @p buffer.
 *                            On output: number of bytes actually written.
 * @param        offset       Offset (in bytes) from the beginning of the data.
 * @param        user_args    User-defined context pointer provided by the
 *                            application.
 *
 * @return
 * - 0 on success,
 * - a negative value if an error occurred,
 * - or @ref ANJ_IO_NEED_NEXT_CALL if the function should be invoked again
 *   to continue reading the remaining data.
 */
typedef int anj_get_external_data_t(void *buffer,
                                    size_t *inout_size,
                                    size_t offset,
                                    void *user_args);

/**
 * This callback is invoked before any invocation of the @ref
 * anj_get_external_data_t callback. It should be used to initialize the
 * external data source.
 *
 * @param        user_args    User-defined context pointer provided by the
 *                            application.
 *
 * @note If this callback returns an error, the @ref anj_close_external_data_t
 * callback will not be invoked.
 *
 * @return
 * - 0 on success,
 * - a negative value if an error occurred
 */
typedef int anj_open_external_data_t(void *user_args);

/**
 * This callback will be called when the @ref anj_get_external_data_t callback
 * returns a value different than @ref ANJ_IO_NEED_NEXT_CALL or when an error
 * occurs while reading external data; such errors can originate either inside
 * the library itself or during communication with the server - for example,
 * if a timeout occurs or the server terminates the transfer.
 *
 * @param        user_args    User-defined context pointer provided by the
 *                            application.
 */
typedef void anj_close_external_data_t(void *user_args);
#    endif // ANJ_WITH_EXTERNAL_DATA

/**
 * Represents a (possibly partial) string or opaque value.
 */
typedef struct {
    /**
     * Pointer to the data buffer.
     *
     * In output contexts (e.g., responding to a Read), this points to the data
     * that will be sent to the LwM2M server.
     * In input contexts (e.g., handling a Write), this points to the data
     * received from the server.
     */
    const void *data;

    /**
     * Offset (in bytes) from the beginning of the full resource value that
     * the current @p data chunk represents.
     *
     * - In output contexts, this must always be set to 0.
     * - In input contexts, this value may be non-zero when parsing a large
     *   resource split across multiple incoming packets.
     */
    size_t offset;

    /**
     * Length (in bytes) of valid data available at @p data.
     *
     * - In output contexts, if both @p chunk_length and @p full_length_hint
     *   are set to 0 and @p data is non-NULL, then the buffer is assumed to
     *   contain a null-terminated string, and its length will be determined
     *   using @p strlen().
     */
    size_t chunk_length;

    /**
     * Full size (in bytes) of the entire resource, if known.
     *
     * - If all of @p offset, @p chunk_length and @p full_length_hint are 0,
     *   this is treated as a zero-length resource.
     * - In output contexts, this must be either 0 or equal to @p chunk_length.
     *   Any other value will be considered an error.
     * - In input contexts, this will remain 0 when receiving content formats
     *   that do not include length metadata (e.g., Plain Text).
     *   Once the last chunk is received, the field will be set to
     *   @p offset + @p chunk_length to indicate completion.
     */
    size_t full_length_hint;
} anj_bytes_or_string_value_t;

/**
 * Object Link value.
 */
typedef struct {
    anj_oid_t oid;
    anj_iid_t iid;
} anj_objlnk_value_t;

/**
 * Stores a complete or partial value of a data model entry, check "Data Types"
 * appendix in LwM2M specification for more information.
 */
typedef union {
    /**
     * Chunk of information valid for when the underlying data type is
     * @ref ANJ_DATA_TYPE_BYTES or @ref ANJ_DATA_TYPE_STRING.
     */
    anj_bytes_or_string_value_t bytes_or_string;

#    ifdef ANJ_WITH_EXTERNAL_DATA
    /**
     * Configuration for resources that use an external data callback.
     *
     * This should only be set for output contexts, and only when the resource's
     * data type is set to either @ref ANJ_DATA_TYPE_EXTERNAL_BYTES or
     * @ref ANJ_DATA_TYPE_EXTERNAL_STRING.
     */
    struct {
        /**
         * Callback function used to retrieve a chunk of data during encoding.
         *
         * This function may be called multiple times to stream the resource's
         * content in parts.
         *
         * @note This callback is mandatory.
         */
        anj_get_external_data_t *get_external_data;

        /**
         * Callback function used to prepare external data source.
         *
         * @note This callback is NOT mandatory.
         */
        anj_open_external_data_t *open_external_data;

        /**
         * Callback function called after all data has been read or when an
         * error occurs.
         *
         * @note This callback is NOT mandatory.
         */
        anj_close_external_data_t *close_external_data;

        /**
         * Opaque pointer that will be passed to @p get_external_data on every
         * call.
         *
         * Can be used by the application to provide additional context or
         * state.
         */
        void *user_args;
    } external_data;
#    endif // ANJ_WITH_EXTERNAL_DATA

    /**
     * Integer value, valid when the underlying data type is
     * @ref ANJ_DATA_TYPE_INT.
     */
    int64_t int_value;

    /**
     * Unsigned Integer value, valid when the underlying data type is
     * @ref ANJ_DATA_TYPE_UINT.
     */
    uint64_t uint_value;

    /**
     * Double-precision floating-point value, valid when the underlying data
     * type is @ref ANJ_DATA_TYPE_DOUBLE.
     */
    double double_value;

    /**
     * Boolean value, valid when the underlying data type is
     * @ref ANJ_DATA_TYPE_BOOL.
     */
    bool bool_value;

    /**
     * Objlnk value, valid when the underlying data type is
     * @ref ANJ_DATA_TYPE_OBJLNK.
     */
    anj_objlnk_value_t objlnk;

    /**
     * Time value, expressed as a UNIX timestamp, valid when the underlying data
     * type is @ref ANJ_DATA_TYPE_TIME.
     */
    int64_t time_value;
} anj_res_value_t;

/**
 * Data structure used to represent an entry produced by the data model.
 */
typedef struct anj_io_out_entry_struct {
    /** defines entry type */
    anj_data_type_t type;
    /** entry value */
    anj_res_value_t value;
    /** resource path */
    anj_uri_path_t path;
    /**
     * Entry timestamp, only meaningful for Send and Notify operations.
     *
     * Is ignored if set to NAN.
     *
     * This can be the actual Unix time in seconds if it is greater than or
     * equal to 2**28s [RFC8428], or a negative value if the time is relative to
     * the current time.
     */
    double timestamp;
} anj_io_out_entry_t;

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_DEFS_H
