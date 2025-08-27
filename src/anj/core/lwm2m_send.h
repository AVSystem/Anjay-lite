/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_SRC_LWM2M_SEND_H
#    define ANJ_SRC_LWM2M_SEND_H

#    include <anj/core.h>
#    include <anj/defs.h>

#    ifdef ANJ_WITH_LWM2M_SEND

/**
 * This function checks if any LwM2M Send request should be sent. Function
 * should be called periodically in order to properly handle the Send operation.
 * If <c>operation</c> field in @p out_msg is set to @ref ANJ_OP_INF_CON_SEND,
 * then Send request needs to be sent.
 *
 * Function can't be called if there is an ongoing exchange.
 *
 * @param      anj               Anjay object to operate on.
 * @param[out] out_handlers      Exchange handlers.
 * @param[out] out_msg           Outgoing message structure.
 */
void _anj_lwm2m_send_process(anj_t *anj,
                             _anj_exchange_handlers_t *out_handlers,
                             _anj_coap_msg_t *out_msg);

#    endif // ANJ_WITH_LWM2M_SEND

#endif // ANJ_SRC_LWM2M_SEND_H
