#include <sodium/crypto_kdf_hkdf_sha256.h>
#include <iostream>
#include <iomanip>
#include <array>

static constexpr std::array<uint8_t, 16> ikm {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static constexpr std::array<uint8_t, 32> salt {
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F
};

static constexpr std::array<uint8_t, 16> context {
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F
};

int main(int argc, char const *argv[])
{
    (void) argc;
    (void) argv;

    std::array<uint8_t, crypto_kdf_hkdf_sha256_KEYBYTES> prk{};
    int rc = crypto_kdf_hkdf_sha256_extract(prk.data(), salt.data(), salt.size(), ikm.data(), ikm.size());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to extract key");
    }
    std::array<uint8_t, 32> key{};
    rc = crypto_kdf_hkdf_sha256_expand(key.data(), key.size(), reinterpret_cast<const char*>(context.data()), context.size(), prk.data());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to expand key");
    }

    // Print PRK in hex
    std::cout << "PRK: ";
    for (const auto& byte : prk) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout << std::endl;

    // Print key in hex
    std::cout << "Key: ";
    for (const auto& byte : key) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout << std::endl;

    return 0;
}

