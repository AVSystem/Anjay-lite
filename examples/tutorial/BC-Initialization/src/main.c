/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

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
        struct timespec ts = { 0, 50 * 1000 * 1000 }; // 50 ms
        nanosleep(&ts, NULL);
    }
    return 0;
}
