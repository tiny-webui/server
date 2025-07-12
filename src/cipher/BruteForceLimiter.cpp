#include "common/Utilities.h"
#include "BruteForceLimiter.h"

using namespace TUI::Cipher;

BruteForceLimiter::BruteForceLimiter(size_t trialsAllowedEachWindow, int64_t initialBlockTimeMs, int64_t maxBlockTimeMs)
    : _trialsAllowedEachWindow(trialsAllowedEachWindow),
      _initialBlockTimeMs(initialBlockTimeMs),
      _maxBlockTimeMs(maxBlockTimeMs)
{
}

void BruteForceLimiter::LogInvalidLogin(const std::string& username)
{
    auto item = _usernameStates.find(username);
    if (item == _usernameStates.end())
    {
        _usernameStates.emplace(username, UsernameState{});
        item = _usernameStates.find(username);
        if (item == _usernameStates.end())
        {
            throw std::runtime_error("Failed to create username state for " + username);
        }
    }
    auto& state = item->second;
    int64_t currentTimeMs = Common::Utilities::GetMonotonicTimestamp();
    if (state.nextValidTimeMs > currentTimeMs)
    {
        /** Still blocked. No need to update the timeout. */
        return;
    }
    state.trials++;
    if (state.trials < _trialsAllowedEachWindow)
    {
        return;
    }
    /** Update the block timeout */
    if (state.blockTimeMs == 0)
    {
        state.blockTimeMs = _initialBlockTimeMs;
    }
    else
    {
        state.blockTimeMs = static_cast<int64_t>(static_cast<double>(state.blockTimeMs) * BLOCK_TIME_MULTIPLIER);
        if (state.blockTimeMs > _maxBlockTimeMs)
        {
            state.blockTimeMs = _maxBlockTimeMs;
        }
    }
    state.nextValidTimeMs = currentTimeMs + state.blockTimeMs;
    /** Reset the trials count */
    state.trials = 0;
}

bool BruteForceLimiter::LogValidLogin(const std::string& username)
{
    auto item = _usernameStates.find(username);
    if (item == _usernameStates.end())
    {
        return false;
    }
    /** This means there was login attempts that triggered the block. */
    auto wasUnderAttack = item->second.blockTimeMs > 0;
    _usernameStates.erase(item);
    return wasUnderAttack;
}

bool BruteForceLimiter::IsBlocked(const std::string& username) const
{
    auto item = _usernameStates.find(username);
    if (item == _usernameStates.end())
    {
        return false;
    }
    if (item->second.nextValidTimeMs == 0)
    {
        return false;
    }
    int64_t currentTimeMs = Common::Utilities::GetMonotonicTimestamp();
    return currentTimeMs < item->second.nextValidTimeMs;
}

