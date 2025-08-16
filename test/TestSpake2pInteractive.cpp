#include <iostream>
#include <iomanip>
#include <array>
#include "cipher/Spake2p.h"
#include "common/Base64.h"
#include "cipher/Ed25519.h"

using namespace TUI::Cipher;
using namespace TUI::Common;

using namespace TUI::Cipher::Ed25519;

static constexpr std::string_view username = "username";

static constexpr std::array<uint8_t, 32> w0 = {
    0x80, 0x92, 0x42, 0x74, 0x46, 0xa4, 0x50, 0x18,
    0x39, 0xe9, 0xd8, 0x41, 0xd1, 0x11, 0xe5, 0xbd,
    0x4a, 0x07, 0x0e, 0x58, 0x47, 0x02, 0xea, 0xb3,
    0xd5, 0xc0, 0x90, 0x2d, 0x65, 0xc0, 0x4d, 0x29
};

static constexpr std::array<uint8_t, 32> L = {
    0x50, 0xda, 0x9d, 0xa1, 0xc4, 0x95, 0x9e, 0x6a,
    0x52, 0xbc, 0x68, 0x2a, 0x9b, 0x31, 0xaf, 0x5e,
    0x51, 0x07, 0xae, 0x23, 0x07, 0xe2, 0xd3, 0x67,
    0xc2, 0xaf, 0xa1, 0x8b, 0x4f, 0x21, 0x7c, 0x00
};

static constexpr std::array<uint8_t, 16> salt = {
    0x29, 0x99, 0xd3, 0x34, 0xa4, 0xd5, 0x13, 0xe8,
    0xec, 0xf8, 0x7d, 0xa2, 0xdc, 0x21, 0xe7, 0x02
};

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    auto getRegistration = [](const std::string& user) -> Spake2p::RegistrationResult {
        if (user != username)
        {
            throw std::runtime_error("Unknown user");
        }
        Spake2p::RegistrationResult reg {
            .w0 = w0,
            .L = L,
            .salt = salt
        };
        return reg;
    };

    Spake2p::Server server(getRegistration);

    while (true)
    {
        std::string line;
        std::cout << "Enter client message (base64): ";
        if (!std::getline(std::cin, line))
        {
            throw std::runtime_error("Failed to read input");
        }
        auto clientMessageBytes = Base64::Decode(line);
        auto clientMessage = HandshakeMessage::Message::Parse(clientMessageBytes);
        auto serverMessageOpt = server.GetNextMessage(clientMessage);
        if (serverMessageOpt.has_value())
        {
            auto serverMessageBytes = serverMessageOpt->Serialize();
            auto serverMessageBase64 = Base64::Encode(serverMessageBytes);
            std::cout << "Server message:" << std::endl << serverMessageBase64 << std::endl;
        }
        if (server.IsHandshakeComplete())
        {
            break;
        }
    }

    auto clientKey = server.GetClientKey();
    auto serverKey = server.GetServerKey();

    std::cout << "client key:" << std::endl;
    for (const auto& byte : clientKey) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout << std::endl;

    std::cout << "server key:" << std::endl;
    for (const auto& byte : serverKey) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout << std::endl;

    return 0;
}

