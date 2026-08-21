/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
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

static int read_file_into_buffer(const char *path,
                                 uint8_t *out_buf,
                                 size_t out_buf_capacity,
                                 size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        log(L_ERROR, "Failed to open %s: %s", path, strerror(errno));
        char cwd[500];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            log(L_INFO, "Current working dir: %s", cwd);
        }
        return -1;
    }
    size_t total = fread(out_buf, 1, out_buf_capacity, f);
    if (ferror(f)) {
        log(L_ERROR, "Failed to read %s", path);
        fclose(f);
        return -1;
    }
    // If the buffer is filled, ensure the file does not contain more data
    if (total == out_buf_capacity) {
        int c = fgetc(f);
        if (c != EOF) {
            log(L_ERROR, "%s too large (>%lu bytes)", path,
                (unsigned long) out_buf_capacity);
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    *out_size = total;
    return 0;
}

// Installs Security Object and adds an instance of it.
// An instance of Security Object provides information needed to connect to
// LwM2M server.
static int install_security_obj(anj_t *anj,
                                anj_dm_security_obj_t *security_obj) {
    uint8_t client_cert_der[ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE];
    uint8_t client_key_der[ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE];
    size_t client_cert_der_size = 0;
    size_t client_key_der_size = 0;

    if (read_file_into_buffer("client_cert.der", client_cert_der,
                              sizeof(client_cert_der), &client_cert_der_size)
            || read_file_into_buffer("client_key.der", client_key_der,
                                     sizeof(client_key_der),
                                     &client_key_der_size)) {
        return -1;
    }

    anj_dm_security_instance_init_t security_inst = {
        .ssid = 1,
        .server_uri = "coaps://eu.iot.avsystem.cloud:5684",
        .security_mode = ANJ_DM_SECURITY_CERTIFICATE,
        .public_key_or_identity = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = client_cert_der,
            .info.buffer.data_size = client_cert_der_size
        },
        .secret_key = {
            .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
            .info.buffer.data = client_key_der,
            .info.buffer.data_size = client_key_der_size
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
        struct timespec ts = { 0, 50 * 1000 * 1000 }; // 50 ms
        nanosleep(&ts, NULL);
    }
    return 0;
}
