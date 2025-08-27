/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_COAP_OPTIONS_H
#    define SRC_ANJ_COAP_OPTIONS_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>
#    include <string.h>

/**
 * CoAP option numbers, as defined in RFC7252/RFC7641/RFC7959.
 * @{
 */
// clang-format off
#define _ANJ_COAP_OPTION_IF_MATCH          1
#define _ANJ_COAP_OPTION_URI_HOST          3
#define _ANJ_COAP_OPTION_ETAG              4
#define _ANJ_COAP_OPTION_IF_NONE_MATCH     5
#define _ANJ_COAP_OPTION_OBSERVE           6
#define _ANJ_COAP_OPTION_URI_PORT          7
#define _ANJ_COAP_OPTION_LOCATION_PATH     8
#define _ANJ_COAP_OPTION_OSCORE            9
#define _ANJ_COAP_OPTION_URI_PATH          11
#define _ANJ_COAP_OPTION_CONTENT_FORMAT    12
#define _ANJ_COAP_OPTION_MAX_AGE           14
#define _ANJ_COAP_OPTION_URI_QUERY         15
#define _ANJ_COAP_OPTION_ACCEPT            17
#define _ANJ_COAP_OPTION_LOCATION_QUERY    20
#define _ANJ_COAP_OPTION_BLOCK2            23
#define _ANJ_COAP_OPTION_BLOCK1            27
#define _ANJ_COAP_OPTION_PROXY_URI         35
#define _ANJ_COAP_OPTION_PROXY_SCHEME      39
#define _ANJ_COAP_OPTION_SIZE1             60
#define _ANJ_COAP_OPTION_SIZE2             28

/**
 * CoAP Signaling option codes, as defined in RFC 8323.
 * Codes reused between different options. Meaning depends on message code.
 */
#define _ANJ_COAP_OPTION_MAX_MESSAGE_SIZE                  2
#define _ANJ_COAP_OPTION_BLOCK_WISE_TRANSFER_CAPABILITY    4
#define _ANJ_COAP_OPTION_CUSTODY                           2
#define _ANJ_COAP_OPTION_ALTERNATIVE_ADDRESS               2
#define _ANJ_COAP_OPTION_HOLD_OFF                          4
#define _ANJ_COAP_OPTION_BAD_CSM_OPTION                    2
// clang-format on

/**
 * Constant returned from some of option-retrieving functions, indicating
 * the absence of requested option.
 */
#    define _ANJ_COAP_OPTION_MISSING 1

#    define _ANJ_COAP_OPTIONS_INIT_EMPTY(Name, OptionsSize) \
        anj_coap_option_t _Opt##Name[OptionsSize];          \
        anj_coap_options_t Name = {                         \
            .options_size = OptionsSize,                    \
            .options_number = 0,                            \
            .options = _Opt##Name,                          \
            .buff_size = 0,                                 \
            .buff_begin = NULL                              \
        }

#    define _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF(          \
            Name, OptionsSize, MsgBuffSize)                  \
        anj_coap_option_t _Opt##Name[OptionsSize];           \
        uint8_t _OptionMsgBuffer##Name[MsgBuffSize] = { 0 }; \
        anj_coap_options_t Name = {                          \
            .options_size = OptionsSize,                     \
            .options_number = 0,                             \
            .options = _Opt##Name,                           \
            .buff_size = MsgBuffSize,                        \
            .buff_begin = (void *) _OptionMsgBuffer##Name    \
        }

typedef struct anj_coap_option {
    const uint8_t *payload;
    size_t payload_len;
    uint16_t option_number;
} anj_coap_option_t;

/**size_t *iterator
 * Note: this struct MUST be initialized with
 * @ref _ANJ_COAP_OPTIONS_INIT_EMPTY or _ANJ_COAP_OPTIONS_INIT_EMPTY_WITH_BUFF
 * before it is used.
 */
typedef struct anj_coap_options {
    anj_coap_option_t *options;
    size_t options_size;
    size_t options_number;

    uint8_t *buff_begin;
    size_t buff_size;
} anj_coap_options_t;

int _anj_coap_options_decode(anj_coap_options_t *opts,
                             const uint8_t *msg,
                             size_t msg_size,
                             size_t *bytes_read);

/*
 * - 0 on success,
 * - _ANJ_COAP_OPTION_MISSING when there are no more options with
 *              given @p option_number to retrieve
 */
int _anj_coap_options_get_data_iterate(const anj_coap_options_t *opts,
                                       uint16_t option_number,
                                       size_t *iterator,
                                       size_t *out_option_size,
                                       void *out_buffer,
                                       size_t out_buffer_size);

int _anj_coap_options_get_empty_iterate(const anj_coap_options_t *opts,
                                        uint16_t option_number,
                                        size_t *iterator,
                                        bool *out_opt);

int _anj_coap_options_get_string_iterate(const anj_coap_options_t *opts,
                                         uint16_t option_number,
                                         size_t *iterator,
                                         size_t *out_option_size,
                                         char *out_buffer,
                                         size_t out_buffer_size);

int _anj_coap_options_get_u16_iterate(const anj_coap_options_t *opts,
                                      uint16_t option_number,
                                      size_t *iterator,
                                      uint16_t *out_value);

int _anj_coap_options_get_u32_iterate(const anj_coap_options_t *opts,
                                      uint16_t option_number,
                                      size_t *iterator,
                                      uint32_t *out_value);

int _anj_coap_options_add_data(anj_coap_options_t *opts,
                               uint16_t opt_number,
                               const void *data,
                               size_t data_size);

static inline int _anj_coap_options_add_string(anj_coap_options_t *opts,
                                               uint16_t opt_number,
                                               const char *data) {
    return _anj_coap_options_add_data(opts, opt_number, data, strlen(data));
}

int _anj_coap_options_add_empty(anj_coap_options_t *opts, uint16_t opt_number);

int _anj_coap_options_add_u16(anj_coap_options_t *opts,
                              uint16_t opt_number,
                              uint16_t value);

int _anj_coap_options_add_u32(anj_coap_options_t *opts,
                              uint16_t opt_number,
                              uint32_t value);

int _anj_coap_options_add_u64(anj_coap_options_t *opts,
                              uint16_t opt_number,
                              uint64_t value);

#endif // SRC_ANJ_COAP_OPTIONS_H
