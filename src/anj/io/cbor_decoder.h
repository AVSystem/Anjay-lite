/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_IO_CBOR_DECODER_H
#    define SRC_ANJ_IO_CBOR_DECODER_H

#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    include <anj/core.h>
#    include <anj/defs.h>

#    if defined(ANJ_WITH_CBOR) || defined(ANJ_WITH_SENML_CBOR) \
            || defined(ANJ_WITH_LWM2M_CBOR)
int _anj_cbor_get_i64_from_ll_number(const _anj_cbor_ll_number_t *number,
                                     int64_t *out_value,
                                     bool allow_convert_fractions);

int _anj_cbor_get_u64_from_ll_number(const _anj_cbor_ll_number_t *number,
                                     uint64_t *out_value);

int _anj_cbor_get_double_from_ll_number(const _anj_cbor_ll_number_t *number,
                                        double *out_value);

int _anj_cbor_get_short_string(_anj_cbor_ll_decoder_t *ctx,
                               _anj_cbor_ll_decoder_bytes_ctx_t **bytes_ctx_ptr,
                               size_t *bytes_consumed_ptr,
                               char *out_string_buf,
                               size_t out_string_buf_size);
#    endif /* defined(ANJ_WITH_CBOR) || defined(ANJ_WITH_SENML_CBOR) || \
              defined(ANJ_WITH_LWM2M_CBOR) */

#    if defined(ANJ_WITH_CBOR) || defined(ANJ_WITH_LWM2M_CBOR)
int _anj_cbor_extract_value(
        _anj_cbor_ll_decoder_t *ctx,
        _anj_cbor_ll_decoder_bytes_ctx_t **bytes_ctx_ptr,
        size_t *bytes_consumed_ptr,
        char (*objlnk_buf)[_ANJ_IO_CBOR_MAX_OBJLNK_STRING_SIZE],
        anj_data_type_t *inout_type_bitmask,
        anj_res_value_t *out_value);
#    endif // defined(ANJ_WITH_CBOR) || defined(ANJ_WITH_LWM2M_CBOR)

#    ifdef ANJ_WITH_CBOR
int _anj_cbor_decoder_init(_anj_io_in_ctx_t *ctx,
                           const anj_uri_path_t *base_path);

int _anj_cbor_decoder_feed_payload(_anj_io_in_ctx_t *ctx,
                                   void *buff,
                                   size_t buff_size,
                                   bool payload_finished);

int _anj_cbor_decoder_get_entry(_anj_io_in_ctx_t *ctx,
                                anj_data_type_t *inout_type_bitmask,
                                const anj_res_value_t **out_value,
                                const anj_uri_path_t **out_path);

int _anj_cbor_decoder_get_entry_count(_anj_io_in_ctx_t *ctx, size_t *out_count);
#    endif // ANJ_WITH_CBOR

#    ifdef ANJ_WITH_SENML_CBOR
int _anj_senml_cbor_decoder_init(_anj_io_in_ctx_t *ctx,
                                 _anj_op_t operation_type,
                                 const anj_uri_path_t *base_path);

int _anj_senml_cbor_decoder_feed_payload(_anj_io_in_ctx_t *ctx,
                                         void *buff,
                                         size_t buff_size,
                                         bool payload_finished);

int _anj_senml_cbor_decoder_get_entry(_anj_io_in_ctx_t *ctx,
                                      anj_data_type_t *inout_type_bitmask,
                                      const anj_res_value_t **out_value,
                                      const anj_uri_path_t **out_path);

int _anj_senml_cbor_decoder_get_entry_count(_anj_io_in_ctx_t *ctx,
                                            size_t *out_count);
#    endif // ANJ_WITH_SENML_CBOR

#    ifdef ANJ_WITH_LWM2M_CBOR
int _anj_lwm2m_cbor_decoder_init(_anj_io_in_ctx_t *ctx,
                                 const anj_uri_path_t *base_path);

int _anj_lwm2m_cbor_decoder_feed_payload(_anj_io_in_ctx_t *ctx,
                                         void *buff,
                                         size_t buff_size,
                                         bool payload_finished);

int _anj_lwm2m_cbor_decoder_get_entry(_anj_io_in_ctx_t *ctx,
                                      anj_data_type_t *inout_type_bitmask,
                                      const anj_res_value_t **out_value,
                                      const anj_uri_path_t **out_path);
#    endif // ANJ_WITH_LWM2M_CBOR

#endif // ANJ_CBOR_ENCODER_H
