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
#include <anj/log.h>
#include <anj/persistence.h>

#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>

#define log(...) anj_log(example_log, __VA_ARGS__)

/* In this example the objects' state is persisted only after a successful
 * bootstrap. However, objects may also be modified later — either by the user
 * or by the LwM2M Management Server (the Server Object only). A resource that
 * commonly changes at runtime is the Lifetime. If you need to keep such changes
 * across client restarts, you must implement your own mechanism that monitors
 * modifications and updates the persistence storage accordingly.*/

#define PERSISTENCE_OBJS_FILE "persistence_objs.bin"
#define PERSISTENCE_ENDPOINT_FILE "persistence_endpoint.bin"

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

static int restore_security_obj(anj_t *anj,
                                anj_dm_security_obj_t *security_obj,
                                const anj_persistence_context_t *ctx) {
    if (anj_dm_security_obj_restore(anj, security_obj, ctx)) {
        log(L_INFO, "Security object restore failed. Using default.");
        return -1;
    }
    log(L_INFO, "Security object restored");
    return 0;
}

static int restore_server_obj(anj_dm_server_obj_t *server_obj,
                              const anj_persistence_context_t *ctx) {
    if (anj_dm_server_obj_restore(server_obj, ctx)) {
        log(L_ERROR, "Server object restore failed. Using default.");
        return -1;
    }
    log(L_INFO, "Server object restored");
    return 0;
}

static int install_security_obj(anj_t *anj,
                                anj_dm_security_obj_t *security_obj,
                                const bool need_default) {
    if (need_default) {
        anj_dm_security_obj_init(security_obj);
        anj_dm_security_instance_init_t security_inst = {
            .ssid = 1,
            .bootstrap_server = true,
            .server_uri = "coap://eu.iot.avsystem.cloud:5693",
            .security_mode = ANJ_DM_SECURITY_NOSEC,
        };
        if (anj_dm_security_obj_add_instance(security_obj, &security_inst)) {
            return -1;
        }
    }
    return anj_dm_security_obj_install(anj, security_obj);
}

static int install_server_obj(anj_t *anj, anj_dm_server_obj_t *server_obj) {
    return anj_dm_server_obj_install(anj, server_obj);
}

typedef struct {
    anj_dm_server_obj_t *server_obj;
    anj_dm_security_obj_t *security_obj;
} persistent_objects_t;

static void connection_status_callback(void *arg,
                                       anj_t *anj,
                                       anj_conn_status_t conn_status) {
    if (conn_status == ANJ_CONN_STATUS_BOOTSTRAPPED) {
        log(L_INFO, "Bootstrap successful");
        FILE *file = fopen(PERSISTENCE_OBJS_FILE, "w+");
        if (!file) {
            log(L_ERROR, "Could not open persistence file for writing");
            return;
        }
        persistent_objects_t *callback_arg = (persistent_objects_t *) arg;
        anj_persistence_context_t persistence_ctx =
                anj_persistence_store_context_create(persistence_write, file);
        if (anj_dm_security_obj_store(anj, callback_arg->security_obj,
                                      &persistence_ctx)
                || anj_dm_server_obj_store(callback_arg->server_obj,
                                           &persistence_ctx)) {
            log(L_ERROR, "Could not store persistent objects");
            fclose(file);
            remove(PERSISTENCE_OBJS_FILE);
        } else {
            log(L_INFO, "Persistent objects stored");
            fclose(file);
        }
    }
}

int main(int argc, char *argv[]) {
    char endpoint_name[128] = { 0 };
    // check if endpoint name was provided as argument
    // if not, try to restore it from persistence
    if (argc != 2) {
        FILE *ep_file = fopen(PERSISTENCE_ENDPOINT_FILE, "r");
        if (!ep_file) {
            log(L_ERROR, "No endpoint name given, and no persistence file "
                         "found to restore it from");
            return -1;
        }
        // restore endpoint name
        anj_persistence_context_t ctx_ep =
                anj_persistence_restore_context_create(persistence_read,
                                                       ep_file);
        if (anj_persistence_string(&ctx_ep, endpoint_name,
                                   sizeof(endpoint_name))) {
            log(L_ERROR, "Failed to restore endpoint name");
            fclose(ep_file);
            return -1;
        }
        log(L_INFO, "Endpoint name restored: %s", endpoint_name);
        fclose(ep_file);
    } else {
        strncpy(endpoint_name, argv[1], sizeof(endpoint_name) - 1);
        // endpoint name provided as an argument - store it in persistence
        FILE *ep_file = fopen(PERSISTENCE_ENDPOINT_FILE, "w+");
        if (!ep_file) {
            log(L_ERROR,
                "Could not open endpoint persistence file for writing");
            return -1;
        }
        anj_persistence_context_t ctx_ep =
                anj_persistence_store_context_create(persistence_write,
                                                     ep_file);
        if (anj_persistence_string(&ctx_ep, argv[1], 0)) {
            log(L_ERROR, "Failed to store endpoint name");
            fclose(ep_file);
            remove(PERSISTENCE_ENDPOINT_FILE);
            return -1;
        }
        log(L_INFO, "Endpoint name stored");
        fclose(ep_file);
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
        .endpoint_name = endpoint_name,
        .connection_status_cb = connection_status_callback,
        .connection_status_cb_arg = &persistent_objects
    };
    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    anj_dm_security_obj_init(&security_obj);
    anj_dm_server_obj_init(&server_obj);
    FILE *file = fopen(PERSISTENCE_OBJS_FILE, "r");
    int res = -1;
    if (file) {
        anj_persistence_context_t ctx =
                anj_persistence_restore_context_create(persistence_read, file);
        res = restore_security_obj(&anj, &security_obj, &ctx);
        if (!res) {
            res = restore_server_obj(&server_obj, &ctx);
        }
        fclose(file);
        // if any of the restores failed, we assume the persistence file is
        // corrupted and should be removed
        if (res) {
            remove(PERSISTENCE_OBJS_FILE);
        }
    }

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj, !!res)
            || install_server_obj(&anj, &server_obj)) {
        return -1;
    }

    while (true) {
        anj_core_step(&anj);
        usleep(50 * 1000);
    }
    return 0;
}
