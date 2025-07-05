#include <stdexcept>
#include <sodium/crypto_aead_xchacha20poly1305.h>
#include <sodium/randombytes.h>
#include "XChaCha20Poly1305.h"

using namespace TUI::Cipher::XChaCha20Poly1305;

static_assert(sizeof(Key) == crypto_aead_xchacha20poly1305_ietf_KEYBYTES, "Key size mismatch");
static_assert(NonceSize == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES, "Nonce size mismatch");
static_assert(TagSize == crypto_aead_xchacha20poly1305_ietf_ABYTES, "Tag size mismatch");

Encryptor::Encryptor(const Key& key)
    : _key(key)
{
}

Encryptor::Encryptor(const std::vector<uint8_t>& key)
    : _key()
{
    if (key.size() != sizeof(Key))
    {
        throw std::invalid_argument("Key size must be 32 bytes");
    }
    std::copy(key.begin(), key.end(), _key.begin());
}

std::vector<uint8_t> Encryptor::Encrypt(const uint8_t* plainText, size_t size)
{
    std::vector<uint8_t> cipherText(NonceSize + size + TagSize, 0);
    randombytes_buf(cipherText.data(), NonceSize);
    unsigned long long outputLen = 0;
    int rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
        cipherText.data() + NonceSize, &outputLen,
        plainText, size,
        nullptr, 0,
        nullptr,
        cipherText.data(),
        _key.data());
    if (rc != 0)
    {
        /** This should always return 0 */
        throw std::runtime_error("Encryption failed");
    }
    return cipherText;
}

std::vector<uint8_t> Encryptor::Encrypt(const std::vector<uint8_t>& plainText)
{
    return Encrypt(plainText.data(), plainText.size());
}

Decryptor::Decryptor(const Key& key)
    : _key(key)
{
}

Decryptor::Decryptor(const std::vector<uint8_t>& key)
    : _key()
{
    if (key.size() != sizeof(Key))
    {
        throw std::invalid_argument("Key size must be 32 bytes");
    }
    std::copy(key.begin(), key.end(), _key.begin());
}

std::vector<uint8_t> Decryptor::Decrypt(const uint8_t* cipherText, size_t size)
{
    if (size < NonceSize + TagSize)
    {
        throw std::invalid_argument("Ciphertext too short");
    }
    std::vector<uint8_t> plainText(size - NonceSize - TagSize, 0);
    unsigned long long outputLen = 0;
    int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
        plainText.data(), &outputLen,
        nullptr, 
        cipherText + NonceSize, size - NonceSize,
        nullptr, 0,
        cipherText,
        _key.data());
    if (rc != 0)
    {
        throw std::runtime_error("Decryption failed");
    }
    return plainText;
}

std::vector<uint8_t> Decryptor::Decrypt(const std::vector<uint8_t>& cipherText)
{
    return Decrypt(cipherText.data(), cipherText.size());
}

