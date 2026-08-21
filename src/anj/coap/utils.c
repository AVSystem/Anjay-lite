/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "../init_internal.h"

#define ANJ_LOG_SOURCE_FILE_ID 64

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/compat/rng.h>
#include <anj/log.h>
#include <anj/utils.h>

#define ANJ_INTERNAL_INCLUDE_COAP
#include <anj_internal/coap.h>
#undef ANJ_INTERNAL_INCLUDE_COAP

#include "../utils.h"
#include "coap.h"
#include "utils.h"

int _anj_coap_token_generate(_anj_coap_token_t *token) {
    token->size = _ANJ_COAP_MAX_TOKEN_LENGTH;
    assert(_ANJ_COAP_MAX_TOKEN_LENGTH == 8);
    uint64_t random;
    return anj_rng_generate((uint8_t *) &random, sizeof(random))
                   ? -1
                   : ((void) memcpy(token->bytes, &random, sizeof(random)), 0);
}

static const char *
_anj_debug_content_format_to_string(uint16_t content_format) {
    switch (content_format) {
    case _ANJ_COAP_FORMAT_PLAINTEXT:
        return "Plain Text";
    case _ANJ_COAP_FORMAT_LINK_FORMAT:
        return "CoRE Link Format";
    case _ANJ_COAP_FORMAT_OPAQUE_STREAM:
        return "Opaque";
    case _ANJ_COAP_FORMAT_CBOR:
        return "CBOR";
    case _ANJ_COAP_FORMAT_SENML_CBOR:
    case _ANJ_COAP_FORMAT_SENML_ETCH_CBOR:
        return "SenML CBOR";
    case _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR:
        return "LwM2M CBOR";
    case _ANJ_COAP_FORMAT_OMA_LWM2M_TLV:
        return "LwM2M TLV";
    default:
        return "unknown/unsupported";
    }
}

void _anj_log_msg_info(const _anj_coap_msg_t *msg, bool received) {
    // Refer to the helper so that compilers do not warn when all
    // L_DEBUG uses are compiled out.
    (void) _anj_debug_content_format_to_string;

    char payload_info[] = ", payload: 65535B";
    if (msg->payload_size > 0 && msg->payload_size < UINT16_MAX) {
        size_t written =
                anj_uint16_to_string_value(&payload_info[11],
                                           (uint16_t) msg->payload_size);
        payload_info[11 + written] = 'B';
        payload_info[12 + written] = '\0';
    } else {
        payload_info[0] = '\0';
    }
    char block_info[] = ", block1: num=4294967295 m=1";
    if (msg->block.block_type != _ANJ_OPTION_BLOCK_NOT_DEFINED) {
        if (msg->block.block_type == _ANJ_OPTION_BLOCK_2) {
            block_info[7] = '2';
        }
        size_t written =
                anj_uint32_to_string_value(&block_info[14], msg->block.number);
        memcpy(&block_info[14 + written], " m=", 3);
        block_info[17 + written] = msg->block.more_flag ? '1' : '0';
        block_info[18 + written] = '\0';
    } else {
        block_info[0] = '\0';
    }

    anj_log(coap, L_DEBUG,
            "%s: %s %s" // operation type and code
            "%s%s"      // URI path if present
            "%s%s"      // content-format if present
            "%s%s" // accept option if present, but only for incoming requests
            "%s"   // payload info (prebuilt)
            "%s"   // block info (prebuilt)
            ", token: %s", // token
            received ? "Received" : "Sending",
            _anj_debug_coap_operation_to_string(msg->operation),
            (_anj_coap_is_response(msg->operation)
                     ? _ANJ_DEBUG_COAP_RESPONSE_CODE(msg->msg_code)
                     : ""),
            (msg->uri.uri_len > 0 ? ", URI Path: " : ""),
            (msg->uri.uri_len > 0 ? _ANJ_DEBUG_URI_PATH(&msg->uri) : ""),
            (msg->content_format != _ANJ_COAP_FORMAT_NOT_DEFINED
                     ? ", Content-Format: "
                     : ""),
            (msg->content_format != _ANJ_COAP_FORMAT_NOT_DEFINED
                     ? _anj_debug_content_format_to_string(msg->content_format)
                     : ""),
            ((msg->accept != _ANJ_COAP_FORMAT_NOT_DEFINED && received)
                     ? ", Accept: "
                     : ""),
            ((msg->accept != _ANJ_COAP_FORMAT_NOT_DEFINED && received)
                     ? _anj_debug_content_format_to_string(msg->accept)
                     : ""),
            payload_info, block_info, _ANJ_DEBUG_COAP_TOKEN(&msg->token));
}
