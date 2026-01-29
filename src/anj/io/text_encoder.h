/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_IO_TEXT_ENCODER_H
#    define SRC_ANJ_IO_TEXT_ENCODER_H

#    include <stddef.h>

#    include <anj/core.h>
#    include <anj/defs.h>

#    ifdef ANJ_WITH_PLAINTEXT

int _anj_text_encoder_init(_anj_io_out_ctx_t *ctx);

int _anj_text_out_ctx_new_entry(_anj_io_out_ctx_t *ctx,
                                const anj_io_out_entry_t *entry);
int _anj_text_get_extended_data_payload(void *out_buff,
                                        size_t out_buff_len,
                                        size_t *inout_copied_bytes,
                                        _anj_io_buff_t *ctx,
                                        const anj_io_out_entry_t *entry);
#    endif // ANJ_WITH_PLAINTEXT

#endif // SRC_ANJ_IO_TEXT_ENCODER_H
