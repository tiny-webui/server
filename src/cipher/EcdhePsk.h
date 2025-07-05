#pragma once

#include <array>
#include <list>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>
#include "OpensslTypes.h"
#include "StepChecker.h"
#include "IAuthenticationPeer.h"
#include "HandshakeMessage.h"

/**
 * Cipher description:
 * 
 * ECDHE-PSK with X25519 ECDH and AES-GCM for key confirmation.
 * 
 * client                                                   server
 *         --        Share key index, PSK pair         --
 * 
 *                        Key exchange
 *         -- key index, client pubkey, client nonce   ->
 *         <-        server pubKey, server nonce       --
 *                      Key confirmation
 *         -- MAC(client confirm key, transcript hash) ->
 *         <- MAC(server confirm key, transcript hash) --
 *                         
 *                          Key usage
 *         <- Encrypted session with client/server keys ->
 * 
 * Ideally, the second and fourth messages should be sent in the same packet.
 * But for the sake of implementation simplicity, they are separated.
 * 
 */


namespace TUI::Cipher::EcdhePsk
{
    using Psk = std::array<uint8_t, 32>;
    constexpr size_t PUBKEY_SIZE = 32;
    constexpr size_t NONCE_SIZE = 32;

    class Client : public IAuthenticationPeer
    {
    public:
        Client(
            const Psk& psk,
            const std::vector<uint8_t>& keyIndex,
            const std::unordered_map<HandshakeMessage::Type, std::vector<uint8_t>>& additionalElements = {},
            size_t keySize = 32);

        std::optional<HandshakeMessage> GetNextMessage(
            const std::optional<HandshakeMessage>& peerMessage) override;
        bool IsHandshakeComplete() override;
        std::vector<uint8_t> GetClientKey() override;
        std::vector<uint8_t> GetServerKey() override;
    private:
        enum class Step
        {
            Init,
            ClientMessage,
            ServerMessage,
            ServerConfirmation
        };
        
        Psk _psk;
        size_t _keySize;
        std::map<HandshakeMessage::Type, std::vector<uint8_t>> _firstMessageAdditionalElements{};
        Openssl::EvpPkey _pkey{};
        std::optional<HandshakeMessage> _clientMessage{std::nullopt};
        std::optional<std::vector<uint8_t>> _serverConfirmKey{std::nullopt};
        std::optional<std::vector<uint8_t>> _clientKey{std::nullopt};
        std::optional<std::vector<uint8_t>> _serverKey{std::nullopt};
        std::optional<std::array<uint8_t, 32>> _transcriptHash{std::nullopt};
        std::shared_ptr<StepChecker<Step>> _stepChecker{StepChecker<Step>::Create(Step::Init)};
        int64_t _numCalls{0};

        /**
         * @brief Step 1
         * 
         * @return HandshakeMessage with:
         * CipherMessage element (client message):
         *   pubKey | nonce 
         *    32B   |  32B  
         * KeyIndex element:
         *   keyIndex
         *     Any
         * [additional elements from constructor]
         */
        HandshakeMessage GetClientMessage();
        /**
         * @brief Step 3
         * 
         * @param serverMessage
         * 
         * @return HandshakeMessage with:
         * CipherMessage element (client confirmation):
         * AES-GCM(client_confirm_key, transcript_hash)
         *                  Any
         */
        HandshakeMessage TakeServerMessage(
            const HandshakeMessage& serverMessage);

        /**
         * @brief Step 5
         * 
         * @param serverMessage
         */
        void TakeServerConfirmation(const HandshakeMessage& serverMessage);
    };

    class Server : public IAuthenticationPeer
    {
    public:

        Server(
            std::function<Psk(const std::vector<uint8_t>&)> getPsk,
            size_t keySize = 32);

        std::optional<HandshakeMessage> GetNextMessage(
            const std::optional<HandshakeMessage>& peerMessage) override;
        bool IsHandshakeComplete() override;
        std::vector<uint8_t> GetClientKey() override;
        std::vector<uint8_t> GetServerKey() override;
    private:
        enum class Step
        {
            Init,
            ClientMessage,
            ClientConfirmation,
        };

        std::function<Psk(const std::vector<uint8_t>&)> _getPsk;
        size_t _keySize;
        std::optional<std::vector<uint8_t>> _clientConfirmKey{std::nullopt};
        std::optional<std::vector<uint8_t>> _serverConfirmKey{std::nullopt};
        std::optional<std::vector<uint8_t>> _clientKey{std::nullopt};
        std::optional<std::vector<uint8_t>> _serverKey{std::nullopt};
        std::optional<std::array<uint8_t, 32>> _transcriptHash{std::nullopt};
        std::shared_ptr<StepChecker<Step>> _stepChecker{StepChecker<Step>::Create(Step::Init)};
        int64_t _numCalls{0};

        /**
         * @brief Step 2
         * 
         * @param clientMessage 
         * @return HandshakeMessage with:
         * CipherMessage element (server message):
         *   pubKey | nonce
         *    32B   |  32B
         */
        HandshakeMessage TakeClientMessage(const HandshakeMessage& clientMessage);

        /**
         * @brief Step 4
         * 
         * @param clientConfirmation
         * @return HandshakeMessage with:
         * CipherMessage element (server confirmation):
         * AES-GCM(server_confirm_key, transcript_hash)
         *                 Any
         */
        HandshakeMessage TakeClientConfirmation(const HandshakeMessage& clientConfirmation);
    };
}
