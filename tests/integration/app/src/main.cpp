/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */
#include <array>
#include <cstdlib>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/device_object.h>
#include <anj/dm/security_object.h>
#include <anj/dm/server_object.h>
#include <anj/log.h>
#include <anj/lwm2m_send.h>
#ifdef ANJ_WITH_PERSISTENCE
#    include <anj/persistence.h>
#endif

#include "commands.hpp"
#include "fota_mock.h"
#include "ipc/ipc.hpp"
#include "test_object.h"

#define log(...) anj_log(test_app, __VA_ARGS__)

static bool initialized;
static Config anjay_config;

static anj_t anj;
static anj_dm_device_obj_t device_obj;
static anj_dm_server_obj_t server_obj;
static anj_dm_security_obj_t security_obj;
static char manufacturer[150] = "AVSystem";

#ifdef ANJ_WITH_PERSISTENCE
#    define PERSISTENCE_OBJS_FILE "persistence_objs.bin"
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

typedef struct {
    anj_dm_server_obj_t *server_obj;
    anj_dm_security_obj_t *security_obj;
} persistent_objects_t;

persistent_objects_t persistent_objects;

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
        auto *callback_arg = static_cast<const persistent_objects_t *>(arg);
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
#endif // ANJ_WITH_PERSISTENCE

static anj_conn_status_t current_conn_status = ANJ_CONN_STATUS_INITIAL;

static void connection_status_changed(void *arg,
                                      anj_t *anj_ptr,
                                      anj_conn_status_t conn_status) {
    (void) arg;
    (void) anj_ptr;
    current_conn_status = conn_status;
    fota_mock_on_conn_status_changed(conn_status);
}

#ifdef ANJ_WITH_SECURITY
static std::vector<anj_crypto_security_info_t> ca_certs;
static std::vector<std::vector<uint8_t>> cert_buf;

#    if !defined(ANJ_WITH_EXTERNAL_CRYPTO_STORAGE)
static int read_file_into_buffer(const std::string &path,
                                 std::vector<uint8_t> &out_buf,
                                 size_t out_buf_capacity) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) {
        log(L_ERROR, "Failed to open %s: %s", path.c_str(), strerror(errno));
        return -1;
    }
    out_buf.resize(out_buf_capacity);
    size_t total = fread(out_buf.data(), 1, out_buf_capacity, f);
    if (ferror(f)) {
        log(L_ERROR, "Failed to read %s", path.c_str());
        fclose(f);
        return -1;
    }
    // If the buffer is filled, ensure the file does not contain more data.
    if (total == out_buf_capacity) {
        int c = fgetc(f);
        if (c != EOF) {
            log(L_ERROR, "%s too large (>%lu bytes)", path.c_str(),
                (unsigned long) out_buf_capacity);
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    out_buf.resize(total);
    return 0;
}
#    endif // !ANJ_WITH_EXTERNAL_CRYPTO_STORAGE

static int security_info_from_file(const std::string &path,
                                   std::vector<uint8_t> &buffer,
                                   anj_crypto_security_info_t &out_info,
                                   const char *description) {
#    if defined(ANJ_WITH_EXTERNAL_CRYPTO_STORAGE)
    (void) buffer;
    (void) description;

    out_info = {};
    out_info.source = ANJ_CRYPTO_DATA_SOURCE_EXTERNAL;
    out_info.info.external.identity = path.c_str();
#    else
    if (read_file_into_buffer(path, buffer, 4096)) {
        log(L_ERROR, "Failed to read %s", description);
        return -1;
    }

    out_info = {};
    out_info.source = ANJ_CRYPTO_DATA_SOURCE_BUFFER;
    out_info.info.buffer.data = buffer.data();
    out_info.info.buffer.data_size = buffer.size();
#    endif

    return 0;
}
#endif // ANJ_WITH_SECURITY

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
#ifdef ANJ_WITH_BOOTSTRAP
    if (anjay_config.bootstrap_config.has_value()) {
        const auto &bootstrap_cfg = anjay_config.bootstrap_config.value();
        core_config.bootstrap_retry_count =
                (uint16_t) bootstrap_cfg.retry_count;
        core_config.bootstrap_retry_timeout =
                anj_time_duration_new(bootstrap_cfg.retry_timeout_s,
                                      ANJ_TIME_UNIT_S);
        if (anjay_config.bootstrap_config->bootstrap_timeout_s.has_value()) {
            core_config.bootstrap_timeout = anj_time_duration_new(
                    bootstrap_cfg.bootstrap_timeout_s.value(), ANJ_TIME_UNIT_S);
        }
    }
#endif // ANJ_WITH_BOOTSTRAP
    core_config.endpoint_name = anjay_config.endpoint.c_str();
    core_config.udp_tx_params = &udp_tx_params;
    core_config.connection_status_cb = connection_status_changed;

#ifdef ANJ_WITH_CERTIFICATES
    anj_net_trust_store_t trust_store;
    if (anjay_config.trust_store.has_value()) {
        size_t ca_certs_count = anjay_config.trust_store.value().size();
        ca_certs.resize(ca_certs_count);
        cert_buf.resize(ca_certs_count);
        ca_certs_count = 0;
        for (const auto &cert : anjay_config.trust_store.value()) {
            if (security_info_from_file(cert,
                                        cert_buf[ca_certs_count],
                                        ca_certs[ca_certs_count],
                                        "ca certificate")) {
                return -1;
            }
            ++ca_certs_count;
        }
        assert(anjay_config.trust_store.value().size() == ca_certs_count);
        trust_store.ca_certs = ca_certs.data();
        trust_store.ca_certs_count = ca_certs_count;
        core_config.trust_store = &trust_store;
    }
#endif // ANJ_WITH_CERTIFICATES

    if (anjay_config.queue_mode.has_value()) {
        core_config.queue_mode_enabled = anjay_config.queue_mode.value();
    }
    if (anjay_config.queue_mode_timeout_s.has_value()) {
        core_config.queue_mode_timeout =
                anj_time_duration_new(anjay_config.queue_mode_timeout_s.value(),
                                      ANJ_TIME_UNIT_S);
    }

#ifdef ANJ_WITH_PERSISTENCE
    persistent_objects.security_obj = &security_obj;
    persistent_objects.server_obj = &server_obj;
    core_config.connection_status_cb = connection_status_callback;
    core_config.connection_status_cb_arg = &persistent_objects;
#endif

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
        std::vector<uint8_t> client_cert_buf;
        std::vector<uint8_t> client_key_buf;
        std::vector<uint8_t> server_cert_buf;
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
        }
#ifdef ANJ_WITH_SECURITY
        else if (server_config.security.kind == "psk") {
            if (!server_config.security.psk_identity.has_value()
                    || !server_config.security.psk_key.has_value()) {
                throw std::runtime_error("PSK identity and key must be "
                                         "provided for PSK security");
            }

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
        } else if (server_config.security.kind == "cert") {
            security_inst.security_mode = ANJ_DM_SECURITY_CERTIFICATE;

            if (server_config.security.client_cert_path.has_value()
                    && security_info_from_file(
                               server_config.security.client_cert_path.value(),
                               client_cert_buf,
                               security_inst.public_key_or_identity,
                               "client certificate")) {
                return -1;
            }

            if (server_config.security.client_key_path.has_value()
                    && security_info_from_file(
                               server_config.security.client_key_path.value(),
                               client_key_buf,
                               security_inst.secret_key,
                               "client key")) {
                return -1;
            }

            if (server_config.security.server_public_key_path.has_value()
                    && security_info_from_file(
                               server_config.security.server_public_key_path
                                       .value(),
                               server_cert_buf,
                               security_inst.server_public_key,
                               "server certificate")) {
                return -1;
            }

            if (server_config.security.server_name_indication.has_value()) {
                security_inst.server_name_indication =
                        server_config.security.server_name_indication->c_str();
            }
            if (server_config.security.certificate_usage.has_value()) {
                security_inst.certificate_usage =
                        (anj_net_certificate_usage_t *) &server_config.security
                                .certificate_usage.value();
            }
        }
#endif // ANJ_WITH_SECURITY
        else {
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
        .manufacturer = manufacturer,
        .model_number = "Anjay Lite Test App",
        .serial_number = "123456789",
        .firmware_version = "1.0"
    };
    if (anj_dm_device_obj_install(&anj, &device_obj, &device_obj_conf)) {
        log(L_ERROR, "Failed to install Device Object");
        return -1;
    }
    if (test_object_install(&anj)) {
        log(L_ERROR, "Failed to install test Object");
        return -1;
    }
    fota_mock_config_t fota_config{};
    fota_config.reboot_required =
            anjay_config.fota_reboot_required.value_or(false);
    if (anjay_config.fota_psk_identity.has_value()) {
        fota_config.psk_identity = anjay_config.fota_psk_identity->c_str();
        fota_config.psk_identity_len = anjay_config.fota_psk_identity->size();
    }
    if (anjay_config.fota_psk_key.has_value()) {
        fota_config.psk_key = anjay_config.fota_psk_key->c_str();
        fota_config.psk_key_len = anjay_config.fota_psk_key->size();
    }
    anj_exchange_udp_tx_params_t fota_coap_udp_tx_params =
            ANJ_EXCHANGE_UDP_TX_PARAMS_DEFAULT;
    if (anjay_config.fota_coap_udp_tx_params.has_value()) {
        const auto &udp_cfg = anjay_config.fota_coap_udp_tx_params.value();
        fota_coap_udp_tx_params.ack_timeout =
                anj_time_duration_new(udp_cfg.ack_timeout_s, ANJ_TIME_UNIT_S);
        fota_coap_udp_tx_params.ack_random_factor = udp_cfg.ack_random_factor;
        fota_coap_udp_tx_params.max_retransmit =
                (uint16_t) udp_cfg.max_retransmit;
        fota_config.coap_udp_tx_params = &fota_coap_udp_tx_params;
    }
    if (fota_mock_install(&anj, &fota_config)) {
        log(L_ERROR, "Failed to install mocked Firmware Update Object");
        return -1;
    }

    initialized = true;
    return 0;
}

#ifdef ANJ_WITH_PERSISTENCE
int init_persistence(const std::string &endpoint_name) {
    if (initialized) {
        log(L_ERROR, "Client is already initialized");
        return -1;
    }

    anjay_config.endpoint = endpoint_name;

    anj_configuration_t core_config = {
        .endpoint_name = anjay_config.endpoint.c_str(),
        .connection_status_cb = connection_status_callback,
        .connection_status_cb_arg = &persistent_objects
    };
    if (anj_core_init(&anj, &core_config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    anj_dm_security_obj_init(&security_obj);
    anj_dm_server_obj_init(&server_obj);
    FILE *file = fopen(PERSISTENCE_OBJS_FILE, "r");

    if (file) {
        anj_persistence_context_t ctx =
                anj_persistence_restore_context_create(persistence_read, file);
        if (restore_security_obj(&anj, &security_obj, &ctx)
                || restore_server_obj(&server_obj, &ctx)) {
            anj_dm_security_obj_init(&security_obj);
            anj_dm_server_obj_init(&server_obj);
            remove(PERSISTENCE_OBJS_FILE);
        }
        fclose(file);
    } else {
        log(L_ERROR, "Failed to open persistence file");
        return -1;
    }

    anj_dm_device_object_init_t device_obj_conf = {
        .firmware_version = "0.1"
    };

    if (anj_dm_device_obj_install(&anj, &device_obj, &device_obj_conf)
            || anj_dm_security_obj_install(&anj, &security_obj)
            || anj_dm_server_obj_install(&anj, &server_obj)) {
        return -1;
    }

    initialized = true;
    return 0;
}
#else  // ANJ_WITH_PERSISTENCE
int init_persistence(const std::string &endpoint_name) {
    log(L_ERROR, "Persistence feature is not enabled in this build");
    return -1;
}
#endif // ANJ_WITH_PERSISTENCE

#ifdef ANJ_WITH_LWM2M_SEND

struct SendOperation {
    bool finished = false;
    std::vector<anj_io_out_entry_t> records;
    anj_send_request_t request;
};

static std::vector<std::unique_ptr<SendOperation>> send_operations;

static int parse_send_path(const std::string &path, anj_uri_path_t &out_path) {
    unsigned oid;
    unsigned iid;
    unsigned rid;
    unsigned riid;
    char trailing_char;

    int uri_len;
    if (sscanf(path.c_str(), "/%u/%u/%u/%u%c", &oid, &iid, &rid, &riid,
               &trailing_char)
            == 4) {
        uri_len = 4;
    } else if (sscanf(path.c_str(), "/%u/%u/%u%c", &oid, &iid, &rid,
                      &trailing_char)
               == 3) {
        uri_len = 3;
    } else {
        return -1;
    }

    out_path = {};
    out_path.ids[0] = (anj_oid_t) oid;
    out_path.ids[1] = (anj_iid_t) iid;
    out_path.ids[2] = (anj_rid_t) rid;
    if (uri_len == 4) {
        out_path.ids[3] = (anj_riid_t) riid;
    }
    out_path.uri_len = (size_t) uri_len;
    return 0;
}

static int parse_send_data_type(const std::string &type,
                                anj_data_type_t &out_type) {
    if (type == "bytes") {
        out_type = ANJ_DATA_TYPE_BYTES;
    } else if (type == "string") {
        out_type = ANJ_DATA_TYPE_STRING;
    } else if (type == "int") {
        out_type = ANJ_DATA_TYPE_INT;
    } else if (type == "uint") {
        out_type = ANJ_DATA_TYPE_UINT;
    } else if (type == "double") {
        out_type = ANJ_DATA_TYPE_DOUBLE;
    } else if (type == "bool") {
        out_type = ANJ_DATA_TYPE_BOOL;
    } else if (type == "objlnk") {
        out_type = ANJ_DATA_TYPE_OBJLNK;
    } else if (type == "time") {
        out_type = ANJ_DATA_TYPE_TIME;
    } else {
        return -1;
    }
    return 0;
}

static int make_send_record(const SendResourceConfig &resource,
                            anj_io_out_entry_t &out_record) {
    out_record = {};
    if (parse_send_path(resource.path, out_record.path)) {
        log(L_ERROR, "Invalid Send resource path: %s", resource.path.c_str());
        return -1;
    }
    if (anj_dm_res_read(&anj, &out_record.path, &out_record.value)) {
        log(L_ERROR, "Failed to read Send resource %s", resource.path.c_str());
        return -1;
    }
    if (parse_send_data_type(resource.type, out_record.type)) {
        log(L_ERROR, "Invalid Send resource type: %s", resource.type.c_str());
        return -1;
    }
    out_record.timestamp = NAN;
    return 0;
}

int send_resources(const SendConfig &config) {
    if (!initialized) {
        log(L_ERROR, "Client is not initialized");
        return 1;
    }

    auto operation = std::make_unique<SendOperation>();
    operation->records.resize(config.resources.size());
    operation->finished = false;
    for (size_t i = 0; i < config.resources.size(); ++i) {
        if (make_send_record(config.resources[i], operation->records[i])) {
            return 1;
        }
    }

    if (config.content_format == "senml_cbor") {
        operation->request.content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR;
    }
#    ifdef ANJ_WITH_LWM2M_CBOR
    else if (config.content_format == "lwm2m_cbor") {
        operation->request.content_format = ANJ_SEND_CONTENT_FORMAT_LWM2M_CBOR;
    }
#    endif // ANJ_WITH_LWM2M_CBOR
    else {
        log(L_ERROR, "Unsupported Send content format: %s",
            config.content_format.c_str());
        return 1;
    }

    operation->request.records = operation->records.data();
    operation->request.records_cnt = config.resources.size();
    operation->request.data = operation.get();
    operation->request.finished_handler = [](anj_t *, uint16_t send_id,
                                             int result, void *data) {
        log(L_INFO, "Send %u finished with result %d", (unsigned) send_id,
            result);
        static_cast<SendOperation *>(data)->finished = true;
    };
    int res = anj_send_new_request(&anj, &operation->request, nullptr);
    if (res) {
        log(L_ERROR, "Failed to queue Send request %d", res);
        return res;
    }

    send_operations.push_back(std::move(operation));
    log(L_INFO, "Send request queued");
    return 0;
}

#else  // ANJ_WITH_LWM2M_SEND
int send_resources(const SendConfig &) {
    log(L_ERROR, "LwM2M Send feature is not enabled in this build");
    return 1;
}
#endif // ANJ_WITH_LWM2M_SEND

int get_conn_status() {
    if (!initialized) {
        log(L_ERROR, "Client is not initialized");
        return -1;
    }
    return current_conn_status;
}

int disable_server(int timeout_s) {
    if (!initialized) {
        log(L_ERROR, "Client is not initialized");
        return -1;
    }

    anj_core_disable_server(&anj,
                            anj_time_duration_new(timeout_s, ANJ_TIME_UNIT_S));
    return 0;
}

int restart_client() {
    if (!initialized) {
        log(L_ERROR, "Client is not initialized");
        return -1;
    }
    anj_core_restart(&anj);
    return 0;
}

int set_manufacturer(const std::string &value) {
    if (!initialized || value.size() >= sizeof(manufacturer)
            || anj_core_ongoing_operation(&anj)) {
        return -1;
    }
    memcpy(manufacturer, value.c_str(), value.size() + 1);
    anj_uri_path_t path{};
    path.ids[0] = 3;
    path.ids[1] = 0;
    path.ids[2] = 0;
    path.uri_len = 3;
    anj_core_data_model_changed(&anj, &path,
                                ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
    return 0;
}

int set_test_value(int iid, double value) {
    if (!initialized) {
        log(L_ERROR, "Client is not initialized");
        return -1;
    }
    if (iid < 0 || iid > UINT16_MAX) {
        log(L_ERROR, "Invalid test Object Instance ID");
        return -1;
    }
    if (anj_core_ongoing_operation(&anj)) {
        log(L_ERROR, "A data model operation is in progress");
        return -1;
    }
    return test_object_set_value(&anj, (anj_iid_t) iid, value);
}

int remove_test_object() {
    if (!initialized || anj_core_ongoing_operation(&anj)) {
        return -1;
    }
    return anj_dm_remove_obj(&anj, 1234);
}

void send_update() {
    anj_core_request_update(&anj);
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
        fota_mock_process();
#ifdef ANJ_WITH_LWM2M_SEND
        send_operations.erase(std::remove_if(send_operations.begin(),
                                             send_operations.end(),
                                             [](const auto &operation) {
                                                 return operation->finished;
                                             }),
                              send_operations.end());
#endif // ANJ_WITH_LWM2M_SEND
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
