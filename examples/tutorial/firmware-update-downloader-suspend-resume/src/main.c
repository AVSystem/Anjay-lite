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
#include <stddef.h>
#include <time.h>

#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/log.h>

#include "firmware_update.h"

#define DOWNLOAD_CHUNK_LIMIT_STEP 100
#define DOWNLOAD_SUSPEND_SECONDS 30

typedef struct {
    size_t next_suspend_at_chunk;
    bool is_suspended;
    anj_time_monotonic_t resume_at;
} download_control_t;

static int install_device_obj(anj_t *anj, anj_dm_device_obj_t *device_obj) {
    anj_dm_device_object_init_t device_obj_conf = {
        .firmware_version = "0.1"
    };
    return anj_dm_device_obj_install(anj, device_obj, &device_obj_conf);
}

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

static void reset_download_control(download_control_t *control) {
    control->next_suspend_at_chunk = DOWNLOAD_CHUNK_LIMIT_STEP;
    control->is_suspended = false;
}

static void handle_download_window(download_control_t *control) {
    if (!fw_update_is_download_active()) {
        reset_download_control(control);
        return;
    }

    const size_t received_chunks = fw_update_get_received_chunk_count();

    if (!control->is_suspended
            && received_chunks >= control->next_suspend_at_chunk) {
        if (!fw_update_suspend_download()) {
            control->is_suspended = true;
            control->resume_at = anj_time_monotonic_add(
                    anj_time_monotonic_now(),
                    anj_time_duration_new(DOWNLOAD_SUSPEND_SECONDS,
                                          ANJ_TIME_UNIT_S));
            control->next_suspend_at_chunk += DOWNLOAD_CHUNK_LIMIT_STEP;
            log(L_INFO,
                "Downloaded %zu chunks; suspending firmware download for "
                "%d "
                "seconds",
                received_chunks, DOWNLOAD_SUSPEND_SECONDS);
        } else {
            log(L_ERROR, "Failed to suspend firmware download - invalid state");
        }
        return;
    }

    if (control->is_suspended
            && anj_time_monotonic_geq(anj_time_monotonic_now(),
                                      control->resume_at)) {
        if (!fw_update_resume_download()) {
            control->is_suspended = false;
            log(L_INFO, "Resuming firmware download");
        } else {
            log(L_ERROR, "Failed to resume firmware download - invalid state");
        }
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
        .endpoint_name = argv[1]
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

    anj_res_value_t firmware_version;
    anj_dm_res_read(&anj, &ANJ_MAKE_RESOURCE_PATH(3, 0, 3), &firmware_version);
    log(L_INFO, "Firmware version: %s",
        (const char *) firmware_version.bytes_or_string.data);

    const firmware_update_config_t fw_update_config = {
        .firmware_version =
                (const char *) firmware_version.bytes_or_string.data,
        .endpoint_name = anj.endpoint_name,
        .retry_delay = anj_time_duration_new(10, ANJ_TIME_UNIT_S),
        .retry_count = 3,
    };
    if (fw_update_object_install(&anj, &fw_update_config)) {
        return -1;
    }

    download_control_t download_control = {
        .next_suspend_at_chunk = DOWNLOAD_CHUNK_LIMIT_STEP,
    };

    while (true) {
        anj_core_step(&anj);
        fw_update_process();
        handle_download_window(&download_control);

        struct timespec ts = { 0, 50 * 1000 * 1000 }; // 50 ms
        nanosleep(&ts, NULL);
    }

    return 0;
}
