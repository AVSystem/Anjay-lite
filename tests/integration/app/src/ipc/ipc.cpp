/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <json.hpp>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../commands.hpp"

#include "ipc.hpp"
#include "tlv.hpp"

using nlohmann::json;

static int client_fd = -1;
static TLV tlv;
std::vector<uint8_t> encoded_messages;

int ipc_init(const char *port_str) {
    const uint16_t port = static_cast<uint16_t>(std::stoi(port_str));

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        return -1;
    }

    sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {
            .s_addr = htonl(INADDR_LOOPBACK)
        }
    };

    if (connect(client_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr))
            < 0) {
        close(client_fd);
        client_fd = -1;
        return -1;
    }

    if (const int flags = fcntl(client_fd, F_GETFL, 0);
            flags < 0 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(client_fd);
        client_fd = -1;
        return -1;
    }

    std::cout << "Connected to Python wrapper" << std::endl;

    return 0;
}

int ipc_loop(bool &running) {
    uint8_t buf[1024] = {};
    const ssize_t n = recv(client_fd, buf, sizeof(buf), 0);

    if (n > 0) {
        tlv.Append(buf, n);
        return 0;
    }

    auto message = tlv.GetFrame();
    if (message.has_value()) {
        Message m = json::parse(message.value());

        json response;
        if (!wrap_map.contains(m.name)) {
            response = { { "error", "unknown command" } };
        } else {
            try {
                json result = wrap_map.at(m.name).fn(m.args);
                response = { { "result", result } };
            } catch (const std::exception &e) {
                response = { { "error", e.what() } };
            }
        }

        encoded_messages.clear();
        TLV::EncodeFrame(response.dump(), encoded_messages);
        send(client_fd, encoded_messages.data(), encoded_messages.size(), 0);

        return 0;
    }

    if (n == 0) {
        running = false;
        return 0;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
    }

    running = false;
    return -1;
}

int ipc_shutdown() {
    return 0;
}
