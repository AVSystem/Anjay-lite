/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_IO_TLV_DECODER_H
#    define SRC_ANJ_IO_TLV_DECODER_H

#    include <stdbool.h>
#    include <stddef.h>

#    include <anj/core.h>
#    include <anj/defs.h>

#    ifdef ANJ_WITH_TLV

int _anj_tlv_decoder_init(_anj_io_in_ctx_t *ctx,
                          const anj_uri_path_t *request_uri);

int _anj_tlv_decoder_feed_payload(_anj_io_in_ctx_t *ctx,
                                  void *buff,
                                  size_t buff_size,
                                  bool payload_finished);

int _anj_tlv_decoder_get_entry(_anj_io_in_ctx_t *ctx,
                               anj_data_type_t *inout_type_bitmask,
                               const anj_res_value_t **out_value,
                               const anj_uri_path_t **out_path);
#    endif // ANJ_WITH_TLV

#endif // SRC_ANJ_IO_TLV_DECODER_H
