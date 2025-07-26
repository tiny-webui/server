#pragma once

#include <cstdint>

namespace TUI::Application
{
    enum class RegisterTlvType : uint8_t
    {
        Username = 0,
        Salt = 1,
        w0 = 2,
        L = 3,
    };
}
