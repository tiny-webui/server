#include <iostream>
#include "cipher/ChaCha20Poly1305.h"
#include "common/Base64.h"

using namespace TUI::Common;
using namespace TUI::Cipher;

static constexpr ChaCha20Poly1305::Key clientKey {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

static constexpr ChaCha20Poly1305::Key serverKey {
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F
};

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <client output>" << std::endl;
        return 1;
    }

    auto clientBytes = Base64::Decode(argv[1]);
    ChaCha20Poly1305::Decryptor decryptor(clientKey);
    auto decrypted = decryptor.Decrypt(clientBytes);
    std::string decryptedString(decrypted.begin(), decrypted.end());
    std::cout << "Decrypted message:" << std::endl << decryptedString << std::endl;
    ChaCha20Poly1305::Encryptor encryptor(serverKey);
    auto encrypted = encryptor.Encrypt(decrypted);
    auto encryptedBase64 = Base64::Encode(encrypted);
    std::cout << "Encrypted message:" << std::endl << encryptedBase64 << std::endl;

    return 0;
}
