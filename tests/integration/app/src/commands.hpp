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
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SecurityConfig,
                                                kind,
                                                psk_identity,
                                                psk_key);

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
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(BootstrapConfig,
                                                retry_count,
                                                retry_timeout_s);

struct Config {
    std::string endpoint;
    std::vector<ServerConfig> servers;
    std::optional<UdpTxParamsConfig> udp_tx_params;
    std::optional<BootstrapConfig> bootstrap_config;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        Config, endpoint, servers, udp_tx_params, bootstrap_config);

////////////////////////////////////////////////////////////////////
///              ALL FUNCTIONS SIGNATURES HERE                   ///
////////////////////////////////////////////////////////////////////

int init(const Config &config);
int send_lifetime();
int get_conn_status();

////////////////////////////////////////////////////////////////////
///              REGISTER API FUNCTION HERE                      ///
////////////////////////////////////////////////////////////////////
inline std::unordered_map<std::string, utils::AutoWrap> wrap_map = {
    { "init", init },
    { "send_lifetime", send_lifetime },
    { "get_conn_status", get_conn_status }
};

#endif // APP_COMMANDS_HPP_
