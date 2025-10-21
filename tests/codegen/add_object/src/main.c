/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <anj/core.h>
#include <anj/log.h>

#include "some_obj.h"

#define log(...) anj_log(example_log, __VA_ARGS__)

int main(int argc, char *argv[]) {
    srand((unsigned int) time(NULL));

    anj_t anj;

    anj_configuration_t config = {
        .endpoint_name = "test-endpoint"
    };

    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    if (anj_dm_add_obj(&anj, sample_object_create())) {
        log(L_ERROR, "install_sample_object error");
        return -1;
    }

    return 0;
}
