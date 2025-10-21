/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ANJ_UNIT_ENABLE_SHORT_ASSERTS

#include <anj/time.h>

#include <anj_unit_test.h>

ANJ_UNIT_TEST(time_api, unit_conversion) {
    anj_time_duration_t duration = anj_time_duration_new(1, ANJ_TIME_UNIT_US);
    ASSERT_EQ(duration.us, 1);
    ASSERT_EQ(anj_time_duration_to_scalar(duration, ANJ_TIME_UNIT_US), 1);

    duration = anj_time_duration_new(1, ANJ_TIME_UNIT_MS);
    ASSERT_EQ(duration.us, 1000UL);
    ASSERT_EQ(anj_time_duration_to_scalar(duration, ANJ_TIME_UNIT_MS), 1UL);

    duration = anj_time_duration_new(1, ANJ_TIME_UNIT_S);
    ASSERT_EQ(duration.us, 1000UL * 1000UL);
    ASSERT_EQ(anj_time_duration_to_scalar(duration, ANJ_TIME_UNIT_S), 1UL);

    duration = anj_time_duration_new(1, ANJ_TIME_UNIT_MIN);
    ASSERT_EQ(duration.us, 60UL * 1000UL * 1000UL);
    ASSERT_EQ(anj_time_duration_to_scalar(duration, ANJ_TIME_UNIT_MIN), 1UL);

    duration = anj_time_duration_new(1, ANJ_TIME_UNIT_HOUR);
    ASSERT_EQ(duration.us, 60UL * 60UL * 1000UL * 1000UL);
    ASSERT_EQ(anj_time_duration_to_scalar(duration, ANJ_TIME_UNIT_HOUR), 1UL);

    duration = anj_time_duration_new(1, ANJ_TIME_UNIT_DAY);
    ASSERT_EQ(duration.us, 24UL * 60UL * 60UL * 1000UL * 1000UL);
    ASSERT_EQ(anj_time_duration_to_scalar(duration, ANJ_TIME_UNIT_DAY), 1UL);
}

ANJ_UNIT_TEST(time_api, string_conversion) {
    anj_time_duration_t duration = { 12312313123123 };
    const char *duration_str =
            ANJ_TIME_DURATION_AS_STRING(duration, ANJ_TIME_UNIT_US);

    ANJ_UNIT_ASSERT_EQUAL_STRING(duration_str, "12312313123123");

    duration = ANJ_TIME_DURATION_INVALID;
    duration_str = ANJ_TIME_DURATION_AS_STRING(duration, ANJ_TIME_UNIT_US);
    ANJ_UNIT_ASSERT_EQUAL_STRING(duration_str, "<invalid>");
}

ANJ_UNIT_TEST(time_api, add_invalid) {
    const anj_time_duration_t duration = { 100 };
    ASSERT_FALSE(anj_time_duration_is_valid(
            anj_time_duration_add(duration, ANJ_TIME_DURATION_INVALID)));
    ASSERT_FALSE(anj_time_duration_is_valid(
            anj_time_duration_add(ANJ_TIME_DURATION_INVALID, duration)));
    ASSERT_FALSE(anj_time_duration_is_valid(anj_time_duration_add(
            ANJ_TIME_DURATION_INVALID, ANJ_TIME_DURATION_INVALID)));

    const anj_time_monotonic_t monotonic = { 100 };
    ASSERT_FALSE(anj_time_monotonic_is_valid(
            anj_time_monotonic_add(monotonic, ANJ_TIME_DURATION_INVALID)));

    const anj_time_real_t real = { 100 };
    ASSERT_FALSE(anj_time_real_is_valid(
            anj_time_real_add(real, ANJ_TIME_DURATION_INVALID)));
}

ANJ_UNIT_TEST(time_api, sub_invalid) {
    const anj_time_duration_t duration = { 100 };
    ASSERT_FALSE(anj_time_duration_is_valid(
            anj_time_duration_sub(duration, ANJ_TIME_DURATION_INVALID)));
    ASSERT_FALSE(anj_time_duration_is_valid(
            anj_time_duration_sub(ANJ_TIME_DURATION_INVALID, duration)));
    ASSERT_FALSE(anj_time_duration_is_valid(anj_time_duration_sub(
            ANJ_TIME_DURATION_INVALID, ANJ_TIME_DURATION_INVALID)));

    const anj_time_monotonic_t monotonic = { 100 };
    ASSERT_FALSE(anj_time_monotonic_is_valid(
            anj_time_monotonic_sub(monotonic, ANJ_TIME_DURATION_INVALID)));

    const anj_time_real_t real = { 100 };
    ASSERT_FALSE(anj_time_real_is_valid(
            anj_time_real_sub(real, ANJ_TIME_DURATION_INVALID)));
}

ANJ_UNIT_TEST(time_api, mul_invalid) {
    const anj_time_duration_t duration = { 100 };
    ASSERT_TRUE(
            anj_time_duration_is_valid(anj_time_duration_mul(duration, 10)));
    ASSERT_FALSE(anj_time_duration_is_valid(
            anj_time_duration_mul(ANJ_TIME_DURATION_INVALID, 10)));
    ASSERT_TRUE(anj_time_duration_is_valid(anj_time_duration_mul(duration, 0)));
    ASSERT_FALSE(anj_time_duration_is_valid(
            anj_time_duration_mul(ANJ_TIME_DURATION_INVALID, 0)));
}

ANJ_UNIT_TEST(time_api, div_invalid) {
    const anj_time_duration_t duration = { 100 };
    ASSERT_TRUE(
            anj_time_duration_is_valid(anj_time_duration_div(duration, 10)));
    ASSERT_FALSE(anj_time_duration_is_valid(
            anj_time_duration_div(ANJ_TIME_DURATION_INVALID, 10)));
    ASSERT_FALSE(
            anj_time_duration_is_valid(anj_time_duration_div(duration, 0)));
    ASSERT_FALSE(anj_time_duration_is_valid(
            anj_time_duration_div(ANJ_TIME_DURATION_INVALID, 0)));
}

ANJ_UNIT_TEST(time_api, eq_invalid) {
    ASSERT_FALSE(anj_time_duration_eq(ANJ_TIME_DURATION_INVALID,
                                      ANJ_TIME_DURATION_INVALID));

    ASSERT_FALSE(anj_time_monotonic_eq(ANJ_TIME_MONOTONIC_INVALID,
                                       ANJ_TIME_MONOTONIC_INVALID));
    ASSERT_FALSE(
            anj_time_real_eq(ANJ_TIME_REAL_INVALID, ANJ_TIME_REAL_INVALID));
}

ANJ_UNIT_TEST(time_api, compare_invalid) {
    ASSERT_FALSE(anj_time_duration_lt(ANJ_TIME_DURATION_INVALID,
                                      ANJ_TIME_DURATION_INVALID));
    ASSERT_FALSE(anj_time_duration_gt(ANJ_TIME_DURATION_INVALID,
                                      ANJ_TIME_DURATION_INVALID));

    ASSERT_FALSE(anj_time_monotonic_lt(ANJ_TIME_MONOTONIC_INVALID,
                                       ANJ_TIME_MONOTONIC_INVALID));
    ASSERT_FALSE(anj_time_monotonic_gt(ANJ_TIME_MONOTONIC_INVALID,
                                       ANJ_TIME_MONOTONIC_INVALID));

    ASSERT_FALSE(
            anj_time_real_lt(ANJ_TIME_REAL_INVALID, ANJ_TIME_REAL_INVALID));
    ASSERT_FALSE(
            anj_time_real_gt(ANJ_TIME_REAL_INVALID, ANJ_TIME_REAL_INVALID));

    const anj_time_duration_t duration = ANJ_TIME_DURATION_ZERO;

    ASSERT_FALSE(anj_time_duration_lt(duration, ANJ_TIME_DURATION_INVALID));
    ASSERT_FALSE(anj_time_duration_gt(duration, ANJ_TIME_DURATION_INVALID));

    const anj_time_monotonic_t monotonic = ANJ_TIME_MONOTONIC_ZERO;
    ASSERT_FALSE(anj_time_monotonic_lt(monotonic, ANJ_TIME_MONOTONIC_INVALID));
    ASSERT_FALSE(anj_time_monotonic_gt(monotonic, ANJ_TIME_MONOTONIC_INVALID));

    const anj_time_real_t real = ANJ_TIME_REAL_ZERO;
    ASSERT_FALSE(anj_time_real_lt(real, ANJ_TIME_REAL_INVALID));
    ASSERT_FALSE(anj_time_real_gt(real, ANJ_TIME_REAL_INVALID));
}
