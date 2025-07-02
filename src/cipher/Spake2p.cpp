#include <stdexcept>
#include <vector>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/thread.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include "Spake2p.h"
#include "OpensslTypes.h"

using namespace TUI::Cipher;
using namespace TUI::Cipher::Spake2p;

RegistrationResult Spake2p::Register(const std::string& username, const std::string& password)
{
    int rc = 0;
    std::array<uint8_t, 16> salt;
    {
        int rc = RAND_bytes(salt.data(), salt.size());
        if (rc != 1)
        {
            throw std::runtime_error("Failed to generate salt");
        }
    }
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
    std::array<uint8_t, 32> w0{};
    std::array<uint8_t, 32> w1{};
    {
        std::array<uint8_t, 64> key{};
        uint32_t threads = 1;
        uint32_t lanes = ARGON2ID_LANES;
        uint32_t memCost = ARGON2ID_MEM_COST;
        uint32_t iterations = ARGON2ID_ITERATIONS;
        // uint32_t size = static_cast<uint32_t>(key.size());
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &threads),
            OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes),
            OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memCost),
            OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iterations),
            // OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SIZE, &size),
            OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, salt.data(), salt.size()),
            OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, keyMaterial.data(), keyMaterial.size()),
            OSSL_PARAM_construct_end()
        };
        Openssl::EvpKdf kdf(EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr));
        Openssl::EvpKdfCtx kdfCtx(EVP_KDF_CTX_new(kdf));
        rc = EVP_KDF_derive(kdfCtx, key.data(), key.size(), params);
        if (rc != 1)
        {
            throw std::runtime_error("Failed to derive key using Argon2ID");
        }
        /**
         * Openssl will do the scalar value clamping when doing the curve 25519 scaler multiplication.
         * So we use the 32 random bytes directly without doing any modulo or clamping.
         */
        std::copy(key.begin(), key.begin() + 32, w0.begin());
        std::copy(key.begin() + 32, key.end(), w1.begin());
    }
    RegistrationResult result{};
    result.w0 = w0;
    result.w1 = w1;
    result.salt = salt;
    return result;
}
