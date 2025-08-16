#include <iostream>
#include <iomanip>
#include <array>
#include "cipher/EcdhePsk.h"
#include "common/Base64.h"

using namespace TUI::Cipher;
using namespace TUI::Common;

static constexpr std::array<uint8_t, 8> keyIndex {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

static constexpr std::array<uint8_t, 32> psk {
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F
};

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    auto getPsk = [&](const std::vector<uint8_t>& keyIndexIn) -> EcdhePsk::Psk {
        if (!std::equal(keyIndexIn.begin(), keyIndexIn.end(), keyIndex.begin(), keyIndex.end())) {
            throw std::runtime_error("Key index does not match");
        }
        return psk;
    };

    EcdhePsk::Server server(getPsk);

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

