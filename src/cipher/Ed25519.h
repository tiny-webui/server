#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <sodium/crypto_core_ed25519.h>

namespace TUI::Cipher::Ed25519
{
    class PubKey;

    class PriKey
    {
    public:
        static constexpr size_t size = crypto_core_ed25519_SCALARBYTES;
        static constexpr size_t keyMaterialSize = crypto_core_ed25519_NONREDUCEDSCALARBYTES;
        /**
         * @brief Generate a new private key.
         * 
         * @return PriKey 
         */
        static PriKey Generate();
        /**
         * @brief Calculate private key from a big number with modulo operation.
         * 
         * @param bn 
         * @return PriKey 
         */
        static PriKey Reduce(const std::array<uint8_t, crypto_core_ed25519_NONREDUCEDSCALARBYTES>& bn);

        /**
         * @brief Load a private key from a byte array.
         * 
         * @param key 
         */
        explicit PriKey(const std::array<uint8_t, crypto_core_ed25519_SCALARBYTES>& key);

        /**
         * @brief Dump the private key as a byte array.
         * 
         * @return std::array<uint8_t, crypto_core_ed25519_SCALARBYTES> 
         */
        std::array<uint8_t, crypto_core_ed25519_SCALARBYTES> Dump() const;

        /** This is the no clamp variant */
        PubKey GetPubKey() const;

        PriKey complement() const;
        PriKey inverse() const;
        PriKey operator-() const;
        PriKey operator+(const PriKey& other) const;
        PriKey operator-(const PriKey& other) const;
        PriKey operator*(const PriKey& other) const;
        /** This is the no clamp variant */
        PubKey operator*(const PubKey& pubKey) const;
    private:
        PriKey();

        std::array<uint8_t, crypto_core_ed25519_SCALARBYTES> _bytes;
    };

    class PubKey
    {
        friend class PriKey;
    public:
        static PubKey Generate();

        /**
         * @brief Load a public key from a byte array.
         * 
         * @param key 
         */
        explicit PubKey(const std::array<uint8_t, crypto_core_ed25519_BYTES>& key);

        /**
         * @brief Dump the public key as a byte array.
         * 
         * @return std::array<uint8_t, crypto_core_ed25519_BYTES> 
         */
        std::array<uint8_t, crypto_core_ed25519_BYTES> Dump() const;

        PubKey operator+(const PubKey& other) const;
        PubKey operator-(const PubKey& other) const;
        bool operator==(const PubKey& other) const;
        bool operator!=(const PubKey& other) const;
    private:
        PubKey();

        std::array<uint8_t, crypto_core_ed25519_BYTES> _bytes;
    };

    using Scalar = PriKey;
    using Point = PubKey;
}
