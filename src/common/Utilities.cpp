#include "Utilities.h"

using namespace TUI::Common;

int64_t Utilities::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}
