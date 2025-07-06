#include <stdexcept>
#include <sodium/crypto_core_ed25519.h>
#include <sodium/crypto_scalarmult_ed25519.h>
#include "Ed25519.h"

using namespace TUI::Cipher;
using namespace TUI::Cipher::Ed25519;

PriKey Ed25519::GetCofactor()
{
    return PriKey{cofactor_bytes};
}

PriKey::PriKey()
    : _bytes()
{
}

PriKey::PriKey(const std::array<uint8_t, crypto_core_ed25519_SCALARBYTES>& key)
    : _bytes(key)
{
}

PriKey PriKey::Generate()
{
    PriKey key;
    crypto_core_ed25519_scalar_random(key._bytes.data());
    return key;
}

PriKey PriKey::Reduce(const std::array<uint8_t, crypto_core_ed25519_NONREDUCEDSCALARBYTES>& bn)
{
    PriKey key;
    crypto_core_ed25519_scalar_reduce(key._bytes.data(), bn.data());
    return key;
}

std::array<uint8_t, crypto_core_ed25519_SCALARBYTES> PriKey::Dump() const
{
    return _bytes;
}

PubKey PriKey::GetPubKey() const
{
    PubKey pubKey;
    int rc = crypto_scalarmult_ed25519_base_noclamp(pubKey._bytes.data(), _bytes.data());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to derive public key from private key");
    }
    return pubKey;
}

PriKey PriKey::complement() const
{
    PriKey result;
    crypto_core_ed25519_scalar_complement(result._bytes.data(), _bytes.data());
    return result;
}

PriKey PriKey::inverse() const
{
    PriKey result;
    crypto_core_ed25519_scalar_invert(result._bytes.data(), _bytes.data());
    return result;
}

PriKey PriKey::operator-() const
{
    PriKey result;
    crypto_core_ed25519_scalar_negate(result._bytes.data(), _bytes.data());
    return result;
}

PriKey PriKey::operator+(const PriKey& other) const
{
    PriKey result;
    crypto_core_ed25519_scalar_add(result._bytes.data(), _bytes.data(), other._bytes.data());
    return result;
}

PriKey PriKey::operator-(const PriKey& other) const
{
    PriKey result;
    crypto_core_ed25519_scalar_sub(result._bytes.data(), _bytes.data(), other._bytes.data());
    return result;
}

PriKey PriKey::operator*(const PriKey& other) const
{
    PriKey result;
    crypto_core_ed25519_scalar_mul(result._bytes.data(), _bytes.data(), other._bytes.data());
    return result;
}

PubKey PriKey::operator*(const PubKey& pubKey) const
{
    PubKey result;
    int rc = crypto_scalarmult_ed25519_noclamp(result._bytes.data(), _bytes.data(), pubKey._bytes.data());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to multiply private key with public key");
    }
    return result;
}

PubKey::PubKey()
    : _bytes()
{
}

PubKey::PubKey(const std::array<uint8_t, crypto_core_ed25519_BYTES>& key)
    : _bytes(key)
{
    if (!crypto_core_ed25519_is_valid_point(_bytes.data()))
    {
        throw std::runtime_error("Invalid public key");
    }
}

PubKey PubKey::Generate()
{
    PubKey key;
    while(true)
    {
        crypto_core_ed25519_random(key._bytes.data());
        if (crypto_core_ed25519_is_valid_point(key._bytes.data()))
        {
            break;
        }
    }
    return key;
}

std::array<uint8_t, crypto_core_ed25519_BYTES> PubKey::Dump() const
{
    return _bytes;
}

PubKey PubKey::operator+(const PubKey& other) const
{
    PubKey result;
    int rc = crypto_core_ed25519_add(result._bytes.data(), _bytes.data(), other._bytes.data());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to add public keys");
    }
    return result;
}

PubKey PubKey::operator-(const PubKey& other) const
{
    PubKey result;
    int rc = crypto_core_ed25519_sub(result._bytes.data(), _bytes.data(), other._bytes.data());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to subtract public keys");
    }
    return result;
}

bool PubKey::operator==(const PubKey& other) const
{
    return _bytes == other._bytes;
}

bool PubKey::operator!=(const PubKey& other) const
{
    return !(*this == other);
}
