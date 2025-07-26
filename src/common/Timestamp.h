#pragma once

#include <cstdint>

namespace TUI::Common::Timestamp
{
    int64_t GetWallClock();
    int64_t GetMonotonic();
}
