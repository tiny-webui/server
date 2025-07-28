#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "HandshakeMessage.h"

namespace TUI::Cipher
{
    class IAuthenticationPeer
    {
    public:
        constexpr static size_t KEY_SIZE = 32;

        virtual ~IAuthenticationPeer() = default;

        virtual std::optional<HandshakeMessage::Message> GetNextMessage(
            const std::optional<HandshakeMessage::Message>& peerMessage) = 0;
        virtual bool IsHandshakeComplete() = 0;
        virtual std::array<uint8_t, KEY_SIZE> GetClientKey() = 0;
        virtual std::array<uint8_t, KEY_SIZE> GetServerKey() = 0;
    };
}
