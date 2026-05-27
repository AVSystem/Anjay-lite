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
#include <string.h>
#include <time.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/device_object.h>
#include <anj/dm/oscore_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/log.h>

#define log(...) anj_log(example_log, __VA_ARGS__)

#define OSCORE_IID 0

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
        .security_mode = ANJ_DM_SECURITY_NOSEC,
        .oscore_security_mode = { 21, OSCORE_IID },
    };
    anj_dm_security_obj_init(security_obj);
    if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
            || anj_dm_security_obj_install(anj, security_obj)) {
        return -1;
    }
    return 0;
}

// Installs OSCORE Object and adds an instance of it.
// An instance of OSCORE Object provides information needed to connect to
// LwM2M server.
static int install_oscore_obj(anj_t *anj, anj_dm_oscore_obj_t *oscore_obj) {
    static const uint8_t MASTER_SECRET[] = "M4$t3r $3cr3t";
    static const uint8_t MASTER_SALT[] = "M4$t3r $4lt";
    static const uint8_t SENDER_ID[] = "SenID";
    static const uint8_t RECIPIENT_ID[] = "RecID";

    anj_dm_oscore_instance_init_t oscore_inst = {
        .iid = OSCORE_IID,
        .master_secret = {
            .tag = ANJ_CRYPTO_SECURITY_TAG_OSCORE_MASTER_SECRET,
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            {
                .buffer = { MASTER_SECRET, sizeof(MASTER_SECRET) - 1 }
            }
        },
        .sender_id = SENDER_ID,
        .sender_id_len = sizeof(SENDER_ID) - 1,
        .recipient_id = RECIPIENT_ID,
        .recipient_id_len = sizeof(RECIPIENT_ID) - 1,
        .oscore_aead_algorithm = 0,
        .oscore_hmac_algorithm = 0,
        .oscore_master_salt = MASTER_SALT,
        .oscore_master_salt_len = sizeof(MASTER_SALT) - 1
    };

    anj_dm_oscore_obj_init(oscore_obj);
    if (anj_dm_oscore_obj_add_instance(oscore_obj, &oscore_inst)
            || anj_dm_oscore_obj_install(anj, oscore_obj)) {
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
    anj_dm_oscore_obj_t oscore_obj;

    anj_configuration_t config = {
        .endpoint_name = argv[1]
    };
    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj)
            || install_server_obj(&anj, &server_obj)
            || install_oscore_obj(&anj, &oscore_obj)) {
        return -1;
    }

    while (true) {
        anj_core_step(&anj);
        struct timespec ts = { 0, 50 * 1000 * 1000 }; // 50 ms
        nanosleep(&ts, NULL);
    }
    return 0;
}
