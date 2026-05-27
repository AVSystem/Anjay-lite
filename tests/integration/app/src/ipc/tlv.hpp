/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef IPC_TLV_HPP
#define IPC_TLV_HPP

#include <string>
#include <vector>

#define SOF_BYTE 0xF7

class TLV {
public:
    void Append(const uint8_t *data, const size_t size) {
        m_Buffer.insert(m_Buffer.end(), data, data + size);
    }

    std::optional<std::string> GetFrame() {
        constexpr size_t HEADER_SIZE = 5;

        if (m_Buffer.size() < HEADER_SIZE) {
            return {};
        }

        const auto sof_byte = m_Buffer[0];
        if (sof_byte != SOF_BYTE) {
            throw std::runtime_error("invalid TLV SOF byte");
        }

        const uint32_t length =
                (uint32_t(m_Buffer[1]) << 24) | (uint32_t(m_Buffer[2]) << 16)
                | (uint32_t(m_Buffer[3]) << 8) | uint32_t(m_Buffer[4]);

        if (m_Buffer.size() < HEADER_SIZE + length) {
            return {};
        }

        auto value = std::string(m_Buffer.begin() + HEADER_SIZE,
                                 m_Buffer.begin() + HEADER_SIZE + length);

        m_Buffer.erase(m_Buffer.begin(),
                       m_Buffer.begin() + HEADER_SIZE + length);
        return value;
    }

    void Clear() {
        m_Buffer.clear();
    }

    static void EncodeFrame(const std::string &message,
                            std::vector<uint8_t> &out_bytes) {
        out_bytes.push_back(SOF_BYTE);
        auto length = static_cast<uint32_t>(message.size());
        for (int i = 3; i >= 0; i--) {
            out_bytes.push_back((length >> (i * 8)) & 0xFF);
        }
        out_bytes.insert(out_bytes.end(), message.begin(), message.end());
    }

private:
    std::vector<uint8_t> m_Buffer;
};

#endif // IPC_TLV_HPP
