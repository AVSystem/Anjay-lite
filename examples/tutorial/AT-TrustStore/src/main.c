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

#define log(...) anj_log(example_log, __VA_ARGS__)

#define CLIENT_KEY_PATH "client_key.der"
#define CLIENT_CERT_PATH "client_cert.der"
#define SERVER_LEAF_CERT_PATH "leaf_cert.pem"
#define TRUST_STORE_CA_PATH "ca_cert.pem"

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
// The client certificate, client private key and server leaf certificate are
// referenced through external crypto storage identities.
static int install_security_obj(anj_t *anj,
                                anj_dm_security_obj_t *security_obj) {
    anj_crypto_security_info_t client_cert;
    anj_crypto_security_info_t client_key;
    anj_crypto_security_info_t server_cert;
    static anj_net_certificate_usage_t certificate_usage =
            ANJ_NET_CERTIFICATE_SERVICE_CERTIFICATE_CONSTRAINT;

    security_info_from_path(&client_cert, CLIENT_CERT_PATH);
    security_info_from_path(&client_key, CLIENT_KEY_PATH);
    security_info_from_path(&server_cert, SERVER_LEAF_CERT_PATH);

    anj_dm_security_instance_init_t security_inst = {
        .ssid = 1,
        .server_uri = "coaps://eu.iot.avsystem.cloud:5684",
        .security_mode = ANJ_DM_SECURITY_CERTIFICATE,
        .public_key_or_identity = client_cert,
        .secret_key = client_key,
        .server_public_key = server_cert,
        .certificate_usage = &certificate_usage
    };
    anj_dm_security_obj_init(security_obj);
    if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
            || anj_dm_security_obj_install(anj, security_obj)) {
        return -1;
    }

    log(L_INFO,
        "Using security credentials from crypto storage identities: '%s', "
        "'%s', '%s'",
        CLIENT_CERT_PATH, CLIENT_KEY_PATH, SERVER_LEAF_CERT_PATH);
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
    anj_crypto_security_info_t trust_store_ca[1];

    security_info_from_path(&trust_store_ca[0], TRUST_STORE_CA_PATH);
    anj_net_trust_store_t trust_store = {
        .ca_certs = trust_store_ca,
        .ca_certs_count = 1
    };

    anj_configuration_t config = {
        .endpoint_name = argv[1],
        .trust_store = &trust_store
    };
    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    log(L_INFO, "Using trust store CA from crypto storage identity: '%s'",
        TRUST_STORE_CA_PATH);

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj)
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
