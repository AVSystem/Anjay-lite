/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 50

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/compat/time.h>
#include <anj/defs.h>
#include <anj/log.h>
#include <anj/time.h>
#include <anj/utils.h>

#include "exchange.h"
#include "exchange_cache.h"

#ifdef ANJ_WITH_CACHE

/**
 * Calculates CoAP EXCHANGE_LIFETIME in milliseconds based on transmission
 * parameters stored in @p ctx.
 *
 * RFC 7252
 * 4.4.  Message Correlation
 * The same Message ID MUST NOT be reused (in communicating with the
 * same endpoint) within the EXCHANGE_LIFETIME (Section 4.8.2).
 *
 * Formula:
 * EXCHANGE_LIFETIME = MAX_TRANSMIT_SPAN + (2 * MAX_LATENCY) + PROCESSING_DELAY
 * MAX_TRANSMIT_SPAN = ACK_TIMEOUT * ((2^MAX_RETRANSMIT) - 1) *
 * ACK_RANDOM_FACTOR PROCESSING_DELAY = ACK_TIMEOUT
 */
static anj_time_duration_t
get_exchange_lifetime(const anj_exchange_udp_tx_params_t *tx_params) {
    double ack_random_factor = tx_params->ack_random_factor;
    uint16_t max_retransmit = tx_params->max_retransmit;
    anj_time_duration_t ack_timeout = tx_params->ack_timeout;
    anj_time_duration_t max_transmit_span = anj_time_duration_fmul(
            ack_timeout,
            ((double) ((1 << max_retransmit) - 1)) * ack_random_factor);

    anj_time_duration_t exchange_lifetime = max_transmit_span;
    exchange_lifetime = anj_time_duration_add(
            exchange_lifetime,
            anj_time_duration_mul(_ANJ_EXCHANGE_COAP_MAX_LATENCY, 2));
    exchange_lifetime = anj_time_duration_add(exchange_lifetime, ack_timeout);

    return exchange_lifetime;
}

/**
 * Invalidates recent and non-recent cache entries that have expired.
 *
 * If expiration_time < @p time_now, the entry is cleared.
 *
 * @param ctx       Exchange context with cache storage.
 * @param time_now  Current time in milliseconds.
 */
static void drop_expired(_anj_exchange_cache_t *ctx,
                         anj_time_monotonic_t time_now) {
    if (anj_time_monotonic_lt(ctx->cache_recent.expiration_time, time_now)) {
        exchange_log(L_TRACE, "Dropped recent cache");
        ctx->cache_recent.expiration_time = ANJ_TIME_MONOTONIC_INVALID;
    }

#    if ANJ_CACHE_ENTRIES_NUMBER > 1
    for (uint8_t i = 0; i < ANJ_ARRAY_SIZE(ctx->cache_non_recent); i++) {
        if (anj_time_monotonic_lt(ctx->cache_non_recent[i].expiration_time,
                                  time_now)) {
            exchange_log(L_TRACE, "Dropped cache n=%d", i);
            ctx->cache_non_recent[i].expiration_time =
                    ANJ_TIME_MONOTONIC_INVALID;
        }
    }
#    endif // ANJ_CACHE_ENTRIES_NUMBER > 1
}

static void save_recent_cache(_anj_exchange_cache_t *ctx,
                              const _anj_coap_msg_t *response,
                              anj_time_monotonic_t expiration_time) {
    memcpy(&ctx->cache_recent.response, response, sizeof(*response));
    if (response->payload && response->payload_size) {
        memcpy(&ctx->cache_recent.payload,
               response->payload,
               response->payload_size);
    }
    ctx->cache_recent.expiration_time = expiration_time;
}

void _anj_exchange_cache_add(_anj_exchange_cache_t *ctx,
                             const anj_exchange_udp_tx_params_t *tx_params,
                             const _anj_coap_msg_t *response) {
    // CoAP Downloader does not support caching
    if (ctx == NULL) {
        return;
    }

    // calculate the expiration time for currently processed entry
    anj_time_monotonic_t time_now = anj_time_monotonic_now();
    anj_time_monotonic_t expiration_time =
            anj_time_monotonic_add(time_now, get_exchange_lifetime(tx_params));

#    if ANJ_CACHE_ENTRIES_NUMBER > 1
    // free slots with expired entries
    drop_expired(ctx, time_now);

    // check if the recent cache expired. If it did, the non-recent ones did too
    if (!anj_time_monotonic_is_valid(ctx->cache_recent.expiration_time)) {
        save_recent_cache(ctx, response, expiration_time);
        exchange_log(L_TRACE, "Saved latest cache");
        return;
    }

    anj_time_monotonic_t oldest_cache_time = ANJ_TIME_MONOTONIC_INVALID;
    uint8_t candidate_id = 0;
    for (uint8_t i = 0; i < ANJ_ARRAY_SIZE(ctx->cache_non_recent); i++) {
        // check if the entry is already invalid
        if (!anj_time_monotonic_is_valid(
                    ctx->cache_non_recent[i].expiration_time)) {
            candidate_id = i;
            exchange_log(L_TRACE, "Found invalid cache and saved i=%d", i);
            break;
        }

        // if not, check if it's the candidate for overwriting
        if (!anj_time_monotonic_gt(ctx->cache_non_recent[i].expiration_time,
                                   oldest_cache_time)) {
            oldest_cache_time = ctx->cache_non_recent[i].expiration_time;
            candidate_id = i;
            exchange_log(L_TRACE, "Found candidate i=%d", i);
        }
    }

    // move the most recent cache to the non-recent array
    ctx->cache_non_recent[candidate_id].mid =
            ctx->cache_recent.response.coap_binding_data.message_id;
    ctx->cache_non_recent[candidate_id].expiration_time =
            ctx->cache_recent.expiration_time;

#    endif // ANJ_CACHE_ENTRIES_NUMBER > 1

    // update most recent cache
    save_recent_cache(ctx, response, expiration_time);
    exchange_log(L_TRACE, "Saved cache");
}

int _anj_exchange_cache_check(_anj_exchange_cache_t *ctx, uint16_t msg_id) {
    // free slots with expired entries
    anj_time_monotonic_t time_now = anj_time_monotonic_now();
    drop_expired(ctx, time_now);
    exchange_log(L_TRACE, "Checking cache");

    // check if it's the most recent message
    if (ctx->cache_recent.response.coap_binding_data.message_id == msg_id
            && anj_time_monotonic_is_valid(ctx->cache_recent.expiration_time)) {
        ctx->handling_retransmission = true;
        exchange_log(L_TRACE, "Found most recent");
        return _ANJ_EXCHANGE_CACHE_HIT_RECENT;
    }

#    if ANJ_CACHE_ENTRIES_NUMBER > 1
    // if not, check the older ones
    for (uint8_t i = 0; i < ANJ_ARRAY_SIZE(ctx->cache_non_recent); i++) {
        if (ctx->cache_non_recent[i].mid == msg_id
                && anj_time_monotonic_is_valid(
                           ctx->cache_non_recent[i].expiration_time)) {
            exchange_log(L_TRACE, "Found non recent");
            return _ANJ_EXCHANGE_CACHE_HIT_NON_RECENT;
        }
    }
#    endif // ANJ_CACHE_ENTRIES_NUMBER > 1

    return _ANJ_EXCHANGE_CACHE_MISS;
}

void _anj_exchange_cache_get(_anj_exchange_cache_t *ctx,
                             _anj_coap_msg_t *response) {
    assert(ctx->handling_retransmission);
    *response = ctx->cache_recent.response;
    response->payload = ctx->cache_recent.payload;
    exchange_log(L_TRACE, "Get recent cache");
}

#endif // ANJ_WITH_CACHE
