/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/defs.h>
#include <anj/utils.h>

#include "../exchange.h"
#include "../utils.h"
#include "attributes.h"
#include "block.h"
#include "coap.h"
#include "common.h"
#ifdef ANJ_COAP_WITH_TCP
#    include "tcp_header.h"
#endif // ANJ_COAP_WITH_TCP
#ifdef ANJ_COAP_WITH_UDP
#    include "udp_header.h"
#endif // ANJ_COAP_WITH_UDP
#include "options.h"

static int add_uri_path(anj_coap_options_t *opts, const _anj_coap_msg_t *msg) {
    int res = 0;

    if (msg->operation == ANJ_OP_BOOTSTRAP_REQ) {
        res = _anj_coap_options_add_string(opts, _ANJ_COAP_OPTION_URI_PATH,
                                           "bs");
    } else if (msg->operation == ANJ_OP_BOOTSTRAP_PACK_REQ) {
        res = _anj_coap_options_add_string(opts, _ANJ_COAP_OPTION_URI_PATH,
                                           "bspack");
    } else if (msg->operation == ANJ_OP_REGISTER) {
        res = _anj_coap_options_add_string(opts, _ANJ_COAP_OPTION_URI_PATH,
                                           "rd");
    } else if (msg->operation == ANJ_OP_INF_CON_SEND
               || msg->operation == ANJ_OP_INF_NON_CON_SEND) {
        res = _anj_coap_options_add_string(opts, _ANJ_COAP_OPTION_URI_PATH,
                                           "dp");
    } else if (msg->operation == ANJ_OP_UPDATE
               || msg->operation == ANJ_OP_DEREGISTER) {
        for (size_t i = 0; i < msg->location_path.location_count; i++) {
            res = _anj_coap_options_add_data(
                    opts, _ANJ_COAP_OPTION_URI_PATH,
                    msg->location_path.location[i],
                    msg->location_path.location_len[i]);
            _RET_IF_ERROR(res);
        }
    }

    return res;
}

static int anj_attr_create_ack_prepare(anj_coap_options_t *opts,
                                       const _anj_attr_create_ack_t *attr) {
    if (!attr->has_uri) {
        return 0;
    }
    char str_buff[ANJ_U16_STR_MAX_LEN];
    size_t str_size = anj_uint16_to_string_value(str_buff, attr->oid);
    int res = _anj_coap_options_add_data(opts, _ANJ_COAP_OPTION_LOCATION_PATH,
                                         str_buff, str_size);
    _RET_IF_ERROR(res);
    str_size = anj_uint16_to_string_value(str_buff, attr->iid);
    res = _anj_coap_options_add_data(opts, _ANJ_COAP_OPTION_LOCATION_PATH,
                                     str_buff, str_size);
    return res;
}

#ifdef ANJ_WITH_COAP_DOWNLOADER
static int anj_attr_downloader_prepare(anj_coap_options_t *opts,
                                       const _anj_attr_downloader_t *attr) {
    int res;
    for (size_t i = 0; i < attr->paths_count; i++) {
        res = _anj_coap_options_add_data(opts, _ANJ_COAP_OPTION_URI_PATH,
                                         attr->path[i], attr->path_len[i]);
        _RET_IF_ERROR(res);
    }
    return 0;
}
#endif // ANJ_WITH_COAP_DOWNLOADER

static int coap_standard_msg_options_add(anj_coap_options_t *opts,
                                         const _anj_coap_msg_t *msg) {
    int res;

    // content-format
    if (msg->payload_size) {
        if (msg->content_format != _ANJ_COAP_FORMAT_NOT_DEFINED) {
            res = _anj_coap_options_add_u16(
                    opts, _ANJ_COAP_OPTION_CONTENT_FORMAT, msg->content_format);
            _RET_IF_ERROR(res);
        } else {
            return _ANJ_ERR_INPUT_ARG;
        }
    }

    // accept option: only for BootstrapPack-Request
    if (msg->accept != _ANJ_COAP_FORMAT_NOT_DEFINED
            && msg->operation == ANJ_OP_BOOTSTRAP_PACK_REQ) {
        res = _anj_coap_options_add_u16(opts, _ANJ_COAP_OPTION_ACCEPT,
                                        msg->accept);
        _RET_IF_ERROR(res);
    }

    // uri-path
    res = add_uri_path(opts, msg);
    _RET_IF_ERROR(res);

    // observe option: only for Notify
    if ((msg->operation == ANJ_OP_INF_CON_NOTIFY
         || msg->operation == ANJ_OP_INF_INITIAL_NOTIFY
         || msg->operation == ANJ_OP_INF_NON_CON_NOTIFY)
            && msg->msg_code == ANJ_COAP_CODE_CONTENT) {
        res = _anj_coap_options_add_u32(opts, _ANJ_COAP_OPTION_OBSERVE,
                                        msg->observe_number);
        _RET_IF_ERROR(res);
    }

    // block option
    if (msg->block.block_type == ANJ_OPTION_BLOCK_1
            || msg->block.block_type == ANJ_OPTION_BLOCK_2) {
        res = _anj_block_prepare(opts, &msg->block);
        _RET_IF_ERROR(res);
    }
#ifdef ANJ_WITH_COMPOSITE_OPERATIONS
    else if (msg->block.block_type == ANJ_OPTION_BLOCK_BOTH) {
        res = _anj_block_prepare(opts, &(_anj_block_t) {
                                           .block_type = ANJ_OPTION_BLOCK_1,
                                           .number = msg->block.number,
                                           .size = msg->block.size,
                                           .more_flag = false
                                       });
        _RET_IF_ERROR(res);
        res = _anj_block_prepare(opts, &(_anj_block_t) {
                                           .block_type = ANJ_OPTION_BLOCK_2,
                                           .number = 0,
                                           .size = msg->block.size,
                                           .more_flag = true
                                       });
        _RET_IF_ERROR(res);
    }
#endif // ANJ_WITH_COMPOSITE_OPERATIONS

    // attributes: uri-query
    if (msg->operation == ANJ_OP_REGISTER || msg->operation == ANJ_OP_UPDATE) {
        res = anj_attr_register_prepare(opts, &msg->attr.register_attr);
    } else if (msg->operation == ANJ_OP_BOOTSTRAP_REQ) {
        res = anj_attr_bootstrap_prepare(opts, &msg->attr.bootstrap_attr,
                                         false);
    } else if (msg->operation == ANJ_OP_BOOTSTRAP_PACK_REQ) {
        res = anj_attr_bootstrap_prepare(opts, &msg->attr.bootstrap_attr, true);
    } else if (msg->operation == ANJ_OP_RESPONSE
               && msg->msg_code == ANJ_COAP_CODE_CREATED) {
        res = anj_attr_create_ack_prepare(opts, &msg->attr.create_attr);
    }
#ifdef ANJ_WITH_COAP_DOWNLOADER
    else if (msg->operation == ANJ_OP_COAP_DOWNLOADER_GET) {
        res = anj_attr_downloader_prepare(opts, &msg->attr.downloader_attr);
    }
#endif // ANJ_WITH_COAP_DOWNLOADER

    return res;
}

static int _anj_coap_payload_serialize(anj_coap_message_t *msg,
                                       uint8_t *buf,
                                       size_t buf_size,
                                       size_t *out_bytes_written) {
    assert(msg);
    assert(buf);
    assert(out_bytes_written);
    assert(buf_size >= msg->occupied_buff_size);

    if (msg->options && msg->options->options_number > 0) {
        anj_coap_option_t last_option =
                msg->options->options[msg->options->options_number - 1];
        msg->occupied_buff_size +=
                (size_t) ((last_option.payload + last_option.payload_len)
                          - msg->options->buff_begin);
    }

    _anj_bytes_appender_t appender =
            _anj_make_bytes_appender(buf + msg->occupied_buff_size,
                                     buf_size - msg->occupied_buff_size);

    if (msg->payload && msg->payload_size > 0) {
        if (_anj_bytes_append(&appender, &_ANJ_COAP_PAYLOAD_MARKER,
                              sizeof(_ANJ_COAP_PAYLOAD_MARKER))
                || _anj_bytes_append(&appender, msg->payload,
                                     msg->payload_size)) {
            return _ANJ_ERR_BUFF;
        }
    }

    *out_bytes_written = buf_size - appender.bytes_left;

    return 0;
}

static int recognize_msg_code(_anj_coap_msg_t *msg) {
    switch (msg->operation) {
    case ANJ_OP_BOOTSTRAP_REQ:
        msg->msg_code = ANJ_COAP_CODE_POST;
        break;
    case ANJ_OP_BOOTSTRAP_PACK_REQ:
        msg->msg_code = ANJ_COAP_CODE_GET;
        break;
    case ANJ_OP_REGISTER:
        msg->msg_code = ANJ_COAP_CODE_POST;
        break;
    case ANJ_OP_UPDATE:
        msg->msg_code = ANJ_COAP_CODE_POST;
        break;
    case ANJ_OP_DEREGISTER:
        msg->msg_code = ANJ_COAP_CODE_DELETE;
        break;
    case ANJ_OP_INF_CON_NOTIFY:
    case ANJ_OP_INF_NON_CON_NOTIFY:
        msg->msg_code = ANJ_COAP_CODE_CONTENT;
        break;
    case ANJ_OP_INF_CON_SEND:
    case ANJ_OP_INF_NON_CON_SEND:
        msg->msg_code = ANJ_COAP_CODE_POST;
        break;
    case ANJ_OP_COAP_RESET:
    case ANJ_OP_COAP_PING_UDP:
    case ANJ_OP_COAP_EMPTY_MSG:
        msg->msg_code = ANJ_COAP_CODE_EMPTY;
        break;
    case ANJ_OP_RESPONSE:
    case ANJ_OP_INF_INITIAL_NOTIFY:
        // msg code must be defined
        break;
    case ANJ_OP_COAP_CSM: /* Following operations are only valid for TCP */
        msg->msg_code = ANJ_COAP_CODE_CSM;
        break;
    case ANJ_OP_COAP_PING:
        msg->msg_code = ANJ_COAP_CODE_PING;
        break;
    case ANJ_OP_COAP_PONG:
        msg->msg_code = ANJ_COAP_CODE_PONG;
        break;
    case ANJ_OP_COAP_DOWNLOADER_GET:
        msg->msg_code = ANJ_COAP_CODE_GET;
        break;
    default:
        return _ANJ_ERR_COAP_BAD_MSG;
    }

    return 0;
}

#ifdef ANJ_COAP_WITH_UDP
static int _anj_coap_udp_header_serialize(anj_coap_message_t *msg,
                                          uint8_t *buf,
                                          size_t buf_size) {
    assert(msg);
    assert(buf);
    assert(buf_size > _ANJ_COAP_UDP_HEADER_LENGTH);

    _anj_bytes_appender_t appender = _anj_make_bytes_appender(buf, buf_size);

    uint8_t version_type_token_length =
            _anj_coap_udp_prepare_version_type_token_len_field(
                    msg->header.header_type.udp.version,
                    msg->header.header_type.udp.type, msg->header.token_length);
    if (_anj_bytes_append(&appender, &version_type_token_length,
                          sizeof(version_type_token_length))) {
        return _ANJ_ERR_BUFF;
    }

    if (_anj_bytes_append(&appender, &msg->header.code,
                          sizeof(msg->header.code))) {
        return _ANJ_ERR_BUFF;
    }

    uint16_t message_id_nbo =
            _anj_convert_be16(msg->header.header_type.udp.message_id_hbo);
    if (_anj_bytes_append(&appender, &message_id_nbo, sizeof(message_id_nbo))) {
        return _ANJ_ERR_BUFF;
    }

    if (_anj_bytes_append(&appender, msg->token, msg->header.token_length)) {
        return _ANJ_ERR_BUFF;
    }

    msg->occupied_buff_size = buf_size - appender.bytes_left;

    // prepare options buffer
    if (msg->options) {
        msg->options->buff_begin = buf + msg->occupied_buff_size;
        msg->options->buff_size = appender.bytes_left;
    }

    return 0;
}

int _anj_coap_encode_udp(_anj_coap_msg_t *msg,
                         uint8_t *out_buff,
                         size_t out_buff_size,
                         size_t *out_msg_size) {
    assert(msg);
    assert(out_buff);
    assert(out_msg_size);
    assert(out_buff_size > _ANJ_COAP_UDP_HEADER_LENGTH);

    if (msg->operation == ANJ_OP_INF_CON_NOTIFY) {
        assert(msg->token.size != 0);
        msg->coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_CONFIRMABLE;
    } else if (msg->operation == ANJ_OP_INF_NON_CON_NOTIFY) {
        assert(msg->token.size != 0);
        msg->coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_NON_CONFIRMABLE;
    } else if (msg->operation == ANJ_OP_RESPONSE
               || msg->operation == ANJ_OP_INF_INITIAL_NOTIFY) {
        assert(msg->token.size != 0);
        msg->coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
    } else if (msg->operation == ANJ_OP_COAP_RESET) {
        msg->coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_RESET;
        msg->payload_size = 0;
        msg->token.size = 0;
    } else if (msg->operation == ANJ_OP_COAP_PING_UDP) {
        msg->coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_CONFIRMABLE;
        msg->payload_size = 0;
        msg->token.size = 0;
    } else if (msg->operation == ANJ_OP_COAP_EMPTY_MSG) {
        msg->coap_binding_data.udp.type = ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT;
        msg->token.size = 0;
        msg->payload_size = 0;
    } else {
        // client request
        msg->coap_binding_data.udp.type =
                (msg->operation == ANJ_OP_INF_NON_CON_SEND)
                        ? ANJ_COAP_UDP_TYPE_NON_CONFIRMABLE
                        : ANJ_COAP_UDP_TYPE_CONFIRMABLE;
    }

    int res;
    res = recognize_msg_code(msg);
    _RET_IF_ERROR(res);

    _ANJ_COAP_OPTIONS_INIT_EMPTY(opts, ANJ_COAP_MAX_OPTIONS_NUMBER);
    anj_coap_message_t coap_msg = {
        .header = _anj_coap_udp_header_init(
                msg->coap_binding_data.udp.type,
                msg->token.size,
                msg->msg_code,
                msg->coap_binding_data.udp.message_id),
        .options = &opts,
        .payload = msg->payload,
        .payload_size = msg->payload_size,
    };
    // if the msg has no token set prior to calling this function,
    // token.size is set properly and it's safe to call memcpy here regardless
    // of msg->token.bytes
    memcpy(coap_msg.token, msg->token.bytes, msg->token.size);

    res = _anj_coap_udp_header_serialize(&coap_msg, out_buff, out_buff_size);
    _RET_IF_ERROR(res);

    res = coap_standard_msg_options_add(&opts, msg);
    _RET_IF_ERROR(res);

    return _anj_coap_payload_serialize(&coap_msg, out_buff, out_buff_size,
                                       out_msg_size);
}
#endif // ANJ_COAP_WITH_UDP

#ifdef ANJ_COAP_WITH_TCP
static int coap_tcp_signalling_msg_options_add(anj_coap_options_t *opts,
                                               const _anj_coap_msg_t *msg) {
    int res = 0;

    if (msg->operation == ANJ_OP_COAP_CSM) {
        res = _anj_coap_options_add_u32(opts, _ANJ_COAP_OPTION_MAX_MESSAGE_SIZE,
                                        msg->signalling_opts.csm.max_msg_size);
        _RET_IF_ERROR(res);

        if (msg->signalling_opts.csm.block_wise_transfer_capable) {
            res = _anj_coap_options_add_empty(
                    opts, _ANJ_COAP_OPTION_BLOCK_WISE_TRANSFER_CAPABILITY);
        }
    } else if (msg->operation == ANJ_OP_COAP_PING) {
        if (msg->signalling_opts.ping_pong.custody) {
            res = _anj_coap_options_add_empty(opts, _ANJ_COAP_OPTION_CUSTODY);
        }
    } else if (msg->operation == ANJ_OP_COAP_PONG) {
        if (msg->signalling_opts.ping_pong.custody) {
            res = _anj_coap_options_add_empty(opts, _ANJ_COAP_OPTION_CUSTODY);
        }
    } else {
        return _ANJ_ERR_COAP_BAD_MSG;
    }

    return res;
}

static int _anj_coap_tcp_header_serialize(anj_coap_message_t *msg,
                                          uint8_t *buf,
                                          size_t buf_size) {
    assert(msg);
    assert(buf);

    uint8_t msg_len;
    uint8_t actual_msg_token_offset;
    uint8_t frame_extended_length_len = 0;

    size_t options_size = 0;
    if (msg->options && msg->options->options_number > 0) {
        size_t i = msg->options->options_number - 1;
        anj_coap_option_t current_option = msg->options->options[i];

        while (!current_option.payload && i > 0) {
            current_option = msg->options->options[--i];
            options_size++;
        }

        if (current_option.payload) {
            options_size +=
                    (size_t) (current_option.payload - msg->options->buff_begin)
                    + current_option.payload_len;
        } else {
            options_size++;
        }
    }

    if (buf_size < options_size) {
        return _ANJ_ERR_BUFF;
    }

    _anj_bytes_appender_t appender =
            _anj_make_bytes_appender(buf, buf_size - options_size);

    size_t aux = options_size + msg->payload_size
                 + (size_t) ((msg->payload_size > 0) ? sizeof(uint8_t) : 0);

    if (aux < _ANJ_COAP_EXTENDED_LENGTH_MIN_8BIT) {
        actual_msg_token_offset = 2;
        msg_len = (uint8_t) aux;

        uint8_t msg_len_token_len =
                _anj_prepare_msg_len_token_len_field(msg_len,
                                                     msg->header.token_length);
        memmove(buf + actual_msg_token_offset + msg->header.token_length, buf,
                aux);
        if (_anj_bytes_append(&appender, &msg_len_token_len,
                              sizeof(msg_len_token_len))) {
            return _ANJ_ERR_BUFF;
        }

    } else if (aux < _ANJ_COAP_EXTENDED_LENGTH_MIN_16BIT) {
        actual_msg_token_offset = 3;
        msg_len = _ANJ_COAP_EXTENDED_LENGTH_UINT8;
        frame_extended_length_len = 1;

        uint8_t msg_len_token_len =
                _anj_prepare_msg_len_token_len_field(msg_len,
                                                     msg->header.token_length);
        memmove(buf + actual_msg_token_offset + msg->header.token_length, buf,
                aux);
        if (_anj_bytes_append(&appender, &msg_len_token_len,
                              sizeof(msg_len_token_len))) {
            return _ANJ_ERR_BUFF;
        }

        uint8_t frame_extended_length_nbo =
                (uint8_t) (aux - _ANJ_COAP_EXTENDED_LENGTH_MIN_8BIT);
        if (_anj_bytes_append(&appender, &frame_extended_length_nbo,
                              sizeof(frame_extended_length_nbo))) {
            return _ANJ_ERR_BUFF;
        }
    } else if (aux < _ANJ_COAP_EXTENDED_LENGTH_MIN_32BIT) {
        actual_msg_token_offset = 4;
        msg_len = _ANJ_COAP_EXTENDED_LENGTH_UINT16;
        frame_extended_length_len = 2;

        uint8_t msg_len_token_len =
                _anj_prepare_msg_len_token_len_field(msg_len,
                                                     msg->header.token_length);
        memmove(buf + actual_msg_token_offset + msg->header.token_length, buf,
                aux);
        if (_anj_bytes_append(&appender, &msg_len_token_len,
                              sizeof(msg_len_token_len))) {
            return _ANJ_ERR_BUFF;
        }

        uint16_t frame_extended_length_nbo = _anj_convert_be16(
                (uint16_t) (aux - _ANJ_COAP_EXTENDED_LENGTH_MIN_16BIT));
        if (_anj_bytes_append(&appender, &frame_extended_length_nbo,
                              sizeof(frame_extended_length_nbo))) {
            return _ANJ_ERR_BUFF;
        }
    } else {
        actual_msg_token_offset = 6;
        msg_len = _ANJ_COAP_EXTENDED_LENGTH_UINT32;
        frame_extended_length_len = 4;

        uint8_t msg_len_token_len =
                _anj_prepare_msg_len_token_len_field(msg_len,
                                                     msg->header.token_length);
        memmove(buf + actual_msg_token_offset + msg->header.token_length, buf,
                aux);
        if (_anj_bytes_append(&appender, &msg_len_token_len,
                              sizeof(msg_len_token_len))) {
            return _ANJ_ERR_BUFF;
        }

        uint32_t frame_extended_length_nbo = _anj_convert_be32(
                (uint32_t) (aux - _ANJ_COAP_EXTENDED_LENGTH_MIN_32BIT));
        if (_anj_bytes_append(&appender, &frame_extended_length_nbo,
                              sizeof(frame_extended_length_nbo))) {
            return _ANJ_ERR_BUFF;
        }
    }

    if (_anj_bytes_append(&appender, &msg->header.code,
                          sizeof(msg->header.code))) {
        return _ANJ_ERR_BUFF;
    }

    if (_anj_bytes_append(&appender, msg->token, msg->header.token_length)) {
        return _ANJ_ERR_BUFF;
    }

    msg->occupied_buff_size = 2 * sizeof(uint8_t) + frame_extended_length_len
                              + msg->header.token_length;
    return 0;
}

int _anj_coap_encode_tcp(_anj_coap_msg_t *msg,
                         uint8_t *out_buff,
                         size_t out_buff_size,
                         size_t *out_msg_size) {
    assert(msg);
    assert(out_buff);
    assert(out_msg_size);

    if (msg->operation == ANJ_OP_COAP_PONG || msg->operation == ANJ_OP_RESPONSE
            || msg->operation == ANJ_OP_INF_INITIAL_NOTIFY
            || msg->operation == ANJ_OP_INF_NON_CON_NOTIFY
            || msg->operation == ANJ_OP_INF_CON_NOTIFY) {
        // token reuse
        assert(msg->token.size != 0);
    } else if (msg->operation == ANJ_OP_COAP_EMPTY_MSG) {
        // token not defined
        msg->token.size = 0;
        msg->payload_size = 0;
    }

    int res;
    res = recognize_msg_code(msg);
    _RET_IF_ERROR(res);

    _ANJ_COAP_OPTIONS_INIT_EMPTY(opts, ANJ_COAP_MAX_OPTIONS_NUMBER);
    anj_coap_message_t coap_msg = {
        .header = _anj_coap_tcp_header_init(msg->token.size, msg->msg_code),
        .options = &opts,
        .payload = msg->payload,
        .payload_size = msg->payload_size,
    };
    memcpy(coap_msg.token, msg->token.bytes, msg->token.size);

    // prepare options buffer
    coap_msg.options->buff_begin = out_buff;
    coap_msg.options->buff_size = out_buff_size;

    if (_anj_coap_tcp_code_is_signalling_message(msg->msg_code)) {
        res = coap_tcp_signalling_msg_options_add(coap_msg.options, msg);
    } else {
        res = coap_standard_msg_options_add(coap_msg.options, msg);
    }
    _RET_IF_ERROR(res);

    res = _anj_coap_tcp_header_serialize(&coap_msg, out_buff, out_buff_size);
    _RET_IF_ERROR(res);

    return _anj_coap_payload_serialize(&coap_msg, out_buff, out_buff_size,
                                       out_msg_size);
}
#endif // ANJ_COAP_WITH_TCP

#define _ANJ_COAP_PAYLOAD_MARKER_SIZE 1
// How accept option size is calculated:
// 1 byte for option delta and length
// 1 byte for option delta (extended) - if there is no previous option
// 2 bytes for longest possible content format
// other options are calculated similarly
#define _ANJ_COAP_ACCEPT_OPTION_MAX_SIZE 4
#define _ANJ_COAP_CONTENT_FORMAT_OPTION_MAX_SIZE 3
#define _ANJ_COAP_OBSERVE_OPTION_MAX_SIZE 4
#define _ANJ_COAP_BLOCK_OPTION_MAX_SIZE 3
#define _ANJ_COAP_DP_PATH_SIZE 3
#define _ANJ_COAP_RD_PATH_SIZE 3
#define _ANJ_COAP_BS_PATH_SIZE 3
#define _ANJ_COAP_BSPACK_PATH_SIZE 7
// URI QUERY and URI PATH size are calculated slightly differently:
// 1 byte for option delta and length
// 1 byte for option delta in case of URI QUERY (extended)
// 2 bytes for longest possible option length (extended)
// option value are calculated separately in the functions below
#define _ANJ_COAP_URI_QUERY_HEADER_MAX_SIZE 4
#define _ANJ_COAP_URI_PATH_HEADER_MAX_SIZE 3
// 2*1 byte for location path option delta and length
// 2*5 bytes for longest possible location path /65123
#define _ANJ_COAP_CREATE_ACK_PATH_SIZE 12

// Single uri query option size calculation:
// _ANJ_COAP_URI_QUERY_HEADER_MAX_SIZE + strlen(arg name) + 1('=') +
// strlen(option)
static size_t
calculate_register_uri_query_size(const _anj_attr_register_t *attr) {
    size_t max_size = 0;

    if (attr->has_Q) {
        // Q -> 2 bytes for option delta and extended delta option,
        // 1 byte for 'Q'
        max_size += 3;
    }
    if (attr->has_endpoint) {
        max_size += _ANJ_COAP_URI_QUERY_HEADER_MAX_SIZE + 3
                    + (attr->endpoint ? strlen(attr->endpoint)
                                      : 0); // ep = <endpoint>
    }
    if (attr->has_lifetime) {
        max_size += _ANJ_COAP_URI_QUERY_HEADER_MAX_SIZE + 3
                    + ANJ_U32_STR_MAX_LEN; // lt = <lifetime>
    }
    if (attr->has_lwm2m_ver) {
        max_size += _ANJ_COAP_URI_QUERY_HEADER_MAX_SIZE + 5
                    + (attr->lwm2m_ver ? strlen(attr->lwm2m_ver)
                                       : 0); // lwm2m = <lwm2m_ver>
    }
    if (attr->has_binding) {
        max_size +=
                _ANJ_COAP_URI_QUERY_HEADER_MAX_SIZE + 3
                + (attr->binding ? strlen(attr->binding) : 0); // b = <binding>
    }
    if (attr->has_sms_number) {
        max_size += _ANJ_COAP_URI_QUERY_HEADER_MAX_SIZE + 4
                    + (attr->sms_number ? strlen(attr->sms_number)
                                        : 0); // sms = <sms_number>
    }
    return max_size;
}

static size_t
calculate_bootstrap_uri_query_size(const _anj_attr_bootstrap_t *attr,
                                   bool bootstrap_pack) {
    size_t max_size = 0;

    if (attr->has_endpoint) {
        max_size += _ANJ_COAP_URI_QUERY_HEADER_MAX_SIZE + 3
                    + (attr->endpoint ? strlen(attr->endpoint)
                                      : 0); // ep = <endpoint>
    }
    if (attr->has_preferred_content_format && !bootstrap_pack) {
        max_size += _ANJ_COAP_URI_QUERY_HEADER_MAX_SIZE + 3
                    + ANJ_U16_STR_MAX_LEN; // pcf = <preferred_content_format>
    }
    return max_size;
}

static size_t calculate_location_path_size(const _anj_location_path_t *path) {
    size_t max_size = 0;
    for (size_t i = 0; i < path->location_count; i++) {
        max_size += _ANJ_COAP_URI_PATH_HEADER_MAX_SIZE + path->location_len[i];
    }
    return max_size;
}

size_t _anj_coap_calculate_msg_header_max_size(const _anj_coap_msg_t *msg) {
    size_t max_size = 0;
    // TODO: add tcp support here
    max_size = _ANJ_COAP_UDP_HEADER_LENGTH + _ANJ_COAP_MAX_TOKEN_LENGTH
               + _ANJ_COAP_PAYLOAD_MARKER_SIZE;

    // calculate options size
    if (msg->operation == ANJ_OP_INF_INITIAL_NOTIFY
            || msg->operation == ANJ_OP_INF_CON_NOTIFY
            || msg->operation == ANJ_OP_INF_NON_CON_NOTIFY) {
        max_size += _ANJ_COAP_OBSERVE_OPTION_MAX_SIZE
                    + _ANJ_COAP_CONTENT_FORMAT_OPTION_MAX_SIZE
                    + _ANJ_COAP_BLOCK_OPTION_MAX_SIZE;
    } else if (msg->operation == ANJ_OP_INF_CON_SEND
               || msg->operation == ANJ_OP_INF_NON_CON_SEND) {
        max_size += _ANJ_COAP_DP_PATH_SIZE
                    + _ANJ_COAP_CONTENT_FORMAT_OPTION_MAX_SIZE
                    + _ANJ_COAP_BLOCK_OPTION_MAX_SIZE;
    } else if (msg->operation == ANJ_OP_REGISTER) {
        max_size +=
                _ANJ_COAP_RD_PATH_SIZE
                + _ANJ_COAP_CONTENT_FORMAT_OPTION_MAX_SIZE
                + _ANJ_COAP_BLOCK_OPTION_MAX_SIZE
                + calculate_register_uri_query_size(&msg->attr.register_attr);
    } else if (msg->operation == ANJ_OP_UPDATE) {
        max_size += calculate_register_uri_query_size(&msg->attr.register_attr)
                    + calculate_location_path_size(&msg->location_path)
                    + _ANJ_COAP_CONTENT_FORMAT_OPTION_MAX_SIZE
                    + _ANJ_COAP_BLOCK_OPTION_MAX_SIZE;
    } else if (msg->operation == ANJ_OP_DEREGISTER) {
        max_size += calculate_location_path_size(&msg->location_path);
    } else if (msg->operation == ANJ_OP_BOOTSTRAP_REQ) {
        max_size +=
                calculate_bootstrap_uri_query_size(&msg->attr.bootstrap_attr,
                                                   false)
                + _ANJ_COAP_BS_PATH_SIZE;
    } else if (msg->operation == ANJ_OP_BOOTSTRAP_PACK_REQ) {
        max_size +=
                calculate_bootstrap_uri_query_size(&msg->attr.bootstrap_attr,
                                                   true)
                + _ANJ_COAP_BSPACK_PATH_SIZE + _ANJ_COAP_ACCEPT_OPTION_MAX_SIZE;
    } else if (msg->operation == ANJ_OP_RESPONSE
               && msg->attr.create_attr.has_uri) {
        max_size += _ANJ_COAP_CREATE_ACK_PATH_SIZE;
    }
    return max_size;
}
