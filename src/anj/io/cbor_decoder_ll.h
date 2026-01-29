/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_IO_CBOR_DECODER_LL_H
#    define SRC_ANJ_IO_CBOR_DECODER_LL_H

#    include <stdbool.h>
#    include <stddef.h>

#    include <anj/core.h>

#    if defined(ANJ_WITH_SENML_CBOR) || defined(ANJ_WITH_LWM2M_CBOR) \
            || defined(ANJ_WITH_CBOR)

#        define ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE (-1)

/**
 * Initializes the low-level CBOR decoder.
 *
 * @param[out] ctx The context variable to initialize. It will be zeroed out,
 *                 and reset to the initial valid state.
 */
void anj_cbor_ll_decoder_init(_anj_cbor_ll_decoder_t *ctx);

/**
 * Provides a data buffer to be parsed by @p ctx.
 *
 * <strong>IMPORTANT:</strong> Only the pointer to @p buff is stored, so the
 * buffer pointed to by that variable has to stay valid until the decoder is
 * discarded, or another payload is provided.
 *
 * <strong>NOTE:</strong> It is only valid to provide the input buffer either
 * immediately after calling @ref anj_cbor_ll_decoder_init, or after some
 * operation has returned @ref _ANJ_IO_WANT_NEXT_PAYLOAD.
 *
 * <strong>NOTE:</strong> The decoder may read-ahead up to 9 bytes of data
 * before actually attempting to decode it. This means that the decoder may
 * request further data chunks even to access elements that are fully contained
 * in the currently available chunk. Those will be decoded from the read-ahead
 * buffer after providing further data.
 *
 * @param ctx              The CBOR decoder context to operate on.
 *
 * @param buff             Pointer to a buffer containing incoming payload.
 *
 * @param buff_len         Size of @p buff in bytes.
 *
 * @param payload_finished Specifies whether the buffer passed is the last chunk
 *                         of a larger payload (e.g. last block of a CoAP
 *                         blockwise transfer).
 *
 *                         If determining that in advance is impractical, it is
 *                         permitted to always pass chunks with this flag set to
 *                         <c>false</c>, and then after next
 *                         @ref _ANJ_IO_WANT_NEXT_PAYLOAD, pass a chunk of size
 *                         0 with this flag set to <c>true</c>.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_ERR_LOGIC if the context is not in a state in which providing
 *   a new payload is possible
 */
int anj_cbor_ll_decoder_feed_payload(_anj_cbor_ll_decoder_t *ctx,
                                     const void *buff,
                                     size_t buff_len,
                                     bool payload_finished);

/**
 * Checks if the CBOR decoder is in some error / exceptional state.
 *
 * @param ctx The CBOR decoder context to operate on.
 *
 * @returns
 * - 0 if the decoder is in a valid state, ready for any of the data consumption
 *   functions
 * - @ref _ANJ_IO_EOF if the decoder has reached the end of payload successfully
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if the decoder is in the middle of parsing
 *   some value and determining the next steps requires calling
 *   @ref anj_cbor_ll_decoder_feed_payload
 * - @ref _ANJ_IO_ERR_FORMAT if an error occurred earlier during parsing and
 *   the decoder can no longer be used
 */
int anj_cbor_ll_decoder_errno(_anj_cbor_ll_decoder_t *ctx);

/**
 * Returns the type of the current value that can be (or currently is) extracted
 * from the context.
 *
 * Before consuming (or preparing to consumption in some cases) the value with
 * one of the:
 *  - @ref anj_cbor_ll_decoder_null(),
 *  - @ref anj_cbor_ll_decoder_bool(),
 *  - @ref anj_cbor_ll_decoder_number(),
 *  - @ref anj_cbor_ll_decoder_bytes(),
 *  - @ref anj_cbor_ll_decoder_enter_array(),
 *  - @ref anj_cbor_ll_decoder_enter_map()
 *
 * the function is guaranteed to return same results each time it is called.
 *
 * @param[in]  ctx      The CBOR decoder context to operate on.
 *
 * @param[out] out_type Ponter to a variable where next type shall be stored.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if end of the current payload has been
 *   reached and calling @ref anj_cbor_ll_decoder_feed_payload is necessary to
 *   access the next value
 * - @ref _ANJ_IO_ERR_FORMAT if an error occurred while parsing the data
 * - @ref _ANJ_IO_ERR_LOGIC if the end-of-stream has already been reached
 */
int anj_cbor_ll_decoder_current_value_type(_anj_cbor_ll_decoder_t *ctx,
                                           _anj_cbor_ll_value_type_t *out_type);

/**
 * Consumes a simple null value.
 *
 * NOTE: May only be called when the next value type is @ref
 * ANJ_CBOR_LL_VALUE_NULL, otherwise an error will be reported.
 *
 * @param[in] ctx The CBOR decoder context to operate on.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if end of the current payload has been
 *   reached and calling @ref anj_cbor_ll_decoder_feed_payload is necessary to
 *   access the next value
 * - @ref _ANJ_IO_ERR_FORMAT if an error occurred while parsing the data
 * - @ref _ANJ_IO_ERR_LOGIC if the end-of-stream has already been reached
 */
int anj_cbor_ll_decoder_null(_anj_cbor_ll_decoder_t *ctx);

/**
 * Consumes a simple boolean value.
 *
 * NOTE: May only be called when the next value type is @ref
 * ANJ_CBOR_LL_VALUE_BOOL, otherwise an error will be reported.
 *
 * @param[in]  ctx       The CBOR decoder context to operate on.
 *
 * @param[out] out_value Pointer to a variable where the value shall be stored.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if end of the current payload has been
 *   reached and calling @ref anj_cbor_ll_decoder_feed_payload is necessary to
 *   access the next value
 * - @ref _ANJ_IO_ERR_FORMAT if an error occurred while parsing the data
 * - @ref _ANJ_IO_ERR_LOGIC if the end-of-stream has already been reached
 */
int anj_cbor_ll_decoder_bool(_anj_cbor_ll_decoder_t *ctx, bool *out_value);

/**
 * Consumes a scalar value from the context.
 *
 * NOTE: May only be called when the next value type is either:
 *  - @ref ANJ_CBOR_LL_VALUE_UINT,
 *  - @ref ANJ_CBOR_LL_VALUE_NEGATIVE_INT,
 *  - @ref ANJ_CBOR_LL_VALUE_FLOAT,
 *  - @ref ANJ_CBOR_LL_VALUE_DOUBLE,
 *  - @ref ANJ_CBOR_LL_VALUE_TIMESTAMP - in this case, the type identified in
 *    <c>out_value->type</c> will reflect the actual underlying data type, i.e.
 *    <c>out_value->type</c> will never be <c>ANJ_CBOR_LL_VALUE_TIMESTAMP</c>
 *
 * @param[in]  ctx       The CBOR decoder context to operate on.
 *
 * @param[out] out_value Pointer to a variable where the value shall be stored.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if end of the current payload has been
 *   reached and calling @ref anj_cbor_ll_decoder_feed_payload is necessary to
 *   access the next value
 * - @ref _ANJ_IO_ERR_FORMAT if an error occurred while parsing the data
 * - @ref _ANJ_IO_ERR_LOGIC if the end-of-stream has already been reached
 */
int anj_cbor_ll_decoder_number(_anj_cbor_ll_decoder_t *ctx,
                               _anj_cbor_ll_number_t *out_value);

/**
 * Prepares for consumption of a byte or text stream element.
 *
 * NOTE: May only be called when the next value type is either:
 *  - @ref ANJ_CBOR_LL_VALUE_BYTE_STRING,
 *  - @ref ANJ_CBOR_LL_VALUE_TEXT_STRING.
 *
 * After successfully calling this function, you shall call @ref
 * anj_cbor_ll_decoder_bytes_get_some, possibly multiple times until it sets
 * the <c>*out_message_finished</c> argument to <c>true</c>, to access the
 * actual data.
 *
 * @param[in]  ctx            The CBOR decoder context to operate on.
 *
 * @param[out] out_bytes_ctx  Pointer to a variable where the bytes context
 *                            pointer shall be stored.
 *
 * @param[out] out_total_size Pointer to a variable where the total size of the
 *                            bytes element will be stored. If the element has
 *                            an indefinite size, @ref
 *                            ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE (-1) will be
 *                            stored - the calling code will need to rely on the
 *                            <c>out_message_finished</c> argument to @ref
 *                            anj_cbor_ll_decoder_bytes_get_some instead. If
 *                            this argument is <c>NULL</c>, it will be ignored.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if end of the current payload has been
 *   reached and calling @ref anj_cbor_ll_decoder_feed_payload is necessary to
 *   access the next value
 * - @ref _ANJ_IO_ERR_FORMAT if an error occurred while parsing the data
 * - @ref _ANJ_IO_ERR_LOGIC if the end-of-stream has already been reached
 */
int anj_cbor_ll_decoder_bytes(_anj_cbor_ll_decoder_t *ctx,
                              _anj_cbor_ll_decoder_bytes_ctx_t **out_bytes_ctx,
                              ptrdiff_t *out_total_size);

/**
 * Consumes some amount of bytes from a byte or text stream element.
 *
 * This function shall be called after a successful call to @ref
 * anj_cbor_ll_decoder_bytes, as many times as necessary until the
 * <c>*out_message_finished</c> argument is set to <c>true</c>, to eventually
 * access and consume the entire stream.
 *
 * <strong>NOTE:</strong> The consumed data is not copied - a pointer to either
 * the previously provided input buffer, or the context's internal read-ahead
 * buffer, is returned instead.
 *
 * @param[in]  bytes_ctx            Bytes context pointer previously returned
 *                                  by @ref anj_cbor_ll_decoder_bytes.
 *
 * @param[out] out_buf              Pointer to a variable that will be set to a
 *                                  pointer to some portion of the stream.
 *
 * @param[out] out_buf_size         Pointer to a variable that will be set to
 *                                  the number of bytes immediately available at
 *                                  <c>*out_buf</c>. Note that this might only
 *                                  be a part of the total size of the string
 *                                  element.
 *
 * @param[out] out_message_finished Pointer to a variable that will be set to
 *                                  <c>true</c> if the currently returned block
 *                                  is the last portion of the string -
 *                                  otherwise <c>false</c>. Note that the last
 *                                  block may have a length of 0.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if end of the current payload has been
 *   reached and calling @ref anj_cbor_ll_decoder_feed_payload is necessary to
 *   access further part of the byte stream
 * - @ref _ANJ_IO_ERR_FORMAT if an error occurred while parsing the data
 * - @ref _ANJ_IO_ERR_LOGIC if the end-of-stream has already been reached
 */
int anj_cbor_ll_decoder_bytes_get_some(
        _anj_cbor_ll_decoder_bytes_ctx_t *bytes_ctx,
        const void **out_buf,
        size_t *out_buf_size,
        bool *out_message_finished);

#        if _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0
/**
 * Prepares to the consumption of the array.
 *
 * NOTE: May only be called when the next value type is @ref
 * ANJ_CBOR_LL_VALUE_ARRAY.
 *
 * NOTE: The decoder has a limit of structure nesting levels. Any payload with
 * higher nesting degree will be rejected by the decoder by entering the error
 * state.
 *
 * @param[in]  ctx      The CBOR decoder context to operate on.
 *
 * @param[out] out_size Pointer to a variable where the total number of elements
 *                      in the array will be stored. If the array has an
 *                      indefinite size, @ref
 *                      ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE (-1) will be
 *                      stored - the calling code will need to rely on the @ref
 *                      anj_cbor_ll_decoder_nesting_level function to determine
 *                      the end of the array instead. If this argument is
 *                      <c>NULL</c>, it will be ignored.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if end of the current payload has been
 *   reached and calling @ref anj_cbor_ll_decoder_feed_payload is necessary to
 *   access the next value
 * - @ref _ANJ_IO_ERR_FORMAT if an error occurred while parsing the data
 * - @ref _ANJ_IO_ERR_LOGIC if the end-of-stream has already been reached
 */
int anj_cbor_ll_decoder_enter_array(_anj_cbor_ll_decoder_t *ctx,
                                    ptrdiff_t *out_size);

/**
 * Prepares to the consumption of the map.
 *
 * NOTE: May only be called when the next value type is @ref
 * ANJ_CBOR_LL_VALUE_MAP.
 *
 * NOTE: The decoder has a limit of structure nesting levels. Any payload with
 * higher nesting degree will be rejected by the decoder by entering the error
 * state.
 *
 * @param[in]  ctx            The CBOR decoder context to operate on.
 *
 * @param[out] out_pair_count Pointer to a variable where the total number of
 *                            element <strong>pairs</strong> in the array will
 *                            be stored. If the array has an indefinite size,
 *                            @ref ANJ_CBOR_LL_DECODER_ITEMS_INDEFINITE (-1)
 *                            will be stored - the calling code will need to
 *                            rely on the @ref
 *                            anj_cbor_ll_decoder_nesting_level function to
 *                            determine the end of the map instead. If this
 *                            argument is <c>NULL</c>, it will be ignored.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if end of the current payload has been
 *   reached and calling @ref anj_cbor_ll_decoder_feed_payload is necessary to
 *   access the next value
 * - @ref _ANJ_IO_ERR_FORMAT if an error occurred while parsing the data
 * - @ref _ANJ_IO_ERR_LOGIC if the end-of-stream has already been reached
 */
int anj_cbor_ll_decoder_enter_map(_anj_cbor_ll_decoder_t *ctx,
                                  ptrdiff_t *out_pair_count);

/**
 * Gets the number of compound entites that the parser is currently inside.
 *
 * The number is incremented by 1 after a successfull call to @ref
 * anj_cbor_ll_decoder_enter_array or @ref anj_cbor_ll_decoder_enter_map, and
 * decreased after reading the last element of that array or map. In particular,
 * if the array or map has zero elements, its value will not be visibly
 * incremented at all.
 *
 * Note that if a decoding error occurred, the nesting level is assumed to be 0
 * instead of returning an explicit error.
 *
 * @param[in]  ctx               The CBOR decoder context to operate on.
 *
 * @param[out] out_nesting_level Pointer to a variable where the current nesting
 *                               level will be stored.
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_WANT_NEXT_PAYLOAD if end of the current payload has been
 *   reached and calling @ref anj_cbor_ll_decoder_feed_payload is necessary to
 *   access the next value
 */
int anj_cbor_ll_decoder_nesting_level(_anj_cbor_ll_decoder_t *ctx,
                                      size_t *out_nesting_level);
#        endif // _ANJ_MAX_CBOR_NEST_STACK_SIZE > 0

#    endif // defined(ANJ_WITH_SENML_CBOR) || defined(ANJ_WITH_LWM2M_CBOR) ||
           // defined(ANJ_WITH_CBOR)

#endif // SRC_ANJ_IO_CBOR_DECODER_LL_H
