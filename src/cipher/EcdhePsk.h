#pragma once

#include <array>
#include <list>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>
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
    constexpr size_t PRIKEY_SIZE = 32;
    constexpr size_t NONCE_SIZE = 32;
    constexpr size_t HASH_SIZE = 32;

    Psk GeneratePsk();

    class Client : public IAuthenticationPeer
    {
    public:
        Client(
            const Psk& psk,
            const std::vector<uint8_t>& keyIndex,
            const std::map<HandshakeMessage::Type, std::vector<uint8_t>>& additionalElements = {});

        std::optional<HandshakeMessage::Message> GetNextMessage(
            const std::optional<HandshakeMessage::Message>& peerMessage) override;
        bool IsHandshakeComplete() override;
        std::array<uint8_t, KEY_SIZE> GetClientKey() override;
        std::array<uint8_t, KEY_SIZE> GetServerKey() override;
    private:
        enum class Step
        {
            Init,
            ClientMessage,
            ServerMessage,
            ServerConfirmation
        };
        
        Psk _psk;
        std::map<HandshakeMessage::Type, std::vector<uint8_t>> _firstMessageAdditionalElements{};
        std::array<uint8_t, PRIKEY_SIZE> _priKey{};
        std::optional<HandshakeMessage::Message> _clientMessage{std::nullopt};
        std::array<uint8_t, KEY_SIZE> _serverConfirmKey{};
        std::array<uint8_t, KEY_SIZE> _clientKey{};
        std::array<uint8_t, KEY_SIZE> _serverKey{};
        std::array<uint8_t, HASH_SIZE> _transcriptHash{};
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
        HandshakeMessage::Message GetClientMessage();
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
        HandshakeMessage::Message TakeServerMessage(
            const HandshakeMessage::Message& serverMessage);

        /**
         * @brief Step 5
         * 
         * @param serverMessage
         */
        void TakeServerConfirmation(const HandshakeMessage::Message& serverMessage);
    };

    class Server : public IAuthenticationPeer
    {
    public:

        Server(
            std::function<Psk(const std::vector<uint8_t>&)> getPsk);

        std::optional<HandshakeMessage::Message> GetNextMessage(
            const std::optional<HandshakeMessage::Message>& peerMessage) override;
        bool IsHandshakeComplete() override;
        std::array<uint8_t, KEY_SIZE> GetClientKey() override;
        std::array<uint8_t, KEY_SIZE> GetServerKey() override;
    private:
        enum class Step
        {
            Init,
            ClientMessage,
            ClientConfirmation,
        };

        std::function<Psk(const std::vector<uint8_t>&)> _getPsk;
        std::array<uint8_t, KEY_SIZE> _clientConfirmKey{};
        std::array<uint8_t, KEY_SIZE> _serverConfirmKey{};
        std::array<uint8_t, KEY_SIZE> _clientKey{};
        std::array<uint8_t, KEY_SIZE> _serverKey{};
        std::array<uint8_t, HASH_SIZE> _transcriptHash{};
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
        HandshakeMessage::Message TakeClientMessage(
            const HandshakeMessage::Message& clientMessage);

        /**
         * @brief Step 4
         * 
         * @param clientConfirmation
         * @return HandshakeMessage with:
         * CipherMessage element (server confirmation):
         * AES-GCM(server_confirm_key, transcript_hash)
         *                 Any
         */
        HandshakeMessage::Message TakeClientConfirmation(
            const HandshakeMessage::Message& clientConfirmation);
    };
}
