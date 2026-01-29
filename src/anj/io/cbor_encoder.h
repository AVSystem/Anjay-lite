/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_IO_CBOR_ENCODER_H
#    define SRC_ANJ_IO_CBOR_ENCODER_H

#    include <stdbool.h>
#    include <stddef.h>

#    include <anj/core.h>
#    include <anj/defs.h>

#    ifdef ANJ_WITH_CBOR
int _anj_cbor_encoder_init(_anj_io_out_ctx_t *ctx);

int _anj_cbor_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                const anj_io_out_entry_t *entry);
#    endif // ANJ_WITH_CBOR

#    ifdef ANJ_WITH_SENML_CBOR
int _anj_senml_cbor_encoder_init(_anj_io_out_ctx_t *ctx,
                                 const anj_uri_path_t *base_path,
                                 size_t items_count,
                                 bool encode_time);

int _anj_senml_cbor_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                      const anj_io_out_entry_t *entry);
#    endif // ANJ_WITH_SENML_CBOR

#    ifdef ANJ_WITH_LWM2M_CBOR
int _anj_lwm2m_cbor_encoder_init(_anj_io_out_ctx_t *ctx,
                                 const anj_uri_path_t *base_path,
                                 size_t items_count);

int _anj_lwm2m_cbor_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                      const anj_io_out_entry_t *entry);

int _anj_get_lwm2m_cbor_map_ends(_anj_io_out_ctx_t *ctx,
                                 void *out_buff,
                                 size_t out_buff_len,
                                 size_t *inout_copied_bytes);
#    endif // ANJ_WITH_LWM2M_CBOR

#    if defined(ANJ_WITH_CBOR) || defined(ANJ_WITH_LWM2M_CBOR)
int _anj_cbor_encode_value(_anj_io_buff_t *buff_ctx,
                           const anj_io_out_entry_t *entry);
#    endif // defined(ANJ_WITH_CBOR) || defined(ANJ_WITH_LWM2M_CBOR)

#endif // SRC_ANJ_IO_CBOR_ENCODER_H
