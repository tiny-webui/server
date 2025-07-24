#include <stdexcept>
#include <sodium/crypto_aead_chacha20poly1305.h>
#include <sodium/randombytes.h>
#include "ChaCha20Poly1305.h"

using namespace TUI::Cipher::ChaCha20Poly1305;

static_assert(sizeof(Key) == crypto_aead_chacha20poly1305_ietf_KEYBYTES, "Key size mismatch");
static_assert(NONCE_SIZE == crypto_aead_chacha20poly1305_IETF_NPUBBYTES, "Nonce size mismatch");
static_assert(TAG_SIZE == crypto_aead_chacha20poly1305_ietf_ABYTES, "Tag size mismatch");

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
    std::vector<uint8_t> cipherText(NONCE_SIZE + size + TAG_SIZE, 0);
    const auto& nonce = _counter.GetBytes();
    /** Increment the counter first to start with 1 */
    _counter++;
    std::copy(nonce.begin(), nonce.end(), cipherText.begin());
    unsigned long long outputLen = 0;
    int rc = crypto_aead_chacha20poly1305_ietf_encrypt(
        cipherText.data() + NONCE_SIZE, &outputLen,
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
    if (size < NONCE_SIZE + TAG_SIZE)
    {
        throw std::invalid_argument("Ciphertext too short");
    }
    /** Check the counter first */
    Cipher::Counter<NONCE_SIZE> counter(cipherText, NONCE_SIZE);
    if (counter <= _counter)
    {
        throw std::runtime_error("Replay message detected");
    }
    std::vector<uint8_t> plainText(size - NONCE_SIZE - TAG_SIZE, 0);
    unsigned long long outputLen = 0;
    int rc = crypto_aead_chacha20poly1305_ietf_decrypt(
        plainText.data(), &outputLen,
        nullptr, 
        cipherText + NONCE_SIZE, size - NONCE_SIZE,
        nullptr, 0,
        cipherText,
        _key.data());
    if (rc != 0)
    {
        throw std::runtime_error("Decryption failed");
    }
    /** Update the counter only if the message is valid */
    _counter = counter;
    return plainText;
}

std::vector<uint8_t> Decryptor::Decrypt(const std::vector<uint8_t>& cipherText)
{
    return Decrypt(cipherText.data(), cipherText.size());
}

