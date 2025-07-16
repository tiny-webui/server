#include <sodium/randombytes.h>
#include <sodium/crypto_kdf_hkdf_sha256.h>
#include "Ed25519.h"
#include "FakeCredentialGenerator.h"

using namespace TUI::Cipher;

static_assert(crypto_kdf_hkdf_sha256_KEYBYTES == FakeCredentialGenerator::SALT_PRK_SIZE, "HKDF prk size mismatch");

FakeCredentialGenerator::FakeCredentialGenerator(size_t cacheSize)
    : _cache(cacheSize)
{
    randombytes_buf(_saltPrk.data(), _saltPrk.size());
}

Spake2p::RegistrationResult FakeCredentialGenerator::GetFakeCredential(const std::string& username)
{
    auto cachedResult = _cache.TryGet(username);
    if (cachedResult.has_value())
    {
        return cachedResult.value();
    }
    Spake2p::RegistrationResult result;
    /** Generate a fake salt = HKDF(_saltSeek, username) */
    int rc = crypto_kdf_hkdf_sha256_expand(
        result.salt.data(), result.salt.size(),
        username.data(), username.size(),
        _saltPrk.data());
    if (rc != 0)
    {
        throw std::runtime_error("Cannot expand salt for fake credential");
    }
    /** Generate fake but valid w0 and L */
    auto w0 = Ed25519::Scalar::Generate();
    auto L = Ed25519::Point::Generate();
    result.w0 = w0.Dump();
    result.L = L.Dump();
    /** Update the cache */
    _cache.Update(username, result);
    return result;
}
