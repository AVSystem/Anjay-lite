/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/log.h>
#include <string.h>

#include <anj_unit_test.h>

bool g_lower_the_default_level_debug;
bool g_lower_the_default_level_trace;
bool g_increase_the_default_level_warning;
bool g_increase_the_default_level_info;

int custom_log_handler(anj_log_level_t level,
                       const char *module,
                       const char *format,
                       ...) {
    (void) level;
    (void) format;
    if (strcmp(module, "lower_the_default_level") == 0
            && level == ANJ_LOG_LEVEL_L_DEBUG) {
        g_lower_the_default_level_debug = true;
    }
    if (strcmp(module, "lower_the_default_level") == 0
            && level == ANJ_LOG_LEVEL_L_TRACE) {
        g_lower_the_default_level_trace = true;
    }
    if (strcmp(module, "increase_the_default_level") == 0
            && level == ANJ_LOG_LEVEL_L_WARNING) {
        g_increase_the_default_level_warning = true;
    }
    if (strcmp(module, "increase_the_default_level") == 0
            && level == ANJ_LOG_LEVEL_L_INFO) {
        g_increase_the_default_level_info = true;
    }
    return 0;
}

ANJ_UNIT_TEST(logger_filtering_check, log_filtering_test) {
    anj_log(lower_the_default_level, L_DEBUG, "Lorem ipsum");
    anj_log(lower_the_default_level, L_TRACE, "Lorem ipsum");
    ANJ_UNIT_ASSERT_TRUE(g_lower_the_default_level_debug);
    ANJ_UNIT_ASSERT_FALSE(g_lower_the_default_level_trace);
    anj_log(increase_the_default_level, L_WARNING, "Lorem ipsum");
    anj_log(increase_the_default_level, L_INFO, "Lorem ipsum");
    ANJ_UNIT_ASSERT_TRUE(g_increase_the_default_level_warning);
    ANJ_UNIT_ASSERT_FALSE(g_increase_the_default_level_info);
}
