#pragma once

#include <array>
#include <string>
#include <cstdint>

namespace TUI::Cipher::Spake2p
{
    constexpr uint32_t ARGON2ID_LANES = 4;
    constexpr uint32_t ARGON2ID_MEM_COST_KiB = 65536;
    constexpr uint32_t ARGON2ID_ITERATIONS = 3;

    struct RegistrationResult
    {
        std::array<uint8_t, 32> w0;
        std::array<uint8_t, 32> w1;
        std::array<uint8_t, 16> salt;
    };

    RegistrationResult Register(const std::string& username, const std::string& password);

    class Client
    {

    };

    class Server
    {

    };
}


