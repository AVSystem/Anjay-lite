/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_EXCHANGE_CACHE_H
#    define SRC_ANJ_EXCHANGE_CACHE_H

#    include <stdint.h>

#    include <anj/defs.h>

#    include "exchange.h"

#    define _ANJ_EXCHANGE_CACHE_HIT_RECENT 0
#    define _ANJ_EXCHANGE_CACHE_HIT_NON_RECENT 1
#    define _ANJ_EXCHANGE_CACHE_MISS 2

#    ifdef ANJ_WITH_CACHE

/**
 * Adds a new response to the exchange cache.
 *
 * The most recent entry is stored in the "recent" slot. Previously cached
 * recent response is either moved to the non-recent array (if valid),
 * or dropped if expired. If no free slot is available in non-recent array,
 * the one with the earliest expiration time is reused.
 *
 * Used to detect and suppress duplicate CoAP requests, per RFC 7252 §4.5.
 *
 * @param ctx       Exchange cache context.
 * @param tx_params Transmission parameters used to calculate expiration time.
 * @param response  Response message to cache.
 */
void _anj_exchange_cache_add(_anj_exchange_cache_t *ctx,
                             const anj_exchange_udp_tx_params_t *tx_params,
                             const _anj_coap_msg_t *response);

/**
 * Checks if a message ID has already been seen during the current
 * EXCHANGE_LIFETIME.
 *
 * If the message ID matches a recent or non-recent cached response,
 * the function indicates the type of match. Matching recent responses
 * also enable retransmission handling logic via the internal flag.
 *
 * Expired entries are dropped before the check.
 *
 * @param ctx        Exchange cache context.
 * @param msg_id     Message ID to check.
 *
 * @returns One of:
 *          - _ANJ_EXCHANGE_CACHE_HIT_RECENT if it matches the most recent
 *            response,
 *          - _ANJ_EXCHANGE_CACHE_HIT_NON-RECENT if found in the cache history,
 *          - _ANJ_EXCHANGE_CACHE_MISS if no match is found.
 */
int _anj_exchange_cache_check(_anj_exchange_cache_t *ctx, uint16_t msg_id);

/**
 * Retrieves the cached response for a retransmitted message.
 *
 * This function must only be called after _anj_exchange_cache_check()
 * has returned _ANJ_EXCHANGE_CACHE_GET_RECENT. It copies the cached
 * response into @p response and resets the retransmission flag.
 *
 * @param ctx     Exchange cache context.
 * @param[out]    response Output parameter that will receive the cached
 *                response.
 *
 * @note Assertions will fail if preconditions are not met.
 */
void _anj_exchange_cache_get(_anj_exchange_cache_t *ctx,
                             _anj_coap_msg_t *response);

#    endif // ANJ_WITH_CACHE

#endif // SRC_ANJ_EXCHANGE_CACHE_H
