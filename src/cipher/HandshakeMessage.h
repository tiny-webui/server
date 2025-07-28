#pragma once

#include <map>
#include <vector>
#include <optional>
#include <cstdint>
#include "common/Tlv.h"

namespace TUI::Cipher::HandshakeMessage
{
    enum class Type : uint8_t
    {
        ProtocolType = 0,
        CipherMessage = 1,
        KeyIndex = 2,
    };
    using Message = TUI::Common::Tlv<Type>;
}

