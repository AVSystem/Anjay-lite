/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_UNIT_TEST_H
#define ANJ_UNIT_TEST_H

#include <stddef.h>

/**
 * Internal functions used by the library to implement the functionality.
 */
/**@{*/
int anj_unit_abort__(const char *msg, const char *file, int line);

// clang-format off
#define ANJ_UNIT_CHECK_EQUAL__(actual, expected, strings)\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), bool), \
    anj_unit_check_equal_i__((int) (actual), (int) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), char),\
    anj_unit_check_equal_c__((char) (actual), (char) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), signed char),\
    anj_unit_check_equal_sc__((signed char) (actual), (signed char) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), short),\
    anj_unit_check_equal_s__((short) (actual), (short) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), int),\
    anj_unit_check_equal_i__((int) (actual), (int) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), long),\
    anj_unit_check_equal_l__((long) (actual), (long) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), long long),\
    anj_unit_check_equal_ll__((long long) (actual), (long long) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), unsigned char),\
    anj_unit_check_equal_uc__((unsigned char) (actual), (unsigned char) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), unsigned short),\
    anj_unit_check_equal_us__((unsigned short) (actual), (unsigned short) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), unsigned int),\
    anj_unit_check_equal_ui__((unsigned int) (actual), (unsigned int) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), unsigned long),\
    anj_unit_check_equal_ul__((unsigned long) (actual), (unsigned long) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), unsigned long long),\
    anj_unit_check_equal_ull__((unsigned long long) (actual), (unsigned long long) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), float),\
    anj_unit_check_equal_f__((float) (actual), (float) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), double),\
    anj_unit_check_equal_d__((double) (actual), (double) (expected), (strings)),\
__builtin_choose_expr(__builtin_types_compatible_p(__typeof__(actual), long double),\
    anj_unit_check_equal_ld__((long double) (actual), (long double) (expected), (strings)),\
    anj_unit_abort__("ANJ_UNIT_ASSERT_EQUAL called for unsupported data type\n", __FILE__, __LINE__)\
)))))))))))))))
// clang-format on

typedef struct {
    char actual_str[64];
    char expected_str[64];
} anj_unit_check_equal_function_strings_t;

#define ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(type, name_suffix) \
    int anj_unit_check_equal_##name_suffix##__(                    \
            type actual,                                           \
            type expected,                                         \
            anj_unit_check_equal_function_strings_t *strings)

ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(char, c);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(signed char, sc);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(short, s);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(int, i);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(long, l);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(long long, ll);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned char, uc);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned short, us);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned int, ui);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned long, ul);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned long long, ull);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(float, f);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(double, d);
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(long double, ld);

void anj_unit_assert_success__(int result, const char *file, int line);

void anj_unit_assert_failed__(int result, const char *file, int line);

void anj_unit_assert_true__(int result, const char *file, int line);

void anj_unit_assert_false__(int result, const char *file, int line);

void anj_unit_assert_equal_func__(int check_result,
                                  const char *actual_str,
                                  const char *expected_str,
                                  const char *file,
                                  int line);

void anj_unit_assert_not_equal_func__(int check_result,
                                      const char *actual_str,
                                      const char *not_expected_str,
                                      const char *file,
                                      int line);

void anj_unit_assert_equal_string__(const char *actual,
                                    const char *expected,
                                    const char *file,
                                    int line);

void anj_unit_assert_not_equal_string__(const char *actual,
                                        const char *not_expected,
                                        const char *file,
                                        int line);

void anj_unit_assert_bytes_equal__(const void *actual,
                                   const void *expected,
                                   size_t num_bytes,
                                   const char *file,
                                   int line);

void anj_unit_assert_bytes_not_equal__(const void *actual,
                                       const void *expected,
                                       size_t num_bytes,
                                       const char *file,
                                       int line);

void anj_unit_assert_null__(const void *pointer, const char *file, int line);

void anj_unit_assert_not_null__(const void *pointer,
                                const char *file,
                                int line);

typedef void (*unit_test_ptr_t)(void);

void anj_unit_test_add(const char *suite_name,
                       const char *test_name,
                       unit_test_ptr_t unit_test);

/**@}*/

/**
 * Defines a unit test case.
 *
 * <example>
 * @code
 * ANJ_UNIT_TEST(module1, fancy_func) {
 *     ANJ_UNIT_ASSERT_SUCCESS(fancy_func(123));
 *     ANJ_UNIT_ASSERT_FAILED(fancy_func(-1));
 * }
 * @endcode
 * </example>
 *
 * @param suite Name of the test suite.
 *
 * @param name  Name of the test case.
 */
#define ANJ_UNIT_TEST(suite, name)                                         \
    static void _anj_unit_test_##suite##_##name(void);                     \
    void _anj_unit_test_constructor_##suite##_##name(void)                 \
            __attribute__((constructor));                                  \
    void _anj_unit_test_constructor_##suite##_##name(void) {               \
        anj_unit_test_add(#suite, #name, _anj_unit_test_##suite##_##name); \
    }                                                                      \
    static void _anj_unit_test_##suite##_##name(void)

/**
 * Assertions.
 */

/**
 * Asserts that the specified value is 0.
 *
 * It is intended to check for successful function return values.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param result Value to check.
 */
#define ANJ_UNIT_ASSERT_SUCCESS(result) \
    anj_unit_assert_success__((result), __FILE__, __LINE__)

/**
 * Asserts that the specified value is not 0.
 *
 * It is intended to check for unsuccessful function return values.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param result Value to check.
 */
#define ANJ_UNIT_ASSERT_FAILED(result) \
    anj_unit_assert_failed__((result), __FILE__, __LINE__)

/**
 * Asserts that the specified value is not 0.
 *
 * It is intended to check for conceptually boolean values.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param result Value to check.
 */
#define ANJ_UNIT_ASSERT_TRUE(result) \
    anj_unit_assert_true__((int) (result), __FILE__, __LINE__)

/**
 * Asserts that the specified value is not 0.
 *
 * It is intended to check for conceptually boolean values.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param result Value to check.
 */
#define ANJ_UNIT_ASSERT_FALSE(result) \
    anj_unit_assert_false__((int) (result), __FILE__, __LINE__)

/**
 * Asserts that the two specified values are equal.
 *
 * All integer and floating-point types are supported.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param actual   The value returned from code under test.
 *
 * @param expected The expected value to compare with.
 */
#define ANJ_UNIT_ASSERT_EQUAL(actual, expected)                     \
    do {                                                            \
        anj_unit_check_equal_function_strings_t strings;            \
        anj_unit_assert_equal_func__(                               \
                ANJ_UNIT_CHECK_EQUAL__(actual, expected, &strings), \
                strings.actual_str,                                 \
                strings.expected_str,                               \
                __FILE__,                                           \
                __LINE__);                                          \
    } while (0)

/**
 * Asserts that the two specified values are not equal.
 *
 * All integer and floating-point types are supported.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param actual       The value returned from code under test.
 *
 * @param not_expected The value to compare with, that is expected not to be
 *                     returned.
 */
#define ANJ_UNIT_ASSERT_NOT_EQUAL(actual, not_expected)                 \
    do {                                                                \
        anj_unit_check_equal_function_strings_t strings;                \
        anj_unit_assert_not_equal_func__(                               \
                ANJ_UNIT_CHECK_EQUAL__(actual, not_expected, &strings), \
                strings.actual_str,                                     \
                strings.expected_str,                                   \
                __FILE__,                                               \
                __LINE__);                                              \
    } while (0)

/**
 * Asserts that corresponding fields in two specified structures are equal.
 *
 * The same types as for @ref ANJ_UNIT_ASSERT_EQUAL are supported.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST
 *
 * @param actual_struct_ptr   Pointer to structure containing actual value.
 *
 * @param expected_struct_ptr Pointer to structure containing expected value.
 *
 * @param field               Name of the structure field to compare.
 */
#define ANJ_UNIT_ASSERT_FIELD_EQUAL(                                          \
        actual_struct_ptr, expected_struct_ptr, field)                        \
    __builtin_choose_expr(                                                    \
            __builtin_types_compatible_p(__typeof__(*(actual_struct_ptr)),    \
                                         __typeof__(*(expected_struct_ptr))), \
            ({                                                                \
                ANJ_UNIT_ASSERT_EQUAL((actual_struct_ptr)->field,             \
                                      (expected_struct_ptr)->field);          \
            }),                                                               \
            anj_unit_abort__("ANJ_UNIT_ASSERT_FIELD_EQUAL called for "        \
                             "different types\n",                             \
                             __FILE__,                                        \
                             __LINE__))

/**
 * Asserts that corresponding fields in two specified structures are not equal.
 *
 * The same types as for @ref ANJ_UNIT_ASSERT_NOT_EQUAL are supported.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST
 *
 * @param actual_struct_ptr   Pointer to structure containing actual value.
 *
 * @param expected_struct_ptr Pointer to structure containing expected value.
 *
 * @param field               Name of the structure field to compare.
 */
#define ANJ_UNIT_ASSERT_FIELD_NOT_EQUAL(                                      \
        actual_struct_ptr, expected_struct_ptr, field)                        \
    __builtin_choose_expr(                                                    \
            __builtin_types_compatible_p(__typeof__(*(actual_struct_ptr)),    \
                                         __typeof__(*(expected_struct_ptr))), \
            ({                                                                \
                ANJ_UNIT_ASSERT_NOT_EQUAL((actual_struct_ptr)->field,         \
                                          (expected_struct_ptr)->field);      \
            }),                                                               \
            anj_unit_abort__("ANJ_UNIT_ASSERT_FIELD_NOT_EQUAL called for "    \
                             "different types\n",                             \
                             __FILE__,                                        \
                             __LINE__))

/**
 * Asserts that two specified string values are equal.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param actual   The value returned from code under test.
 *
 * @param expected The expected value to compare with.
 */
#define ANJ_UNIT_ASSERT_EQUAL_STRING(actual, expected) \
    anj_unit_assert_equal_string__(actual, expected, __FILE__, __LINE__)

/**
 * Asserts that two specified string values are not equal.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param actual       The value returned from code under test.
 *
 * @param not_expected The expected value to compare with.
 */
#define ANJ_UNIT_ASSERT_NOT_EQUAL_STRING(actual, not_expected) \
    anj_unit_assert_not_equal_string__(actual, not_expected, __FILE__, __LINE__)

/**
 * Asserts that two buffers contain same data.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param actual    The value returned from code under test.
 *
 * @param expected  A null-terminated string literal which contents will be
 *                  compared to the @p actual. Its length determines number of
 *                  bytes to compare. Note: the trailing NULL character is NOT
 *                  considered a part of the string, i.e. only
 *                  (sizeof(expected) - 1) bytes are compared.
 */
#define ANJ_UNIT_ASSERT_EQUAL_BYTES(actual, expected)                        \
    __builtin_choose_expr(                                                   \
            __builtin_types_compatible_p(__typeof__(expected), const char[]) \
                    || __builtin_types_compatible_p(__typeof__(expected),    \
                                                    char[]),                 \
            anj_unit_assert_bytes_equal__((actual),                          \
                                          (expected),                        \
                                          sizeof(expected) - 1,              \
                                          __FILE__,                          \
                                          __LINE__),                         \
            anj_unit_abort__("ANJ_UNIT_ASSERT_EQUAL_BYTES called for "       \
                             "unsupported data type\n",                      \
                             __FILE__,                                       \
                             __LINE__))

/**
 * Asserts that two buffers contain different data.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param actual    The value returned from code under test.
 *
 * @param expected  A null-terminated string literal which contents will be
 *                  compared to the @p actual. Its length determines number of
 *                  bytes to compare. Note: the trailing NULL character is NOT
 *                  considered a part of the string, i.e. only
 *                  (sizeof(expected) - 1) bytes are compared.
 */
#define ANJ_UNIT_ASSERT_NOT_EQUAL_BYTES(actual, expected)                    \
    __builtin_choose_expr(                                                   \
            __builtin_types_compatible_p(__typeof__(expected), const char[]) \
                    || __builtin_types_compatible_p(__typeof__(expected),    \
                                                    char[]),                 \
            anj_unit_assert_bytes_not_equal__((actual),                      \
                                              (expected),                    \
                                              sizeof(expected) - 1,          \
                                              __FILE__,                      \
                                              __LINE__),                     \
            anj_unit_abort__("ANJ_UNIT_ASSERT_NOT_EQUAL_BYTES called for "   \
                             "unsupported data type\n",                      \
                             __FILE__,                                       \
                             __LINE__))

/**
 * Asserts that two buffers contain same data.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param actual    The value returned from code under test.
 *
 * @param expected  The expected value to compare with.
 *
 * @param num_bytes Number of bytes in each buffer.
 */
#define ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(actual, expected, num_bytes) \
    anj_unit_assert_bytes_equal__(                                     \
            actual, expected, num_bytes, __FILE__, __LINE__)

/**
 * Asserts that two buffers contain different data.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param actual    The value returned from code under test.
 *
 * @param expected  The expected value to compare with.
 *
 * @param num_bytes Number of bytes in each buffer.
 */
#define ANJ_UNIT_ASSERT_NOT_EQUAL_BYTES_SIZED(actual, expected, num_bytes) \
    anj_unit_assert_bytes_not_equal__(                                     \
            actual, expected, num_bytes, __FILE__, __LINE__)

/**
 * Asserts that the specified pointer is <c>NULL</c>.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param pointer Value to check.
 */
#define ANJ_UNIT_ASSERT_NULL(pointer) \
    anj_unit_assert_null__(pointer, __FILE__, __LINE__)

/**
 * Asserts that the specified pointer is not <c>NULL</c>.
 *
 * This macro shall be called from unit test cases defined in
 * @ref ANJ_UNIT_TEST.
 *
 * @param pointer Value to check.
 */
#define ANJ_UNIT_ASSERT_NOT_NULL(pointer) \
    anj_unit_assert_not_null__(pointer, __FILE__, __LINE__)

/**
 * Convenience aliases for assert macros.
 */
#define ASSERT_OK ANJ_UNIT_ASSERT_SUCCESS
#define ASSERT_FAIL ANJ_UNIT_ASSERT_FAILED

#define ASSERT_TRUE ANJ_UNIT_ASSERT_TRUE
#define ASSERT_FALSE ANJ_UNIT_ASSERT_FALSE

#define ASSERT_EQ ANJ_UNIT_ASSERT_EQUAL
#define ASSERT_NE ANJ_UNIT_ASSERT_NOT_EQUAL

#define ASSERT_FIELD_EQ ANJ_UNIT_ASSERT_FIELD_EQUAL
#define ASSERT_FIELD_NE ANJ_UNIT_ASSERT_FIELD_NOT_EQUAL

#define ASSERT_EQ_STR ANJ_UNIT_ASSERT_EQUAL_STRING
#define ASSERT_NE_STR ANJ_UNIT_ASSERT_NOT_EQUAL_STRING

#define ASSERT_EQ_BYTES ANJ_UNIT_ASSERT_EQUAL_BYTES
#define ASSERT_NE_BYTES ANJ_UNIT_ASSERT_NOT_EQUAL_BYTES

#define ASSERT_EQ_BYTES_SIZED ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED
#define ASSERT_NE_BYTES_SIZED ANJ_UNIT_ASSERT_NOT_EQUAL_BYTES_SIZED

#define ASSERT_NULL ANJ_UNIT_ASSERT_NULL
#define ASSERT_NOT_NULL ANJ_UNIT_ASSERT_NOT_NULL

#endif // ANJ_UNIT_TEST_H
