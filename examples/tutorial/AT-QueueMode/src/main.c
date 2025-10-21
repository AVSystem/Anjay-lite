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
#include <anj/defs.h>
#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/log.h>

#define log(...) anj_log(example_log, __VA_ARGS__)

// Installs Device Object and adds an instance of it.
// An instance of Device Object provides the data related to a device.
static int install_device_obj(anj_t *anj, anj_dm_device_obj_t *device_obj) {
    anj_dm_device_object_init_t device_obj_conf = {
        .firmware_version = "0.1"
    };
    return anj_dm_device_obj_install(anj, device_obj, &device_obj_conf);
}

// Installs Server Object and adds an instance of it.
// An instance of Server Object provides the data related to a LwM2M Server.
static int install_server_obj(anj_t *anj, anj_dm_server_obj_t *server_obj) {
    anj_dm_server_instance_init_t server_inst = {
        .ssid = 1,
        .lifetime = 50,
        .binding = "U",
        .bootstrap_on_registration_failure = &(bool) { false },
    };
    anj_dm_server_obj_init(server_obj);
    if (anj_dm_server_obj_add_instance(server_obj, &server_inst)
            || anj_dm_server_obj_install(anj, server_obj)) {
        return -1;
    }
    return 0;
}

// Installs Security Object and adds an instance of it.
// An instance of Security Object provides information needed to connect to
// LwM2M server.
static int install_security_obj(anj_t *anj,
                                anj_dm_security_obj_t *security_obj) {
    anj_dm_security_instance_init_t security_inst = {
        .ssid = 1,
        .server_uri = "coap://eu.iot.avsystem.cloud:5683",
        .security_mode = ANJ_DM_SECURITY_NOSEC
    };
    anj_dm_security_obj_init(security_obj);
    if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
            || anj_dm_security_obj_install(anj, security_obj)) {
        return -1;
    }
    return 0;
}

static void connection_status_callback(void *arg,
                                       anj_t *anj,
                                       anj_conn_status_t conn_status) {
    (void) arg;

    if (conn_status == ANJ_CONN_STATUS_QUEUE_MODE) {
        anj_time_duration_t time = anj_core_next_step_time(anj);
        uint64_t time_ms =
                (uint64_t) anj_time_duration_to_scalar(time, ANJ_TIME_UNIT_MS);

        // Simulate entering low power mode for period of time returned by
        // previous function
        struct timespec ts = {
            // Warning: unchecked cast
            .tv_sec = (time_t) (time_ms / 1000),
            .tv_nsec = (long) ((time_ms % 1000) * 1000000L)
        };
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        log(L_ERROR, "No endpoint name given");
        return -1;
    }

    anj_t anj;
    anj_dm_device_obj_t device_obj;
    anj_dm_server_obj_t server_obj;
    anj_dm_security_obj_t security_obj;

    anj_configuration_t config = {
        .endpoint_name = argv[1],
        .queue_mode_enabled = true,
        .queue_mode_timeout = anj_time_duration_new(5, ANJ_TIME_UNIT_S),
        .connection_status_cb = connection_status_callback
    };
    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj)
            || install_server_obj(&anj, &server_obj)) {
        return -1;
    }

    while (true) {
        anj_core_step(&anj);
        usleep(50 * 1000);
    }
    return 0;
}
