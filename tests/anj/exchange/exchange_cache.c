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

#    define DEFAULT_EXCHANGE_LIFETIME 247000

_anj_exchange_ctx_t ctx;
_anj_exchange_cache_t cache;

#    define INIT()                             \
        ctx.tx_params.ack_random_factor = 1.5; \
        ctx.tx_params.ack_timeout_ms = 2000;   \
        ctx.tx_params.max_retransmit = 4;      \
        _anj_exchange_init(&ctx, 42);          \
        _anj_exchange_setup_cache(&ctx, &cache);

/**
 * Add first entry
 */
ANJ_UNIT_TEST(exchange_cache, add_first_response) {
    INIT();
    set_mock_time(0);

    _anj_coap_msg_t dummy_response = {
        .coap_binding_data.udp.message_id = 0x1234,
        .payload_size = 4,
        .payload = (uint8_t *) "bbmm"
    };

    _anj_exchange_cache_add(&cache, &ctx.tx_params, &dummy_response);

    ASSERT_EQ(cache.cache_recent.response.coap_binding_data.udp.message_id,
              0x1234);
    ASSERT_EQ(cache.cache_recent.expiration_time, DEFAULT_EXCHANGE_LIFETIME);
    ASSERT_EQ(cache.cache_recent.response.payload_size, 4);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "bbmm", 4);
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        ASSERT_EQ(cache.cache_non_recent[i].expiration_time,
                  ANJ_TIME_UNDEFINED);
    }
}

/**
 * Add response when recent exists and non-recent has a free slot
 */
ANJ_UNIT_TEST(exchange_cache, add_response_moves_recent_to_nonrecent) {
    INIT();
    set_mock_time(100000);

    // recent cache is already filled
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xABCD;
    cache.cache_recent.expiration_time = 120000;
    cache.cache_recent.response.payload_size = 3;
    memcpy(cache.cache_recent.payload, "gcc", 3);

    // one empty slot in non-recent
    cache.cache_non_recent[0].expiration_time = ANJ_TIME_UNDEFINED;

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
    ASSERT_EQ(cache.cache_recent.expiration_time,
              100000 + DEFAULT_EXCHANGE_LIFETIME);
    ASSERT_EQ(cache.cache_recent.response.payload_size, 4);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "llvm", 4);

    // the old recent should now be in first non-recent slot
    ASSERT_EQ(cache.cache_non_recent[0].mid, 0xABCD);
    ASSERT_EQ(cache.cache_non_recent[0].expiration_time, 120000);

    // remaining non-recent slots must still be undefined
    for (size_t i = 1; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        ASSERT_EQ(cache.cache_non_recent[i].expiration_time,
                  ANJ_TIME_UNDEFINED);
    }
}

/**
 * Add response when non-recent has expired slot — should reuse it instead of
 * overwriting valid ones
 */
ANJ_UNIT_TEST(exchange_cache, add_response_uses_expired_nonrecent_slot) {
    INIT();
    set_mock_time(110000);

    // recent cache holds a valid entry
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time = 150000;
    cache.cache_recent.response.payload_size = 5;
    memcpy(cache.cache_recent.payload, "chlbk", 5);

    // fill all non-recent slots
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = (uint16_t) (0x1000 + i);
        cache.cache_non_recent[i].expiration_time = 150000 - (i * 1000);
    }

    // make the last entry expired
    cache.cache_non_recent[ANJ_CACHE_ENTRIES_NUMBER - 2].expiration_time =
            95000;

    // new response to store
    _anj_coap_msg_t dummy_response = {
        .coap_binding_data.udp.message_id = 0xBBBB,
        .payload_size = 4,
        .payload = (uint8_t *) "krmb"
    };

    _anj_exchange_cache_add(&cache, &ctx.tx_params, &dummy_response);

    const uint64_t expected_expiration = 110000 + DEFAULT_EXCHANGE_LIFETIME;

    // recent should be updated
    ASSERT_EQ(cache.cache_recent.response.coap_binding_data.udp.message_id,
              0xBBBB);
    ASSERT_EQ(cache.cache_recent.expiration_time, expected_expiration);
    ASSERT_EQ(cache.cache_recent.response.payload_size, 4);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "krmb", 4);

    // expired entry should be overwritten with cache_recent
    ASSERT_EQ(cache.cache_non_recent[ANJ_CACHE_ENTRIES_NUMBER - 2].mid, 0xAAAA);
    ASSERT_EQ(cache.cache_non_recent[ANJ_CACHE_ENTRIES_NUMBER - 2]
                      .expiration_time,
              150000);

    // all others stay unchanged
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent) - 1; ++i) {
        ASSERT_EQ(cache.cache_non_recent[i].mid, 0x1000 + i);
        ASSERT_EQ(cache.cache_non_recent[i].expiration_time,
                  150000 - (i * 1000));
    }
}

/**
 * Add response when all non-recent slots are valid — should overwrite the one
 * with earliest expiration time
 */
ANJ_UNIT_TEST(exchange_cache, add_response_overwrites_oldest_valid_nonrecent) {
    INIT();
    set_mock_time(120000);

    // recent cache contains a valid response
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time = 121000;
    cache.cache_recent.response.payload_size = 4;
    memcpy(cache.cache_recent.payload, "test", 4);

    // all non-recent entries are valid; slot 1 has the earliest expiration
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = (uint16_t) (0x1000 + i);
        cache.cache_non_recent[i].expiration_time =
                125000 + (i * 1000); // ascending
    }
    cache.cache_non_recent[1].expiration_time = 121500; // oldest valid

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
    ASSERT_EQ(cache.cache_recent.expiration_time,
              120000 + DEFAULT_EXCHANGE_LIFETIME);
    ASSERT_EQ(cache.cache_recent.response.payload_size, 5);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "esacz", 5);

    // slot 1 should be overwritten with old recent
    ASSERT_EQ(cache.cache_non_recent[1].mid, 0xAAAA);
    ASSERT_EQ(cache.cache_non_recent[1].expiration_time, 121000);

    // all other non-recent entries should be unchanged
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        if (i == 1) {
            continue; // already validated
        }
        ASSERT_EQ(cache.cache_non_recent[i].mid, 0x1000 + i);
        ASSERT_EQ(cache.cache_non_recent[i].expiration_time,
                  125000 + (i * 1000));
    }
}

/**
 * Adds new response as recent when the existing recent entry is expired.
 */
ANJ_UNIT_TEST(exchange_cache, adds_new_response_when_recent_is_expired) {
    INIT();
    set_mock_time(121000);

    // Expired recent entry
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xABCD;
    cache.cache_recent.expiration_time = 100000;

    // Non-expired non-recent entries
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = 0x2000 + i;
        cache.cache_non_recent[i].expiration_time = 200000 + i * 1000;
    }

    _anj_coap_msg_t new_response = {
        .coap_binding_data.udp.message_id = 0x4321,
        .payload_size = 6,
        .payload = (uint8_t *) "whllbm"
    };

    _anj_exchange_cache_add(&cache, &ctx.tx_params, &new_response);

    ASSERT_EQ(cache.cache_recent.response.coap_binding_data.udp.message_id,
              0x4321);
    ASSERT_EQ(cache.cache_recent.expiration_time,
              121000 + DEFAULT_EXCHANGE_LIFETIME);
    ASSERT_EQ(cache.cache_recent.response.payload_size, 6);
    ASSERT_EQ_BYTES_SIZED(cache.cache_recent.payload, (uint8_t *) "whllbm", 6);

    // non-recent entries remain unchanged - no unnecesary logic is executed.
    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        ASSERT_EQ(cache.cache_non_recent[i].mid, 0x2000 + i);
        ASSERT_EQ(cache.cache_non_recent[i].expiration_time, 200000 + i * 1000);
    }
}

/**
 * Returns HIT_RECENT when msg_id matches cache_recent
 */
ANJ_UNIT_TEST(exchange_cache, returns_recent_if_mid_matches_recent) {
    INIT();
    set_mock_time(100000);

    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xABCD;
    cache.cache_recent.expiration_time = 121000; // still valid
    cache.handling_retransmission = false;

    int result = _anj_exchange_cache_check(&cache, 0xABCD);

    ASSERT_EQ(result, _ANJ_EXCHANGE_CACHE_HIT_RECENT);
    ASSERT_TRUE(cache.handling_retransmission);
}

/**
 * Returns HIT_NON_RECENT when msg_id matches one of non-recent entries
 */
ANJ_UNIT_TEST(exchange_cache, returns_nonrecent_if_mid_matches_nonrecent) {
    INIT();
    set_mock_time(121000);

    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time = 130000;

    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = 0x1000 + i;
        cache.cache_non_recent[i].expiration_time =
                130000 + (i * 1000); // all still valid
    }

    int result = _anj_exchange_cache_check(&cache, 0x1002); // matches i == 2

    ASSERT_EQ(result, _ANJ_EXCHANGE_CACHE_HIT_NON_RECENT);
    ASSERT_FALSE(cache.handling_retransmission);
}

/**
 * Returns GET_NO_ENTRY when msg_id is not found in recent nor non-recent
 */
ANJ_UNIT_TEST(exchange_cache, returns_no_entry_if_mid_matches_nothing) {
    INIT();
    set_mock_time(121000);

    cache.handling_retransmission = false;
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time = 130000;

    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        cache.cache_non_recent[i].mid = 0x1000 + i;
        cache.cache_non_recent[i].expiration_time = 130000 + (i * 1000);
    }

    int result = _anj_exchange_cache_check(&cache, 0xFFFF);

    ASSERT_EQ(result, _ANJ_EXCHANGE_CACHE_MISS);
    ASSERT_FALSE(cache.handling_retransmission);
}

/**
 * Expired non-recent entry should be ignored during cache lookup
 */
ANJ_UNIT_TEST(exchange_cache, expired_nonrecent_entry_is_ignored) {
    INIT();
    set_mock_time(121000);

    cache.handling_retransmission = false;
    cache.cache_recent.response.coap_binding_data.udp.message_id = 0xAAAA;
    cache.cache_recent.expiration_time = 130000;

    // Expired recent entry
    cache.cache_non_recent[2].mid = 0xBEEF;
    cache.cache_non_recent[2].expiration_time = 120000;

    for (size_t i = 0; i < ANJ_ARRAY_SIZE(cache.cache_non_recent); ++i) {
        if (i == 2)
            continue;
        cache.cache_non_recent[i].mid = 0x1000 + i;
        cache.cache_non_recent[i].expiration_time = 150000 + (i * 1000);
    }

    int result = _anj_exchange_cache_check(&cache, 0xBEEF);

    ASSERT_EQ(result, _ANJ_EXCHANGE_CACHE_MISS);
    ASSERT_FALSE(cache.handling_retransmission);
    ASSERT_EQ(cache.cache_non_recent[2].expiration_time, ANJ_TIME_UNDEFINED);
}

/**
 * Copies recent response and clears handling_retransmission flag
 */
ANJ_UNIT_TEST(exchange_cache, copies_recent_response_and_clears_flag) {
    INIT();

    _anj_coap_msg_t dummy = {
        .coap_binding_data.udp.message_id = 0xDEAD,
        .payload_size = 3,
        .payload = (uint8_t *) "OK\n"
    };

    cache.cache_recent.response = dummy;
    memcpy(cache.cache_recent.payload, dummy.payload, 3);
    cache.cache_recent.expiration_time = 150000;
    cache.handling_retransmission = true;

    _anj_coap_msg_t output = { 0 };

    _anj_exchange_cache_get(&cache, &output);

    ASSERT_EQ(output.coap_binding_data.udp.message_id, 0xDEAD);
    ASSERT_EQ(output.payload_size, 3);
    ASSERT_EQ_BYTES_SIZED(output.payload, (uint8_t *) "OK\n", 3);
    ASSERT_TRUE(cache.handling_retransmission);
}

#endif // ANJ_WITH_CACHE
