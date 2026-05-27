/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "../init_internal.h"

#ifndef SRC_ANJ_COAP_UTILS_H
#    define SRC_ANJ_COAP_UTILS_H

#    include <stdbool.h>

#    define ANJ_INTERNAL_INCLUDE_COAP
#    include <anj_internal/coap.h>
#    undef ANJ_INTERNAL_INCLUDE_COAP

int _anj_coap_token_generate(_anj_coap_token_t *token);

void _anj_log_msg_info(const _anj_coap_msg_t *msg, bool received);

#endif // SRC_ANJ_COAP_UTILS_H
