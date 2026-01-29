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
#include <string.h>
#include <unistd.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>

#include "firmware_update.h"

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
    static const char PSK_IDENTITY[] = "identity";
    static const char PSK_KEY[] = "P4s$w0rd";

    anj_dm_security_instance_init_t security_inst = {
        .ssid = 1,
        .server_uri = "coaps://eu.iot.avsystem.cloud:5684",
        .security_mode = ANJ_DM_SECURITY_PSK,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = PSK_IDENTITY,
            .info.buffer.data_size = strlen(PSK_IDENTITY)
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = PSK_KEY,
            .info.buffer.data_size = strlen(PSK_KEY)
        }
    };
    anj_dm_security_obj_init(security_obj);
    if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
            || anj_dm_security_obj_install(anj, security_obj)) {
        return -1;
    }
    return 0;
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
    if (fw_update_object_install(
                &anj,
                (const char *) firmware_version.bytes_or_string.data,
                anj.endpoint_name)) {
        return -1;
    }

    while (true) {
        anj_core_step(&anj);
        fw_update_process();
        usleep(50 * 1000);
    }
    return 0;
}
