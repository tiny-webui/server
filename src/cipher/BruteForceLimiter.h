#pragma once

#include <unordered_map>
#include <string>

/**
 * The brute force limiter silently limits the frequency of brute force attacks on valid usernames.
 * To avoid leaking information about the validity of usernames, 
 * the user should use the correct salt and false w0 and L for the handshake.
 * So the attacker needs to do the computation no matter if there was a username or a password error.
 */

namespace TUI::Cipher
{
    class BruteForceLimiter
    {
    public:
        static constexpr double BLOCK_TIME_MULTIPLIER = 2.0;

        BruteForceLimiter(size_t trialsAllowedEachWindow, int64_t initialBlockTimeMs, int64_t maxBlockTimeMs);

        /**
         * @brief Log an invalid login for the given username.
         * If the number of trials exceeds the limit, the username will be blocked for a given time.
         * 
         * @param username 
         */
        void LogInvalidLogin(const std::string& username);
        /**
         * @brief Log a valid login for the given username.
         * This will reset the trial count for the username.
         * 
         * @param username
         * @return true if the username was blocked. This should be presented to the user to take actions.
         */
        bool LogValidLogin(const std::string& username);
        /**
         * @brief Check if the given username is blocked.
         * 
         * @param username 
         * @return true 
         * @return false 
         */
        bool IsBlocked(const std::string& username) const;
    private:
        struct UsernameState
        {
            size_t trials{0};
            int64_t blockTimeMs{0};
            int64_t nextValidTimeMs{0};
        };

        size_t _trialsAllowedEachWindow;
        int64_t _initialBlockTimeMs;
        int64_t _maxBlockTimeMs;
        std::unordered_map<std::string, UsernameState> _usernameStates{};
    };
}
