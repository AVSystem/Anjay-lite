/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_INIT_H
#define ANJ_INTERNAL_INIT_H

// IWYU pragma: begin_exports
#include <anj/anj_config.h>
#include <anj/init.h>
// IWYU pragma: end_exports

#ifdef __cplusplus
extern "C" {
#endif

/* GCC may warn (e.g., with -Wpedantic) about “ISO C forbids an empty
 * translation unit”. At the time of writing, GCC does not support
 * -Wno-empty-translation-unit, so we add a harmless declaration via the init
 * header to ensure each translation unit is non-empty.
 */
typedef int _anj_enforce_non_empty_translation_unit;

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_INIT_H
