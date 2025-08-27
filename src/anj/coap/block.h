/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_COAP_BLOCK_H
#    define SRC_ANJ_COAP_BLOCK_H

#    include "coap.h"
#    include "options.h"

int _anj_block_decode(anj_coap_options_t *opts, _anj_block_t *block);

int _anj_block_prepare(anj_coap_options_t *opts, const _anj_block_t *block);

#endif // SRC_ANJ_COAP_BLOCK_H
