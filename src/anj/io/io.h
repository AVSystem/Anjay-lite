/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_IO_H
#    define ANJ_IO_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/core.h>
#    include <anj/defs.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/** Invalid input arguments. */
#    define _ANJ_IO_ERR_INPUT_ARG (-1)
/** Invalid data type. */
#    define _ANJ_IO_ERR_IO_TYPE (-2)
/** Given format does not match the specified input data type. */
#    define _ANJ_IO_ERR_FORMAT (-3)
/** Given format is unsupported */
#    define _ANJ_IO_ERR_UNSUPPORTED_FORMAT (-4)
/** Invalid call. */
#    define _ANJ_IO_ERR_LOGIC (-5)
/** Given path is not consistent with the value of depth. */
#    define _ANJ_IO_WARNING_DEPTH (-6)

/** There is no more data to return from an input context. */
#    define _ANJ_IO_EOF 1
/**
 * Available payload has been exhausted. Please call
 * @ref _anj_io_in_ctx_feed_payload again to continue parsing. If no more data
 * is available, this shall be treated as an error.
 */
#    define _ANJ_IO_WANT_NEXT_PAYLOAD 2
/**
 * The payload format does not contain enough metadata to determine data type of
 * the resource. Please call @ref _anj_io_in_ctx_get_entry again with a concrete
 * data type specified.
 */
#    define _ANJ_IO_WANT_TYPE_DISAMBIGUATION 3
/**
 * Please call anj_io_XXX_ctx_get_payload function again, there is more
 * available data to be copied to the output buffer.
 *
 * @note Defined in include_public/anj/defs.h because it can be returned by user
 *       code inside the @ref anj_get_external_data_t callback implementation.
 *
 * #define ANJ_IO_NEED_NEXT_CALL 4
 */

/**
 * Must be called to prepare @p ctx to build response message payload.
 * It initializes @p ctx and selects the appropriate encoder based on the other
 * function arguments, provided that @p format is equal to
 * _ANJ_COAP_FORMAT_NOT_DEFINED.
 * If @p items_count equals <c>1</c> and it is a _READ @p operation_type
 * message, one of the simple encoders, such as CBOR or Plain Text, will be
 * selected (depending on the project configuration). In the case of multiple
 * records or SEND operations, one of the complex formatters such as SenML CBOR,
 * SenML-ETCH CBOR or LWM2M CBOR will be used. If @p format is different from
 * _ANJ_COAP_FORMAT_NOT_DEFINED it is checked if it can be used for the given
 * arguments.
 * For @p items_count equal to <c>0</c>, after calling this function, you need
 * to call @ref _anj_io_out_ctx_get_payload with @ref _anj_io_out_ctx_new_entry
 * omitted.
 * @p base_path does not need to be set in the case of a simple formatter. If a
 * response to a READ request is being prepared then @p base_path must be set to
 * the value indicated in the request. In other cases, its value should be set
 * using the @ref ANJ_MAKE_ROOT_PATH() macro.
 *
 * @param ctx            Context to operate on.
 * @param operation_type Type of operation for which payload is being prepared.
 *                       Only the following operations are supported and will
 *                       result in successful initialization:
 *                       - @ref ANJ_OP_DM_READ
 *                       - @ref ANJ_OP_DM_READ_COMP
 *                       - @ref ANJ_OP_INF_OBSERVE
 *                       - @ref ANJ_OP_INF_OBSERVE_COMP
 *                       - @ref ANJ_OP_INF_CANCEL_OBSERVE
 *                       - @ref ANJ_OP_INF_CANCEL_OBSERVE_COMP
 *                       - @ref ANJ_OP_INF_NON_CON_NOTIFY
 *                       - @ref ANJ_OP_INF_CON_SEND
 *                       - @ref ANJ_OP_INF_NON_CON_SEND
 * @param base_path      Pointer to path.
 * @param items_count    Number of resources or resource instanes that will be
 *                       in the message.
 * @param format         If value is different from @ref
 *                       _ANJ_COAP_FORMAT_NOT_DEFINED then it is taken into
 *                       account.
 *
 * @return 0 on success, a negative value in case of invalid arguments or
 * unsupported format.
 */
int _anj_io_out_ctx_init(_anj_io_out_ctx_t *ctx,
                         _anj_op_t operation_type,
                         const anj_uri_path_t *base_path,
                         size_t items_count,
                         uint16_t format);

/**
 * Call to add new @p entry.
 * During this call the @p entry is encoded with given format and internal
 * buffer is filled with payload. After calling this function, you need to call
 * @ref _anj_io_out_ctx_get_payload to copy the message. Remember that during
 * the whole operation @p entry must not change and before next @ref
 * _anj_io_out_ctx_new_entry call, the entire previous record must be copied.
 *
 * @param      ctx          Context to operate on.
 * @param      entry        Single record.
 *
 * @return 0 on success, a negative value in case of error.
 */
int _anj_io_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                              const anj_io_out_entry_t *entry);

/**
 * Call to copy encoded message to payload buffer.
 *
 * The buffer into which the encoded message is written is given by @p out_buff.
 * @p out_copied_bytes returns information about the number of bytes written
 * during a single function call, If the function return @ref
 * ANJ_IO_NEED_NEXT_CALL it means that the buffer space has run out. If you
 * support block operation then at this point you can send the buffer as one
 * message block and call this function again with the same @p entry and the
 * rest of the record will be written. In case return value equals <c>0</c> then
 * you can call @ref _anj_io_out_ctx_new_entry with another entry and @ref
 * _anj_io_out_ctx_get_payload with pointer to the buffer shifted by the value
 * of @p out_copied_bytes, also remember to update the value of @p out_buff_len.
 *
 * The @p out_buff already contains the LWM2M message payload. Use the @ref
 * _anj_io_out_ctx_get_format function to check the format used.
 *
 * Example use (error checking omitted for brevity):
 * @code
 * // New out ctx
 * _anj_io_out_ctx_t ctx;
 *
 * // Initializes ctx for READ operation response on single resource
 * _anj_io_out_ctx_init(&ctx, ANJ_OP_DM_READ, NULL, 1, _ANJ_COAP_FORMAT_CBOR);
 *
 * // Prepares resource record of int types
 * anj_io_out_entry_t entry;
 * entry.type = ANJ_DATA_TYPE_INT;
 * entry.value.int_value = 0x01;
 *
 * // Variables needed to call @ref anj_io_out_ctx_serialize
 * size_t payload_size = 0;
 * uint8_t buff[50];
 *
 * // Adds entry to the message, after these calls buff can be added to lwm2m
 * // message as payload. Return value equals to ANJ_IO_NEED_NEXT_CALL it means
 * // that the entry didn't fit in the output buffer and after sending the
 * // payload as one part of block transfer anj_io_out_ctx_serialize must be
 * // called again with the same entry.
 * _anj_io_out_ctx_new_entry(&ctx, &entry);
 * _anj_io_out_ctx_get_payload(&ctx, buff, sizeof(buff), &payload_size);
 * @endcode
 *
 * @param[out] out_buff         Payload buffer.
 * @param      out_buff_len     Length of payload buffer.
 * @param[out] out_copied_bytes Number of bytes that are written into the
 *                              buffer.
 * @param      ctx              Context to operate on.
 *
 * @return
 * - 0 on success,
 * - ANJ_IO_NEED_NEXT_CALL if entry didn't fit in the output buffer and this
 *   function have to be called again,
 * - _ANJ_IO_ERR_LOGIC if this function is called but there is no more data in
 *   internal buffer,
 * - error code returned by @ref anj_get_external_data_t.
 */
int _anj_io_out_ctx_get_payload(_anj_io_out_ctx_t *ctx,
                                void *out_buff,
                                size_t out_buff_len,
                                size_t *out_copied_bytes);

/**
 * Returns the value of the currently used format.
 *
 * @param ctx Context to operate on.
 *
 * @return Format used.
 */
uint16_t _anj_io_out_ctx_get_format(_anj_io_out_ctx_t *ctx);

#    ifdef ANJ_WITH_EXTERNAL_DATA
/**
 * Invoke @ref anj_close_external_data_t callback for @p entry record.
 *
 * @param      entry        Single record.
 */
void _anj_io_out_ctx_close_external_data_cb(const anj_io_out_entry_t *entry);
#    endif // ANJ_WITH_EXTERNAL_DATA

/**
 * Initializes @p ctx so that it can be used to parse incoming payload
 * containing data model data.
 *
 * @param[out] ctx       Context variable to initialize.
 * @param operation_type Type of operation for which payload is being parsed.
 *                       Only the following operations are supported and will
 *                       result in successful initialization:
 *                       - @ref ANJ_OP_DM_READ_COMP
 *                       - @ref ANJ_OP_DM_WRITE_REPLACE
 *                       - @ref ANJ_OP_DM_WRITE_PARTIAL_UPDATE
 *                       - @ref ANJ_OP_DM_WRITE_COMP
 *                       - @ref ANJ_OP_DM_CREATE
 *                       - @ref ANJ_OP_INF_OBSERVE_COMP
 * @param base_path      URI path that has been specified in the operation
 *                       parameters (i.e., CoAP options); may be <c>NULL</c> in
 *                       case of the Composite operations.
 * @param format         CoAP Content-Format number specifying the format of
 *                       incoming data.
 *
 * @return 0 on success, a negative value in case of invalid arguments or
 * unsupported format.
 */
int _anj_io_in_ctx_init(_anj_io_in_ctx_t *ctx,
                        _anj_op_t operation_type,
                        const anj_uri_path_t *base_path,
                        uint16_t format);

/**
 * Provides a data buffer to be parsed by @p ctx.
 *
 * <strong>IMPORTANT:</strong> Only the pointer to @p buff is stored, so the
 * buffer pointed to by that variable has to stay valid until the input context
 * is discarded, or another payload is provided.
 *
 * <strong>NOTE:</strong> @p buff is passed non-const and depending on the
 * content format, it may be modified by the context. For example, base64
 * decoding of binary data, if necessary, will be performed in place by
 * overwriting data in the buffer. <em>Please copy the buffer first if you need
 * to retain its original contents.</em>
 *
 * <strong>NOTE:</strong> It is only valid to provide the input buffer either
 * immediately after calling @ref _anj_io_in_ctx_init, or after @ref
 * _anj_io_in_ctx_get_entry has returned @ref _ANJ_IO_WANT_NEXT_PAYLOAD.
 *
 * @param ctx              Input context to operate on.
 * @param buff             Pointer to a buffer containing incoming payload.
 * @param buff_len         Size of @p buff in bytes.
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
 * @return 0 on success, a negative value in case of invalid arguments, or if
 *         the payload could already be determined as unparsable during the
 *         initial parsing stage.
 */
int _anj_io_in_ctx_feed_payload(_anj_io_in_ctx_t *ctx,
                                void *buff,
                                size_t buff_len,
                                bool payload_finished);

/**
 * Retrieves the next entry parsed by the input context, either in full or in
 * part.
 *
 * Resources of types: Integer, Unsigned Integer, Float, Boolean, Time, Objlnk,
 * and entries without a value payload, are always returned after having been
 * parsed in full. String and Opaque resources may be parsed in chunks.
 *
 * If an entry has been parsed in full, then @p inout_type_bitmask will be set
 * to a concrete type (at most one bit will be set), @p out_path will be set,
 * and if the type is not @ref ANJ_DATA_TYPE_NULL, @p out_value will also be
 * populated.
 *
 * If a String or Opaque resource has been parsed in part, then
 * @p inout_type_bitmask will be set to that concrete type, and @p out_value
 * will be set to a partial chunk of the parsed value. @p out_path may be
 * populated with the first chunk if available, in which case it will also be
 * repeated with each subsequent chunk. However, it may also only be populated
 * with the last chunk. In some formats (such as SenML CBOR) this may depend on
 * the order of encoded elements.
 *
 * When processing a String or Opaque resource, the last chunk of the resource
 * is signalled by <c>out_value->bytes_or_string.full_length_hint</c> being
 * equal to <c>out_value->bytes_or_string.offset +
 * out_value->bytes_or_string.chunk_length</c> <strong>and</strong> @p out_path
 * being populated. If either of these conditions is not true while this
 * function returned success and <c>*inout_type_bitmask</c> is either
 * <c>ANJ_DATA_TYPE_BYTES</c> or <c>ANJ_DATA_TYPE_STRING</c>, then the
 * next call to this function will return the next chunk of the same entry. Note
 * that the final chunk may have a length of zero.
 *
 * If the last chunk of the payload did not contain enough data to provide the
 * value in either its entirety or even in part, then the function returns
 * @ref _ANJ_IO_WANT_NEXT_PAYLOAD. Values of the output arguments shall be
 * treated as undefined in that case. Parsing will be continued after the next
 * portion of the input payload (e.g. the next payload from a CoAP blockwise
 * transfer) is provided by calling @ref _anj_io_in_ctx_feed_payload - the user
 * can then retry the call to this function.
 *
 * If the type of the resource cannot be reliably determined (e.g. in case of
 * the Plain Text format), then @p out_value will not be populated and the
 * function will return @ref _ANJ_IO_WANT_TYPE_DISAMBIGUATION. The user shall
 * then retry the call with the value of <c>*inout_type_bitmask</c> providing
 * the single type as which the resource shall be parsed.
 *
 * In case @p out_value or @p out_path cannot be populated, either or both of
 * them will be set to <c>NULL</c>.
 *
 * Below is an example of code that reads a payload from a file descriptor
 * <c>fd</c> and prints out the parsed entries (using auxiliary functions that
 * are omitted from this example for brevity).
 *
 * @code
 * _anj_io_in_ctx_t ctx;
 * int result = _anj_io_in_ctx_init(&ctx, ANJ_OP_DM_WRITE_COMP, NULL,
 *                                  _ANJ_COAP_FORMAT_SENML_CBOR);
 * if (result) {
 *     abort();
 * }
 * result = _ANJ_IO_WANT_NEXT_PAYLOAD;
 * char buf[1024];
 * char *bytes_or_string_buf = NULL;
 * anj_data_type_t type_bitmask = ANJ_DATA_TYPE_ANY;
 * const anj_res_value_t *value = NULL;
 * const anj_uri_path_t *path = NULL;
 * while (result != _ANJ_IO_EOF) {
 *     if (result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
 *         ssize_t bytes_count = read(fd, buf, sizeof(buf));
 *         if (bytes_count < 0
 *                 || _anj_io_in_ctx_feed_payload(&ctx, buf,
 *                                                (size_t) bytes_count,
 *                                                bytes_count == 0)) {
 *             abort();
 *         }
 *     }
 *     if ((result = _anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value,
 *                                            &path))
 *             == _ANJ_IO_WANT_TYPE_DISAMBIGUATION) {
 *         assert(path);
 *         type_bitmask = determine_type(type_bitmask, path);
 *         result = _anj_io_in_ctx_get_entry(&ctx, &type_bitmask, &value,
 *                                           &path);
 *     }
 *     if (result < 0 || result == _ANJ_IO_WANT_TYPE_DISAMBIGUATION) {
 *         abort();
 *     }
 *     if (result) {
 *         assert(result == _ANJ_IO_EOF
 *                || result == _ANJ_IO_WANT_NEXT_PAYLOAD);
 *         continue;
 *     }
 *     assert(value || type_bitmask == ANJ_DATA_TYPE_NULL);
 *     if (type_bitmask != ANJ_DATA_TYPE_BYTES
 *             && type_bitmask != ANJ_DATA_TYPE_STRING) {
 *         assert(path);
 *         process_resource(path, type_bitmask, value);
 *     } else {
 *         if (!(bytes_or_string_buf = (char *) realloc(
 *                       bytes_or_string_buf,
 *                       value->bytes_or_string.full_length_hint
 *                               ? value->bytes_or_string.full_length_hint
 *                               : value->bytes_or_string.offset
 *                                         + value->bytes_or_string
 *                                                   .chunk_length))) {
 *             abort();
 *         }
 *         memcpy(bytes_or_string_buf + value->bytes_or_string.offset,
 *                value->bytes_or_string.data,
 *                value->bytes_or_string.chunk_length);
 *         if (!path
 *                 || value->bytes_or_string.offset
 *                                        + value->bytes_or_string.chunk_length
 *                                != value->bytes_or_string.full_length_hint) {
 *             // Not the last chunk
 *             continue;
 *         }
 *         process_resource(
 *                 path, type_bitmask,
 *                 &(const anj_res_value_t) {
 *                     .bytes_or_string = {
 *                         .data = bytes_or_string_buf,
 *                         .offset = 0,
 *                         .chunk_length =
 *                                 value->bytes_or_string.full_length_hint,
 *                         .full_length_hint =
 *                                 value->bytes_or_string.full_length_hint
 *                     }
 *                 });
 *         free(bytes_or_string_buf);
 *         bytes_or_string_buf = NULL;
 *     }
 *     type_bitmask = ANJ_DATA_TYPE_ANY;
 * }
 * @endcode
 *
 * @param        ctx                Context to operate on.
 * @param[inout] inout_type_bitmask Bidirectional parameter controlling the type
 *                                  of the resource. On input, it shall be set
 *                                  to a bit mask of all the data types that the
 *                                  caller is prepared to handle. On output,
 *                                  that bit mask will be narrowed down to the
 *                                  data types as which the incoming data may be
 *                                  interpreted as. If that is a single data
 *                                  type, then @p out_value will be populated as
 *                                  well. Otherwise, @ref
 *                                  _ANJ_IO_WANT_TYPE_DISAMBIGUATION will be
 *                                  returned and the user shall retry the call
 *                                  with this parameter set to a single type as
 *                                  which the resource shall be parsed.
 * @param[out]   out_value          Full or partial resource value.
 * @param[out]   out_path           Data model path that the value corresponds
 *                                  to. This will be a path to a Single-instance
 *                                  Resource or a Resource Instance, unless
 *                                  <c>*inout_type_bitmask ==
 *                                  ANJ_DATA_TYPE_NULL</c>, in which case it
 *                                  might also signify an empty Object Instance,
 *                                  or a query in a Read-Composite request.
 *
 * @returns
 * - Success states (non-negative values):
 *   - 0 - the entry has been parsed in its entirety (or at least in part in
 *     case of @ref ANJ_DATA_TYPE_BYTES or @ref ANJ_DATA_TYPE_STRING types),
 *     and the function can be called again to retrieve the next one (or the
 *     next chunk of Bytes or String entry)
 *   - @ref _ANJ_IO_EOF - there are no more entries in the payload; this may
 *     only be returned if @ref _anj_io_in_ctx_feed_payload has been previously
 *     called with the <c>payload_finished</c> parameter set to <c>true</c>;
 *     @p out_value and @p out_path will be set to <c>NULL</c> - the EOF
 *     condition is treated as occurring after the last entry, not as part of it
 *   - @ref _ANJ_IO_WANT_NEXT_PAYLOAD - end of payload has been encountered
 *     while parsing an entry; the user shall call @ref
 *     _anj_io_in_ctx_feed_payload and retry calling this function; the output
 *     arguments will not be populated in case of this return value
 *   - @ref _ANJ_IO_WANT_TYPE_DISAMBIGUATION - resource value has been
 *     encountered, but the payload format does not contain enough metadata to
 *     determine its data type; the user shall determine the type and retry the
 *     call with a concrete type specified through @p inout_type_bitmask;
 *     @p out_path will be populated in case of this return value
 * - Error conditions (negative values):
 *   - @ref _ANJ_IO_ERR_INPUT_ARG - invalid arguments
 *   - @ref _ANJ_IO_ERR_FORMAT - error parsing the input data, including when
 *     the resource is not compatible with any of the types provided through
 *     @p inout_type_bitmask
 *   - @ref _ANJ_IO_ERR_LOGIC - the input context is not in a state in which
 *     calling this function is legal
 */
int _anj_io_in_ctx_get_entry(_anj_io_in_ctx_t *ctx,
                             anj_data_type_t *inout_type_bitmask,
                             const anj_res_value_t **out_value,
                             const anj_uri_path_t **out_path);

/**
 * Retrieves the number of elements in the incoming data.
 *
 * This data, if available, will be populated inside the context after the first
 * successful call to @ref _anj_io_in_ctx_get_entry, and may be retrieved at any
 * time afterwards.
 *
 * @param ctx       Input context to operate on.
 * @param out_count Variable that, on successful exit, will be filled with the
 *                  total number of entries in the entire payload (not just the
 *                  part already provided to the context).
 *
 * @returns
 * - 0 on success
 * - @ref _ANJ_IO_ERR_INPUT_ARG in case of invalid arguments
 * - @ref _ANJ_IO_ERR_FORMAT if the format of the input data does not support
 *   retrieving this information in advance (e.g., LwM2M TLV, CBOR indefinite
 *   arrays)
 * - @ref _ANJ_IO_ERR_LOGIC if the function is called before the first
 *   successful call to @ref _anj_io_in_ctx_get_entry
 */
int _anj_io_in_ctx_get_entry_count(_anj_io_in_ctx_t *ctx, size_t *out_count);

/**
 * Must be called to prepare @p ctx to build message payload of the REGISTER
 * operation.
 *
 * Example use of register_ctx API (error checking omitted for brevity):
 * @code
 * #define BUFF_SIZE 100
 * // New register payload ctx
 * _anj_io_register_ctx_t ctx;
 * uint8_t out_buff[BUFF_SIZE];
 * size_t out_copied_bytes = 0;
 * size_t offset = 0;
 *
 * _anj_io_register_ctx_init(&ctx);
 *
 * _anj_io_register_ctx_new_entry(&ctx, &ANJ_MAKE_OBJECT_PATH(1), "1.1");
 * _anj_io_register_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                  &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_register_ctx_new_entry(&ctx, &ANJ_MAKE_INSTANCE_PATH(1,0), NULL);
 * _anj_io_register_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                  &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_register_ctx_new_entry(&ctx, &ANJ_MAKE_INSTANCE_PATH(1,1), NULL);
 * _anj_io_register_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                  &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_register_ctx_new_entry(&ctx, &ANJ_MAKE_INSTANCE_PATH(3,0), NULL);
 * _anj_io_register_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                  &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_register_ctx_new_entry(&ctx, &ANJ_MAKE_OBJECT_PATH(5), NULL);
 * _anj_io_register_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                  &out_copied_bytes);
 * offset += out_copied_bytes;
 *
 * // outgoing message:
 * // </1>;ver=1.2,</1/0>,</1/1>,</3/0>,</5>
 * @endcode
 *
 * @param ctx  Context to operate on.
 */
void _anj_io_register_ctx_init(_anj_io_register_ctx_t *ctx);

/**
 * Process another Object or Object Instance record. Remember to keep the order,
 * the values of the @p path must be increasing. For Object Instance @p version
 * presence is treated as error.
 *
 * For Core Objects if the version does not match the LwM2M version being used
 * (@ref ANJ_WITH_LWM2M12) @p version must be provided. For Non-core Objects
 * missing @p version is equivalent to 1.0 and is optional. The @p version
 * format is X.X where X is the digit ("The Object Version of an Object is
 * composed of 2 digits separated by a dot '.'" [LwM2M Specification]).
 *
 * As required by the specification, Security Object ID:0 and OSCORE Object
 * ID:21 must be skipped.
 *
 * IMPORTANT: Add Object to the REGISTER payload only if @version is defined or
 * this Object doesn't have any Instance.
 *
 * @param ctx     Context to operate on.
 * @param path    Object or Object Instance path.
 * @param version Object version, ignored if NULL.
 *
 * @return
 * - 0 on success,
 * - _ANJ_IO_ERR_LOGIC if the internal buffer is not empty,
 * - _ANJ_IO_ERR_INPUT_ARG if:
 *  - @p path is not Object or Object Instance path,
 *  - the ascending order of @p path is not respected,
 *  - @p version format is incorrect,
 *  - @p version is given for Object Instance @p path,
 *  - Object ID is equal to @ref ANJ_OBJ_ID_SECURITY or @ref
 *    ANJ_OBJ_ID_OSCORE.
 */
int _anj_io_register_ctx_new_entry(_anj_io_register_ctx_t *ctx,
                                   const anj_uri_path_t *path,
                                   const char *version);

/**
 * Call to copy encoded message to payload buffer.
 *
 * The buffer into which the encoded message is written is given by @p out_buff.
 * @p out_copied_bytes returns information about the number of bytes written
 * during a single function call, If the function return @ref
 * ANJ_IO_NEED_NEXT_CALL it means that the buffer space has run out. If you
 * support block operation then at this point you can send the buffer as one
 * message block and call this function again and the rest of the record will be
 * written. In case return value equals <c>0</c> then you can call @ref
 * _anj_io_register_ctx_new_entry with another Object/Object Instance and @ref
 * _anj_io_register_ctx_get_payload with pointer to the buffer shifted by the
 * value of @p out_copied_bytes, also remember to update the value of @p
 * out_buff_len.
 *
 * The @p out_buff already contains the LwM2M message payload.
 *
 * @param      ctx              Context to operate on.
 * @param[out] out_buff         Payload buffer.
 * @param      out_buff_len     Length of payload buffer.
 * @param[out] out_copied_bytes Number of bytes that are written into the
 *                              buffer.
 *
 * @return
 * - 0 on success,
 * - ANJ_IO_NEED_NEXT_CALL if record didn't fit in the output buffer and this
 *   function have to be called again,
 * - _ANJ_IO_ERR_LOGIC if this function is called but there is no more data in
 *   internal buffer.
 */
int _anj_io_register_ctx_get_payload(_anj_io_register_ctx_t *ctx,
                                     void *out_buff,
                                     size_t out_buff_len,
                                     size_t *out_copied_bytes);

#    ifdef ANJ_WITH_BOOTSTRAP_DISCOVER
/**
 * Must be called to prepare @p ctx to build message payload of the
 * BOOTSTRAP-DISCOVER operation. Information about supported version of LwM2M is
 * placed on the beginning of the message. It depends on @ref ANJ_WITH_LWM2M12
 * flag.
 *
 * Example use of bootstrap_discover_ctx API (error checking omitted for
 * brevity):
 * @code
 * #define BUFF_SIZE 200
 * // New bootstrap-discover payload ctx
 * _anj_io_bootstrap_discover_ctx_t ctx;
 * uint8_t out_buff[BUFF_SIZE];
 * size_t out_copied_bytes = 0;
 * size_t offset = 0;
 * uint16_t ssid = 10;
 *
 * _anj_io_bootstrap_discover_ctx_init(&ctx, &ANJ_MAKE_ROOT_PATH());
 *
 * _anj_io_bootstrap_discover_ctx_new_entry(ctx,
 *     &ANJ_MAKE_INSTANCE_PATH(0,0), NULL, &ssid,
 *      "coaps://server_1.example.com");
 * _anj_io_bootstrap_discover_ctx_get_payload(&ctx, &out_buff[offset],
 *                      BUFF_SIZE - offset, &out_copied_bytes);
 * offset += out_copied_bytes;
 * // Security Object iid = 1 contains the credentials for the LwM2M
 * // Bootstrap-Server, according to the technical specification we don't
 * // provide SSID (prohibited) and URI (optional).
 * _anj_io_bootstrap_discover_ctx_new_entry(ctx,
 *          &ANJ_MAKE_INSTANCE_PATH(0,1), NULL, NULL, NULL);
 * _anj_io_bootstrap_discover_ctx_get_payload(&ctx, &out_buff[offset],
 *                      BUFF_SIZE - offset, &out_copied_bytes);
 * offset += out_copied_bytes;
 * // For Server instance we always provide SSID value.
 * _anj_io_bootstrap_discover_ctx_new_entry(ctx,
 *          &ANJ_MAKE_INSTANCE_PATH(1,0), NULL, &ssid, NULL);
 * _anj_io_bootstrap_discover_ctx_get_payload(&ctx, &out_buff[offset],
 *                      BUFF_SIZE - offset, &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_bootstrap_discover_ctx_new_entry(ctx,
 *          &ANJ_MAKE_INSTANCE_PATH(3,0), NULL, NULL, NULL);
 * _anj_io_bootstrap_discover_ctx_get_payload(&ctx, &out_buff[offset],
 *                      BUFF_SIZE - offset, &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_bootstrap_discover_ctx_new_entry(ctx, &ANJ_MAKE_OBJECT_PATH(4),
 *                                              , NULL, NULL, NULL);
 * _anj_io_bootstrap_discover_ctx_get_payload(&ctx, &out_buff[offset],
 *                      BUFF_SIZE - offset, &out_copied_bytes);
 * offset += out_copied_bytes;
 * // For object 55 we defined the version.
 * _anj_io_bootstrap_discover_ctx_new_entry(ctx, &ANJ_MAKE_OBJECT_PATH(55),
 *                      "1.9", NULL, NULL);
 * _anj_io_bootstrap_discover_ctx_get_payload(&ctx, &out_buff[offset],
 *                      BUFF_SIZE - offset, &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_bootstrap_discover_ctx_new_entry(ctx,
 *                  &ANJ_MAKE_INSTANCE_PATH(55,0), NULL, NULL, NULL);
 * _anj_io_bootstrap_discover_ctx_get_payload(&ctx, &out_buff[offset],
 *                      BUFF_SIZE - offset, &out_copied_bytes);
 * offset += out_copied_bytes;
 *
 * // outgoing message:
 * </>;lwm2m=1.2,</0/0>;ssid=10;uri="coaps://server_1.example.com",
 * </0/1>,</1/0/>;ssid=10,</3/0>,</4>,</55>;ver=1.9,</55/0>
 * @endcode
 *
 * @param ctx        Context to operate on.
 * @param base_path  Object ID from request, its absence means Root
 *                   (empty) path, you might use @ref ANJ_MAKE_ROOT_PATH()
 *                   to create it.
 *
 * @return
 * - 0 on success,
 * - _ANJ_IO_ERR_INPUT_ARG if @p path is not Object or Root path.
 */
int _anj_io_bootstrap_discover_ctx_init(_anj_io_bootstrap_discover_ctx_t *ctx,
                                        const anj_uri_path_t *base_path);

/**
 * Adds another Object or Object Instance to the buffer. Remember to keep the
 * order, the values of @p path must be increasing. For any Object Instance @p
 * version presence is treated as error, also for any Object the presence of @p
 * ssid and @p uri is not allowed.
 *
 * For Core Objects if the version does not match the LwM2M version being used
 * (@ref ANJ_WITH_LWM2M12) @p version must be provided. For Non-core Object
 * missing @p version is equivalent to 1.0 and is optional. The @p version
 * format is X.X where X is the digit ("The Object Version of an Object is
 * composed of 2 digits separated by a dot '.'" [LwM2M Specification]).
 *
 * Each element of the Instances list of the Security Object (Object ID:0)
 * includes the associated Short Server ID - @p ssid and LwM2M Server URI: @p
 * uri in its parameters list (except the Bootstrap-Server Security Object
 * Instance) while the elements of the Instances list of the Server Object
 * (Object ID:1) also report the associated Short Server ID: @p ssid in their
 * parameters list. If the LwM2M Client supports OSCORE, each element of the
 * Instances list of the OSCORE Object (Object ID:21) includes the associated
 * Short Server ID - @p ssid in its parameters list, except the OSCORE Object
 * Instance which is associated with LwM2M Bootstrap-Server. For others Objects
 * @p ssid and @p uri values presence, is treated as an error.
 *
 * IMPORTANT: Add Object to the BOOTSTRAP-DISCOVER payload only if @version is
 * defined or this Object doesn't have any Instance.
 *
 * @param ctx     Context to operate on.
 * @param path    Object Instance path.
 * @param version Object version, ignored if NULL.
 * @param ssid    Short server ID of Object Instance, relevant for Security,
 *                Server and OSCORE Object Instances.
 * @param uri     Server URI relevant for Security Object Instances.
 *
 * @return
 * - 0 on success,
 * - _ANJ_IO_ERR_INPUT_ARG if:
 *  -  @p ssid for Servers Object Instance are not provided,
 *  -  @p uri is provided for Object Instance other than Security Object
 *     Instance,
 *  -  @p ssid is provided for Object Instance other than Security, Server and
 *     Oscore Object Instance,
 *  -  given @p path is outside the base_path or it's not Object or Object
 *     Instance path,
 *  -  ascending order of @p path is not respected,
 *  -  @p version format is incorrect,
 *  -  @p ssid or @p uri is given for Object @p path,
 *  -  @p version is given for Object Instance @p path,
 * - _ANJ_IO_ERR_LOGIC if the internal buffer is not empty.
 */
int _anj_io_bootstrap_discover_ctx_new_entry(
        _anj_io_bootstrap_discover_ctx_t *ctx,
        const anj_uri_path_t *path,
        const char *version,
        const uint16_t *ssid,
        const char *uri);

/**
 * Call to copy encoded message to payload buffer.
 *
 * The buffer into which the encoded message is written is given by @p out_buff.
 * @p out_copied_bytes returns information about the number of bytes written
 * during a single function call, If the function return @ref
 * ANJ_IO_NEED_NEXT_CALL it means that the buffer space has run out. If you
 * support block operation then at this point you can send the buffer as one
 * message block and call this function again and the rest of the record will be
 * written. In case return value equals <c>0</c> then you can call @ref
 * _anj_io_bootstrap_discover_ctx_new_entry with another Object/Object Instance
 * and @ref _anj_io_bootstrap_discover_ctx_get_payload with pointer to the
 * buffer shifted by the value of @p out_copied_bytes, also remember to update
 * the value of @p out_buff_len.
 *
 * The @p out_buff already contains the LWM2M message payload.
 *
 * @param      ctx              Context to operate on.
 * @param[out] out_buff         Payload buffer.
 * @param      out_buff_len     Length of payload buffer.
 * @param[out] out_copied_bytes Number of bytes that are written into the
 *                              buffer.
 *
 * @return
 * - 0 on success,
 * - ANJ_IO_NEED_NEXT_CALL if record didn't fit in the output buffer and this
 *   function have to be called again,
 * - _ANJ_IO_ERR_LOGIC if this function is called but there is no more data in
 *   internal buffer.
 */
int _anj_io_bootstrap_discover_ctx_get_payload(
        _anj_io_bootstrap_discover_ctx_t *ctx,
        void *out_buff,
        size_t out_buff_len,
        size_t *out_copied_bytes);

#    endif // ANJ_WITH_BOOTSTRAP_DISCOVER

#    ifdef ANJ_WITH_DISCOVER

/**
 * Must be called to prepare @p ctx to build message payload of the DISCOVER
 * operation.
 *
 * Example use of discover_ctx API (error checking omitted for brevity):
 * @code
 * #define BUFF_SIZE 100
 * // New discover payload ctx
 * _anj_io_discover_ctx_t ctx;
 * uint8_t out_buff[BUFF_SIZE];
 * size_t out_copied_bytes = 0;
 * size_t offset = 0;
 *
 * _anj_attr_notification_t device_obj_attr = {
 *     .has_min_period = true,
 *     .min_period = 10,
 *     .has_max_period = true,
 *     .max_period = 60
 * };
 * uint32_t depth = 3;
 * uint8_t dim = 2;
 *
 * _anj_io_discover_ctx_init(&ctx, &ANJ_MAKE_OBJECT_PATH(3), &depth,
 *                                  NULL,NULL);
 *
 * _anj_io_discover_ctx_new_entry(ctx, &ANJ_MAKE_OBJECT_PATH(3),
 *                                             &device_obj_attr, "1.2", NULL);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_discover_ctx_new_entry(ctx, &ANJ_MAKE_INSTANCE_PATH(3, 0),
 *                                                          NULL, NULL, NULL);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_discover_ctx_new_entry(ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 1),
 *                                                          NULL, NULL, NULL);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_discover_ctx_new_entry(ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 2),
 *                                                          NULL, NULL, NULL);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_discover_ctx_new_entry(ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3),
 *                                                          NULL, NULL, NULL);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_discover_ctx_new_entry(ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 4),
 *                                                          NULL, NULL, NULL);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_discover_ctx_new_entry(ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 6),
 *                                                          NULL, NULL, &dim);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_discover_ctx_new_entry(ctx,
 *           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 0), NULL, NULL, NULL);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 * _anj_io_discover_ctx_new_entry(ctx,
 *           &ANJ_MAKE_RESOURCE_INSTANCE_PATH(3, 0, 6, 1), NULL, NULL, NULL);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 *
 * _anj_attr_notification_t baterry_level_res_attr = {
 *      .has_greater_than = true,
 *      .greater_than = 50.0,
 * };
 * _anj_io_discover_ctx_new_entry(ctx, &ANJ_MAKE_RESOURCE_PATH(3, 0, 9),
 *                                      &baterry_level_res_attr, NULL, NULL);
 * _anj_io_discover_ctx_get_payload(&ctx, &out_buff[offset], BUFF_SIZE - offset,
 *                                                             &out_copied_bytes);
 * offset += out_copied_bytes;
 *
 * // outgoing message:
 * </3>;ver=1.0;pmin=10;pmax=60,</3/0>,</3/0/1>,</3/0/2>,</3/0/3>,</3/0/4>,
 * </3/0/6>;dim=2,</3/0/6/0>,</3/0/6/1>,</3/0/9>;gt=50
 * @endcode
 *
 * @param ctx            Context to operate on.
 * @param base_path      Targeted path from request.
 * @param depth          Depth parameter from request, if not present, defualt
 *                       value will be used.
 * @return
 * - 0 on success,
 * - _ANJ_IO_ERR_INPUT_ARG if:
 *  -   @p base_path is root ("/") or Resource Instance path,
 *  -   @p depth is greater than 3.
 */
int _anj_io_discover_ctx_init(_anj_io_discover_ctx_t *ctx,
                              const anj_uri_path_t *base_path,
                              const uint32_t *depth);

/**
 * Adds another Object, Object Instance Resource or Resource Instance to the
 * buffer. Remember to keep the order, the values of @p path must be increasing.
 * For @p path that is not Object @p version presence is treated as error, also
 * for @p path that is not Resource, the presence of @p dim is not allowed.
 *
 * @p attributes must be given at the level from which they apply. For example
 * if base_path from @ref _anj_io_discover_ctx_init contains Object Instance
 * value, but there are some attributes assigned to Object then they should be
 * given in @ref _anj_io_discover_ctx_new_entry call on the Object Instance
 * level. If some @p attributes are assigned on the Resource level, then then
 * they are omitted at the Resource Instances level.
 *
 * For Core Objects if the version does not match the LwM2M version being used
 * (@ref ANJ_WITH_LWM2M12) @p version must be provided. For Non-core Object
 * missing @p version is equivalent to 1.0 and is optional. The @p version
 * format is X.X where X is the digit ("The Object Version of an Object is
 * composed of 2 digits separated by a dot '.'" [LwM2M Specification]).
 *
 * Remember to keep the order, the values of the path must be
 * increasing. This function checks if given @p path is in accordance with depth
 * paramter. If depth is not defined, default values are:
 * base_path points to Object ID -> message contains Object, Object
 *                                  Instances, and Resources
 * base_path points to Object Instance ID -> message contains Object
 *                                           Instances, and Resources
 * base_path points to Resource ID -> message contains Resource and
 *                                    Resource Instances
 *
 * IMPORTANT: The user doesn't need to check compliance with depth parameter
 * and ignore the appearance of _ANJ_IO_WARNING_DEPTH warning. In this
 * situation, you can iterate over all elements of the the data model and skip
 * the records for which this warning occurred.
 *
 * @param ctx        Context to operate on.
 * @param path       Object Instance path.
 * @param attributes Attributes assigned to the @p path.
 * @param version    Object version, ignored if NULL.
 * @param dim        Number of the Resource Instances. Ignored if NULL.
 *
 * @return
 * - 0 on success,
 * - _ANJ_IO_WARNING_DEPTH if record can't be added beacuse of depth value,
 * - _ANJ_IO_ERR_INPUT_ARG if:
 *  -  given @p path is outside the base_path,
 *  -  ascending order of @p path is not respected,
 *  -  @p version format is incorrect,
 *  -  @p dim is given for @p path that is not Resource,
 *  -  @p version is given for @p path that is not Object,
 * - _ANJ_IO_ERR_LOGIC if:
 *  -  internal buffer is not empty,
 *  -  if number of provided Resource Instances doesn't match last provided
 *     @p dim value (if depth parameter allows to write Resource Instance).
 */
int _anj_io_discover_ctx_new_entry(_anj_io_discover_ctx_t *ctx,
                                   const anj_uri_path_t *path,
                                   const _anj_attr_notification_t *attributes,
                                   const char *version,
                                   const uint16_t *dim);

/**
 * Call to copy encoded message to payload buffer.
 *
 * The buffer into which the encoded message is written is given by @p out_buff.
 * @p out_copied_bytes returns information about the number of bytes written
 * during a single function call, If the function return @ref
 * ANJ_IO_NEED_NEXT_CALL it means that the buffer space has run out. If you
 * support block operation then at this point you can send the buffer as one
 * message block and call this function again and the rest of the record will be
 * written. In case return value equals <c>0</c> then you can call @ref
 * _anj_io_discover_ctx_new_entry with another Object/Object Instance and @ref
 * _anj_io_discover_ctx_get_payload with pointer to the buffer shifted by the
 * value of @p out_copied_bytes, also remember to update the value of @p
 * out_buff_len.
 *
 * The @p out_buff already contains the LWM2M message payload.
 *
 * @param      ctx              Context to operate on.
 * @param[out] out_buff         Payload buffer.
 * @param      out_buff_len     Length of payload buffer.
 * @param[out] out_copied_bytes Number of bytes that are written into the
 *                              buffer.
 *
 * @return
 * - 0 on success,
 * - ANJ_IO_NEED_NEXT_CALL if record didn't fit in the output buffer and this
 *   function have to be called again,
 * - _ANJ_IO_ERR_LOGIC if this function is called but there is no more data in
 *   internal buffer.
 */
int _anj_io_discover_ctx_get_payload(_anj_io_discover_ctx_t *ctx,
                                     void *out_buff,
                                     size_t out_buff_len,
                                     size_t *out_copied_bytes);

#    endif // ANJ_WITH_DISCOVER

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_IO_H
