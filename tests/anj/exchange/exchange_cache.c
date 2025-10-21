/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */
#include <anj/init.h>

#ifdef ANJ_WITH_CACHE

#    include <stddef.h>
#    include <stdint.h>
#    include <string.h>

#    include <anj/compat/time.h>
#    include <anj/utils.h>

#    include "../../../src/anj/exchange.h"
#    include "../../../src/anj/exchange_cache.h"
#    include "../mock/time_api_mock.h"

#    include <anj_unit_test.h>

#    define DEFAULT_EXCHANGE_LIFETIME \
        anj_time_duration_new(247, ANJ_TIME_UNIT_S)

_anj_exchange_ctx_t ctx;
_anj_exchange_cache_t cache;

#    define INIT(startTime_s)                                         \
        mock_time_reset();                                            \
        mock_time_advance(                                            \
                anj_time_duration_new(startTime_s, ANJ_TIME_UNIT_S)); \
        ctx.tx_params.ack_random_factor = 1.5;                        \
        ctx.tx_params.ack_timeout =                                   \
                anj_time_duration_new(2000, ANJ_TIME_UNIT_MS);        \
        ctx.tx_params.max_retransmit = 4;                             \
        _anj_exchange_init(&ctx);                                     \
        _anj_exchange_setup_cache(&ctx, &cache);

/**
 * Add first entry
 */
ANJ_UNIT_TEST(exchange_cache, add_first_response) {
    INIT(0);

    anj_time_real_t real_time = anj_time_real_now();

    _anj_coap_msg_t dummy_response = {
        .coap_binding_data.udp.message_id = 0x1234,
        .payload_size = 4,
        .payload = (uint8_t *) "bbmm"
    };

    _anj_exchange_cache_add(&cache, &ctx.tx_params, &dummy_response);

    ASSERT_EQ(cache.cache_recent.response.coap_binding_data.udp.message_id,
              0x1234);
    ASSERT_TRUE(anj_time_real_eq(cache.cache_recent.expiration_time,
                                 anj_time_real_add(real_time,
                                                   DEFAULT_EXCHANGE_LIFETIME)));
    ASSERT_EQ(cache.cache_recent.response.payload_size, 4);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "bbmm", 4);
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        ASSERT_FALSE(anj_time_real_is_valid(
                cache.cache_non_recent[i].expiration_time));
    }
}

/**
 * Add response when recent exists and non-recent has a free slot
 */
ANJ_UNIT_TEST(exchange_cache, add_response_moves_recent_to_nonrecent) {
    INIT(100);
    anj_time_real_t real_time = anj_time_real_now();

    // recent cache is already filled
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xABCD;
    cache.cache_recent.expiration_time =
            anj_time_real_add(real_time,
                              anj_time_duration_new(20, ANJ_TIME_UNIT_S));

    cache.cache_recent.response.payload_size = 3;
    memcpy(cache.cache_recent.payload, "gcc", 3);

    // one empty slot in non-recent
    cache.cache_non_recent[0].expiration_time = ANJ_TIME_REAL_INVALID;

    // new response to add
    _anj_coap_msg_t dummy_response = {
        .coap_binding_data.udp.message_id = 0x4321,
        .payload_size = 4,
        .payload = (uint8_t *) "llvm"
    };

    _anj_exchange_cache_add(&cache, &ctx.tx_params, &dummy_response);

    // recent should now have the new response
    ASSERT_EQ(cache.cache_recent.response.coap_binding_data.udp.message_id,
              0x4321);
    ASSERT_TRUE(anj_time_real_eq(cache.cache_recent.expiration_time,
                                 anj_time_real_add(real_time,
                                                   DEFAULT_EXCHANGE_LIFETIME)));
    ASSERT_EQ(cache.cache_recent.response.payload_size, 4);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "llvm", 4);

    // the old recent should now be in first non-recent slot
    ASSERT_EQ(cache.cache_non_recent[0].mid, 0xABCD);
    ASSERT_TRUE(anj_time_real_eq(
            cache.cache_non_recent[0].expiration_time,
            anj_time_real_add(real_time,
                              anj_time_duration_new(20, ANJ_TIME_UNIT_S))));

    // remaining non-recent slots must still be undefined
    for (size_t i = 1; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        ASSERT_FALSE(anj_time_real_is_valid(
                cache.cache_non_recent[i].expiration_time));
    }
}

/**
 * Add response when non-recent has expired slot — should reuse it instead of
 * overwriting valid ones
 */
ANJ_UNIT_TEST(exchange_cache, add_response_uses_expired_nonrecent_slot) {
    INIT(110);
    anj_time_real_t real_time = anj_time_real_now();

    // recent cache holds a valid entry
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time =
            anj_time_real_add(real_time,
                              anj_time_duration_new(150, ANJ_TIME_UNIT_S));
    cache.cache_recent.response.payload_size = 5;
    memcpy(cache.cache_recent.payload, "chlbk", 5);

    // fill all non-recent slots
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = (uint16_t) (0x1000 + i);
        cache.cache_non_recent[i].expiration_time = anj_time_real_add(
                real_time, anj_time_duration_new(150 - i, ANJ_TIME_UNIT_S));
    }

    // make the last entry expired
    cache.cache_non_recent[ANJ_CACHE_ENTRIES_NUMBER - 2].expiration_time =
            anj_time_real_add(real_time,
                              anj_time_duration_new(95, ANJ_TIME_UNIT_S));

    // new response to store
    _anj_coap_msg_t dummy_response = {
        .coap_binding_data.udp.message_id = 0xBBBB,
        .payload_size = 4,
        .payload = (uint8_t *) "krmb"
    };

    _anj_exchange_cache_add(&cache, &ctx.tx_params, &dummy_response);

    const anj_time_real_t expected_expiration =
            anj_time_real_add(real_time, DEFAULT_EXCHANGE_LIFETIME);

    // recent should be updated
    ASSERT_EQ(cache.cache_recent.response.coap_binding_data.udp.message_id,
              0xBBBB);
    ASSERT_TRUE(anj_time_real_eq(cache.cache_recent.expiration_time,
                                 expected_expiration));
    ASSERT_EQ(cache.cache_recent.response.payload_size, 4);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "krmb", 4);

    // expired entry should be overwritten with cache_recent
    ASSERT_EQ(cache.cache_non_recent[ANJ_CACHE_ENTRIES_NUMBER - 2].mid, 0xAAAA);
    ASSERT_TRUE(anj_time_real_eq(
            cache.cache_non_recent[ANJ_CACHE_ENTRIES_NUMBER - 2]
                    .expiration_time,
            anj_time_real_add(real_time,
                              anj_time_duration_new(150, ANJ_TIME_UNIT_S))));

    // all others stay unchanged
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent) - 1; ++i) {
        ASSERT_EQ(cache.cache_non_recent[i].mid, 0x1000 + i);
        ASSERT_TRUE(anj_time_real_eq(
                cache.cache_non_recent[i].expiration_time,

                anj_time_real_add(real_time,
                                  anj_time_duration_new(150 - i,
                                                        ANJ_TIME_UNIT_S))));
    }
}

/**
 * Add response when all non-recent slots are valid — should overwrite the one
 * with earliest expiration time
 */
ANJ_UNIT_TEST(exchange_cache, add_response_overwrites_oldest_valid_nonrecent) {
    INIT(120);
    anj_time_real_t real_time = anj_time_real_now();

    // recent cache contains a valid response
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time =
            anj_time_real_add(real_time,
                              anj_time_duration_new(121, ANJ_TIME_UNIT_S));
    cache.cache_recent.response.payload_size = 4;
    memcpy(cache.cache_recent.payload, "test", 4);

    // all non-recent entries are valid; slot 1 has the earliest expiration
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = (uint16_t) (0x1000 + i);
        cache.cache_non_recent[i].expiration_time = anj_time_real_add(
                real_time,
                anj_time_duration_new(125 + i, ANJ_TIME_UNIT_S)); // ascending
    }
    cache.cache_non_recent[1].expiration_time = anj_time_real_add(
            real_time,
            anj_time_duration_new(121500, ANJ_TIME_UNIT_MS)); // oldest valid

    // new response to store
    _anj_coap_msg_t dummy_response = {
        .coap_binding_data.udp.message_id = 0xBBBB,
        .payload_size = 5,
        .payload = (uint8_t *) "esacz"
    };

    _anj_exchange_cache_add(&cache, &ctx.tx_params, &dummy_response);

    // recent must now contain new response
    ASSERT_EQ(cache.cache_recent.response.coap_binding_data.udp.message_id,
              0xBBBB);
    ASSERT_TRUE(anj_time_real_eq(cache.cache_recent.expiration_time,
                                 anj_time_real_add(real_time,
                                                   DEFAULT_EXCHANGE_LIFETIME)));
    ASSERT_EQ(cache.cache_recent.response.payload_size, 5);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "esacz", 5);

    // slot 1 should be overwritten with old recent
    ASSERT_EQ(cache.cache_non_recent[1].mid, 0xAAAA);
    ASSERT_TRUE(anj_time_real_eq(
            cache.cache_non_recent[1].expiration_time,
            anj_time_real_add(real_time,
                              anj_time_duration_new(121, ANJ_TIME_UNIT_S))));

    // all other non-recent entries should be unchanged
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        if (i == 1) {
            continue; // already validated
        }
        ASSERT_EQ(cache.cache_non_recent[i].mid, 0x1000 + i);
        ASSERT_TRUE(anj_time_real_eq(
                cache.cache_non_recent[i].expiration_time,
                anj_time_real_add(real_time,
                                  anj_time_duration_new(125 + i,
                                                        ANJ_TIME_UNIT_S))));
    }
}

/**
 * Adds new response as recent when the existing recent entry is expired.
 */
ANJ_UNIT_TEST(exchange_cache, adds_new_response_when_recent_is_expired) {
    INIT(121);
    anj_time_real_t real_time = anj_time_real_now();

    // Expired recent entry
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xABCD;
    cache.cache_recent.expiration_time =
            anj_time_real_new(100, ANJ_TIME_UNIT_S);

    // Non-expired non-recent entries
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = 0x2000 + i;
        cache.cache_non_recent[i].expiration_time = anj_time_real_add(
                real_time, anj_time_duration_new(200 + i, ANJ_TIME_UNIT_S));
    }

    _anj_coap_msg_t new_response = {
        .coap_binding_data.udp.message_id = 0x4321,
        .payload_size = 6,
        .payload = (uint8_t *) "whllbm"
    };

    _anj_exchange_cache_add(&cache, &ctx.tx_params, &new_response);

    ASSERT_EQ(cache.cache_recent.response.coap_binding_data.udp.message_id,
              0x4321);
    ASSERT_TRUE(anj_time_real_eq(cache.cache_recent.expiration_time,
                                 anj_time_real_add(real_time,
                                                   DEFAULT_EXCHANGE_LIFETIME)));
    ASSERT_EQ(cache.cache_recent.response.payload_size, 6);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "whllbm", 6);

    // non-recent entries remain unchanged - no unnecesary logic is executed.
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        ASSERT_EQ(cache.cache_non_recent[i].mid, 0x2000 + i);
        ASSERT_TRUE(anj_time_real_eq(
                cache.cache_non_recent[i].expiration_time,
                anj_time_real_add(real_time,
                                  anj_time_duration_new(200 + i,
                                                        ANJ_TIME_UNIT_S))));
    }
}

/**
 * Returns HIT_RECENT when msg_id matches cache_recent
 */
ANJ_UNIT_TEST(exchange_cache, returns_recent_if_mid_matches_recent) {
    INIT(100);

    anj_time_real_t real_time = anj_time_real_now();

    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xABCD;
    cache.cache_recent.expiration_time = anj_time_real_add(
            real_time,
            anj_time_duration_new(121, ANJ_TIME_UNIT_S)); // still valid
    cache.handling_retransmission = false;

    int result = _anj_exchange_cache_check(&cache, 0xABCD);

    ASSERT_EQ(result, _ANJ_EXCHANGE_CACHE_HIT_RECENT);
    ASSERT_TRUE(cache.handling_retransmission);
}

/**
 * Returns HIT_NON_RECENT when msg_id matches one of non-recent entries
 */
ANJ_UNIT_TEST(exchange_cache, returns_nonrecent_if_mid_matches_nonrecent) {
    INIT(121);
    anj_time_real_t real_time = anj_time_real_now();

    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time =
            anj_time_real_add(real_time,
                              anj_time_duration_new(130, ANJ_TIME_UNIT_S));

    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = 0x1000 + i;
        cache.cache_non_recent[i].expiration_time = anj_time_real_add(
                real_time,
                anj_time_duration_new(130 + i,
                                      ANJ_TIME_UNIT_S)); // all still valid
    }

    int result = _anj_exchange_cache_check(&cache, 0x1002); // matches i == 2

    ASSERT_EQ(result, _ANJ_EXCHANGE_CACHE_HIT_NON_RECENT);
    ASSERT_FALSE(cache.handling_retransmission);
}

/**
 * Returns GET_NO_ENTRY when msg_id is not found in recent nor non-recent
 */
ANJ_UNIT_TEST(exchange_cache, returns_no_entry_if_mid_matches_nothing) {
    INIT(121);

    cache.handling_retransmission = false;
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time =
            anj_time_real_new(130, ANJ_TIME_UNIT_S);

    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = 0x1000 + i;
        cache.cache_non_recent[i].expiration_time =
                anj_time_real_new(130 + i, ANJ_TIME_UNIT_S);
    }

    int result = _anj_exchange_cache_check(&cache, 0xFFFF);

    ASSERT_EQ(result, _ANJ_EXCHANGE_CACHE_MISS);
    ASSERT_FALSE(cache.handling_retransmission);
}

/**
 * Expired non-recent entry should be ignored during cache lookup
 */
ANJ_UNIT_TEST(exchange_cache, expired_nonrecent_entry_is_ignored) {
    INIT(121);

    cache.handling_retransmission = false;
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time =
            anj_time_real_new(130, ANJ_TIME_UNIT_S);

    // Expired recent entry
    cache.cache_non_recent[2].mid = 0xBEEF;
    cache.cache_non_recent[2].expiration_time =
            anj_time_real_new(120, ANJ_TIME_UNIT_S);

    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        if (i == 2)
            continue;
        cache.cache_non_recent[i].mid = 0x1000 + i;
        cache.cache_non_recent[i].expiration_time =
                anj_time_real_new(150 + i, ANJ_TIME_UNIT_S);
    }

    int result = _anj_exchange_cache_check(&cache, 0xBEEF);

    ASSERT_EQ(result, _ANJ_EXCHANGE_CACHE_MISS);
    ASSERT_FALSE(cache.handling_retransmission);
    ASSERT_FALSE(
            anj_time_real_is_valid(cache.cache_non_recent[2].expiration_time));
}

/**
 * Copies recent response and clears handling_retransmission flag
 */
ANJ_UNIT_TEST(exchange_cache, copies_recent_response_and_clears_flag) {
    INIT(0);

    _anj_coap_msg_t dummy = {
        .coap_binding_data.udp.message_id = 0xDEAD,
        .payload_size = 3,
        .payload = (uint8_t *) "OK\n"
    };

    cache.cache_recent.response = dummy;
    memcpy(cache.cache_recent.payload, dummy.payload, 3);
    cache.cache_recent.expiration_time =
            anj_time_real_new(150, ANJ_TIME_UNIT_S);
    cache.handling_retransmission = true;

    _anj_coap_msg_t output = { 0 };

    _anj_exchange_cache_get(&cache, &output);

    ASSERT_EQ(output.coap_binding_data.udp.message_id, 0xDEAD);
    ASSERT_EQ(output.payload_size, 3);
    ASSERT_EQ_BYTES_SIZED(output.payload, (uint8_t *) "OK\n", 3);
    ASSERT_TRUE(cache.handling_retransmission);
}

#endif // ANJ_WITH_CACHE
