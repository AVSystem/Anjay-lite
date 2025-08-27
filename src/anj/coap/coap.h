/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_H
#    define ANJ_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/defs.h>

#    define ANJ_INTERNAL_INCLUDE_COAP
#    include <anj_internal/coap.h>
#    undef ANJ_INTERNAL_INCLUDE_COAP

/**
 * CoAP Content-Formats, as defined in "Constrained RESTful Environments (CoRE)
 * Parameters":
 * https://www.iana.org/assignments/core-parameters/core-parameters.xhtml
 *
 */
#    define _ANJ_COAP_FORMAT_NOT_DEFINED 0xFFFF

#    define _ANJ_COAP_FORMAT_PLAINTEXT 0
#    define _ANJ_COAP_FORMAT_LINK_FORMAT 40
#    define _ANJ_COAP_FORMAT_OPAQUE_STREAM 42
#    define _ANJ_COAP_FORMAT_CBOR 60
#    define _ANJ_COAP_FORMAT_SENML_JSON 110
#    define _ANJ_COAP_FORMAT_SENML_CBOR 112
#    define _ANJ_COAP_FORMAT_SENML_ETCH_JSON 320
#    define _ANJ_COAP_FORMAT_SENML_ETCH_CBOR 322
#    define _ANJ_COAP_FORMAT_OMA_LWM2M_TLV 11542
#    define _ANJ_COAP_FORMAT_OMA_LWM2M_JSON 11543
#    define _ANJ_COAP_FORMAT_OMA_LWM2M_CBOR 11544

/**
 * Determines whether the given CoAP code represents an error response.
 *
 * This macro evaluates to `true` if the provided @p Code corresponds to a
 * client or server error (i.e., in the 4.xx or 5.xx CoAP code class), based on
 * the CoAP code definitions from @ref anj_coap_code_constants.
 *
 * @param Code A CoAP code value, typically created using @ref ANJ_COAP_CODE().
 *
 * @return true if the code is a valid client/server error response, false
 *         otherwise.
 */
#    define _ANJ_COAP_CODE_IS_ERROR(Code)    \
        ((Code) >= ANJ_COAP_CODE_BAD_REQUEST \
         && (Code) <= ANJ_COAP_CODE_PROXYING_NOT_SUPPORTED)

/** Invalid input arguments. */
#    define _ANJ_ERR_INPUT_ARG (-1)
/** Not supported binding type */
#    define _ANJ_ERR_BINDING (-2)
/** Options array is not big enough */
#    define _ANJ_ERR_OPTIONS_ARRAY (-3)
/** @ref ANJ_COAP_MAX_ATTR_OPTION_SIZE is too small */
#    define _ANJ_ERR_ATTR_BUFF (-4)
/** Malformed CoAP message. */
#    define _ANJ_ERR_MALFORMED_MESSAGE (-5)
/** No space in buffer. */
#    define _ANJ_ERR_BUFF (-6)
/** COAP message not supported or not recognized. */
#    define _ANJ_ERR_COAP_BAD_MSG (-7)
/** Location paths number oversizes @ref ANJ_COAP_MAX_LOCATION_PATHS_NUMBER */
#    define _ANJ_ERR_LOCATION_PATHS_NUMBER (-8)

#    ifdef ANJ_COAP_WITH_TCP
/** Incomplete CoAP message. */
#        define _ANJ_INF_COAP_TCP_INCOMPLETE_MESSAGE 1
/** More data present in TCP packet. */
#        define _ANJ_INF_COAP_TCP_MORE_DATA_PRESENT 2
#    endif // ANJ_COAP_WITH_TCP

/**
 * Maximum possible size of CoAP ACK message without payload.
 *
 * This value is used to calculate the maximum size of single chunk of payload.
 * LwM2M response may contain 4 options: Content-Format, Block1, Block2 and
 * Empty Observe (Observe ACK).
 *
 * MSG_HEADER_SIZE + TOKEN_SIZE + OPTIONS_SIZE + PAYLOAD_MARKER_SIZE
 * 4 + 8 + 3(content format) + 2*4(block1 and block2) +1(Empty observe) + 1
 */
#    define _ANJ_COAP_UDP_RESPONSE_MSG_HEADER_MAX_SIZE 25

#    ifdef ANJ_COAP_WITH_UDP
/**
 * Based on @p msg decodes CoAP message, compliant with the LwM2M version 1.1
 * or 1.2 (check @ref ANJ_WITH_LWM2M12 config flag). All information from
 * message is decoded and stored in the @p data. Each possible option, has its
 * own field in @ref _anj_coap_msg_t and if present in the message then it is
 * decoded. In order to be able to send the response, the data that must be in
 * the CoAP header are copied to @ref _anj_coap_binding_data_t.
 *
 * @param      msg      LWM2M/CoAP message.
 * @param      msg_size Length of the message.
 * @param[out] out_data Empty LwM2M data instance.
 *
 * NOTES: Check tests/lwm2m_decode.c to see the examples of usage.
 *
 * @return
 * - 0 on success,
 * - negative value in case of error,
 */
int _anj_coap_decode_udp(uint8_t *msg,
                         size_t msg_size,
                         _anj_coap_msg_t *out_data);
#    endif // ANJ_COAP_WITH_UDP
#    ifdef ANJ_COAP_WITH_TCP
/**
 * Based on @p msg decodes CoAP message, compliant with the LwM2M version 1.1
 * or 1.2 (check @ref ANJ_WITH_LWM2M12 config flag). All information from
 * message is decoded and stored in the @p out_data. Each possible option has
 * its own field in @ref _anj_coap_msg_t and if present in the message then it
 * is decoded. In order to be able to send the response, the data that must be
 * in the CoAP header are copied to @ref _anj_coap_binding_data_t.
 *
 * @param      msg                  LWM2M/CoAP message.
 * @param      msg_size             Length of the message.
 * @param[out] out_data             Empty LwM2M data instance.
 * @param[out] out_new_data_offset  Offset of remaining data after succesful
 * decoding.
 *
 * @return
 * - 0 on success,
 * - negative value in case of error,
 * - positive value:
 *  - _ANJ_INF_COAP_TCP_INCOMPLETE_MESSAGE if the amount of data is too small
 *    to be parsed properly,
 *  - _ANJ_INF_COAP_TCP_MORE_DATA_PRESENT after succesful parsing, if there are
 *    still some data in the @p msg.
 */
int _anj_coap_decode_tcp(uint8_t *msg,
                         size_t msg_size,
                         _anj_coap_msg_t *out_data,
                         size_t *out_new_data_offset);
#    endif // ANJ_COAP_WITH_TCP
#    ifdef ANJ_COAP_WITH_UDP
/**
 * Based on @p msg prepares CoAP message, compliant with the LwM2M version 1.1
 * or 1.2 (check @ref ANJ_WITH_LWM2M12 config flag). All information related
 * with given @p msg.operation are placed into the message, <b>except the MID
 * and Token</b>. Message ID and Token need to be filled separately, before this
 * function call.
 *
 * @param      msg           Structured LwM2M message.
 * @param[out] out_buff      Buffer for serialized LwM2M message.
 * @param      out_buff_size Buffer size.
 * @param[out] out_msg_size  Size of the prepared message.
 *
 * NOTES: Check tests/lwm2m_prepare.c to see the examples of usage.
 *
 * @return 0 on success, or an one of the error codes defined at the top of this
 * file.
 */
int _anj_coap_encode_udp(_anj_coap_msg_t *msg,
                         uint8_t *out_buff,
                         size_t out_buff_size,
                         size_t *out_msg_size);
#    endif // ANJ_COAP_WITH_UDP
#    ifdef ANJ_COAP_WITH_TCP
/**
 * Based on @p msg prepares CoAP message, compliant with the LwM2M version 1.1
 * or 1.2 (check @ref ANJ_WITH_LWM2M12 config flag). All information related
 * with given @ref _anj_op_t are placed into the message, <b>except the
 * Token</b>. Token needs to be filled separately, before this function call.
 *
 * @param      msg           Structured LwM2M message.
 * @param[out] out_buff      Buffer for serialized LwM2M message.
 * @param      out_buff_size Buffer size.
 * @param[out] out_msg_size  Size of the prepared message.
 *
 * NOTES: Check tests/lwm2m_prepare.c to see the examples of usage.
 *
 * @return 0 on success, or an one of the error codes defined at the top of this
 * file.
 */
int _anj_coap_encode_tcp(_anj_coap_msg_t *msg,
                         uint8_t *out_buff,
                         size_t out_buff_size,
                         size_t *out_msg_size);
#    endif // ANJ_COAP_WITH_TCP

/**
 * Returns the maximum possible size of the CoAP message without payload. This
 * value is used to calculate the maximum size of single chunk of payload.
 * Should be used for client initiated exchanges. For server initiated exchanges
 * use @ref _ANJ_COAP_UDP_RESPONSE_MSG_HEADER_MAX_SIZE. In order to optimize the
 * code, the calculations performed are not precise and for some types of
 * messages the resulting size may be several bytes smaller than the calculated
 * one.
 *
 * @param   msg   Prepared LwM2M message.
 *
 * @return  Maximum possible size of the CoAP message without payload.
 */
size_t _anj_coap_calculate_msg_header_max_size(const _anj_coap_msg_t *msg);

#endif // ANJ_H
