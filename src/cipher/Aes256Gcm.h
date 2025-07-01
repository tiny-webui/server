#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <optional>
#include <openssl/evp.h>
#include "BigUint.h"
#include "OpensslTypes.h"

namespace TUI::Cipher::Aes256Gcm
{
    using Key = std::array<uint8_t, 32>;
    using Tag = std::array<uint8_t, 16>;
    using Iv = BigUint<12>;

    class Encryptor
    {
    public:
        Encryptor(const Key& key);

        /**
         * @brief Encrypt a message. IV is managed internally.
         * 
         * @param plainText 
         * @return std::vector<uint8_t> Format: IV (12B) | CipherText (no padding) || Tag (16B)
         */
        std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& plainText);
    
    private:
        Iv _iv{};
        Openssl::EvpCipherCtx _ctx{};
    };

    class Decryptor
    {
    public:
        Decryptor(const Key& key);

        /**
         * @brief Decrypt a message encrypted with the Encryptor
         * 
         * @param cipherText 
         * @return std::vector<uint8_t> 
         */
        std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& cipherText);
    
    private:
        std::optional<Iv> _lastIv{std::nullopt};
        Openssl::EvpCipherCtx _ctx{};
    };
}


