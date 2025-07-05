#include <iostream>
#include "cipher/XChaCha20Poly1305.h"
#include "Utility.h"

using namespace TUI::Cipher::XChaCha20Poly1305;

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    Key key{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
    };
    Encryptor encryptor(key);
    Decryptor decryptor(key);

    std::vector<std::string> messages = {
        "Hello, World!",
        "This is a test message.",
        "AES-256 GCM encryption and decryption.",
        "OpenSSL is a powerful library.",
        ""
    };

    for (const auto& message: messages)
    {
        std::vector<uint8_t> plainText(message.begin(), message.end());
        auto cipherText = encryptor.Encrypt(plainText);
        auto decryptedText = decryptor.Decrypt(cipherText);
        std::string decryptedMessage(decryptedText.begin(), decryptedText.end());
        AssertWithMessage(
            decryptedMessage == message,
            "Decrypted message does not match original message");
    }

    return 0;
}

