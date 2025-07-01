#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <optional>
#include "OpensslTypes.h"
#include "StepChecker.h"

/**
 * @brief 
 * @attention This ONLY does the key exchange. NOT the key confirmation.
 * The user must enforce a key confirmation using the transcript hash.
 * 
 */

namespace TUI::Cipher::EcdhePsk
{
    using Psk = std::array<uint8_t, 32>;
    constexpr size_t PUBKEY_SIZE = 32;
    constexpr size_t NONCE_SIZE = 32;

    class Client
    {
    public:

        Client(const Psk& psk, size_t keySize = 32);

        /**
         * @brief Step 1
         * 
         * @return std::vector<uint8_t>
         *   pubKey | nonce | key index
         *    32B   |  32B  |  Varies
         */
        std::vector<uint8_t> GetClientMessage(const std::vector<uint8_t>& keyIndex);
        /**
         * @brief Step 3
         * 
         * @param serverMessage 
         */
        void TakeServerMessage(const std::vector<uint8_t>& serverMessage);

        std::vector<uint8_t> GetClientKey();
        std::vector<uint8_t> GetServerKey();
        std::array<uint8_t, 32> GetTranscriptHash();
    private:
        enum class Step
        {
            Init,
            ClientMessage,
            ServerMessage
        };
        
        Psk _psk;
        size_t _keySize;
        Openssl::EvpPkey _pkey{};
        std::optional<std::vector<uint8_t>> _clientMessage{std::nullopt};
        std::optional<std::vector<uint8_t>> _clientKey{std::nullopt};
        std::optional<std::vector<uint8_t>> _serverKey{std::nullopt};
        std::optional<std::array<uint8_t, 32>> _transcriptHash{std::nullopt};
        std::shared_ptr<StepChecker<Step>> _stepChecker{StepChecker<Step>::Create(Step::Init)};
    };

    class Server
    {
    public:
        enum class Step
        {
            Init,
            ServerMessage
        };

        static std::vector<uint8_t> ParseKeyIndexFromClientMessage(const std::vector<uint8_t>& clientMessage);

        Server(const Psk& psk, size_t keySize = 32);
        
        /**
         * @brief Step 2
         * 
         * @param clientMessage 
         * @return std::vector<uint8_t> 
         *   pubKey | nonce
         *    32B   |  32B
         */
        std::vector<uint8_t> GetServerMessage(const std::vector<uint8_t>& clientMessage);

        std::vector<uint8_t> GetClientKey();
        std::vector<uint8_t> GetServerKey();
        std::array<uint8_t, 32> GetTranscriptHash();
    private:
        Psk _psk;
        size_t _keySize;
        std::optional<std::vector<uint8_t>> _clientKey;
        std::optional<std::vector<uint8_t>> _serverKey;
        std::optional<std::array<uint8_t, 32>> _transcriptHash;
        std::shared_ptr<StepChecker<Step>> _stepChecker{StepChecker<Step>::Create(Step::Init)};
    };
}
