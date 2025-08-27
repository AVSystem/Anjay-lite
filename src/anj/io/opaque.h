/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_IO_OPAQUE_H
#    define SRC_ANJ_IO_OPAQUE_H

#    include <stdbool.h>
#    include <stddef.h>

#    include <anj/core.h>
#    include <anj/defs.h>

#    ifdef ANJ_WITH_OPAQUE

int _anj_opaque_out_init(_anj_io_out_ctx_t *ctx);

int _anj_opaque_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                  const anj_io_out_entry_t *entry);

int _anj_opaque_get_extended_data_payload(void *out_buff,
                                          size_t out_buff_len,
                                          size_t *out_copied_bytes,
                                          _anj_io_buff_t *ctx,
                                          const anj_io_out_entry_t *entry);

int _anj_opaque_decoder_init(_anj_io_in_ctx_t *ctx,
                             const anj_uri_path_t *request_uri);

int _anj_opaque_decoder_feed_payload(_anj_io_in_ctx_t *ctx,
                                     void *buff,
                                     size_t buff_size,
                                     bool payload_finished);

int _anj_opaque_decoder_get_entry(_anj_io_in_ctx_t *ctx,
                                  anj_data_type_t *inout_type_bitmask,
                                  const anj_res_value_t **out_value,
                                  const anj_uri_path_t **out_path);

int _anj_opaque_decoder_get_entry_count(_anj_io_in_ctx_t *ctx,
                                        size_t *out_count);

#    endif // ANJ_WITH_OPAQUE

#endif // SRC_ANJ_IO_OPAQUE_H
