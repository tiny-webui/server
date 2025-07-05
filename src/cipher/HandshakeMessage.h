#pragma once

#include <map>
#include <vector>
#include <optional>
#include <cstdint>

namespace TUI::Cipher
{
    class HandshakeMessage
    {
    public:
        enum class Type
        {
            ProtocolType = 0,
            CipherMessage = 1,
            KeyIndex = 2,
        };

        explicit HandshakeMessage(const std::map<Type, std::vector<uint8_t>>& elements);

        explicit HandshakeMessage(const std::vector<uint8_t>& data);

        std::vector<uint8_t> Serialize() const;

        std::optional<std::vector<uint8_t>> GetElement(Type type) const;
    private:
        /** DO NOT use unordered_map. The order is crucial for a stable serialize output */
        std::map<Type, std::vector<uint8_t>> _elements;
    };
}

