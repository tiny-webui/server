#include <stdexcept>
#include <vector>
#include <sodium/randombytes.h>
#include <sodium/crypto_pwhash_argon2id.h>
#include "Spake2p.h"
#include "Ed25519.h"

using namespace TUI::Cipher;
using namespace TUI::Cipher::Spake2p;

static_assert(ARGON2ID_LANES == 1, "SPAKE2p only supports 1 lane for now");
static_assert(sizeof(RegistrationResult::salt) == crypto_pwhash_argon2id_SALTBYTES, "Salt size mismatch");
static_assert(sizeof(RegistrationResult::w0) == Ed25519::Scalar::size, "w0 size mismatch");
static_assert(sizeof(RegistrationResult::w1) == Ed25519::Scalar::size, "w1 size mismatch");

RegistrationResult Spake2p::Register(const std::string& username, const std::string& password)
{
    int rc = 0;
    std::array<uint8_t, 16> salt;
    randombytes_buf(salt.data(), salt.size());
    /** len(password) | password | len(username) | username | len("tui") | "tui" */
    std::vector<uint8_t> keyMaterial{};
    {
        uint16_t passwordLen = static_cast<uint16_t>(password.size());
        keyMaterial.insert(
            keyMaterial.end(),
            reinterpret_cast<uint8_t*>(&passwordLen),
            reinterpret_cast<uint8_t*>(&passwordLen) + sizeof(passwordLen));
        keyMaterial.insert(keyMaterial.end(), password.begin(), password.end());
        uint16_t usernameLen = static_cast<uint16_t>(username.size());
        keyMaterial.insert(
            keyMaterial.end(),
            reinterpret_cast<uint8_t*>(&usernameLen),
            reinterpret_cast<uint8_t*>(&usernameLen) + sizeof(usernameLen));
        keyMaterial.insert(keyMaterial.end(), username.begin(), username.end());
        std::string idVerifier = "tui";
        uint16_t idVerifierLen = static_cast<uint16_t>(idVerifier.size());
        keyMaterial.insert(
            keyMaterial.end(),
            reinterpret_cast<uint8_t*>(&idVerifierLen),
            reinterpret_cast<uint8_t*>(&idVerifierLen) + sizeof(idVerifierLen));
        keyMaterial.insert(keyMaterial.end(), idVerifier.begin(), idVerifier.end());
    }
    std::array<uint8_t, Ed25519::Scalar::keyMaterialSize> w0s{};
    std::array<uint8_t, Ed25519::Scalar::keyMaterialSize> w1s{};
    {
        std::array<uint8_t, Ed25519::Scalar::keyMaterialSize * 2> key{};
        /** Lane defaults to 1 with libsodium */
        rc = crypto_pwhash_argon2id(
            key.data(), key.size(),
            reinterpret_cast<const char*>(keyMaterial.data()), keyMaterial.size(),
            salt.data(),
            ARGON2ID_ITERATIONS,
            ARGON2ID_MEM_COST_BYTES,
            crypto_pwhash_argon2id_ALG_ARGON2ID13);
        if (rc != 0)
        {
            throw std::runtime_error("Failed to derive key using Argon2ID");
        }
        std::copy(key.begin(), key.begin() + Ed25519::Scalar::keyMaterialSize, w0s.begin());
        std::copy(key.begin() + Ed25519::Scalar::keyMaterialSize, key.end(), w1s.begin());
    }
    auto w0 = Ed25519::Scalar::Reduce(w0s);
    auto w1 = Ed25519::Scalar::Reduce(w1s);
    RegistrationResult result{};
    result.w0 = w0.Dump();
    result.w1 = w1.Dump();
    result.salt = salt;
    return result;
}
