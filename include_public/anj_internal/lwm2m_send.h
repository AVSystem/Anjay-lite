/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_LWM2M_SEND_H
#define ANJ_INTERNAL_LWM2M_SEND_H

#ifndef ANJ_INTERNAL_INCLUDE_SEND
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_SEND

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ANJ_WITH_LWM2M_SEND

/** @anj_internal_api_do_not_use */
typedef struct _anj_send_ctx_struct {
    const anj_send_request_t *requests_queue[ANJ_LWM2M_SEND_QUEUE_SIZE];
    // ids == 0 means that the slot is free, each ids[x] corresponds to a
    // requests_queue slot, active_exchange is always related to ids[0]
    uint16_t ids[ANJ_LWM2M_SEND_QUEUE_SIZE];
    bool active_exchange;
    // set when aborting all requests
    bool abort_in_progress;
    uint16_t send_id_counter;
    // variables used to process the message payload
    bool data_to_copy;
    size_t op_count;
} _anj_send_ctx_t;

#endif // ANJ_WITH_LWM2M_SEND

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_LWM2M_SEND_H
