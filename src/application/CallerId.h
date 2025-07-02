#pragma once

#include "common/Uuid.h"

namespace TUI::Application
{
    struct CallerId
    {
        Common::Uuid userId;
        Common::Uuid connectionId;
        auto operator<=>(const CallerId&) const = default;    
    };
}

namespace std
{
    template<>
    struct hash<TUI::Application::CallerId>
    {
        size_t operator()(const TUI::Application::CallerId& callerId) const noexcept
        {
            size_t hash1 = callerId.userId.Hash();
            size_t hash2 = callerId.connectionId.Hash();
            return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
        }
    };
}
