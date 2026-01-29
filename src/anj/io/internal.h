/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_IO_INTERNAL_H
#    define SRC_ANJ_IO_INTERNAL_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/defs.h>

#    include "io.h"

#    define CBOR_TAG_INTEGER_DATE_TIME 0x01

/**
 * Enumeration for supported SenML labels. Their numeric values correspond to
 * their CBOR representation wherever possible.
 */
typedef enum {
    SENML_LABEL_BASE_TIME = -3,
    SENML_LABEL_BASE_NAME = -2,
    SENML_LABEL_NAME = 0,
    SENML_LABEL_VALUE = 2,
    SENML_LABEL_VALUE_STRING = 3,
    SENML_LABEL_VALUE_BOOL = 4,
    SENML_LABEL_TIME = 6,
    SENML_LABEL_VALUE_OPAQUE = 8,
    /* NOTE: Objlnk is represented as a string "vlo" */
    SENML_EXT_LABEL_OBJLNK = 0x766C6F /* "vlo */
} senml_label_t;

#    define SENML_EXT_OBJLNK_REPR "vlo"

/* See "2.1.  Major Types" in RFC 7049 */
typedef enum {
    _CBOR_MAJOR_TYPE_BEGIN = 0,
    CBOR_MAJOR_TYPE_UINT = 0,
    CBOR_MAJOR_TYPE_NEGATIVE_INT = 1,
    CBOR_MAJOR_TYPE_BYTE_STRING = 2,
    CBOR_MAJOR_TYPE_TEXT_STRING = 3,
    CBOR_MAJOR_TYPE_ARRAY = 4,
    CBOR_MAJOR_TYPE_MAP = 5,
    CBOR_MAJOR_TYPE_TAG = 6,
    CBOR_MAJOR_TYPE_FLOAT_OR_SIMPLE_VALUE = 7,
    _CBOR_MAJOR_TYPE_END
} cbor_major_type_t;

typedef enum {
    /**
     * Section "2.  Specification of the CBOR Encoding":
     *
     * > When it [5 lower bits of major type] is 24 to 27, the additional
     * > bytes for a variable-length integer immediately follow; the values 24
     * > to 27 of the additional information specify that its length is a 1-,
     * > 2-, 4-, or 8-byte unsigned integer, respectively.
     *
     * > Additional informationvalue 31 is used for indefinite-length items,
     * > described in Section 2.2.  Additional information values 28 to 30 are
     * > reserved for future expansion.
     */
    CBOR_EXT_LENGTH_1BYTE = 24,
    CBOR_EXT_LENGTH_2BYTE = 25,
    CBOR_EXT_LENGTH_4BYTE = 26,
    CBOR_EXT_LENGTH_8BYTE = 27,
    CBOR_EXT_LENGTH_INDEFINITE = 31
} cbor_ext_length_t;

/**
 * This enum is for:
 *
 * > Major type 7:  floating-point numbers and simple data types that need
 * > no content, as well as the "break" stop code.  See Section 2.3.
 */
typedef enum {
    /* See "2.3.  Floating-Point Numbers and Values with No Content" */
    CBOR_VALUE_BOOL_FALSE = 20,
    CBOR_VALUE_BOOL_TRUE = 21,
    CBOR_VALUE_NULL = 22,
    CBOR_VALUE_UNDEFINED = 23,
    CBOR_VALUE_IN_NEXT_BYTE = CBOR_EXT_LENGTH_1BYTE,
    CBOR_VALUE_FLOAT_16 = CBOR_EXT_LENGTH_2BYTE,
    CBOR_VALUE_FLOAT_32 = CBOR_EXT_LENGTH_4BYTE,
    CBOR_VALUE_FLOAT_64 = CBOR_EXT_LENGTH_8BYTE
} cbor_primitive_value_t;

#    define CBOR_INDEFINITE_STRUCTURE_BREAK 0xFF

void _anj_io_reset_internal_buff(_anj_io_buff_t *ctx);

size_t _anj_io_out_add_objlink(_anj_io_buff_t *buff_ctx,
                               size_t buf_pos,
                               anj_oid_t oid,
                               anj_iid_t iid);

int _anj_io_add_link_format_record(const anj_uri_path_t *uri_path,
                                   const char *version,
                                   const uint16_t *dim,
                                   bool first_record,
                                   _anj_io_buff_t *ctx);

void _anj_io_get_payload_from_internal_buff(_anj_io_buff_t *ctx,
                                            void *out_buff,
                                            size_t out_buff_len,
                                            size_t *copied_bytes);

#    ifdef ANJ_WITH_EXTERNAL_DATA
int _anj_call_get_external_data(const anj_io_out_entry_t *entry,
                                void *buffer,
                                size_t *inout_size,
                                size_t offset);
#    endif // ANJ_WITH_EXTERNAL_DATA

#endif // SRC_ANJ_IO_INTERNAL_H
