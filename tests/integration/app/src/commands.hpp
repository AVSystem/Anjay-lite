/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef APP_COMMANDS_HPP_
#define APP_COMMANDS_HPP_

#include <json.hpp>
#include <optional>

#include "utils.hpp"

/**
 * High level type of incoming message
 */
struct Message {
    std::string name;
    nlohmann::json args;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Message, name, args);

////////////////////////////////////////////////////////////////////
/// ALL TYPES USED IN FUNCTION SIGNATURES SHOULD BE DEFINED HERE ///
////////////////////////////////////////////////////////////////////

struct SecurityConfig {
    std::string kind;
    std::optional<std::string> psk_identity;
    std::optional<std::string> psk_key;
    std::optional<std::string> client_cert_path;
    std::optional<std::string> client_key_path;
    std::optional<std::string> server_public_key_path;
    std::optional<std::string> server_name_indication;
    std::optional<int> certificate_usage;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SecurityConfig,
                                                kind,
                                                psk_identity,
                                                psk_key,
                                                client_cert_path,
                                                client_key_path,
                                                server_public_key_path,
                                                server_name_indication,
                                                certificate_usage);

struct CommunicationRetryConfig {
    int retry_count;
    int retry_timer_s;
    int seq_delay_timer_s;
    int seq_retry_count;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CommunicationRetryConfig,
                                                retry_count,
                                                retry_timer_s,
                                                seq_delay_timer_s,
                                                seq_retry_count);

struct ServerConfig {
    bool bootstrap;
    std::string uri;
    SecurityConfig security;
    std::optional<int> lifetime;
    std::optional<CommunicationRetryConfig> communication_retry;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        ServerConfig, bootstrap, uri, security, lifetime, communication_retry);

struct UdpTxParamsConfig {
    int ack_timeout_s;
    double ack_random_factor;
    int max_retransmit;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(UdpTxParamsConfig,
                                                ack_timeout_s,
                                                ack_random_factor,
                                                max_retransmit);

struct BootstrapConfig {
    int retry_count;
    int retry_timeout_s;
    std::optional<int> bootstrap_timeout_s;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(BootstrapConfig,
                                                retry_count,
                                                retry_timeout_s,
                                                bootstrap_timeout_s);

struct Config {
    std::string endpoint;
    std::optional<std::vector<std::string>> trust_store;
    std::vector<ServerConfig> servers;
    std::optional<UdpTxParamsConfig> udp_tx_params;
    std::optional<BootstrapConfig> bootstrap_config;
    std::optional<bool> queue_mode;
    std::optional<int> queue_mode_timeout_s;
    // When true, the FOTA mock (see fota_mock.c) defers reporting a
    // successful Update Result until after a client restart (i.e. a
    // De-register/Register cycle triggered via anj_core_restart), mimicking
    // a device reboot. When false or absent, the mock reports success
    // immediately, without restarting the client.
    std::optional<bool> fota_reboot_required;
    // Optional PSK credentials used by the FOTA mock (see fota_mock.c) to
    // download firmware packages over CoAPs (DTLS), independently of any
    // credentials used for the (nosec, in these tests) LwM2M Server
    // connection. Both fields must be set to enable CoAPs downloads; plain
    // coap:// downloads work regardless of whether these are set.
    std::optional<std::string> fota_psk_identity;
    std::optional<std::string> fota_psk_key;
    // Optional UDP transmission parameters for the FOTA mock's CoAP
    // downloader (see fota_mock.c), independent of udp_tx_params above
    // (which only applies to the LwM2M Server connection). Used by tests to
    // make a stalled Pull-mode download fail quickly, e.g. by setting
    // max_retransmit to 0.
    std::optional<UdpTxParamsConfig> fota_coap_udp_tx_params;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Config,
                                                trust_store,
                                                endpoint,
                                                servers,
                                                udp_tx_params,
                                                bootstrap_config,
                                                queue_mode,
                                                queue_mode_timeout_s,
                                                fota_reboot_required,
                                                fota_psk_identity,
                                                fota_psk_key,
                                                fota_coap_udp_tx_params);

struct SendResourceConfig {
    std::string path;
    std::string type;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SendResourceConfig, path, type);

struct SendConfig {
    std::string content_format;
    std::vector<SendResourceConfig> resources;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SendConfig, content_format, resources);

////////////////////////////////////////////////////////////////////
///              ALL FUNCTIONS SIGNATURES HERE                   ///
////////////////////////////////////////////////////////////////////

int init(const Config &config);
int init_persistence(const std::string &endpoint_name);
int send_resources(const SendConfig &config);
int get_conn_status();
int disable_server(int timeout_s);
int restart_client();
int set_manufacturer(const std::string &value);
int set_test_value(int iid, double value);
int remove_test_object();
void send_update();

int add_monotonic_time_offset(int offset_ms);
int add_real_time_offset(int offset_ms);

////////////////////////////////////////////////////////////////////
///              REGISTER API FUNCTION HERE                      ///
////////////////////////////////////////////////////////////////////
inline std::unordered_map<std::string, utils::AutoWrap> wrap_map = {
    { "init", init },
    { "init_persistence", init_persistence },
    { "send", send_resources },
    { "get_conn_status", get_conn_status },
    { "disable_server", disable_server },
    { "restart_client", restart_client },
    { "set_manufacturer", set_manufacturer },
    { "set_test_value", set_test_value },
    { "remove_test_object", remove_test_object },
    { "add_monotonic_time_offset", add_monotonic_time_offset },
    { "add_real_time_offset", add_real_time_offset },
    { "send_update", send_update }

};

#endif // APP_COMMANDS_HPP_
