/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_COAP_COMMON_H
#    define SRC_ANJ_COAP_COMMON_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/defs.h>

#    include "coap.h"
#    include "options.h"

/** CoAP payload marker */
#    define _ANJ_COAP_PAYLOAD_MARKER ((uint8_t) { 0xFF })

#    ifdef ANJ_COAP_WITH_TCP
#        define _ANJ_COAP_EXTENDED_LENGTH_MIN_8BIT 13U
#        define _ANJ_COAP_EXTENDED_LENGTH_MIN_16BIT 269U
#        define _ANJ_COAP_EXTENDED_LENGTH_MIN_32BIT 65805U

#        define _ANJ_COAP_EXTENDED_LENGTH_UINT8 13U
#        define _ANJ_COAP_EXTENDED_LENGTH_UINT16 14U
#        define _ANJ_COAP_EXTENDED_LENGTH_UINT32 15U
#    endif // ANJ_COAP_WITH_TCP

#    define _ANJ_FIELD_GET(field, mask, shift) \
        ((uint8_t) ((((uint32_t) (field)) & ((uint32_t) (mask))) >> (shift)))
#    define _ANJ_FIELD_SET(field, mask, shift, value)                      \
        (field) = (uint8_t) ((((uint32_t) (field)) & ~((uint32_t) (mask))) \
                             | ((((uint32_t) (value) << (shift))           \
                                 & ((uint32_t) (mask)))))

#    define _RET_IF_ERROR(Val) \
        if (Val) {             \
            return Val;        \
        }

typedef struct {
    uint8_t msg_length;
    uint32_t extended_length_hbo;
} anj_coap_tcp_header_t;

#    define _ANJ_COAP_MESSAGE_ID_LEN 2
typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t message_id_hbo;
} anj_coap_udp_header_t;

typedef struct {
    union {
        anj_coap_tcp_header_t tcp;
        anj_coap_udp_header_t udp;
    } header_type;

    uint8_t code;
    uint8_t token_length;
} anj_coap_header_t;

typedef struct {
    anj_coap_header_t header;
    char token[_ANJ_COAP_MAX_TOKEN_LENGTH];
    anj_coap_options_t *options;
    uint8_t *payload;
    size_t payload_size;
    size_t occupied_buff_size;
} anj_coap_message_t;

typedef struct {
    uint8_t *write_ptr;
    size_t bytes_left;
} _anj_bytes_appender_t;

typedef struct {
    const uint8_t *read_ptr;
    size_t bytes_left;
} anj_bytes_dispenser_t;

static inline uint8_t _anj_code_get_class(uint8_t code) {
    return (uint8_t) _ANJ_FIELD_GET(code, (uint8_t) ANJ_COAP_CODE_CLASS_MASK,
                                    (uint8_t) ANJ_COAP_CODE_CLASS_SHIFT);
}

static inline uint8_t _anj_code_get_detail(uint8_t code) {
    return (uint8_t) _ANJ_FIELD_GET(code, (uint8_t) ANJ_COAP_CODE_DETAIL_MASK,
                                    (uint8_t) ANJ_COAP_CODE_DETAIL_SHIFT);
}

static inline bool _anj_coap_code_is_request(uint8_t code) {
    return _anj_code_get_class(code) == 0 && _anj_code_get_detail(code) > 0;
}

static inline _anj_bytes_appender_t
_anj_make_bytes_appender(void *data, size_t size_bytes) {
    return (_anj_bytes_appender_t) { (uint8_t *) data, size_bytes };
}

static inline anj_bytes_dispenser_t
_anj_make_bytes_dispenser(void *data, size_t size_bytes) {
    return (anj_bytes_dispenser_t) { (uint8_t *) data, size_bytes };
}

int _anj_bytes_append(_anj_bytes_appender_t *appender,
                      const void *data,
                      size_t size_bytes);

int _anj_bytes_extract(anj_bytes_dispenser_t *dispenser,
                       void *out,
                       size_t size_bytes);

#endif // SRC_ANJ_COAP_COMMON_H
