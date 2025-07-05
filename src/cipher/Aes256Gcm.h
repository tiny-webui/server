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
        explicit Encryptor(const Key& key);
        explicit Encryptor(const std::vector<uint8_t>& key);

        /**
         * @brief Encrypt a message. IV is managed internally.
         * 
         * @param plainText 
         * @return std::vector<uint8_t> 
         *     IV   | CipherText (no padding) |   Tag
         *   12B LE |    variable             |   16B
         */
        std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& plainText);
        template<size_t N>
        std::vector<uint8_t> Encrypt(const std::array<uint8_t, N>& plainText)
        {
            return Encrypt(std::vector<uint8_t>(plainText.begin(), plainText.end()));
        }

    private:
        Iv _iv{};
        Openssl::EvpCipherCtx _ctx{};
    };

    class Decryptor
    {
    public:
        explicit Decryptor(const Key& key);
        explicit Decryptor(const std::vector<uint8_t>& key);

        /**
         * @brief Decrypt a message encrypted with the Encryptor
         * 
         * @param cipherText 
         * @return std::vector<uint8_t> 
         */
        std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& cipherText);
        template<size_t N>
        std::vector<uint8_t> Decrypt(const std::array<uint8_t, N>& cipherText)
        {
            return Decrypt(std::vector<uint8_t>(cipherText.begin(), cipherText.end()));
        }

    private:
        std::optional<Iv> _lastIv{std::nullopt};
        Openssl::EvpCipherCtx _ctx{};
    };
}


