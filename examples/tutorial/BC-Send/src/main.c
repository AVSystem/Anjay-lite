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
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/log.h>

#include "temperature_obj.h"

#define log(...) anj_log(example_log, __VA_ARGS__)

#define RECORDS_CNT_SEND_TRIGGER 10
#define MAX_RECORDS (2 * RECORDS_CNT_SEND_TRIGGER)

typedef struct fin_handler_data {
    size_t records_cnt;
    size_t record_idx;
    anj_io_out_entry_t *records;
    bool send_in_progress;
} fin_handler_data_t;

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

static void
send_finished_handler(anj_t *anjay, uint16_t send_id, int result, void *data_) {
    (void) anjay;
    (void) send_id;
    (void) result;

    assert(data_);
    fin_handler_data_t *data = (fin_handler_data_t *) data_;

    /* move the records not yet processed to the begining of the array */
    memmove(data->records,
            data->records + data->records_cnt,
            (MAX_RECORDS - data->records_cnt) * sizeof(anj_io_out_entry_t));

    data->record_idx = data->record_idx - data->records_cnt;
    data->send_in_progress = false;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        log(L_ERROR, "No endpoint name given");
        return -1;
    }

    // Use current time as seed for random generator used by
    // update_temperature_obj_value()
    srand((unsigned int) time(NULL));

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

    if (anj_dm_add_obj(&anj, get_temperature_obj())) {
        log(L_ERROR, "install_temperature_object error");
        return -1;
    }

    anj_res_value_t value;
    anj_time_monotonic_t next_read_time =
            anj_time_monotonic_add(anj_time_monotonic_now(),
                                   anj_time_duration_new(1, ANJ_TIME_UNIT_S));
    uint16_t send_id = 0;
    anj_io_out_entry_t records[MAX_RECORDS];
    fin_handler_data_t data = { 0 };

    while (true) {
        anj_core_step(&anj);
        update_temperature_obj_value();
        usleep(50 * 1000);
        if (anj_time_monotonic_lt(next_read_time, anj_time_monotonic_now())) {
            next_read_time = anj_time_monotonic_add(
                    anj_time_monotonic_now(),
                    anj_time_duration_new(1, ANJ_TIME_UNIT_S));
            if (data.record_idx < MAX_RECORDS) {
                if (anj_dm_res_read(&anj,
                                    &ANJ_MAKE_RESOURCE_PATH(3303, 0, 5700),
                                    &value)) {
                    log(L_ERROR, "Failed to read resource");
                } else {
                    records[data.record_idx].path =
                            ANJ_MAKE_RESOURCE_PATH(3303, 0, 5700);
                    records[data.record_idx].type = ANJ_DATA_TYPE_DOUBLE;
                    records[data.record_idx].value = value;
                    records[data.record_idx].timestamp =
                            anj_time_real_to_fscalar(anj_time_real_now(),
                                                     ANJ_TIME_UNIT_S);
                    data.record_idx++;
                }
            } else {
                log(L_WARNING,
                    "Records array full, abort send operation ID: "
                    "%u",
                    send_id);
                if (anj_send_abort(&anj, send_id)) {
                    log(L_ERROR, "Failed to abort send operation");
                } else {
                    data.record_idx = 0;
                    data.send_in_progress = false;
                }
            }
        }

        if (data.record_idx >= RECORDS_CNT_SEND_TRIGGER
                && !data.send_in_progress) {
            data.records_cnt = data.record_idx;
            data.records = records;
            data.send_in_progress = true;

            /* Record list full, request send */
            anj_send_request_t send_req = {
                .finished_handler = send_finished_handler,
                .data = (void *) &data,
                .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
                .records_cnt = data.records_cnt,
                .records = records
            };

            if (anj_send_new_request(&anj, &send_req, &send_id)) {
                log(L_ERROR, "Failed to request new send");
                data.send_in_progress = false;
            }
        }
    }
    return 0;
}
