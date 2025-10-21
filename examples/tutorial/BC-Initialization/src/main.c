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
#include <unistd.h>

#include <anj/core.h>
#include <anj/log.h>

#define log(...) anj_log(example_log, __VA_ARGS__)

int main(int argc, char *argv[]) {
    if (argc != 2) {
        log(L_ERROR, "No endpoint name given");
        return -1;
    }

    anj_t anj;
    anj_configuration_t config = {
        .endpoint_name = argv[1]
    };

    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    while (true) {
        anj_core_step(&anj);
        usleep(50 * 1000);
    }
    return 0;
}
