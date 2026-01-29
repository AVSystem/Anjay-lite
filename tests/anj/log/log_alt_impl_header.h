/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJAY_LITE_LOG_ALT_IMPL_HEADER_H
#define ANJAY_LITE_LOG_ALT_IMPL_HEADER_H

#include <anj/log.h>

int custom_log_handler(anj_log_level_t level,
                       const char *module,
                       const char *format,
                       ...);

#define ANJ_LOG_HANDLER_IMPL_MACRO(Module, Level, ...) \
    custom_log_handler(                                \
            ANJ_LOG_LEVEL_##Level, ANJ_QUOTE_MACRO(Module), __VA_ARGS__)

#endif // ANJAY_LITE_LOG_ALT_IMPL_HEADER_H
