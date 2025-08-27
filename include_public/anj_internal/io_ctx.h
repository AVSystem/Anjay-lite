/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_IO_CTX_H
#define ANJ_INTERNAL_IO_CTX_H

#include <anj/utils.h>

#define ANJ_INTERNAL_INCLUDE_COAP
#include <anj_internal/coap.h> // IWYU pragma: export
#undef ANJ_INTERNAL_INCLUDE_COAP

#ifndef ANJ_INTERNAL_INCLUDE_IO_CTX
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_IO_CTX

#ifdef __cplusplus
extern "C" {
#endif

/** @anj_internal_api_do_not_use */
#define _ANJ_IO_MAX_PATH_STRING_SIZE sizeof("/65535/65535/65535/65535")

/** @anj_internal_api_do_not_use */
#define _ANJ_IO_CBOR_MAX_OBJLNK_STRING_SIZE sizeof("65535:65535")

/**
 * @anj_internal_api_do_not_use
 * objlink is the largest possible simple variable + 1 byte for header
 */
#define _ANJ_IO_CBOR_SIMPLE_RECORD_MAX_LENGTH \
    (_ANJ_IO_CBOR_MAX_OBJLNK_STRING_SIZE + 1)
/**
 * @anj_internal_api_do_not_use
 * max 3 bytes for array UINT16_MAX elements
 * 1 byte for map
 * 14 bytes for basename 21 65 2F36353533352F3635353334 as /65534/65534
 * 14 bytes for name 00 63 2F36353533352F3635353334 as /65534/65534
 * 10 bytes for basetime 22 FB 1122334455667788
 * 4 bytes for objlink header
 * 1 bytes for string value header
 * resource with objlink is biggest possible value that can be directly written
 * into the internal_buff
 */
#define _ANJ_IO_SENML_CBOR_SIMPLE_RECORD_MAX_LENGTH \
    (3 + 1 + 14 + 14 + 10 + 4 + 1 + _ANJ_IO_CBOR_MAX_OBJLNK_STRING_SIZE)

/**
 * @anj_internal_api_do_not_use
 * Largest possible single LwM2M CBOR record, starts with closing maps of the
 * previous record and contains an objlink:
 *
 *     FF         map end
 *    FF          map end
 *   FF           map end
 *  19 FF FE      oid: 65534
 *  BF            map begin
 *   19 FF FE     iid: 65534
 *   BF           map begin
 *    19 FF FE    rid: 65534
 *    BF          map begin
 *     19 FF FE   riid: 65534
 *     6B 36 35 35 33 34 3A 36 35 35 33 34  objlink
 */
#define _ANJ_IO_LWM2M_CBOR_SIMPLE_RECORD_MAX_LENGTH (30)

/** @anj_internal_api_do_not_use */
#define _ANJ_IO_BOOT_DISC_RECORD_MAX_LENGTH \
    sizeof("</>;lwm2m=1.2,</0/65534>;ssid=65534;uri=\"")

/** @anj_internal_api_do_not_use */
#define _ANJ_IO_REGISTER_RECORD_MAX_LENGTH sizeof(",</65534>;ver=9.9")

/** @anj_internal_api_do_not_use */
#define _ANJ_IO_ATTRIBUTE_RECORD_MAX_LEN sizeof(";gt=-2.2250738585072014E-308")

/** @anj_internal_api_do_not_use */
#define _ANJ_IO_DISCOVER_RECORD_MAX_LEN \
    sizeof(",</65534/65534/65534>;dim=65534")

/** @anj_internal_api_do_not_use */
#define _ANJ_IO_PLAINTEXT_SIMPLE_RECORD_MAX_LENGTH ANJ_DOUBLE_STR_MAX_LEN

/** @anj_internal_api_do_not_use */
#define _ANJ_IO_CTX_BUFFER_LENGTH _ANJ_IO_SENML_CBOR_SIMPLE_RECORD_MAX_LENGTH

// According to IEEE 754-1985, the longest notation for value represented by
// double type is 24 characters
#define _ANJ_IO_CTX_DOUBLE_BUFF_STR_SIZE 24

ANJ_STATIC_ASSERT(
        _ANJ_IO_CTX_BUFFER_LENGTH >= _ANJ_IO_CBOR_SIMPLE_RECORD_MAX_LENGTH
                && _ANJ_IO_CTX_BUFFER_LENGTH
                           >= _ANJ_IO_LWM2M_CBOR_SIMPLE_RECORD_MAX_LENGTH
                && _ANJ_IO_CTX_BUFFER_LENGTH
                           >= _ANJ_IO_BOOT_DISC_RECORD_MAX_LENGTH
                && _ANJ_IO_CTX_BUFFER_LENGTH
                           >= _ANJ_IO_REGISTER_RECORD_MAX_LENGTH
                && _ANJ_IO_CTX_BUFFER_LENGTH >= _ANJ_IO_ATTRIBUTE_RECORD_MAX_LEN
                && _ANJ_IO_CTX_BUFFER_LENGTH >= _ANJ_IO_DISCOVER_RECORD_MAX_LEN
                && _ANJ_IO_CTX_BUFFER_LENGTH
                           >= _ANJ_IO_PLAINTEXT_SIMPLE_RECORD_MAX_LENGTH,
        internal_buff_badly_defined);

#if defined(ANJ_WITH_SENML_CBOR) || defined(ANJ_WITH_LWM2M_CBOR) \
        || defined(ANJ_WITH_CBOR)

/** @anj_internal_api_do_not_use */
#    if defined(ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES) \
            || defined(ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS)
#        define _ANJ_MAX_SUBPARSER_NEST_STACK_SIZE 1
#    else /* defined(ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES) || \
             defined(ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS) */
#        define _ANJ_MAX_SUBPARSER_NEST_STACK_SIZE 0
#    endif /* defined(ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES) || \
              defined(ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS) */

/**
 * @anj_internal_api_do_not_use
 * Only decimal fractions or indefinite length bytes can cause nesting.
 */
#    ifdef ANJ_WITH_CBOR
#        define _ANJ_MAX_SIMPLE_CBOR_NEST_STACK_SIZE \
            _ANJ_MAX_SUBPARSER_NEST_STACK_SIZE
#    else // ANJ_WITH_CBOR
#        define _ANJ_MAX_SIMPLE_CBOR_NEST_STACK_SIZE 0
#    endif // ANJ_WITH_CBOR

/**
 * @anj_internal_api_do_not_use
 * The LwM2M requires wrapping entries in [ {} ], but decimal fractions or
 * indefinite length bytes add another level of nesting in form of an array.
 */
#    ifdef ANJ_WITH_SENML_CBOR
#        define _ANJ_MAX_SENML_CBOR_NEST_STACK_SIZE \
            (2 + _ANJ_MAX_SUBPARSER_NEST_STACK_SIZE)
#    else // ANJ_WITH_SENML_CBOR
#        define _ANJ_MAX_SENML_CBOR_NEST_STACK_SIZE 0
#    endif // ANJ_WITH_SENML_CBOR

/**
 * @anj_internal_api_do_not_use
 * In LwM2M CBOR, there may be {a: {b: {c: {[d]: value}}}}
 * Decimal fractions or indefinite length bytes don't add extra level here.
 */
#    ifdef ANJ_WITH_LWM2M_CBOR
#        define _ANJ_MAX_LWM2M_CBOR_NEST_STACK_SIZE 5
#    else // ANJ_WITH_LWM2M_CBOR
#        define _ANJ_MAX_LWM2M_CBOR_NEST_STACK_SIZE 0
#    endif // ANJ_WITH_LWM2M_CBOR

/** @anj_internal_api_do_not_use */
#    define _ANJ_MAX_CBOR_NEST_STACK_SIZE                    \
        ANJ_MAX(_ANJ_MAX_SIMPLE_CBOR_NEST_STACK_SIZE,        \
                ANJ_MAX(_ANJ_MAX_SENML_CBOR_NEST_STACK_SIZE, \
                        _ANJ_MAX_LWM2M_CBOR_NEST_STACK_SIZE))
#endif // defined(ANJ_WITH_SENML_CBOR) || defined(ANJ_WITH_LWM2M_CBOR) ||
       // defined(ANJ_WITH_CBOR)

/** @anj_internal_api_do_not_use */
#define _ANJ_BASE64_ENCODED_MULTIPLIER 4
/** @anj_internal_api_do_not_use */
typedef struct {
    uint8_t buf[_ANJ_BASE64_ENCODED_MULTIPLIER];
    size_t cache_offset;
} _anj_text_encoder_b64_cache_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    // the total number of bytes to be copied, with each call of
    // _anj_io_out_ctx_get_payload() this value is decreased until all bytes are
    // read, for ANJ_WITH_EXTERNAL_* types logic is different - this field is
    // set to a fixed value
    size_t remaining_bytes;
    // the number of bytes that have been copied to out_buff from internal
    // buffer and from external source, if offset exceeds the
    // bytes_in_internal_buff, it means that external source is used
    size_t offset;
    // the number of bytes to be copied from the internal buffer, must not be
    // changed in _anj_io_out_ctx_get_payload() until last byte is read
    size_t bytes_in_internal_buff;
    bool is_extended_type;
    uint8_t internal_buff[_ANJ_IO_CTX_BUFFER_LENGTH];
    _anj_text_encoder_b64_cache_t b64_cache;

#ifdef ANJ_WITH_EXTERNAL_DATA
    // Used only for external string, in CBOR Indefinite-length strings single
    // UTF-8 character must be encoded in one chunk, so in worst case scenario
    // (4 bytes long character) up to 3 bytes must be stored for the next call
    // of _anj_io_out_ctx_get_payload()
    uint8_t utf8_buff[3];
    uint8_t bytes_in_utf8_buff;
#endif // ANJ_WITH_EXTERNAL_DATA
} _anj_io_buff_t;

#ifdef ANJ_WITH_PLAINTEXT
/** @anj_internal_api_do_not_use */
typedef struct {
    bool entry_added;
} _anj_text_encoder_t;
#endif // ANJ_WITH_PLAINTEXT

#ifdef ANJ_WITH_OPAQUE
/** @anj_internal_api_do_not_use */
typedef struct {
    bool entry_added;
} _anj_opaque_encoder_t;
#endif // ANJ_WITH_OPAQUE

#ifdef ANJ_WITH_CBOR
/** @anj_internal_api_do_not_use */
typedef struct {
    bool entry_added;
} _anj_cbor_encoder_t;
#endif // ANJ_WITH_CBOR

#ifdef ANJ_WITH_SENML_CBOR
/** @anj_internal_api_do_not_use */
typedef struct {
    bool encode_time;
    double last_timestamp;
    size_t items_count;
    anj_uri_path_t base_path;
    size_t base_path_len;
    bool first_entry_added;
} _anj_senml_cbor_encoder_t;
#endif // ANJ_WITH_SENML_CBOR

#if defined(ANJ_WITH_SENML_CBOR) || defined(ANJ_WITH_LWM2M_CBOR) \
        || defined(ANJ_WITH_CBOR)
/** @anj_internal_api_do_not_use */
typedef struct _anj_cbor_ll_decoder_struct _anj_cbor_ll_decoder_t;

/** @anj_internal_api_do_not_use */
typedef enum {
    /* decoder is operational */
    ANJ_CBOR_LL_DECODER_STATE_OK,
    /* decoder reached end of stream */
    ANJ_CBOR_LL_DECODER_STATE_FINISHED,
    /* decoder could not make sense out of some part of the stream */
    ANJ_CBOR_LL_DECODER_STATE_ERROR
} _anj_cbor_ll_decoder_state_t;

/** @anj_internal_api_do_not_use */
typedef enum {
    ANJ_CBOR_LL_VALUE_NULL,
    ANJ_CBOR_LL_VALUE_UINT,
    ANJ_CBOR_LL_VALUE_NEGATIVE_INT,
    ANJ_CBOR_LL_VALUE_BYTE_STRING,
    ANJ_CBOR_LL_VALUE_TEXT_STRING,
    ANJ_CBOR_LL_VALUE_ARRAY,
    ANJ_CBOR_LL_VALUE_MAP,
    ANJ_CBOR_LL_VALUE_FLOAT,
    ANJ_CBOR_LL_VALUE_DOUBLE,
    ANJ_CBOR_LL_VALUE_BOOL,
    ANJ_CBOR_LL_VALUE_TIMESTAMP
} _anj_cbor_ll_value_type_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    _anj_cbor_ll_value_type_t type;
    union {
        uint64_t u64;
        int64_t i64;
        float f32;
        double f64;
    } value;
} _anj_cbor_ll_number_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    // If indefinite, this contains available bytes only for the current chunk.
    size_t bytes_available;
#    ifdef ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
    // Used only for indefinite length bytes.
    size_t initial_nesting_level;
    bool indefinite;
#    endif // ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES
#    ifdef ANJ_WITH_CBOR_DECODE_STRING_TIME
    struct {
        size_t bytes_read;
        bool initialized;
        char buffer[sizeof("9999-12-31T23:59:60.999999999+99:59")];
    } string_time;
#    endif // ANJ_WITH_CBOR_DECODE_STRING_TIME
} _anj_cbor_ll_decoder_bytes_ctx_t;

/** @anj_internal_api_do_not_use */
typedef enum {
    ANJ_CBOR_LL_SUBPARSER_NONE,
    ANJ_CBOR_LL_SUBPARSER_STRING,
    ANJ_CBOR_LL_SUBPARSER_BYTES,
    ANJ_CBOR_LL_SUBPARSER_EPOCH_BASED_TIME,
#    ifdef ANJ_WITH_CBOR_DECODE_STRING_TIME
    ANJ_CBOR_LL_SUBPARSER_STRING_TIME,
#    endif // ANJ_WITH_CBOR_DECODE_STRING_TIME
#    ifdef ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS
    ANJ_CBOR_LL_SUBPARSER_DECIMAL_FRACTION,
#    endif // ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS
} _anj_cbor_ll_subparser_type_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    /* Type of the nested structure (ANJ_CBOR_LL_VALUE_BYTE_STRING,
     * ANJ_CBOR_LL_VALUE_TEXT_STRING, ANJ_CBOR_LL_VALUE_ARRAY or
     * ANJ_CBOR_LL_VALUE_MAP). */
    _anj_cbor_ll_value_type_t type;
    union {
        /* Number of items of the entry that were parsed */
        size_t total;
        /* For indefinite structures, only the even/odd state is tracked */
        bool odd;
    } items_parsed;
    /* Number of all items to be parsed (in case of definite length), or
     * NUM_ITEMS_INDEFINITE. */
    ptrdiff_t all_items;
} _anj_cbor_ll_nested_state_t;

/** @anj_internal_api_do_not_use */
struct _anj_cbor_ll_decoder_struct {
    const uint8_t *input_begin;
    const uint8_t *input;
    const uint8_t *input_end;
    bool input_last;

    uint8_t prebuffer[9];
    uint8_t prebuffer_size;
    uint8_t prebuffer_offset;

    _anj_cbor_ll_decoder_state_t state;
    bool needs_preprocessing;
    bool after_tag;
    /**
     * This structure contains information about currently processed value. The
     * value is "processed" as long as it is not fully consumed, so for example,
     * the current_item::value_type is of type "bytes" until it gets read
     * entirely by the user.
     */
    struct {
        /* Type to be decoded or currently being decoded. */
        _anj_cbor_ll_value_type_t value_type;

        /* Initial CBOR header byte of the value currently being decoded. */
        uint8_t initial_byte;
    } current_item;

    _anj_cbor_ll_subparser_type_t subparser_type;
    union {
        _anj_cbor_ll_decoder_bytes_ctx_t string_or_bytes_or_string_time;
#    ifdef ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS
        struct {
            size_t array_level;
            bool entered_array;
            double exponent;
            double mantissa;
        } decimal_fraction;
#    endif // ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS
    } subparser;

#    if _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0
    size_t nest_stack_size;
    /**
     * A stack of recently entered nested types (e.g. arrays/maps). The type
     * lands on a nest_stack, if one of the following functions is called:
     *  - anj_cbor_ll_decoder_enter_array(),
     *  - anj_cbor_ll_decoder_enter_map().
     *
     * The last element (if any) indicates what kind of recursive structure we
     * are currently parsing. If too many nest levels are found, the parser
     * exits with error.
     */
    _anj_cbor_ll_nested_state_t nest_stack[_ANJ_MAX_CBOR_NEST_STACK_SIZE];
#    endif // _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0
};
#endif // defined(ANJ_WITH_SENML_CBOR) || defined(ANJ_WITH_LWM2M_CBOR) ||
       // defined(ANJ_WITH_CBOR)

#ifdef ANJ_WITH_CBOR
/** @anj_internal_api_do_not_use */
typedef struct {
    _anj_cbor_ll_decoder_t ctx;
    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx;
    size_t bytes_consumed;
    char objlnk_buf[_ANJ_IO_CBOR_MAX_OBJLNK_STRING_SIZE];
    bool entry_parsed;
} _anj_cbor_decoder_t;
#endif // ANJ_WITH_CBOR

#ifdef ANJ_WITH_LWM2M_CBOR
/** @anj_internal_api_do_not_use */
typedef struct {
    anj_uri_path_t base_path;
    anj_uri_path_t last_path;
    uint8_t maps_opened;
    size_t items_count;
} _anj_lwm2m_cbor_encoder_t;
#endif // ANJ_WITH_LWM2M_CBOR

#ifdef ANJ_WITH_SENML_CBOR
/** @anj_internal_api_do_not_use */
typedef struct {
    bool map_entered : 1;
    bool has_name : 1;
    bool has_value : 1;
    bool has_basename : 1;
    bool path_processed : 1;
    bool label_ready : 1;

    char short_string_buf[_ANJ_IO_CBOR_MAX_OBJLNK_STRING_SIZE];
    int32_t label;

    ptrdiff_t pairs_remaining;

    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx;
    size_t bytes_consumed;
} _anj_senml_entry_parse_state_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    char path[_ANJ_IO_MAX_PATH_STRING_SIZE];
    anj_data_type_t type;
    union {
        bool boolean;
        anj_objlnk_value_t objlnk;
        _anj_cbor_ll_number_t number;
        anj_bytes_or_string_value_t bytes;
    } value;
} anj_senml_cached_entry_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    _anj_cbor_ll_decoder_t ctx;

#    ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    /* Indicates that the current operation is a composite read or a composite
     * observe */
    bool composite_read_observe : 1;
#    endif // ANJ_WITH_COMPOSITE_OPERATIONS
    bool toplevel_array_entered : 1;

    ptrdiff_t entry_count;

    /* Currently processed entry - shared between entire context chain. */
    _anj_senml_entry_parse_state_t entry_parse;
    anj_senml_cached_entry_t entry;
    /* Current basename set in the payload. */
    char basename[_ANJ_IO_MAX_PATH_STRING_SIZE];
    /* A path which must be a prefix of the currently processed `path`. */
    anj_uri_path_t base;

} _anj_senml_cbor_decoder_t;
#endif // ANJ_WITH_SENML_CBOR

#ifdef ANJ_WITH_LWM2M_CBOR
/** @anj_internal_api_do_not_use */
typedef struct {
    anj_uri_path_t path;
    uint8_t relative_paths_lengths[ANJ_URI_PATH_MAX_LENGTH];
    uint8_t relative_paths_num;
} _anj_lwm2m_cbor_path_stack_t;

/** @anj_internal_api_do_not_use */
typedef struct {
    _anj_cbor_ll_decoder_t ctx;

    bool toplevel_map_entered : 1;
    bool path_parsed : 1;
    bool in_path_array : 1;
    bool expects_map : 1;

    anj_uri_path_t base;
    _anj_lwm2m_cbor_path_stack_t path_stack;

    _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx;
    size_t bytes_consumed;
    char objlnk_buf[_ANJ_IO_CBOR_MAX_OBJLNK_STRING_SIZE];
} _anj_lwm2m_cbor_decoder_t;
#endif // ANJ_WITH_LWM2M_CBOR

#ifdef ANJ_WITH_TLV
/** @anj_internal_api_do_not_use */
typedef struct {
    anj_id_type_t type;
    size_t length;
    size_t bytes_read;
} _tlv_entry_t;

/** @anj_internal_api_do_not_use */
#    define _ANJ_TLV_MAX_DEPTH 3
/** @anj_internal_api_do_not_use */
typedef struct {
    bool want_payload;
    bool want_disambiguation;
    /** buffer provided by _anj_io_in_ctx_feed_payload */
    void *buff;
    size_t buff_size;
    size_t buff_offset;
    bool payload_finished;

    anj_uri_path_t uri_path;

    // Currently processed path
    bool has_path;
    anj_uri_path_t current_path;

    uint8_t type_field;
    size_t id_length_buff_bytes_need;
    uint8_t id_length_buff[5];
    size_t id_length_buff_read_offset;
    size_t id_length_buff_write_offset;

    _tlv_entry_t *entries;
    _tlv_entry_t entries_block[_ANJ_TLV_MAX_DEPTH];
} _anj_tlv_decoder_t;
#endif // ANJ_WITH_TLV

#ifdef ANJ_WITH_PLAINTEXT
/** @anj_internal_api_do_not_use */
typedef struct {
    /** auxiliary buffer used for accumulate data for decoding */
    union {
        /** general purpose auxiliary buffer */
        struct {
            char buf[ANJ_MAX(_ANJ_IO_CTX_DOUBLE_BUFF_STR_SIZE,
                             ANJ_MAX(ANJ_I64_STR_MAX_LEN,
                                     ANJ_U64_STR_MAX_LEN))];
            size_t size;
        } abuf;

        /** auxiliary buffer used for accumulate data for decoding used for
         *  base64 */
        struct {
            /** if input is not divisible by 4, residual is stored in this
             * buffer */
            char res_buf[3];
            size_t res_buf_size;

            /** the general idea is to use input buffer as output buffer
             * BUT it is needed to use 9 bytes long auxiliary buffer */
            char out_buf[9];
            size_t out_buf_size;
        } abuf_b64;
    } aux;

    /** buffer provided by _anj_io_in_ctx_feed_payload */
    void *buff;
    size_t buff_size;
    bool payload_finished : 1;

    bool want_payload : 1;
    bool return_eof_next_time : 1;
    bool eof_already_returned : 1;
    bool padding_detected : 1;
} _anj_text_decoder_t;
#endif // ANJ_WITH_PLAINTEXT

#ifdef ANJ_WITH_OPAQUE
/** @anj_internal_api_do_not_use */
typedef struct {
    bool want_payload : 1;
    bool payload_finished : 1;
    bool eof_already_returned : 1;
} _anj_opaque_decoder_t;
#endif // ANJ_WITH_OPAQUE

/**
 * @anj_internal_api_do_not_use
 * Register payload context
 * Do not modify this structure directly, its fields are changed during anj_io
 * api calls.
 */
typedef struct {
    _anj_io_buff_t buff;
    anj_uri_path_t last_path;
    bool first_record_added;
} _anj_io_register_ctx_t;

#ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
/**
 * @anj_internal_api_do_not_use
 * Bootstrap-Discover payload context
 * Do not modify this structure directly, its fields are changed during anj_io
 * api calls.
 */
typedef struct {
    _anj_io_buff_t buff;
    anj_uri_path_t last_path;
    anj_uri_path_t base_path;
    bool first_record_added;
    const char *uri;
} _anj_io_bootstrap_discover_ctx_t;
#endif // ANJ_WITH_BOOTSTRAP_DISCOVER

#ifdef ANJ_WITH_DISCOVER
/**
 * @anj_internal_api_do_not_use
 * Discover payload context
 * Do not modify this structure directly, its fields are changed during anj_io
 * api calls.
 */
typedef struct {
    _anj_io_buff_t buff;
    anj_uri_path_t last_path;
    anj_uri_path_t base_path;
    uint8_t depth;
    uint16_t dim_counter;
    bool first_record_added;
    _anj_attr_notification_t attr;
    size_t attr_record_len;
    size_t attr_record_offset;
} _anj_io_discover_ctx_t;
#endif // ANJ_WITH_DISCOVER

/**
 * @anj_internal_api_do_not_use
 * Payload encoding context
 * Do not modify this structure directly, its fields are changed during anj_io
 * api calls.
 */
typedef struct {
    /** format used */
    uint16_t format;
    /** currently used entry */
    const anj_io_out_entry_t *entry;
    /** internally stores a coded message for a single entry */
    _anj_io_buff_t buff;
    /** indicates if context operates on null number of records */
    bool empty;
    /** stores the encoder's internal ctx for the duration of the operation */
    union {
#ifdef ANJ_WITH_PLAINTEXT
        _anj_text_encoder_t text;
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_OPAQUE
        _anj_opaque_encoder_t opaque;
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_CBOR
        _anj_cbor_encoder_t cbor;
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_SENML_CBOR
        _anj_senml_cbor_encoder_t senml;
#endif // ANJ_WITH_SENML_CBOR
#ifdef ANJ_WITH_LWM2M_CBOR
        _anj_lwm2m_cbor_encoder_t lwm2m;
#endif // ANJ_WITH_LWM2M_CBOR
    } encoder;
} _anj_io_out_ctx_t;

/**
 * @anj_internal_api_do_not_use
 * Payload decoding context.
 *
 * Do not modify this structure directly, its fields are changed during anj_io
 * API calls.
 */
typedef struct {
    /** format used */
    uint16_t format;
    /** stores the out value for the currently processed entry */
    anj_res_value_t out_value;
    /** stores the out path for the currently processed entry */
    anj_uri_path_t out_path;
    /** stores the decoder's internal ctx for the duration of the operation */
    union {
#ifdef ANJ_WITH_PLAINTEXT
        _anj_text_decoder_t text;
#endif // ANJ_WITH_PLAINTEXT
#ifdef ANJ_WITH_OPAQUE
        _anj_opaque_decoder_t opaque;
#endif // ANJ_WITH_OPAQUE
#ifdef ANJ_WITH_CBOR
        _anj_cbor_decoder_t cbor;
#endif // ANJ_WITH_CBOR
#ifdef ANJ_WITH_SENML_CBOR
        _anj_senml_cbor_decoder_t senml_cbor;
#endif // ANJ_WITH_SENML_CBOR
#ifdef ANJ_WITH_LWM2M_CBOR
        _anj_lwm2m_cbor_decoder_t lwm2m_cbor;
#endif // ANJ_WITH_LWM2M_CBOR
#ifdef ANJ_WITH_TLV
        _anj_tlv_decoder_t tlv;
#endif // ANJ_WITH_TLV
    } decoder;
} _anj_io_in_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* ANJ_INTERNAL_IO_CTX_H */
