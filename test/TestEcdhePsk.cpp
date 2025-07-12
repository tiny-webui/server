#include <iostream>
#include "cipher/EcdhePsk.h"
#include "Utility.h"

using namespace TUI::Cipher;
using namespace TUI::Cipher::EcdhePsk;

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    Psk psk = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
    
    std::vector<uint8_t> keyIndex = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    auto getPsk = [&](const std::vector<uint8_t>& keyIndexIn) -> Psk {
        if (!std::equal(keyIndexIn.begin(), keyIndexIn.end(), keyIndex.begin(), keyIndex.end())) {
            throw std::runtime_error("Key index does not match");
        }
        return psk;
    };
    
    Client client(psk, keyIndex, {
        {HandshakeMessage::Type::ProtocolType, {0x01, 0x02, 0x03}}});
    Server server(getPsk);

    {
        std::optional<HandshakeMessage> clientMessageOpt{std::nullopt};
        std::optional<HandshakeMessage> serverMessageOpt{std::nullopt};
        while(!client.IsHandshakeComplete() || !server.IsHandshakeComplete())
        {
            if (!client.IsHandshakeComplete())
            {
                /** Mimic transmission */
                if (serverMessageOpt.has_value())
                {
                    serverMessageOpt = HandshakeMessage(serverMessageOpt->Serialize());
                }
                clientMessageOpt = client.GetNextMessage(serverMessageOpt);
            }
            if (!server.IsHandshakeComplete())
            {
                if (clientMessageOpt.has_value())
                {
                    clientMessageOpt = HandshakeMessage(clientMessageOpt->Serialize());
                }
                serverMessageOpt = server.GetNextMessage(clientMessageOpt);
            }
        }
    }

    auto clientKeyClient = client.GetClientKey();
    auto serverKeyClient = client.GetServerKey();
    auto clientKeyServer = server.GetClientKey();
    auto serverKeyServer = server.GetServerKey();
    AssertWithMessage(
        clientKeyClient == clientKeyServer,
        "client key should match"
    );
    AssertWithMessage(
        serverKeyClient == serverKeyServer,
        "Server key should match"
    );

    return 0;
}


