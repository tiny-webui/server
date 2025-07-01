#include <iostream>
#include "cipher/EcdhePsk.h"
#include "Utility.h"

using namespace TUI::Cipher::EcdhePsk;

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    Psk psk = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
    
    Client client(psk, 32);
    Server server(psk, 32);

    std::vector<uint8_t> keyIndex = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    auto clientMessage = client.GetClientMessage(keyIndex);
    auto keyIndexParsed = Server::ParseKeyIndexFromClientMessage(clientMessage);
    AssertWithMessage(
        keyIndexParsed == keyIndex,
        "Key index should match"
    );
    auto serverMessage = server.GetServerMessage(clientMessage);
    client.TakeServerMessage(serverMessage);
    auto clientKeyClient = client.GetClientKey();
    auto serverKeyClient = client.GetServerKey();
    auto transcriptHashClient = client.GetTranscriptHash();
    auto clientKeyServer = server.GetClientKey();
    auto serverKeyServer = server.GetServerKey();
    auto transcriptHashServer = server.GetTranscriptHash();
    AssertWithMessage(
        clientKeyClient == clientKeyServer,
        "client key should match"
    );
    AssertWithMessage(
        serverKeyClient == serverKeyServer,
        "Server key should match"
    );
    AssertWithMessage(
        transcriptHashClient == transcriptHashServer,
        "Transcript hash should match"
    );

    return 0;
}


