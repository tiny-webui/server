#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <map>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/AsyncGenerator.h>
#include <js-style-co-routine/Promise.h>
#include "network/IConnection.h"
#include "network/IServer.h"
#include "CallerId.h"
#include "cipher/XChaCha20Poly1305.h"
#include "cipher/EcdhePsk.h"
#include "cipher/FakeCredentialGenerator.h"
#include "cipher/BruteForceLimiter.h"
#include "common/Cache.h"

namespace TUI::Application::SecureSession
{
    class Connection : public Network::IConnection<CallerId>
    {
    public:
        /** @todo Compression? */
        Connection(
            std::shared_ptr<Network::IConnection<void>> connection,
            const CallerId& callerId,
            Cipher::XChaCha20Poly1305::Encryptor encryptor,
            Cipher::XChaCha20Poly1305::Decryptor decryptor,
            bool turnOffEncryption,
            std::function<void(CallerId)> onClose);
        ~Connection() override;
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        Connection(Connection&&) = delete;
        Connection& operator=(Connection&&) = delete;

        void Close() override;
        bool IsClosed() const noexcept override;
        void Send(std::vector<std::uint8_t> message) override;
        JS::Promise<std::optional<std::vector<std::uint8_t>>> ReceiveAsync() override;
        CallerId GetId() const override;

    private:
        std::shared_ptr<Network::IConnection<void>> _connection;
        CallerId _callerId;
        Cipher::XChaCha20Poly1305::Encryptor _encryptor;
        Cipher::XChaCha20Poly1305::Decryptor _decryptor;
        bool _turnOffEncryption;
        std::function<void(CallerId)> _onClose;
        bool _closed{false};
    };

    class Server : public Network::IServer<CallerId>, public std::enable_shared_from_this<Server>
    {
    public:
        /** 10 seconds */
        static constexpr uint64_t AUTH_TIMEOUT_MS = 10 * 1000;
        /** 5 minutes */
        static constexpr uint64_t RESUMPTION_KEY_TIMEOUT_MS = 5 * 60 * 1000;

        using GetUserCredentialFunc = std::function<std::optional<std::pair<std::string, Common::Uuid>>(const std::string&)>;

        enum class ProtocolType : uint8_t
        {
            Password = 0,
            Psk = 1,
        };

        static std::shared_ptr<Network::IServer<CallerId>> Create(
            Tev& tev,
            std::shared_ptr<Network::IServer<void>> server,
            GetUserCredentialFunc getUserCredential);
        ~Server() override;
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        Server(Server&&) = delete;
        Server& operator=(Server&&) = delete;

        void Close() override;
        bool IsClosed() const noexcept override;
        JS::Promise<std::optional<std::shared_ptr<Network::IConnection<CallerId>>>> AcceptAsync() override;

    private:
        Tev& _tev;
        std::shared_ptr<Network::IServer<void>> _server;
        std::function<std::optional<std::pair<std::string, Common::Uuid>>(const std::string&)> _getUserCredential;
        JS::AsyncGenerator<std::shared_ptr<Network::IConnection<CallerId>>> _connectionGenerator{};
        std::map<std::string, std::pair<Cipher::EcdhePsk::Psk, CallerId>> _sessionResumptionKeys{};
        std::map<std::string, Tev::Timeout> _resumptionKeyTimeouts{};
        std::unordered_map<CallerId, std::shared_ptr<Connection>> _connections{};
        bool _closed{false};
        Cipher::FakeCredentialGenerator _fakeCredentialGenerator{10000};
        /** 5 trials per window, 5 min to 6 hours of lock out time. */
        Cipher::BruteForceLimiter _bruteForceLimiter{5, 5 * 60 * 1000, 6 * 60 * 60 * 1000};

        Server(
            Tev& tev,
            std::shared_ptr<Network::IServer<void>> server,
            GetUserCredentialFunc getUserCredential);

        JS::Promise<void> HandleRawConnections();
        JS::Promise<void> HandleHandshakeAsync(std::shared_ptr<Network::IConnection<void>> connection);
    };
}
