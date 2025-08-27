/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_COAP_UDP_H
#    define SRC_ANJ_COAP_UDP_H

#    include <assert.h>
#    include <stddef.h>
#    include <stdint.h>
#    include <string.h>

#    include <anj/defs.h>

#    include "common.h"

#    define _ANJ_COAP_UDP_HEADER_LENGTH 4

#    define _ANJ_COAP_UDP_HEADER_VERSION_MASK 0xC0
#    define _ANJ_COAP_UDP_HEADER_VERSION_SHIFT 6

static inline uint8_t
_anj_coap_udp_header_get_version(uint8_t version_type_token_length) {
    uint8_t val = _ANJ_FIELD_GET(version_type_token_length,
                                 _ANJ_COAP_UDP_HEADER_VERSION_MASK,
                                 _ANJ_COAP_UDP_HEADER_VERSION_SHIFT);
    assert(val <= 3);
    return val;
}

static inline void
_anj_coap_udp_header_set_version(uint8_t *version_type_token_length,
                                 uint8_t version) {
    assert(version_type_token_length);
    assert(version <= 3);
    _ANJ_FIELD_SET(*version_type_token_length,
                   _ANJ_COAP_UDP_HEADER_VERSION_MASK,
                   _ANJ_COAP_UDP_HEADER_VERSION_SHIFT, version);
}

#    define _ANJ_COAP_UDP_HEADER_TOKEN_LENGTH_MASK 0x0F
#    define _ANJ_COAP_UDP_HEADER_TOKEN_LENGTH_SHIFT 0

static inline uint8_t
_anj_coap_udp_header_get_token_length(uint8_t version_type_token_length) {
    uint8_t val = _ANJ_FIELD_GET(version_type_token_length,
                                 _ANJ_COAP_UDP_HEADER_TOKEN_LENGTH_MASK,
                                 _ANJ_COAP_UDP_HEADER_TOKEN_LENGTH_SHIFT);
    assert(val <= _ANJ_COAP_UDP_HEADER_TOKEN_LENGTH_MASK);
    return val;
}

static inline void
_anj_coap_udp_header_set_token_length(uint8_t *version_type_token_length,
                                      uint8_t token_length) {
    assert(version_type_token_length);
    _ANJ_FIELD_SET(*version_type_token_length,
                   _ANJ_COAP_UDP_HEADER_TOKEN_LENGTH_MASK,
                   _ANJ_COAP_UDP_HEADER_TOKEN_LENGTH_SHIFT, token_length);
}

#    define _ANJ_COAP_UDP_HEADER_TYPE_MASK 0x30
#    define _ANJ_COAP_UDP_HEADER_TYPE_SHIFT 4

static inline _anj_coap_udp_type_t
_anj_coap_udp_header_get_type(uint8_t version_type_token_length) {
    uint8_t val = _ANJ_FIELD_GET(version_type_token_length,
                                 _ANJ_COAP_UDP_HEADER_TYPE_MASK,
                                 _ANJ_COAP_UDP_HEADER_TYPE_SHIFT);
    return (_anj_coap_udp_type_t) val;
}

static inline void
_anj_coap_udp_header_set_type(uint8_t *version_type_token_length,
                              _anj_coap_udp_type_t type) {
    assert(version_type_token_length);
    _ANJ_FIELD_SET(*version_type_token_length, _ANJ_COAP_UDP_HEADER_TYPE_MASK,
                   _ANJ_COAP_UDP_HEADER_TYPE_SHIFT, type);
}

static inline uint8_t _anj_coap_udp_prepare_version_type_token_len_field(
        uint8_t version, uint8_t type, uint8_t token_len) {
    uint8_t version_type_token_length = 0;
    _anj_coap_udp_header_set_version(&version_type_token_length, version);
    _anj_coap_udp_header_set_type(&version_type_token_length,
                                  (_anj_coap_udp_type_t) type);
    _anj_coap_udp_header_set_token_length(&version_type_token_length,
                                          token_len);
    return version_type_token_length;
}

static inline void _anj_coap_udp_header_set(anj_coap_header_t *coap_header,
                                            _anj_coap_udp_type_t type,
                                            uint8_t token_length,
                                            uint8_t code,
                                            uint16_t message_id_hbo) {
    coap_header->header_type.udp.version = 1;
    coap_header->header_type.udp.type = (uint8_t) type;
    coap_header->token_length = token_length;
    coap_header->code = code;
    coap_header->header_type.udp.message_id_hbo = message_id_hbo;
}

static inline anj_coap_header_t
_anj_coap_udp_header_init(_anj_coap_udp_type_t type,
                          uint8_t token_length,
                          uint8_t code,
                          uint16_t message_id_hbo) {
    anj_coap_header_t coap_header = { 0 };
    _anj_coap_udp_header_set(&coap_header, type, token_length, code,
                             message_id_hbo);
    return coap_header;
}

#endif // SRC_ANJ_COAP_UDP_H
