#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace TUI::Cipher::XChaCha20Poly1305
{
    using Key = std::array<uint8_t, 32>;
    constexpr size_t NONCE_SIZE = 24;
    constexpr size_t TAG_SIZE = 16;

    class Encryptor
    {
    public:
        explicit Encryptor(const Key& key);
        explicit Encryptor(const std::vector<uint8_t>& key);

        /**
         * @brief 
         * 
         * @param plainText 
         * @return std::vector<uint8_t>
         *  Nonce  | CipherText | Tag
         *   24B   |   variable | 16B
         */
        std::vector<uint8_t> Encrypt(const uint8_t* plainText, size_t size);
        std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& plainText);
        template<size_t N>
        std::vector<uint8_t> Encrypt(const std::array<uint8_t, N>& plainText)
        {
            return Encrypt(plainText.data(), plainText.size());
        }
    private:
        Key _key{};
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
        std::vector<uint8_t> Decrypt(const uint8_t* cipherText, size_t size);
        std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& cipherText);
        template<size_t N>
        std::vector<uint8_t> Decrypt(const std::array<uint8_t, N>& cipherText)
        {
            return Decrypt(cipherText.data(), cipherText.size());
        }
    private:
        Key _key{};
    };
}
