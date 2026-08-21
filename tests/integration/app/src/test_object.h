/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef TEST_OBJECT_H
#define TEST_OBJECT_H

#include <anj/core.h>

#ifdef __cplusplus
extern "C" {
#endif

int test_object_install(anj_t *anj);
int test_object_set_value(anj_t *anj, anj_iid_t iid, double value);

#ifdef __cplusplus
}
#endif

#endif // TEST_OBJECT_H
