/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_COAP_TCP_H
#    define SRC_ANJ_COAP_TCP_H

#    include <assert.h>
#    include <stddef.h>
#    include <stdint.h>
#    include <string.h>

#    include <anj/defs.h>

#    include "common.h"

#    define _ANJ_SIGNALLING_MESSAGE_CODE_CLASS 7

static inline bool _anj_coap_tcp_code_is_signalling_message(uint8_t code) {
    return _anj_code_get_class(code) == _ANJ_SIGNALLING_MESSAGE_CODE_CLASS;
}

#    define _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_MASK 0xF0
#    define _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_SHIFT 4

static inline uint8_t
_anj_coap_tcp_header_get_message_length(uint8_t msg_len_token_len) {
    uint8_t message_length =
            _ANJ_FIELD_GET(msg_len_token_len,
                           _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_MASK,
                           _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_SHIFT);
    assert(message_length <= _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_MASK
           >> _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_SHIFT);
    return message_length;
}

static inline void
_anj_coap_tcp_header_set_message_length(uint8_t *msg_len_token_len,
                                        uint8_t message_length) {
    assert(msg_len_token_len);
    assert(message_length <= _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_MASK
           >> _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_SHIFT);
    _ANJ_FIELD_SET(*msg_len_token_len, _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_MASK,
                   _ANJ_COAP_TCP_HEADER_MESSAGE_LENGTH_SHIFT, message_length);
}

#    define _ANJ_COAP_TCP_HEADER_TOKEN_LENGTH_MASK 0x0F
#    define _ANJ_COAP_TCP_HEADER_TOKEN_LENGTH_SHIFT 0

static inline uint8_t
_anj_coap_tcp_header_get_token_length(uint8_t msg_len_token_len) {
    uint8_t token_length =
            _ANJ_FIELD_GET(msg_len_token_len,
                           _ANJ_COAP_TCP_HEADER_TOKEN_LENGTH_MASK,
                           _ANJ_COAP_TCP_HEADER_TOKEN_LENGTH_SHIFT);

    return token_length;
}

static inline void
_anj_coap_tcp_header_set_token_length(uint8_t *msg_len_token_len,
                                      uint8_t token_length) {
    assert(msg_len_token_len);
    _ANJ_FIELD_SET(*msg_len_token_len, _ANJ_COAP_TCP_HEADER_TOKEN_LENGTH_MASK,
                   _ANJ_COAP_TCP_HEADER_TOKEN_LENGTH_SHIFT, token_length);
}

static inline uint8_t _anj_prepare_msg_len_token_len_field(uint8_t msg_len,
                                                           uint8_t token_len) {
    uint8_t msg_len_token_len = 0;
    _anj_coap_tcp_header_set_message_length(&msg_len_token_len, msg_len);
    _anj_coap_tcp_header_set_token_length(&msg_len_token_len, token_len);
    return msg_len_token_len;
}

static inline void _anj_coap_tcp_header_set(anj_coap_header_t *coap_header,
                                            uint8_t token_len,
                                            uint8_t code) {
    coap_header->token_length = token_len;
    coap_header->code = code;
}

static inline anj_coap_header_t _anj_coap_tcp_header_init(uint8_t token_len,
                                                          uint8_t code) {
    anj_coap_header_t coap_header = { 0 };
    _anj_coap_tcp_header_set(&coap_header, token_len, code);
    return coap_header;
}

#endif // SRC_ANJ_COAP_TCP_H
