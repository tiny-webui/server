#include <iostream>
#include <thread>
#include <chrono>
#include "cipher/BruteForceLimiter.h"
#include "Utility.h"

using namespace TUI::Cipher;

/**
 * @attention This test may not pass on systems where the CPU is too slow or too busy.
 * But this is not likely to happen.
 */

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    std::string username = "testuser";

    BruteForceLimiter limiter(3, 100, 500);
    for (int i = 0; i < 3; i++)
    {
        AssertWithMessage(!limiter.IsBlocked(username), "Username should not be blocked yet");
        limiter.LogInvalidLogin(username);
    }
    AssertWithMessage(limiter.IsBlocked(username), "Username should be blocked after 3 invalid attempts");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    AssertWithMessage(limiter.IsBlocked(username), "Username should still be blocked after a short wait");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    AssertWithMessage(!limiter.IsBlocked(username), "Username should not be blocked after the block time has passed");
    for (int i = 0; i < 3; i++)
    {
        AssertWithMessage(!limiter.IsBlocked(username), "Username should not be blocked yet");
        limiter.LogInvalidLogin(username);
    }
    AssertWithMessage(limiter.IsBlocked(username), "Username should be blocked after 3 more invalid attempts");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    AssertWithMessage(limiter.IsBlocked(username), "Username should still be blocked after a short wait");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    AssertWithMessage(!limiter.IsBlocked(username), "Username should not be blocked after the block time has passed");
    limiter.LogValidLogin(username);
    AssertWithMessage(!limiter.IsBlocked(username), "Username should not be blocked after a valid login");
    for (int i = 0; i < 3; i++)
    {
        AssertWithMessage(!limiter.IsBlocked(username), "Username should not be blocked yet");
        limiter.LogInvalidLogin(username);
    }
    AssertWithMessage(limiter.IsBlocked(username), "Username should be blocked after 3 invalid attempts");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    AssertWithMessage(!limiter.IsBlocked(username), "Valid login should reset the block state");

    return 0;
}

