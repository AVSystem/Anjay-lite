/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/defs.h>
#include <anj/utils.h>

#include <anj_unit_test.h>

static void test_double_to_string(double value, const char *result) {
    char buff[100] = { 0 };
    anj_double_to_string_value(buff, value);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, result, strlen(result));
}

ANJ_UNIT_TEST(utils, double_to_str_custom) {
    test_double_to_string(0, "0");
    test_double_to_string((double) UINT16_MAX, "65535");
    test_double_to_string((double) UINT32_MAX - 0.02, "4294967294.98");
    test_double_to_string((double) UINT32_MAX, "4294967295");
    test_double_to_string((double) UINT32_MAX + 1.0, "4294967296");
    test_double_to_string(0.0005999999999999999, "0.0005999999999999999");
    test_double_to_string(0.00000122, "0.00000122");
    test_double_to_string(0.000000002, "0.000000002");
    test_double_to_string(777.000760, "777.00076");
    test_double_to_string(10.022, "10.022");
    test_double_to_string(100.022, "100.022");
    test_double_to_string(1000.033, "1000.033");
    test_double_to_string(99999.03, "99999.03");
    test_double_to_string(999999999.4440002, "999999999.4440002");
    test_double_to_string(1234e15, "1234000000000000000");
    test_double_to_string(1e16, "10000000000000000");
    test_double_to_string(1000000000000001, "1000000000000001");
    test_double_to_string(2111e18, "2.111e+21");
    test_double_to_string(0.0, "0");
    test_double_to_string(NAN, "nan");
    test_double_to_string(INFINITY, "inf");
    test_double_to_string(-INFINITY, "-inf");
    test_double_to_string(-(double) UINT32_MAX, "-4294967295");
    test_double_to_string(-10.022, "-10.022");
    test_double_to_string(-100.022, "-100.022");
    test_double_to_string(-1234e15, "-1234000000000000000");
    test_double_to_string(-2111e18, "-2.111e+21");
    test_double_to_string(-124e-15, "-1.24e-13");
    test_double_to_string(-4568e-22, "-4.568e-19");
    test_double_to_string(1.0, "1");
    test_double_to_string(78e120, "7.8e+121");
    test_double_to_string(1e20, "1e+20");
}

static void
test_string_to_double(const char *buff, double expected, bool failed) {
    double value;
    if (!failed) {
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_string_to_double_value(&value, buff, strlen(buff)));
        ANJ_UNIT_ASSERT_EQUAL(value, expected);
    } else {
        ANJ_UNIT_ASSERT_FAILED(
                anj_string_to_double_value(&value, buff, strlen(buff)));
    }
}

ANJ_UNIT_TEST(utils, str_to_double_custom) {
    test_string_to_double("0", 0.0, false);
    test_string_to_double("1", 1.0, false);
    test_string_to_double("-1", -1.0, false);
    test_string_to_double("0.0005999999999999999", 0.0005999999999999999,
                          false);
    test_string_to_double("0.00000122", 0.00000122, false);
    test_string_to_double("0.000000002", 0.000000002, false);
    test_string_to_double("-0.000000002", -0.000000002, false);
    test_string_to_double("777.000760", 777.00076, false);
    test_string_to_double("10.022", 10.022, false);
    test_string_to_double("100.022", 100.022, false);
    test_string_to_double("1000.033", 1000.033, false);
    test_string_to_double("99999.03", 99999.03, false);
    test_string_to_double("999999999.4440002", 999999999.4440002, false);
    test_string_to_double("1234000000000000000", 1234e15, false);
    test_string_to_double("-1234000000000000000", -1234e15, false);
    test_string_to_double("1234e10", 1234e10, false);
    test_string_to_double("1234E10", 1234e10, false);
    test_string_to_double("1234e+10", 1234e10, false);
    test_string_to_double("1234e-10", 1234e-10, false);
    test_string_to_double("-1234e10", -1234e10, false);
    test_string_to_double("-1234E10", -1234e10, false);
    test_string_to_double("-1234e+10", -1234e10, false);
    test_string_to_double("-1234e-10", -1234e-10, false);
    test_string_to_double("-2.2250738585072014E-307", -2.2250738585072014E-307,
                          false);
    test_string_to_double("-2.22507385850720145E-308", 0, true);
    test_string_to_double("e10", 0, true);
    test_string_to_double("e10", 0, true);
    test_string_to_double("e+10", 0, true);
    test_string_to_double("e-10", 0, true);
    test_string_to_double("-e10", 0, true);
    test_string_to_double("2e", 0, true);
    test_string_to_double("2e+", 0, true);
    test_string_to_double("2e-", 0, true);
    test_string_to_double("-2e", 0, true);
    test_string_to_double("2xe10", 0, true);
    test_string_to_double("2ex10", 0, true);
    test_string_to_double("1234ee10", 0, true);
    test_string_to_double("e", 0, true);
    test_string_to_double("20dd.4", 0, true);
}

static void
test_string_to_uint64(const char *buff, uint64_t expected, bool failed) {
    uint64_t value;
    if (!failed) {
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_string_to_uint64_value(&value, buff, strlen(buff)));
        ANJ_UNIT_ASSERT_EQUAL(value, expected);
    } else {
        ANJ_UNIT_ASSERT_FAILED(
                anj_string_to_uint64_value(&value, buff, strlen(buff)));
    }
}

ANJ_UNIT_TEST(utils, string_to_uint64) {
    test_string_to_uint64("", 0, true);
    test_string_to_uint64("0", 0, false);
    test_string_to_uint64("1", 1, false);
    test_string_to_uint64("2", 2, false);
    test_string_to_uint64("255", 255, false);
    test_string_to_uint64("256", 256, false);
    test_string_to_uint64("65535", 65535, false);
    test_string_to_uint64("65536", 65536, false);
    test_string_to_uint64("4294967295", 4294967295, false);
    test_string_to_uint64("4294967296", 4294967296, false);
    test_string_to_uint64("18446744073709551615", UINT64_MAX, false);
    test_string_to_uint64("18446744073709551616", 0, true);
    test_string_to_uint64("99999999999999999999", 0, true);
    test_string_to_uint64("184467440737095516160", 0, true);
    test_string_to_uint64("b", 0, true);
    test_string_to_uint64("-1", 0, true);
    test_string_to_uint64("255b", 0, true);
    test_string_to_uint64("123b5", 0, true);
}

static void
test_string_to_uint32(const char *buff, uint64_t expected, bool failed) {
    uint32_t value;
    if (!failed) {
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_string_to_uint32_value(&value, buff, strlen(buff)));
        ANJ_UNIT_ASSERT_EQUAL(value, expected);
    } else {
        ANJ_UNIT_ASSERT_FAILED(
                anj_string_to_uint32_value(&value, buff, strlen(buff)));
    }
}

ANJ_UNIT_TEST(utils, string_to_uint32) {
    test_string_to_uint32("", 0, true);
    test_string_to_uint32("0", 0, false);
    test_string_to_uint32("1", 1, false);
    test_string_to_uint32("2", 2, false);
    test_string_to_uint32("255", 255, false);
    test_string_to_uint32("256", 256, false);
    test_string_to_uint32("65535", 65535, false);
    test_string_to_uint32("65536", 65536, false);
    test_string_to_uint32("4294967295", 4294967295, false);
    test_string_to_uint32("4294967296", 4294967296, true);
    test_string_to_uint32("42949672951", 4294967296, true);
    test_string_to_uint32("b", 0, true);
    test_string_to_uint32("-1", 0, true);
    test_string_to_uint32("255b", 0, true);
    test_string_to_uint32("123b5", 0, true);
}

static void
test_string_to_int64(const char *buff, int64_t expected, bool failed) {
    int64_t value;
    if (!failed) {
        ANJ_UNIT_ASSERT_SUCCESS(
                anj_string_to_int64_value(&value, buff, strlen(buff)));
        ANJ_UNIT_ASSERT_EQUAL(value, expected);
    } else {
        ANJ_UNIT_ASSERT_FAILED(
                anj_string_to_int64_value(&value, buff, strlen(buff)));
    }
}

ANJ_UNIT_TEST(utils, string_to_int64) {
    test_string_to_int64("", 0, true);
    test_string_to_int64("0", 0, false);
    test_string_to_int64("1", 1, false);
    test_string_to_int64("+1", 1, false);
    test_string_to_int64("-1", -1, false);
    test_string_to_int64("2", 2, false);
    test_string_to_int64("+2", 2, false);
    test_string_to_int64("-2", -2, false);
    test_string_to_int64("255", 255, false);
    test_string_to_int64("+255", 255, false);
    test_string_to_int64("-255", -255, false);
    test_string_to_int64("256", 256, false);
    test_string_to_int64("+256", 256, false);
    test_string_to_int64("-256", -256, false);
    test_string_to_int64("65535", 65535, false);
    test_string_to_int64("+65535", 65535, false);
    test_string_to_int64("-65535", -65535, false);
    test_string_to_int64("65536", 65536, false);
    test_string_to_int64("+65536", 65536, false);
    test_string_to_int64("-65536", -65536, false);
    test_string_to_int64("4294967295", 4294967295, false);
    test_string_to_int64("+4294967295", 4294967295, false);
    test_string_to_int64("-4294967295", -4294967295, false);
    test_string_to_int64("4294967296", 4294967296, false);
    test_string_to_int64("+4294967296", 4294967296, false);
    test_string_to_int64("-4294967296", -4294967296, false);
    test_string_to_int64("9223372036854775807", INT64_MAX, false);
    test_string_to_int64("+9223372036854775807", INT64_MAX, false);
    test_string_to_int64("-9223372036854775808", INT64_MIN, false);
    test_string_to_int64("9223372036854775808", 0, true);
    test_string_to_int64("9999999999999999999", 0, true);
    test_string_to_int64("92233720368547758070", 0, true);
    test_string_to_int64("18446744073709551615", 0, true);
    test_string_to_int64("b", 0, true);
    test_string_to_int64("255b", 0, true);
    test_string_to_int64("123b5", 0, true);
    test_string_to_int64("-b", 0, true);
    test_string_to_int64("-255b", 0, true);
    test_string_to_int64("-123b5", 0, true);
}

static void test_uint16_to_string(uint16_t value, const char *expected) {
    char buff[ANJ_U16_STR_MAX_LEN] = { 0 };
    size_t ret = anj_uint16_to_string_value(buff, value);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, expected, ret);
    ANJ_UNIT_ASSERT_EQUAL(ret, strlen(expected));
}

ANJ_UNIT_TEST(utils, uint16_to_string) {
    test_uint16_to_string(0, "0");
    test_uint16_to_string(1, "1");
    test_uint16_to_string(2, "2");
    test_uint16_to_string(255, "255");
    test_uint16_to_string(256, "256");
    test_uint16_to_string(UINT16_MAX, "65535");
}

static void test_uint32_to_string(uint32_t value, const char *expected) {
    char buff[ANJ_U32_STR_MAX_LEN] = { 0 };
    size_t ret = anj_uint32_to_string_value(buff, value);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, expected, ret);
    ANJ_UNIT_ASSERT_EQUAL(ret, strlen(expected));
}

ANJ_UNIT_TEST(utils, uint32_to_string) {
    test_uint32_to_string(0, "0");
    test_uint32_to_string(1, "1");
    test_uint32_to_string(2, "2");
    test_uint32_to_string(255, "255");
    test_uint32_to_string(256, "256");
    test_uint32_to_string(65535, "65535");
    test_uint32_to_string(65536, "65536");
    test_uint32_to_string(UINT32_MAX, "4294967295");
}

static void test_uint64_to_string(uint64_t value, const char *expected) {
    char buff[ANJ_U64_STR_MAX_LEN] = { 0 };
    size_t ret = anj_uint64_to_string_value(buff, value);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, expected, ret);
    ANJ_UNIT_ASSERT_EQUAL(ret, strlen(expected));
}

ANJ_UNIT_TEST(utils, uint64_to_string) {
    test_uint64_to_string(0, "0");
    test_uint64_to_string(1, "1");
    test_uint64_to_string(2, "2");
    test_uint64_to_string(255, "255");
    test_uint64_to_string(256, "256");
    test_uint64_to_string(65535, "65535");
    test_uint64_to_string(65536, "65536");
    test_uint64_to_string(4294967295, "4294967295");
    test_uint64_to_string(4294967296, "4294967296");
    test_uint64_to_string(UINT64_MAX, "18446744073709551615");
}

static void test_int64_to_string(int64_t value, const char *expected) {
    char buff[ANJ_I64_STR_MAX_LEN] = { 0 };
    size_t ret = anj_int64_to_string_value(buff, value);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buff, expected, ret);
    ANJ_UNIT_ASSERT_EQUAL(ret, strlen(expected));
}

ANJ_UNIT_TEST(utils, int64_to_string) {
    test_int64_to_string(0, "0");
    test_int64_to_string(1, "1");
    test_int64_to_string(-1, "-1");
    test_int64_to_string(2, "2");
    test_int64_to_string(-2, "-2");
    test_int64_to_string(4294967295, "4294967295");
    test_int64_to_string(-4294967296, "-4294967296");
    test_int64_to_string(INT64_MAX, "9223372036854775807");
    test_int64_to_string(INT64_MIN, "-9223372036854775808");
}
