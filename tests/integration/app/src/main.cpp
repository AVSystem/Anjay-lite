/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */
#include <cstdlib>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/log.h>
#include <anj/lwm2m_send.h>

#include "commands.hpp"
#include "ipc/ipc.hpp"

#define log(...) anj_log(test_app, __VA_ARGS__)

static bool initialized;
static Config anjay_config;

static anj_t anj;
static anj_dm_device_obj_t device_obj;
static anj_dm_server_obj_t server_obj;
static anj_dm_security_obj_t security_obj;

int init(const Config &config) {
    if (initialized) {
        log(L_ERROR, "Client is already initialized");
        return -1;
    }

    anjay_config = config;

    anj_configuration_t core_config{};

    anj_exchange_udp_tx_params_t udp_tx_params =
            ANJ_EXCHANGE_UDP_TX_PARAMS_DEFAULT;
    if (anjay_config.udp_tx_params.has_value()) {
        const auto &udp_cfg = anjay_config.udp_tx_params.value();
        udp_tx_params.ack_timeout =
                anj_time_duration_new(udp_cfg.ack_timeout_s, ANJ_TIME_UNIT_S);
        udp_tx_params.ack_random_factor = udp_cfg.ack_random_factor;
        udp_tx_params.max_retransmit = (uint16_t) udp_cfg.max_retransmit;
    }
    if (anjay_config.bootstrap_config.has_value()) {
        const auto &bootstrap_cfg = anjay_config.bootstrap_config.value();
        core_config.bootstrap_retry_count =
                (uint16_t) bootstrap_cfg.retry_count;
        core_config.bootstrap_retry_timeout =
                anj_time_duration_new(bootstrap_cfg.retry_timeout_s,
                                      ANJ_TIME_UNIT_S);
    }

    core_config.endpoint_name = anjay_config.endpoint.c_str();
    core_config.udp_tx_params = &udp_tx_params;
    if (anj_core_init(&anj, &core_config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    anj_dm_security_obj_init(&security_obj);
    anj_dm_server_obj_init(&server_obj);

    for (const auto &server_config : anjay_config.servers) {
        anj_dm_security_instance_init_t security_inst{};
        anj_dm_server_instance_init_t server_inst{};
        anj_communication_retry_res_t comm_retry_res;
        security_inst.server_uri = server_config.uri.c_str();
        if (server_config.bootstrap) {
            security_inst.bootstrap_server = true;
        } else {
            security_inst.bootstrap_server = false;
            security_inst.ssid = 1;

            server_inst.ssid = security_inst.ssid;
            if (server_config.lifetime.has_value()) {
                server_inst.lifetime = server_config.lifetime.value();
            } else {
                server_inst.lifetime = 50; // default lifetime
            }

            server_inst.binding = "U";

            if (server_config.communication_retry.has_value()) {
                const auto &comm_cfg =
                        server_config.communication_retry.value();
                comm_retry_res.retry_count = (uint16_t) comm_cfg.retry_count;
                comm_retry_res.retry_timer = comm_cfg.retry_timer_s;
                comm_retry_res.seq_delay_timer = comm_cfg.seq_delay_timer_s;
                comm_retry_res.seq_retry_count =
                        (uint16_t) comm_cfg.seq_retry_count;
                server_inst.comm_retry_res = &comm_retry_res;
            }

            static const bool bootstrap_on_registration_failure = false;
            server_inst.bootstrap_on_registration_failure =
                    &bootstrap_on_registration_failure;
        }

        if (server_config.security.kind == "nosec") {
            security_inst.security_mode = ANJ_DM_SECURITY_NOSEC;
        } else if (server_config.security.kind == "psk") {
            security_inst.security_mode = ANJ_DM_SECURITY_PSK;
            anj_crypto_security_info_t public_key_or_identity{};
            public_key_or_identity.source = ANJ_CRYPTO_DATA_SOURCE_BUFFER;
            public_key_or_identity.info.buffer.data =
                    server_config.security.psk_identity->c_str();
            public_key_or_identity.info.buffer.data_size =
                    server_config.security.psk_identity->size();

            anj_crypto_security_info_t secret_key{};
            secret_key.source = ANJ_CRYPTO_DATA_SOURCE_BUFFER;
            secret_key.info.buffer.data =
                    server_config.security.psk_key->c_str();
            secret_key.info.buffer.data_size =
                    server_config.security.psk_key->size();

            security_inst.public_key_or_identity = public_key_or_identity;
            security_inst.secret_key = secret_key;
        } else {
            log(L_ERROR, "Unsupported security kind: %s",
                server_config.security.kind.c_str());
            return -1;
        }

        if (anj_dm_security_obj_add_instance(&security_obj, &security_inst)) {
            log(L_ERROR, "Failed to add Security Object instance");
            return -1;
        }
        if (!server_config.bootstrap) {
            if (anj_dm_server_obj_add_instance(&server_obj, &server_inst)) {
                log(L_ERROR, "Failed to add Server Object instance");
                return -1;
            }
        }
    }

    if (anj_dm_security_obj_install(&anj, &security_obj)) {
        log(L_ERROR, "Failed to install Security Object");
        return -1;
    }
    if (anj_dm_server_obj_install(&anj, &server_obj)) {
        log(L_ERROR, "Failed to install Server Object");
        return -1;
    }

    static const anj_dm_device_object_init_t device_obj_conf = {
        .manufacturer = "AVSystem",
        .model_number = "Anjay Lite Test App",
        .serial_number = "123456789",
        .firmware_version = "1.0"
    };
    if (anj_dm_device_obj_install(&anj, &device_obj, &device_obj_conf)) {
        log(L_ERROR, "Failed to install Device Object");
        return -1;
    }

    initialized = true;
    return 0;
}

static anj_io_out_entry_t lifetime_send_record;
static anj_send_request_t lifetime_send_request;
static bool lifetime_send_in_progress;

int send_lifetime() {
    if (!initialized) {
        log(L_ERROR, "Client is not initialized");
        return 1;
    }
    if (lifetime_send_in_progress) {
        log(L_ERROR, "Lifetime send is already in progress");
        return 1;
    }

    anj_uri_path_t path{};
    path.ids[0] = 1;
    path.ids[1] = 0;
    path.ids[2] = 1;
    path.uri_len = 3;
    if (anj_dm_res_read(&anj, &path, &lifetime_send_record.value)) {
        log(L_ERROR, "Failed to read Lifetime resource value");
        return 1;
    }

    lifetime_send_record.path = path;
    lifetime_send_record.type = ANJ_DATA_TYPE_UINT;
    lifetime_send_record.timestamp = NAN;

    lifetime_send_request.records = &lifetime_send_record;
    lifetime_send_request.records_cnt = 1;
    lifetime_send_request.content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR;
    lifetime_send_request.finished_handler = [](anj_t *anjay, uint16_t send_id,
                                                int result, void *data) {
        log(L_INFO, "Lifetime send finished with result %d", result);
        lifetime_send_in_progress = false;
    };

    int res = anj_send_new_request(&anj, &lifetime_send_request, nullptr);
    if (res) {
        log(L_ERROR, "Failed to queue Lifetime send request");
        return res;
    }

    log(L_INFO, "Lifetime send request queued");
    lifetime_send_in_progress = true;

    return 0;
}

int get_conn_status() {
    if (!initialized) {
        log(L_ERROR, "Client is not initialized");
        return -1;
    }
    return anj.server_state.conn_status;
}

static void sleep_ms(int ms) {
    struct timespec ts {};
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, nullptr);
}

static void client_loop() {
    if (initialized) {
        anj_core_step(&anj);
    }
    sleep_ms(50);
}

static void client_shutdown() {
    if (!initialized) {
        return;
    }

    log(L_INFO, "Shutting down...");
    int ret;
    do {
        ret = anj_core_shutdown(&anj);
    } while (ret == ANJ_NET_EAGAIN);
    log(L_INFO, "Shutdown complete");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        log(L_ERROR, "Usage: %s <port>", argv[0]);
        return -1;
    }

    ipc_init(argv[1]);

    bool running = true;

    while (running) {
        ipc_loop(running);
        client_loop();
    }

    client_shutdown();
    ipc_shutdown();

    return 0;
}
