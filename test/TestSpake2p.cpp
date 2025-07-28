#include <iostream>
#include <iomanip>
#include "cipher/Spake2p.h"
#include "Utility.h"

using namespace TUI::Cipher;
using namespace TUI::Cipher::Spake2p;

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    std::string username = "user";
    std::string password = "password";

    auto result = Spake2p::Register(username, password);
    Client client(username, password);
    Server server(
        [&](const std::string& keyIndex) -> RegistrationResult {
            if (keyIndex != username)
            {
                throw std::runtime_error("Unknown user");
            }
            return result;
        });
    
    std::optional<HandshakeMessage::Message> clientMessage{std::nullopt};
    std::optional<HandshakeMessage::Message> serverMessage{std::nullopt};
    while(!client.IsHandshakeComplete() || !server.IsHandshakeComplete())
    {
        if (!client.IsHandshakeComplete())
        {
            if (serverMessage.has_value())
            {
                serverMessage = HandshakeMessage::Message::Parse(
                    serverMessage->Serialize());
            }
            clientMessage = client.GetNextMessage(serverMessage);
        }
        if (!server.IsHandshakeComplete())
        {
            if (clientMessage.has_value())
            {
                clientMessage = HandshakeMessage::Message::Parse(
                    clientMessage->Serialize());
            }
            serverMessage = server.GetNextMessage(clientMessage);
        }
    }
    auto clientClientKey = client.GetClientKey();
    auto clientServerKey = client.GetServerKey();
    auto serverClientKey = server.GetClientKey();
    auto serverServerKey = server.GetServerKey();
    
    AssertWithMessage(
        clientClientKey == serverClientKey,
        "Client keys should match");
    AssertWithMessage(
        clientServerKey == serverServerKey,
        "Server keys should match");

    return 0;
}
