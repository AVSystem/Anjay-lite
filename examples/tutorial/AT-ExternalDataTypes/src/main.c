/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#define _DEFAULT_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/log.h>

#define FILE_PATH "./libanj.a"
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

static struct external_data_user_args {
    int fd;
} file_external_data_args = { -1 };

// Callback used by Anjay Lite to read bytes from file
static int get_external_data(void *buffer,
                             size_t *inout_size,
                             size_t offset,
                             void *user_args) {
    struct external_data_user_args *args =
            (struct external_data_user_args *) user_args;
    size_t read_bytes = 0;

    while (*inout_size != read_bytes) {
        ssize_t ret_val =
                pread(args->fd,
                      buffer,
                      *inout_size - read_bytes,
                      // We don't care about the off_t argument overflowing,
                      // because even if off_t were 32 bytes wide, an offset
                      // that large would still let us handle files bigger than
                      // the maximum file size that can be sent over CoAP
                      (off_t) (offset + read_bytes));

        if (ret_val == 0) {
            *inout_size = read_bytes;
            log(L_INFO, "The file has been completely read");
            return 0;
        } else if (ret_val < 0) {
            log(L_ERROR, "Error during reading from the file");
            return -1;
        }
        read_bytes += (size_t) ret_val;
    }
    return ANJ_IO_NEED_NEXT_CALL;
}

// Callback used by Anjay Lite to open file
static int open_external_data(void *user_args) {
    struct external_data_user_args *args =
            (struct external_data_user_args *) user_args;
    assert(args->fd == -1);

    args->fd = open(FILE_PATH, O_RDONLY | O_CLOEXEC);
    if (args->fd == -1) {
        log(L_ERROR, "Error during opening the file");
        return -1;
    }

    log(L_INFO, "File opened");
    return 0;
}

// Callback used by Anjay Lite to close file
static void close_external_data(void *user_args) {
    struct external_data_user_args *args =
            (struct external_data_user_args *) user_args;
    close(args->fd);
    args->fd = -1;
    log(L_INFO, "File closed");
}

// Callback that will be call during Read operation
static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) obj;

    if (iid == 0 && rid == 0 && riid == 0) {
        out_value->external_data.get_external_data = get_external_data;
        out_value->external_data.open_external_data = open_external_data;
        out_value->external_data.close_external_data = close_external_data;

        out_value->external_data.user_args = (void *) &file_external_data_args;
        return 0;
    }

    return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
}

// Installs Binary App Data Container with fixed instance count.
// An instance of Binary App Data Container is used as an example
// to introduce external data support.
static int install_binary_app_data_container_object(anj_t *anj) {
    static const anj_dm_handlers_t handlers = {
        .res_read = res_read,
    };

    // Definition of resource instance
    static const anj_riid_t insts[] = { 0 };

    // Definition of resource
    static const anj_dm_res_t res = {
        .rid = 0,
        .kind = ANJ_DM_RES_RM,
        .type = ANJ_DATA_TYPE_EXTERNAL_BYTES,
        .insts = insts,
        .max_inst_count = 1
    };

    // Definition of instance
    static const anj_dm_obj_inst_t obj_insts = {
        .iid = 0,
        .res_count = 1,
        .resources = &res
    };

    // Definition of object
    static const anj_dm_obj_t obj = {
        .oid = 19,
        .insts = &obj_insts,
        .handlers = &handlers,
        .max_inst_count = 1
    };

    return anj_dm_add_obj(anj, &obj);
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
            || install_server_obj(&anj, &server_obj)
            || install_binary_app_data_container_object(&anj)) {
        return -1;
    }

    while (true) {
        anj_core_step(&anj);
        usleep(50 * 1000);
    }
    return 0;
}
