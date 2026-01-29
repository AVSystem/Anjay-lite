/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/log.h>
#include <anj/ntp.h>
#include <anj/persistence.h>

#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>

#define log(...) anj_log(example_log, __VA_ARGS__)

#define PERSISTENCE_NTP_FILE "persistence_ntp.bin"

static int persistence_read(void *ctx, void *buf, size_t size) {
    FILE *file = (FILE *) ctx;
    if (fread(buf, 1, size, file) != size) {
        return -1;
    }
    return 0;
}

static int persistence_write(void *ctx, const void *buf, size_t size) {
    FILE *file = (FILE *) ctx;
    if (fwrite(buf, 1, size, file) != size) {
        return -1;
    }
    return 0;
}

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

static anj_time_real_t g_last_sync_real;
static anj_time_monotonic_t g_last_sync_monotonic;
static bool g_time_synced = false;

static int64_t get_time(clockid_t clk_id) {
    struct timespec res;
    if (clock_gettime(clk_id, &res)) {
        return 0;
    }
    return (int64_t) res.tv_sec * 1000 * 1000 + (int64_t) res.tv_nsec / 1000;
}

anj_time_monotonic_t anj_time_monotonic_now(void) {
#ifdef CLOCK_MONOTONIC
    return anj_time_monotonic_new(get_time(CLOCK_MONOTONIC), ANJ_TIME_UNIT_US);
#else  /* CLOCK_MONOTONIC */
    log(L_ERROR, "CLOCK_MONOTONIC is not available on this platform");
    return anj_time_monotonic_new(0, ANJ_TIME_UNIT_US);
#endif /* CLOCK_MONOTONIC */
}

anj_time_real_t anj_time_real_now(void) {
    anj_time_duration_t delta = anj_time_duration_sub(
            anj_time_monotonic_to_duration(anj_time_monotonic_now()),
            anj_time_monotonic_to_duration(g_last_sync_monotonic));

    return anj_time_real_add(g_last_sync_real, delta);
}

static void ntp_event_callback(void *arg,
                               anj_ntp_t *ntp,
                               anj_ntp_status_t status,
                               anj_time_real_t synchronized_time) {
    (void) arg;

    switch (status) {
    case ANJ_NTP_STATUS_INITIAL:
    case ANJ_NTP_STATUS_PERIOD_EXCEEDED: {
        log(L_INFO, "NTP synchronization started");
        anj_ntp_start(ntp);
        break;
    }
    case ANJ_NTP_STATUS_FINISHED_WITH_ERROR: {
        log(L_ERROR, "NTP synchronization failed");
        break;
    }
    case ANJ_NTP_STATUS_FINISHED_SUCCESSFULLY: {
        log(L_INFO, "NTP synchronization succeeded");
        log(L_INFO, "\n   Old time: %s\n   New time: %s\n   Delta:    %f s",
            ANJ_TIME_REAL_AS_STRING(g_last_sync_real, ANJ_TIME_UNIT_MS),
            ANJ_TIME_REAL_AS_STRING(synchronized_time, ANJ_TIME_UNIT_MS),
            ((double) anj_time_duration_to_scalar(
                    anj_time_real_diff(synchronized_time, anj_time_real_now()),
                    ANJ_TIME_UNIT_MS))
                    / 1000.0);
        g_time_synced = true;
        g_last_sync_real = synchronized_time;
        g_last_sync_monotonic = anj_time_monotonic_now();
        break;
    }
    case ANJ_NTP_STATUS_OBJECT_UPDATED: {
        FILE *file = fopen(PERSISTENCE_NTP_FILE, "w+");
        if (!file) {
            log(L_ERROR, "Could not open persistence file for writing");
            return;
        }
        anj_persistence_context_t ctx =
                anj_persistence_store_context_create(persistence_write, file);
        if (anj_ntp_obj_store(ntp, &ctx)) {
            log(L_ERROR, "Failed to persist NTP object state");
            fclose(file);
            remove(PERSISTENCE_NTP_FILE);
            return;
        }
        fclose(file);
        log(L_INFO, "NTP object state persisted successfully");
        break;
    }
    default:
        break;
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

    anj_ntp_t ntp;
    anj_ntp_configuration_t ntp_config = {
        .event_cb = ntp_event_callback,
        .ntp_server_address = "pool.ntp.org",
        .backup_ntp_server_address = "time.google.com",
        .ntp_period_hours = 1,
        .response_timeout = anj_time_duration_new(3, ANJ_TIME_UNIT_S),
    };
    if (anj_ntp_init(&anj, &ntp, &ntp_config)) {
        log(L_ERROR, "Failed to initialize NTP module");
        return -1;
    }

    FILE *file = fopen(PERSISTENCE_NTP_FILE, "r");
    if (file) {
        anj_persistence_context_t ctx =
                anj_persistence_restore_context_create(persistence_read, file);
        if (anj_ntp_obj_restore(&ntp, &ctx)) {
            fclose(file);
            remove(PERSISTENCE_NTP_FILE);
        } else {
            fclose(file);
        }
    }

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj)
            || install_server_obj(&anj, &server_obj)) {
        return -1;
    }

    while (true) {
        usleep(50 * 1000);
        anj_ntp_step(&ntp);

        if (g_time_synced) {
            // Once time is synchronized, we can proceed to normal Anjay
            // operation
            anj_core_step(&anj);
        }
    }
    return 0;
}
