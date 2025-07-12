#pragma once

#include <array>
#include <string>
#include <cstdint>
#include <optional>
#include "IAuthenticationPeer.h"
#include "StepChecker.h"
#include "Ed25519.h"

namespace TUI::Cipher::Spake2p
{
    constexpr uint32_t ARGON2ID_LANES = 1;
    constexpr uint32_t ARGON2ID_MEM_COST_BYTES = 64 * 1024 * 1024;
    constexpr uint32_t ARGON2ID_ITERATIONS = 3;
    constexpr std::array<uint8_t, Ed25519::PubKey::size> M_BYTES = {
        0xd0, 0x48, 0x03, 0x2c, 0x6e, 0xa0, 0xb6, 0xd6, 
        0x97, 0xdd, 0xc2, 0xe8, 0x6b, 0xda, 0x85, 0xa3, 
        0x3a, 0xda, 0xc9, 0x20, 0xf1, 0xbf, 0x18, 0xe1, 
        0xb0, 0xc6, 0xd1, 0x66, 0xa5, 0xce, 0xcd, 0xaf
    };
    constexpr std::array<uint8_t, Ed25519::PubKey::size> N_BYTES = {
        0xd3, 0xbf, 0xb5, 0x18, 0xf4, 0x4f, 0x34, 0x30, 
        0xf2, 0x9d, 0x0c, 0x92, 0xaf, 0x50, 0x38, 0x65, 
        0xa1, 0xed, 0x32, 0x81, 0xdc, 0x69, 0xb3, 0x5d, 
        0xd8, 0x68, 0xba, 0x85, 0xf8, 0x86, 0xc4, 0xab
    };

    struct RegistrationResult
    {
        std::array<uint8_t, 32> w0;
        std::array<uint8_t, 32> L;
        std::array<uint8_t, 16> salt;
    };

    RegistrationResult Register(const std::string& username, const std::string& password);

    class Client : public IAuthenticationPeer
    {
    public:
        Client(
            const std::string& username,
            const std::string& password,
            const std::map<HandshakeMessage::Type, std::vector<uint8_t>>& additionalElements = {});
        
        std::optional<HandshakeMessage> GetNextMessage(
            const std::optional<HandshakeMessage>& peerMessage) override;
        bool IsHandshakeComplete() override;
        std::array<uint8_t, KEY_SIZE> GetClientKey() override;
        std::array<uint8_t, KEY_SIZE> GetServerKey() override;
    private:
        enum class Step
        {
            Init,
            RetrieveSalt,
            ShareP,
            ConfirmP,
        };

        std::string _username;
        std::string _password;
        std::map<HandshakeMessage::Type, std::vector<uint8_t>> _firstMessageAdditionalElements{};
        std::shared_ptr<StepChecker<Step>> _stepChecker{StepChecker<Step>::Create(Step::Init)};
        std::optional<Ed25519::Scalar> _w0{std::nullopt};
        std::optional<Ed25519::Scalar> _w1{std::nullopt};
        std::optional<Ed25519::Scalar> _x{std::nullopt};
        std::optional<Ed25519::Point> _X{std::nullopt};
        std::array<uint8_t, KEY_SIZE> _clientKey{};
        std::array<uint8_t, KEY_SIZE> _serverKey{};

        /**
         * @brief Retrieve the salt with username as the keyIndex.
         * 
         * @return HandshakeMessage with:
         * KeyIndex element:
         *  username (w/o null terminator)
         *              Any
         */
        HandshakeMessage RetrieveSalt();
        /**
         * @brief Get the Share P object
         * 
         * @param serverMessage 
         * @return HandshakeMessage with:
         * CipherMessage element:
         *   ShareP
         *    32B
         */
        HandshakeMessage GetShareP(const HandshakeMessage& serverMessage);
        /**
         * @brief Get the Confirm P object
         * 
         * @param serverMessage 
         * @return HandshakeMessage with:
         * CipherMessage element:
         *   MAC(K_confirmP, shareV)
         *    Not VA but don't care
         */
        HandshakeMessage GetConfirmP(const HandshakeMessage& serverMessage);
    };

    class Server : public IAuthenticationPeer
    {
    public:
        Server(std::function<RegistrationResult(const std::string&)> getUserRegistration);

        std::optional<HandshakeMessage> GetNextMessage(
            const std::optional<HandshakeMessage>& peerMessage) override;
        bool IsHandshakeComplete() override;
        std::array<uint8_t, KEY_SIZE> GetClientKey() override;
        std::array<uint8_t, KEY_SIZE> GetServerKey() override;
    private:
        enum class Step
        {
            Init,
            RetrieveSalt,
            ShareVConfirmV,
            ConfirmP,
        };

        std::string _username;
        std::function<RegistrationResult(const std::string&)> _getUserRegistration;
        std::optional<RegistrationResult> _registrationResult{std::nullopt};
        std::shared_ptr<StepChecker<Step>> _stepChecker{StepChecker<Step>::Create(Step::Init)};
        std::optional<Ed25519::Point> _Y{std::nullopt};
        std::array<uint8_t, KEY_SIZE> _clientKey{};
        std::array<uint8_t, KEY_SIZE> _serverKey{};
        std::array<uint8_t, KEY_SIZE> _confirmPKey{};

        /**
         * @brief Retrieve the salt with username as the keyIndex.
         * 
         * @param clientMessage 
         * @return HandshakeMessage with:
         * CipherMessage element:
         *    Salt
         *    16B
         */
        HandshakeMessage RetrieveSalt(const HandshakeMessage& clientMessage);
        /**
         * @brief Get the Share V Confirm V object
         * 
         * @param clientMessage 
         * @return HandshakeMessage with:
         * cipherMessage element:
         *   ShareV | MAC(K_confirmV, shareP)
         *    32 B  | Not VA but don't care
         */
        HandshakeMessage GetShareVConfirmV(const HandshakeMessage& clientMessage);
        /**
         * @brief Take the Confirm P message from the client.
         * 
         * @param clientMessage 
         */
        void TakeConfirmP(const HandshakeMessage& clientMessage);
    };
}


