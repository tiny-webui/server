#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include "HandshakeMessage.h"

namespace TUI::Cipher
{
    class IAuthenticationPeer
    {
    public:
        virtual ~IAuthenticationPeer() = default;

        virtual std::optional<HandshakeMessage> GetNextMessage(
            const std::optional<HandshakeMessage>& peerMessage) = 0;
        virtual bool IsHandshakeComplete() = 0;
        virtual std::vector<uint8_t> GetClientKey() = 0;
        virtual std::vector<uint8_t> GetServerKey() = 0;
    };
}
