/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 4

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <anj/defs.h>
#include <anj/utils.h>

#include "../utils.h"
#include "attributes.h"
#include "block.h"
#include "coap.h"
#include "common.h"

#include "options.h"
#include "udp_header.h"

#define _URI_PATH_MAX_LEN_STR sizeof("65534")

static int get_uri_path(anj_coap_options_t *options,
                        anj_uri_path_t *uri,
                        bool *is_bs_uri) {
    size_t it = 0;
    size_t out_option_size;
    char buff[_URI_PATH_MAX_LEN_STR];
    int res;

    *is_bs_uri = false;
    uri->uri_len = 0;

    while (true) {
        res = _anj_coap_options_get_data_iterate(options,
                                                 _ANJ_COAP_OPTION_URI_PATH, &it,
                                                 &out_option_size, buff,
                                                 sizeof(buff));

        if (res == _ANJ_COAP_OPTION_MISSING && uri->uri_len != 0) {
            return 0;
        }
        _RET_IF_ERROR(res);

        if (uri->uri_len == ANJ_URI_PATH_MAX_LENGTH) {
            return _ANJ_ERR_MALFORMED_MESSAGE; // uri path too long
        }
        // if `bs` in first record -> BootstrapFinish operation
        if (!uri->uri_len && out_option_size == 2 && buff[0] == 'b'
                && buff[1] == 's') {
            *is_bs_uri = true;
            return 0;
        }
        // empty option
        if (!out_option_size) {
            if (!uri->uri_len) { // empty path in first segment
                return 0;
            } else {
                return _ANJ_ERR_MALFORMED_MESSAGE;
            }
        }
        // try to convert string value to int
        uint32_t converted_value;
        if (anj_string_to_uint32_value(&converted_value, buff,
                                       out_option_size)) {
            return _ANJ_ERR_MALFORMED_MESSAGE;
        }
        uri->ids[uri->uri_len] = (uint16_t) converted_value;
        uri->uri_len++;
    }

    return 0;
}

#ifdef ANJ_WITH_COAP_DOWNLOADER
static int etag_decode(anj_coap_options_t *opts, _anj_etag_t *etag) {
    size_t etag_size = 0;
    int res = _anj_coap_options_get_data_iterate(opts, _ANJ_COAP_OPTION_ETAG,
                                                 NULL, &etag_size, etag->bytes,
                                                 _ANJ_MAX_ETAG_LENGTH);
    if (res == _ANJ_COAP_OPTION_MISSING) {
        return 0;
    }
    etag->size = (uint8_t) etag_size;
    return res;
}

static int
get_downloader_response_options(anj_coap_options_t *options,
                                _anj_attr_downloader_t *downloader_attr) {
    int res = etag_decode(options, &downloader_attr->etag);
    _RET_IF_ERROR(res);
    res = _anj_coap_options_get_u32_iterate(options, _ANJ_COAP_OPTION_SIZE2,
                                            NULL, &downloader_attr->total_size);
    if (res == _ANJ_COAP_OPTION_MISSING) {
        return 0;
    }
    return res;
}
#endif // ANJ_WITH_COAP_DOWNLOADER

#define _OBSERVE_OPTION_MAX_LEN 3

static int get_observe_option(anj_coap_options_t *options,
                              bool *opt_present,
                              uint8_t *out_value) {
    uint8_t observe_buff[_OBSERVE_OPTION_MAX_LEN] = { 0 };
    size_t observe_option_size = 0;

    *out_value = 0;
    *opt_present = false;

    int res = _anj_coap_options_get_data_iterate(
            options, _ANJ_COAP_OPTION_OBSERVE, NULL, &observe_option_size,
            observe_buff, sizeof(observe_buff));
    if (res == _ANJ_COAP_OPTION_MISSING) {
        return 0;
    } else if (res) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    *opt_present = true;
    for (size_t i = 0; i < observe_option_size; i++) {
        if (observe_buff[i]) {
            // only two possible values `0` or `1`
            // check RFC7641: https://datatracker.ietf.org/doc/rfc7641/
            *out_value = 1;
            break;
        }
    }
    return 0;
}

static int validate_uri_path(_anj_op_t operation, anj_uri_path_t *uri) {
    switch (operation) {
    case ANJ_OP_DM_READ:
    case ANJ_OP_INF_OBSERVE:
    case ANJ_OP_INF_CANCEL_OBSERVE:
        if (!anj_uri_path_has(uri, ANJ_ID_OID)) {
            return _ANJ_ERR_INPUT_ARG;
        }
        break;
    case ANJ_OP_DM_WRITE_PARTIAL_UPDATE:
    case ANJ_OP_DM_WRITE_REPLACE:
        if (!anj_uri_path_has(uri, ANJ_ID_IID)) {
            return _ANJ_ERR_INPUT_ARG;
        }
        break;
    case ANJ_OP_DM_DISCOVER:
        if (anj_uri_path_has(uri, ANJ_ID_RIID)) {
            return _ANJ_ERR_INPUT_ARG;
        }
        break;
    case ANJ_OP_DM_EXECUTE:
        if (!anj_uri_path_is(uri, ANJ_ID_RID)) {
            return _ANJ_ERR_INPUT_ARG;
        }
        break;
    case ANJ_OP_DM_CREATE:
        if (!anj_uri_path_is(uri, ANJ_ID_OID)) {
            return _ANJ_ERR_INPUT_ARG;
        }
        break;
    case ANJ_OP_DM_DELETE:
        if (anj_uri_path_is(uri, ANJ_ID_RID)) {
            return _ANJ_ERR_INPUT_ARG;
        }
        break;
    default:
        break;
    }
    return 0;
}

static int recognize_lwm2m_operation(anj_coap_options_t *options,
                                     _anj_coap_msg_t *inout_data,
                                     bool is_bs_uri) {
    uint8_t observe_value;
    bool observe_opt_present;
    if (get_observe_option(options, &observe_opt_present, &observe_value)) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    switch (inout_data->msg_code) {
    case ANJ_COAP_CODE_GET:
        if (observe_opt_present) {
            if (observe_value) {
                inout_data->operation = ANJ_OP_INF_CANCEL_OBSERVE;
            } else {
                inout_data->operation = ANJ_OP_INF_OBSERVE;
            }
        } else {
            if (inout_data->accept == _ANJ_COAP_FORMAT_LINK_FORMAT) {
                inout_data->operation = ANJ_OP_DM_DISCOVER;
            } else {
                inout_data->operation = ANJ_OP_DM_READ;
            }
        }
        break;

    case ANJ_COAP_CODE_POST:
        if (is_bs_uri) {
            inout_data->operation = ANJ_OP_BOOTSTRAP_FINISH;
        } else if (anj_uri_path_is(&inout_data->uri, ANJ_ID_OID)) {
            inout_data->operation = ANJ_OP_DM_CREATE;
        } else if (anj_uri_path_is(&inout_data->uri, ANJ_ID_IID)) {
            inout_data->operation = ANJ_OP_DM_WRITE_PARTIAL_UPDATE;
        } else if (anj_uri_path_is(&inout_data->uri, ANJ_ID_RID)) {
            // IMPORTANT: Transport Bindings
            // (OMA-TS-LightweightM2M_Transport-V1_2-20201110-A) allows the
            // Write (Partial Update) operation to target a Resource if the
            // Resource is a Multiple-Instance. This case requires the use of
            // hierarchical Content-Format, so Plain-Text or lack of
            // Content-Format means the Execute operation.
            if (inout_data->content_format == _ANJ_COAP_FORMAT_NOT_DEFINED
                    || inout_data->content_format
                                   == _ANJ_COAP_FORMAT_PLAINTEXT) {
                inout_data->operation = ANJ_OP_DM_EXECUTE;
            } else {
                inout_data->operation = ANJ_OP_DM_WRITE_PARTIAL_UPDATE;
            }
        } else {
            return _ANJ_ERR_MALFORMED_MESSAGE;
        }
        break;

    case ANJ_COAP_CODE_FETCH:
        if (observe_opt_present) {
            if (observe_value) {
                inout_data->operation = ANJ_OP_INF_CANCEL_OBSERVE_COMP;
            } else {
                inout_data->operation = ANJ_OP_INF_OBSERVE_COMP;
            }
        } else {
            inout_data->operation = ANJ_OP_DM_READ_COMP;
        }
        break;

    case ANJ_COAP_CODE_PUT:
        if (inout_data->content_format != _ANJ_COAP_FORMAT_NOT_DEFINED) {
            inout_data->operation = ANJ_OP_DM_WRITE_REPLACE;
        } else {
            inout_data->operation = ANJ_OP_DM_WRITE_ATTR;
        }
        break;

    case ANJ_COAP_CODE_IPATCH:
        inout_data->operation = ANJ_OP_DM_WRITE_COMP;
        break;
    case ANJ_COAP_CODE_DELETE:
        inout_data->operation = ANJ_OP_DM_DELETE;
        break;
    default:
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    return 0;
}

static int decode_payload(anj_coap_message_t *out_coap_msg,
                          anj_bytes_dispenser_t *dispenser) {
    if (dispenser->bytes_left == 0) {
        // no payload after options
        out_coap_msg->payload_size = 0;
        out_coap_msg->payload = NULL;
        return 0;
    }

    // ensured by decode_options
    assert(*dispenser->read_ptr == _ANJ_COAP_PAYLOAD_MARKER);

    out_coap_msg->payload = (uint8_t *) (intptr_t) dispenser->read_ptr + 1;
    out_coap_msg->payload_size = dispenser->bytes_left - 1;

    if (out_coap_msg->payload_size == 0) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    return 0;
}

static int decode_token(anj_coap_message_t *out_coap_msg,
                        anj_bytes_dispenser_t *dispenser) {
    if (_anj_bytes_extract(dispenser, out_coap_msg->token,
                           out_coap_msg->header.token_length)) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    return 0;
}

static int decode_options(anj_coap_message_t *out_coap_msg,
                          anj_bytes_dispenser_t *dispenser) {
    int res;
    size_t bytes_read;

    res = _anj_coap_options_decode(out_coap_msg->options, dispenser->read_ptr,
                                   dispenser->bytes_left, &bytes_read);

    dispenser->read_ptr += bytes_read;
    dispenser->bytes_left -= bytes_read;

    return res;
}

static int decode_attributes(_anj_coap_msg_t *inout_data,
                             anj_coap_message_t *out_coap_msg) {
    if (inout_data->operation == ANJ_OP_DM_DISCOVER) {
        return anj_attr_discover_decode(out_coap_msg->options,
                                        &inout_data->attr.discover_attr);
    } else if (inout_data->operation == ANJ_OP_DM_WRITE_ATTR
               || inout_data->operation == ANJ_OP_INF_OBSERVE
               || inout_data->operation == ANJ_OP_INF_OBSERVE_COMP) {
        return anj_attr_notification_attr_decode(
                out_coap_msg->options, &inout_data->attr.notification_attr);
    }

    return 0;
}

static int handle_lwm2m_request(_anj_coap_msg_t *inout_data,
                                anj_coap_message_t *out_coap_msg) {
    // update accept option if present in message
    _anj_coap_options_get_u16_iterate(out_coap_msg->options,
                                      _ANJ_COAP_OPTION_ACCEPT, NULL,
                                      &inout_data->accept);

    // get uri path if present
    bool is_bs_uri;
    int res = get_uri_path(out_coap_msg->options, &inout_data->uri, &is_bs_uri);
    // _ANJ_COAP_OPTION_MISSING is not an error
    if (res < 0) {
        return res;
    }

    if (recognize_lwm2m_operation(out_coap_msg->options, inout_data,
                                  is_bs_uri)) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    if (res == 0
            && (res = validate_uri_path(inout_data->operation,
                                        &inout_data->uri))) {
        return res;
    }

    return decode_attributes(inout_data, out_coap_msg);
}

static int get_location_path(anj_coap_options_t *opt,
                             _anj_location_path_t *loc_path) {
    memset(loc_path, 0, sizeof(_anj_location_path_t));

    for (size_t i = 0; i < opt->options_number; i++) {
        if (opt->options[i].option_number == _ANJ_COAP_OPTION_LOCATION_PATH) {
            if (loc_path->location_count
                    >= ANJ_COAP_MAX_LOCATION_PATHS_NUMBER) {
                return _ANJ_ERR_LOCATION_PATHS_NUMBER;
            }
            loc_path->location_len[loc_path->location_count] =
                    opt->options[i].payload_len;
            loc_path->location[loc_path->location_count] =
                    (const char *) opt->options[i].payload;
            loc_path->location_count++;
        }
    }
    return 0;
}

static int recognize_operation_and_options_udp(anj_coap_message_t *out_coap_msg,
                                               _anj_coap_msg_t *inout_data) {
    int res = 0;

    _anj_coap_options_get_u16_iterate(out_coap_msg->options,
                                      _ANJ_COAP_OPTION_CONTENT_FORMAT, NULL,
                                      &inout_data->content_format);

    if (inout_data->coap_binding_data.type == ANJ_COAP_UDP_TYPE_RESET) {
        inout_data->operation = ANJ_OP_COAP_RESET;
    } else if (inout_data->coap_binding_data.type
                       == ANJ_COAP_UDP_TYPE_CONFIRMABLE
               && inout_data->msg_code == ANJ_COAP_CODE_EMPTY) {
        inout_data->operation = ANJ_OP_COAP_PING_UDP;
    } else if (inout_data->msg_code >= ANJ_COAP_CODE_GET
               && inout_data->msg_code <= ANJ_COAP_CODE_IPATCH
               && (inout_data->coap_binding_data.type
                           == ANJ_COAP_UDP_TYPE_CONFIRMABLE
                   || inout_data->coap_binding_data.type
                              == ANJ_COAP_UDP_TYPE_NON_CONFIRMABLE)) {
        if (inout_data->coap_binding_data.type
                == ANJ_COAP_UDP_TYPE_NON_CONFIRMABLE) {
            if (inout_data->msg_code == ANJ_COAP_CODE_POST) {
                inout_data->operation = ANJ_OP_DM_EXECUTE;
            } else {
                return _ANJ_ERR_MALFORMED_MESSAGE;
            }
        }
        res = handle_lwm2m_request(inout_data, out_coap_msg);
    } else if (inout_data->msg_code >= ANJ_COAP_CODE_CREATED
               && inout_data->msg_code
                          <= ANJ_COAP_CODE_PROXYING_NOT_SUPPORTED) {
        // server response
        inout_data->operation = ANJ_OP_RESPONSE;
        res = get_location_path(out_coap_msg->options,
                                &inout_data->location_path);
        _RET_IF_ERROR(res);
#ifdef ANJ_WITH_COAP_DOWNLOADER
        res = get_downloader_response_options(
                out_coap_msg->options, &inout_data->attr.downloader_attr);
        _RET_IF_ERROR(res);
#endif // ANJ_WITH_COAP_DOWNLOADER
    } else if (inout_data->msg_code == ANJ_COAP_CODE_EMPTY
               && inout_data->coap_binding_data.type
                          == ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT) {
        inout_data->operation = ANJ_OP_COAP_EMPTY_MSG;
        return 0;
    } else {
        res = _ANJ_ERR_COAP_BAD_MSG;
    }
    _RET_IF_ERROR(res);

    // Block option can be present in lwm2m request and the response
    return _anj_block_decode(out_coap_msg->options, &inout_data->block);
}

static void copy_struct_fields_udp(anj_coap_message_t *out_coap_msg,
                                   _anj_coap_msg_t *out_data) {
    out_data->payload = out_coap_msg->payload;
    out_data->payload_size = out_coap_msg->payload_size;
    out_data->token.size = out_coap_msg->header.token_length;
    memcpy(out_data->token.bytes, out_coap_msg->token,
           out_coap_msg->header.token_length);
    out_data->coap_binding_data.message_id =
            out_coap_msg->header.udp_header.message_id_hbo;
    out_data->coap_binding_data.type =
            (_anj_coap_udp_type_t) out_coap_msg->header.udp_header.type;
    out_data->msg_code = out_coap_msg->header.code;
}

static bool is_udp_msg_header_valid(const anj_coap_message_t *out_coap_msg) {
    if (out_coap_msg->header.udp_header.version != 1) {
        return false;
    }

    if (out_coap_msg->header.token_length > _ANJ_COAP_MAX_TOKEN_LENGTH) {
        return false;
    }

    switch (out_coap_msg->header.udp_header.type) {
    case ANJ_COAP_UDP_TYPE_ACKNOWLEDGEMENT:
        if (_anj_coap_code_is_request(out_coap_msg->header.code)) {
            return false;
        }
        break;
    case ANJ_COAP_UDP_TYPE_NON_CONFIRMABLE:
        // ANJ_COAP_CODE_EMPTY with ANJ_COAP_UDP_TYPE_CONFIRMABLE
        // means "CoAP ping"
        if (out_coap_msg->header.code == ANJ_COAP_CODE_EMPTY) {
            return false;
        }
        break;
    case ANJ_COAP_UDP_TYPE_RESET:
        if (out_coap_msg->header.code != ANJ_COAP_CODE_EMPTY) {
            return false;
        }
        break;

    default:
        break;
    }

    return true;
}

static int decode_header_udp(anj_coap_message_t *out_coap_msg,
                             anj_bytes_dispenser_t *dispenser) {
    uint8_t version_type_token_length;
    if (_anj_bytes_extract(dispenser,
                           &version_type_token_length,
                           sizeof(version_type_token_length))) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    out_coap_msg->header.udp_header.version =
            _anj_coap_udp_header_get_version(version_type_token_length);
    out_coap_msg->header.udp_header.type =
            (uint8_t) _anj_coap_udp_header_get_type(version_type_token_length);
    out_coap_msg->header.token_length =
            _anj_coap_udp_header_get_token_length(version_type_token_length);
    if (out_coap_msg->header.token_length > _ANJ_COAP_MAX_TOKEN_LENGTH) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    if (_anj_bytes_extract(dispenser, &out_coap_msg->header.code,
                           sizeof(out_coap_msg->header.code))) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    uint16_t message_id_nbo;
    if (_anj_bytes_extract(dispenser, &message_id_nbo,
                           sizeof(message_id_nbo))) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }
    out_coap_msg->header.udp_header.message_id_hbo =
            _anj_convert_be16(message_id_nbo);

    if (!is_udp_msg_header_valid(out_coap_msg)) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    if (dispenser->bytes_left > 0
            && out_coap_msg->header.code == ANJ_COAP_CODE_EMPTY) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    return 0;
}

static int _anj_coap_udp_frame_decode(void *datagram,
                                      size_t datagram_size,
                                      _anj_coap_msg_t *out_data) {
    _ANJ_COAP_OPTIONS_INIT_EMPTY(opts, ANJ_COAP_MAX_OPTIONS_NUMBER);
    anj_coap_message_t out_coap_msg = {
        .options = &opts
    };

    anj_bytes_dispenser_t dispenser =
            _anj_make_bytes_dispenser(datagram, datagram_size);

    int res;
    res = decode_header_udp(&out_coap_msg, &dispenser);
    _RET_IF_ERROR(res)

    if (out_coap_msg.header.token_length > 0) {
        res = decode_token(&out_coap_msg, &dispenser);
        _RET_IF_ERROR(res)
    }

    res = decode_options(&out_coap_msg, &dispenser);
    _RET_IF_ERROR(res)

    res = decode_payload(&out_coap_msg, &dispenser);
    _RET_IF_ERROR(res)

    copy_struct_fields_udp(&out_coap_msg, out_data);

    return recognize_operation_and_options_udp(&out_coap_msg, out_data);
}

int _anj_coap_decode_udp(uint8_t *datagram,
                         size_t datagram_size,
                         _anj_coap_msg_t *out_data) {
    assert(datagram);
    assert(out_data);

    if (datagram_size == 0) {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }

    memset(out_data, 0, sizeof(_anj_coap_msg_t));
    out_data->accept = _ANJ_COAP_FORMAT_NOT_DEFINED;
    out_data->content_format = _ANJ_COAP_FORMAT_NOT_DEFINED;

    return _anj_coap_udp_frame_decode(datagram, datagram_size, out_data);
}
