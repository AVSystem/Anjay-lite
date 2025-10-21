/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "anj_unit_test.h"

/**
 * A simple implementation of a linked list.
 */
typedef void (*remove_callback_t)(void *element);
typedef struct node {
    void *element;
    struct node *next;
} node_t;

typedef struct {
    node_t *head;
    node_t *tail;
    remove_callback_t remove_callback;
} list_t;

static void set_remove_callback(list_t *list,
                                remove_callback_t remove_callback) {
    assert(list);
    assert(remove_callback);

    list->remove_callback = remove_callback;
}

static node_t *add_element(list_t *list, void *element) {
    assert(list);
    assert(element);

    node_t *new_node = (node_t *) calloc(1, sizeof(node_t));

    if (!new_node) {
        anj_unit_abort__("Memory allocation failed\n", __FILE__, __LINE__);
    }

    new_node->element = element;
    if (list->head == NULL) {
        assert(!list->tail);
        list->head = new_node;
        list->tail = new_node;
    } else {
        list->tail->next = new_node;
        list->tail = new_node;
    }

    return new_node;
}

static size_t list_size(const list_t *list) {
    size_t size = 0;
    node_t *node = list->head;
    while (node) {
        size++;
        node = node->next;
    }
    return size;
}

static void free_list(list_t *list) {
    assert(list);

    node_t *node;
    while (list->head) {
        node = list->head;
        if (list->remove_callback) {
            list->remove_callback(node->element);
        }
        list->head = list->head->next;
        free(node);
    }
    list->tail = NULL;
}

typedef struct {
    const char *suite_name;
    list_t unit_tests;
} suite_element_t;

typedef struct {
    const char *test_name;
    unit_test_ptr_t unit_test;
} unit_test_element_t;

static list_t _suites_list;
static jmp_buf _anj_unit_jmp_buf;

typedef enum {
    COLORS_BLACK = 30,
    COLORS_RED,
    COLORS_GREEN,
    COLORS_YELLOW,
    COLORS_BLUE,
    COLORS_PURPLE,
    COLORS_CYAN,
    COLORS_WHITE
} colors_t;

static void ansi_bold(bool bold) {
    printf("\033[%dm", (int) bold);
}

static void ansi_format(colors_t color, bool bold) {
    ansi_bold(bold);
    printf("\033[%dm", (int) color);
}

static void ansi_reset(void) {
    printf("\033[0m");
}

static void
test_fail_vprintf(const char *file, int line, const char *fmt, va_list list) {
    ansi_format(COLORS_RED, true);
    printf("[%s:%d] ", file, line);
    vprintf(fmt, list);
    ansi_reset();
}

void _anj_unit_test_fail_printf(const char *file,
                                int line,
                                const char *fmt,
                                ...) {
    va_list ap;
    va_start(ap, fmt);
    test_fail_vprintf(file, line, fmt, ap);
    va_end(ap);
}

int anj_unit_abort__(const char *msg, const char *file, int line) {
    _anj_unit_test_fail_printf(file, line, msg);
    abort();
    return -1;
}

static void print_char(uint8_t value, bool bold) {
    ansi_bold(bold);
    printf(" %02x", value);
}

static void test_fail_print_hex_diff(const uint8_t *actual,
                                     const uint8_t *expected,
                                     size_t buffer_size,
                                     size_t diff_start_offset,
                                     size_t diff_bytes,
                                     size_t context_size) {
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

    size_t start = (size_t) MAX(
            (ptrdiff_t) diff_start_offset - (ptrdiff_t) context_size, 0);
    size_t end =
            MIN(diff_start_offset + diff_bytes + context_size, buffer_size);

#undef MIN
#undef MAX

    size_t i;
    size_t marker_offset = sizeof("expected:") + 1
                           + (diff_start_offset - start) * (sizeof(" 00") - 1);

    printf("  actual:");
    for (i = start; i < end; ++i) {
        print_char(actual[i], actual[i] != expected[i]);
    }
    ansi_reset();
    printf("\nexpected:");
    for (i = start; i < end; ++i) {
        print_char(expected[i], actual[i] != expected[i]);
    }
    ansi_reset();
    printf("\n%*s\n", (int) marker_offset, "^");
}

static suite_element_t *find_test_suite(const char *suite_name) {
    node_t *suite_n = _suites_list.head;
    suite_element_t *suite_ele = NULL;

    while (suite_n
           && (suite_ele = suite_n->element,
               strcmp(suite_name, suite_ele->suite_name))) {
        suite_n = suite_n->next;
        suite_ele = NULL;
    }
    return suite_ele;
}

static suite_element_t *add_test_suite(const char *suite_name) {
    suite_element_t *new_suite_element =
            (suite_element_t *) calloc(1, sizeof(suite_element_t));

    if (!new_suite_element) {
        anj_unit_abort__("Memory allocation failed\n", __FILE__, __LINE__);
    }
    new_suite_element->suite_name = suite_name;
    return add_element(&_suites_list, new_suite_element)->element;
}

void anj_unit_test_add(const char *suite_name,
                       const char *test_name,
                       unit_test_ptr_t unit_test) {
    assert(suite_name);
    assert(test_name);
    assert(unit_test);

    suite_element_t *suite_ele = find_test_suite(suite_name);

    if (suite_ele == NULL) {
        suite_ele = add_test_suite(suite_name);
    }

    unit_test_element_t *new_element =
            (unit_test_element_t *) calloc(1, sizeof(unit_test_element_t));
    if (!new_element) {
        anj_unit_abort__("Memory allocation failed\n", __FILE__, __LINE__);
    }
    new_element->test_name = test_name;
    new_element->unit_test = unit_test;

    add_element(&suite_ele->unit_tests, new_element);
}

static void
_anj_unit_assert_fail(const char *file, int line, const char *format, ...) {
    va_list list;

    va_start(list, format);
    test_fail_vprintf(file, line, format, list);
    va_end(list);

    longjmp(_anj_unit_jmp_buf, 1);
}

#define _anj_unit_assert(Condition, File, Line, ...)            \
    do {                                                        \
        if (!(Condition)) {                                     \
            _anj_unit_assert_fail((File), (Line), __VA_ARGS__); \
        }                                                       \
    } while (0)

void anj_unit_assert_success__(int result, const char *file, int line) {
    _anj_unit_assert(result == 0, file, line, "expected success\n");
}

void anj_unit_assert_failed__(int result, const char *file, int line) {
    _anj_unit_assert(result != 0, file, line, "expected failure\n");
}

void anj_unit_assert_true__(int result, const char *file, int line) {
    _anj_unit_assert(result != 0, file, line, "expected true\n");
}

void anj_unit_assert_false__(int result, const char *file, int line) {
    _anj_unit_assert(result == 0, file, line, "expected false\n");
}

#define CHECK_EQUAL_BODY(format, ...)                                          \
    {                                                                          \
        snprintf(strings->actual_str, sizeof(strings->actual_str), format,     \
                 actual);                                                      \
        snprintf(strings->expected_str, sizeof(strings->expected_str), format, \
                 expected);                                                    \
        return (__VA_ARGS__);                                                  \
    }

#define CHECK_EQUAL_BODY_INT(format) \
    CHECK_EQUAL_BODY(format, actual == expected)

#define CHECK_EQUAL_BODY_FLOAT(format) \
    CHECK_EQUAL_BODY(format,           \
                     isnan(actual) ? isnan(expected) : actual == expected)

// clang-format off
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(char, c) CHECK_EQUAL_BODY_INT("%c")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(signed char, sc) CHECK_EQUAL_BODY_INT("%d")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(short, s) CHECK_EQUAL_BODY_INT("%d")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(int, i) CHECK_EQUAL_BODY_INT("%d")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(long, l) CHECK_EQUAL_BODY_INT("%ld")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(long long, ll) CHECK_EQUAL_BODY_INT("%lld")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned char, uc) CHECK_EQUAL_BODY_INT("0x%02x")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned short, us) CHECK_EQUAL_BODY_INT("%u")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned int, ui) CHECK_EQUAL_BODY_INT("%u")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned long, ul) CHECK_EQUAL_BODY_INT("%lu")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(unsigned long long, ull) CHECK_EQUAL_BODY_INT("%llu")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(float, f) CHECK_EQUAL_BODY_FLOAT("%.12g")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(double, d) CHECK_EQUAL_BODY_FLOAT("%.12g")
ANJ_UNIT_CHECK_EQUAL_FUNCTION_DECLARE__(long double, ld) CHECK_EQUAL_BODY_FLOAT("%.12Lg")

void anj_unit_assert_equal_func__(int check_result,
                                  const char *actual_str,
                                  const char *expected_str,
                                  const char *file,
                                  int line) {
    _anj_unit_assert(check_result, file, line, "expected <%s> was <%s>\n",
                     expected_str, actual_str);
}
// clang-format on

void anj_unit_assert_not_equal_func__(int check_result,
                                      const char *actual_str,
                                      const char *not_expected_str,
                                      const char *file,
                                      int line) {
    (void) actual_str;
    _anj_unit_assert(!check_result, file, line,
                     "expected value other than <%s>\n", not_expected_str);
}

void anj_unit_assert_equal_string__(const char *actual,
                                    const char *expected,
                                    const char *file,
                                    int line) {
    if (actual != NULL || expected != NULL) {
        _anj_unit_assert(!!expected, file, line, "expected NULL was <%s>\n",
                         actual);
        _anj_unit_assert(!!actual, file, line, "expected <%s> was NULL\n",
                         expected);
        _anj_unit_assert(!strcmp(actual, expected), file, line,
                         "expected <%s> was <%s>\n", expected, actual);
    }
}

void anj_unit_assert_not_equal_string__(const char *actual,
                                        const char *not_expected,
                                        const char *file,
                                        int line) {
    _anj_unit_assert(not_expected || actual, file, line,
                     "not_expected and actual are both NULL\n");
    if (not_expected && actual) {
        _anj_unit_assert(strcmp(actual, not_expected), file, line,
                         "expected value other than <%s>\n", not_expected);
    }
}

static size_t find_first__(bool equal,
                           const uint8_t *a,
                           const uint8_t *b,
                           size_t start,
                           size_t size) {
    size_t at;
    for (at = start; at < size; ++at) {
        if ((a[at] == b[at]) == equal) {
            return at;
        }
    }

    return size;
}

#define find_first_equal(a, b, start, size) \
    find_first__(true, (a), (b), (start), (size))
#define find_first_different(a, b, start, size) \
    find_first__(false, (a), (b), (start), (size))

static void
print_differences(const void *actual, const void *expected, size_t num_bytes) {
    static const size_t CONTEXT_SIZE = 5;
    static const size_t MAX_ERRORS = 3;
    size_t found_errors = 0;
    size_t at = 0;
    const uint8_t *actual_ptr = (const uint8_t *) actual;
    const uint8_t *expected_ptr = (const uint8_t *) expected;

    for (found_errors = 0; found_errors < MAX_ERRORS; ++found_errors) {
        size_t error_start =
                find_first_different(actual_ptr, expected_ptr, at, num_bytes);
        size_t error_end;
        size_t error_bytes = 0;

        if (error_start >= num_bytes) {
            return;
        }

        error_end = find_first_equal(actual_ptr, expected_ptr, error_start,
                                     num_bytes);
        error_bytes = error_end - error_start;

        while (error_end < num_bytes) {
            at = find_first_different(actual_ptr, expected_ptr, error_end,
                                      num_bytes);
            if (at - error_end <= CONTEXT_SIZE * 2) {
                error_end = find_first_equal(actual_ptr, expected_ptr, at,
                                             num_bytes);
                error_bytes += error_end - at;
                at = error_end;
            } else {
                break;
            }
        }

        printf("- %lu different byte(s) at offset %lu:\n",
               (unsigned long) error_bytes, (unsigned long) error_start);
        test_fail_print_hex_diff(actual_ptr, expected_ptr, num_bytes,
                                 error_start, error_end - error_start,
                                 CONTEXT_SIZE);
    }

    printf("- (more errors skipped)\n");
}

static void compare_bytes(const void *actual,
                          const void *expected,
                          size_t num_bytes,
                          bool expect_same,
                          const char *file,
                          int line) {
    bool values_equal = !memcmp(actual, expected, num_bytes);
    if (values_equal == expect_same) {
        return;
    }

    _anj_unit_test_fail_printf(file, line, "byte sequences are %sequal:\n",
                               expect_same ? "not " : "");

    if (expect_same) {
        print_differences(actual, expected, num_bytes);
    }

    longjmp(_anj_unit_jmp_buf, 1);
}

void anj_unit_assert_bytes_equal__(const void *actual,
                                   const void *expected,
                                   size_t num_bytes,
                                   const char *file,
                                   int line) {
    compare_bytes(actual, expected, num_bytes, true, file, line);
}

void anj_unit_assert_bytes_not_equal__(const void *actual,
                                       const void *expected,
                                       size_t num_bytes,
                                       const char *file,
                                       int line) {
    compare_bytes(actual, expected, num_bytes, false, file, line);
}

void anj_unit_assert_null__(const void *pointer, const char *file, int line) {
    _anj_unit_assert(!pointer, file, line, "expected NULL\n");
}

void anj_unit_assert_not_null__(const void *pointer,
                                const char *file,
                                int line) {
    _anj_unit_assert(!!pointer, file, line, "expected not NULL\n");
}

static void list_tests_for_suite(const suite_element_t *suite) {
    const node_t *test_n = suite->unit_tests.head;
    printf("%s (%lu tests)\n", suite->suite_name,
           list_size(&suite->unit_tests));

    while (test_n) {
        printf("  - %s\n",
               ((unit_test_element_t *) (test_n->element))->test_name);

        test_n = test_n->next;
    }
}

static int parse_command_line_args(int argc,
                                   char *argv[],
                                   const char *volatile *out_selected_suite,
                                   const char *volatile *out_selected_test) {
    while (1) {
        static const struct option long_options[] = {
            { "help", no_argument, 0, 'h' },
            { "list", optional_argument, 0, 'l' },
            { 0, 0, 0, 0 }
        };

        int option_index = 0;
        int c;

        c = getopt_long(argc, argv, "hl::v", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'h':
            printf("NAME\n"
                   "    %s - execute compiled-in test cases\n"
                   "\n"
                   "SYNOPSIS\n"
                   "    %s [OPTION]... [TEST_SUITE_NAME] "
                   "[TEST_CASE_NAME]\n"
                   "\n"
                   "OPTIONS\n"
                   "    -h, --help - display this message and exit.\n"
                   "    -l, --list [TEST_SUITE_NAME] - list all available "
                   "test cases and exit. If TEST_SUITE_NAME is specified, "
                   "list only test cases that belong to given test "
                   "suite.\n",
                   argv[0], argv[0]);
            printf("EXAMPLES\n"
                   "    %s            # run all tests\n"
                   "    %s -l         # list all tests, do not run any\n"
                   "    %s -l suite   # list all tests from suite "
                   "'suite', do not run any\n"
                   "    %s suite      # run all tests from suite 'suite'\n"
                   "    %s suite case # run only test 'case' from suite "
                   "'suite'\n",
                   argv[0], argv[0], argv[0], argv[0], argv[0]);
            return -1;
        case 'l': {
            suite_element_t *suite_ele;
            /* getopt does not recognize optional arguments in the form of
             * "-l foo"; check for such possibility */
            const char *arg = optarg ? optarg
                                     : (argv[optind] && argv[optind][0] != '-'
                                                ? argv[optind]
                                                : NULL);
            if (arg) {
                suite_ele = find_test_suite(arg);
                if (suite_ele) {
                    list_tests_for_suite(suite_ele);
                } else {
                    printf("test suite '%s' does not exist\n", arg);
                }
            } else {
                node_t *suite_n = _suites_list.head;
                while (suite_n) {
                    list_tests_for_suite((suite_element_t *) suite_n->element);
                    suite_n = suite_n->next;
                }
            }
            return -1;
        }
        default:
            break;
        }
    }

    for (; optind < argc; ++optind) {
        if (!*out_selected_suite) {
            *out_selected_suite = argv[optind];
        } else if (!*out_selected_test) {
            *out_selected_test = argv[optind];
        }
    }

    return 0;
}

static void print_suite_name(const char *suite_name) {
    ansi_bold(true);
    printf("[");
    ansi_format(COLORS_YELLOW, true);
    printf("%s", suite_name);
    ansi_reset();
    ansi_bold(true);
    printf("]");
    ansi_reset();
}

static int execute_unit_tests(list_t *tests_list,
                              const char *suite_name,
                              const char *selected_test,
                              size_t *out_total,
                              size_t *out_passed) {
    assert(tests_list);
    node_t *test_n = tests_list->head;

    int result = 0;
    size_t test_count = list_size(tests_list);
    size_t passed_test_count = 0;

    while (test_n) {
        unit_test_element_t *element = (unit_test_element_t *) test_n->element;

        if (selected_test && strcmp(selected_test, element->test_name)) {
            test_n = test_n->next;
            continue;
        }

        print_suite_name(suite_name);
        ansi_bold(true);
        printf(" %s - running...\n", element->test_name);
        ansi_reset();
        /**
         * stdio is line-buffered by default, and logger's default output
         * handler uses stderr - flush stdout to make sure that ANSI sequence is
         * printed before logs.
         */
        fflush(stdout);

        if (setjmp(_anj_unit_jmp_buf) == 0) {
            element->unit_test();
            passed_test_count++;
            ansi_format(COLORS_GREEN, false);
            printf("Test passed\n\n");
        } else {
            result = 1;
            ansi_format(COLORS_RED, false);
            printf("Test failed\n\n");
        }
        ansi_reset();

        test_n = test_n->next;
    }

    print_suite_name(suite_name);
    ansi_bold(true);
    printf(" Test suite result: ");
    ansi_format((passed_test_count == test_count)
                                || (selected_test && passed_test_count == 1)
                        ? COLORS_GREEN
                        : COLORS_RED,
                true);
    printf("%zu/%zu\n", passed_test_count, test_count);
    ansi_reset();
    *out_passed = passed_test_count;
    *out_total = test_count;
    return result;
}

static void remove_suite(void *element) {
    suite_element_t *suite_element = (suite_element_t *) element;
    free_list(&suite_element->unit_tests);
    free(suite_element);
}

static void remove_unit_tests(void *element) {
    unit_test_element_t *unit_test_element = (unit_test_element_t *) element;
    free(unit_test_element);
}

int main(int argc, char *argv[]) {
    const char *selected_suite = NULL;
    const char *selected_test = NULL;
    node_t *suite_n = _suites_list.head;
    suite_element_t *suite_ele;

    set_remove_callback(&_suites_list, remove_suite);
    while (suite_n) {
        suite_ele = (suite_element_t *) suite_n->element;
        suite_n = suite_n->next;
        set_remove_callback(&suite_ele->unit_tests, remove_unit_tests);
    }

    if (parse_command_line_args(argc, argv, &selected_suite, &selected_test)) {
        free_list(&_suites_list);
        return 0;
    }

    size_t total_tests = 0;
    size_t passed_tests = 0;
    int result = 0;

    suite_n = _suites_list.head;
    while (suite_n) {
        suite_ele = (suite_element_t *) suite_n->element;
        suite_n = suite_n->next;

        if (selected_suite && strcmp(selected_suite, suite_ele->suite_name)) {
            continue;
        }
        size_t suite_passed_tests = 0;
        size_t suite_total_tests = 0;
        if (execute_unit_tests(&suite_ele->unit_tests, suite_ele->suite_name,
                               selected_test, &suite_total_tests,
                               &suite_passed_tests)) {
            result = 1;
        }
        total_tests += suite_total_tests;
        passed_tests += suite_passed_tests;
    }

    ansi_bold(true);
    printf("\n[ALL TESTS] Summary: ");
    ansi_format((result == 0) ? COLORS_GREEN : COLORS_RED, true);
    printf("%zu/%zu passed\n\n", passed_tests, total_tests);
    ansi_reset();

    free_list(&_suites_list);
    return result;
}
