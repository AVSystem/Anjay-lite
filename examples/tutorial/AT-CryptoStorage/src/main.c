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
#include <anj/crypto.h>
#include <anj/defs.h>
#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/log.h>
#include <anj/persistence.h>

#define log(...) anj_log(example_log, __VA_ARGS__)

#define PERSISTENCE_OBJS_FILE "persistence_objs.bin"

// These files provide the initial credentials used to connect to the Bootstrap
// Server. Security credentials provisioned later through Bootstrap Write are
// moved by the crypto storage layer into crypto_record_*.dat files.
#define BOOTSTRAP_CLIENT_CERT_PATH "bootstrap_client_cert.der"
#define BOOTSTRAP_CLIENT_KEY_PATH "bootstrap_client_key.der"
#define BOOTSTRAP_SERVER_CERT_PATH "bootstrap_server_cert.der"

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

// External crypto identities let the TLS backend fetch credentials on demand.
// In this example they are plain filesystem paths understood by the default
// POSIX crypto storage implementation.
static void security_info_from_path(anj_crypto_security_info_t *out_info,
                                    const char *path) {
    memset(out_info, 0, sizeof(*out_info));
    out_info->source = ANJ_CRYPTO_DATA_SOURCE_EXTERNAL;
    out_info->info.external.identity = path;
}

// Installs Device Object and adds an instance of it.
// An instance of Device Object provides the data related to a device.
static int install_device_obj(anj_t *anj, anj_dm_device_obj_t *device_obj) {
    anj_dm_device_object_init_t device_obj_conf = {
        .firmware_version = "0.1"
    };
    return anj_dm_device_obj_install(anj, device_obj, &device_obj_conf);
}

static int restore_security_obj(anj_t *anj,
                                anj_dm_security_obj_t *security_obj,
                                const anj_persistence_context_t *ctx) {
    if (anj_dm_security_obj_restore(anj, security_obj, ctx)) {
        log(L_WARNING,
            "Security Object restore failed. Falling back to bootstrap "
            "account.");
        return -1;
    }
    log(L_INFO, "Security Object restored from persistence");
    return 0;
}

static int restore_server_obj(anj_dm_server_obj_t *server_obj,
                              const anj_persistence_context_t *ctx) {
    if (anj_dm_server_obj_restore(server_obj, ctx)) {
        log(L_WARNING,
            "Server Object restore failed. Falling back to bootstrap flow.");
        return -1;
    }
    log(L_INFO, "Server Object restored from persistence");
    return 0;
}

// Installs Security Object and optionally adds a bootstrap account configured
// with certificate-based credentials stored as external identities.
static int install_security_obj(anj_t *anj,
                                anj_dm_security_obj_t *security_obj,
                                bool initial_bootstrap_configuration) {
    if (initial_bootstrap_configuration) {
        anj_crypto_security_info_t client_cert;
        anj_crypto_security_info_t client_key;
        anj_crypto_security_info_t server_cert;

        security_info_from_path(&client_cert, BOOTSTRAP_CLIENT_CERT_PATH);
        security_info_from_path(&client_key, BOOTSTRAP_CLIENT_KEY_PATH);
        security_info_from_path(&server_cert, BOOTSTRAP_SERVER_CERT_PATH);

        anj_dm_security_instance_init_t security_inst = {
            .server_uri = "coaps://eu.iot.avsystem.cloud:5694",
            .bootstrap_server = true,
            .security_mode = ANJ_DM_SECURITY_CERTIFICATE,
            .public_key_or_identity = client_cert,
            .secret_key = client_key,
            .server_public_key = server_cert
        };
        if (anj_dm_security_obj_add_instance(security_obj, &security_inst)) {
            log(L_ERROR, "Failed to add Bootstrap Server security instance");
            return -1;
        }
        log(L_INFO,
            "Using bootstrap credentials from file paths: '%s', '%s', '%s'",
            BOOTSTRAP_CLIENT_CERT_PATH, BOOTSTRAP_CLIENT_KEY_PATH,
            BOOTSTRAP_SERVER_CERT_PATH);
    }

    return anj_dm_security_obj_install(anj, security_obj);
}

// Installs Server Object and DOES NOT add an instance of it.
// A regular LwM2M Server Object instance is expected to be provisioned through
// Bootstrap Write or restored from persistence.
static int install_server_obj(anj_t *anj, anj_dm_server_obj_t *server_obj) {
    return anj_dm_server_obj_install(anj, server_obj);
}

typedef struct {
    anj_dm_server_obj_t *server_obj;
    anj_dm_security_obj_t *security_obj;
} persistent_objects_t;

static void store_persistent_objects(anj_t *anj,
                                     persistent_objects_t *persistent_objects) {
    FILE *file = fopen(PERSISTENCE_OBJS_FILE, "w+");
    if (!file) {
        log(L_ERROR, "Could not open persistence file for writing");
        return;
    }

    anj_persistence_context_t persistence_ctx =
            anj_persistence_store_context_create(persistence_write, file);
    // Storing both objects together keeps the Security/Server instance pairing
    // consistent across restarts.
    if (anj_dm_security_obj_store(anj, persistent_objects->security_obj,
                                  &persistence_ctx)
            || anj_dm_server_obj_store(persistent_objects->server_obj,
                                       &persistence_ctx)) {
        log(L_ERROR, "Could not store persistent objects");
        fclose(file);
        remove(PERSISTENCE_OBJS_FILE);
        return;
    }

    fclose(file);
    log(L_INFO, "Persistent objects stored");
}

static void connection_status_callback(void *arg,
                                       anj_t *anj,
                                       anj_conn_status_t conn_status) {
    if (conn_status == ANJ_CONN_STATUS_BOOTSTRAPPED) {
        log(L_INFO,
            "Bootstrap finished successfully; storing bootstrapped objects");
        store_persistent_objects(anj, (persistent_objects_t *) arg);
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

    persistent_objects_t persistent_objects = {
        .server_obj = &server_obj,
        .security_obj = &security_obj
    };

    anj_configuration_t config = {
        .endpoint_name = argv[1],
        .connection_status_cb = connection_status_callback,
        .connection_status_cb_arg = &persistent_objects
    };
    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    anj_dm_security_obj_init(&security_obj);
    anj_dm_server_obj_init(&server_obj);

    bool initial_bootstrap_configuration = true;
    FILE *file = fopen(PERSISTENCE_OBJS_FILE, "r");
    if (file) {
        anj_persistence_context_t ctx =
                anj_persistence_restore_context_create(persistence_read, file);
        if (!restore_security_obj(&anj, &security_obj, &ctx)
                && !restore_server_obj(&server_obj, &ctx)) {
            initial_bootstrap_configuration = false;
        } else {
            // If either restore fails, discard the partially restored state and
            // rebuild the default bootstrap account instead.
            anj_dm_security_obj_init(&security_obj);
            anj_dm_server_obj_init(&server_obj);
            remove(PERSISTENCE_OBJS_FILE);
        }
        fclose(file);
    }

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj,
                                    initial_bootstrap_configuration)
            || install_server_obj(&anj, &server_obj)) {
        return -1;
    }

    while (true) {
        anj_core_step(&anj);
        struct timespec ts = { 0, 50 * 1000 * 1000 }; // 50 ms
        nanosleep(&ts, NULL);
    }
    return 0;
}
