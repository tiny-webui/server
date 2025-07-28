#include <stdio.h>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include <js-style-co-routine/AsyncGenerator.h>
#include "common/Uuid.h"
#include "application/SecureSession.h"
#include "network/IConnection.h"
#include "network/IServer.h"
#include "cipher/Spake2p.h"
#include "schema/IServer.h"
#include "common/Base64.h"
#include "Utility.h"

using namespace TUI;
using namespace TUI::Application;

static JS::Promise<void> DelayAsync(Tev& tev, uint64_t delayMs)
{
    JS::Promise<void> promise;
    auto timeout = tev.SetTimeout([promise]() mutable {
        promise.Resolve();
    }, delayMs);
    co_await promise;
}

class TestConnection : public TUI::Network::IConnection<void>
{
public:
    TestConnection(Tev& tev, const std::string& username, const std::string& password)
        : _tev(tev)
    {
        std::map<Cipher::HandshakeMessage::Type, std::vector<uint8_t>> additionalElements = {
            {Cipher::HandshakeMessage::Type::ProtocolType, {static_cast<uint8_t>(SecureSession::Server::ProtocolType::Password)}}
        };
        _auth = std::make_shared<Cipher::Spake2p::Client>(username, password, additionalElements);
        StartAuth();
    }

    TestConnection(Tev& tev, const std::vector<uint8_t>& resumptionKeyIndex, const Cipher::EcdhePsk::Psk& resumptionKey)
        : _tev(tev)
    {
        std::map<Cipher::HandshakeMessage::Type, std::vector<uint8_t>> additionalElements = {
            {Cipher::HandshakeMessage::Type::ProtocolType, {static_cast<uint8_t>(SecureSession::Server::ProtocolType::Psk)}}
        };
        _auth = std::make_shared<Cipher::EcdhePsk::Client>(resumptionKey, resumptionKeyIndex, additionalElements);
        StartAuth();
    }

    ~TestConnection() override
    {
        Close();
    }
    TestConnection(const TestConnection&) = delete;
    TestConnection& operator=(const TestConnection&) = delete;
    TestConnection(TestConnection&&) = delete;
    TestConnection& operator=(TestConnection&&) = delete;

    void Close() override
    {
        if (_closed)
        {
            return;
        }
        _closed = true;
        _messageGenerator.Finish();
        if (!_handshakeComplete)
        {
            _handshakePromise.Reject("Connection closed before handshake completion");
        }
    }

    bool IsClosed() const noexcept override
    {
        return _closed;
    }

    void Send(std::vector<std::uint8_t> message) override
    {
        /** Send: rx */
        if (_closed)
        {
            throw std::runtime_error("Connection is closed");
        }
        if (_handshakeComplete)
        {
            throw std::runtime_error("Handshake is complete, no messages expected");
        }
        try
        {
            if (!_auth->IsHandshakeComplete())
            {
                /** Auth message */
                auto handshakeMessage = Cipher::HandshakeMessage::Message::Parse(
                    std::move(message));
                auto replyOpt = _auth->GetNextMessage(handshakeMessage);
                if (replyOpt.has_value())
                {
                    /** Push the sending to the next cycle to break the call stack. */
                    _tev.RunInNextCycle([=, this]() {
                            _messageGenerator.Feed(replyOpt->Serialize());
                        });
                }
                if (_auth->IsHandshakeComplete())
                {
                    /** Send the negotiation request */
                    Schema::IServer::ProtocolNegotiationRequest negotiationRequest;
                    negotiationRequest.set_turn_off_encryption(false);
                    auto negotiationRequestStr = static_cast<nlohmann::json>(negotiationRequest).dump();
                    std::vector<uint8_t> negotiationRequestBytes(negotiationRequestStr.begin(), negotiationRequestStr.end());
                    Cipher::ChaCha20Poly1305::Encryptor encryptor{_auth->GetClientKey()};
                    auto negotiationRequestCipher = encryptor.Encrypt(negotiationRequestBytes);
                    /** Push this message after the potential last auth reply to the server */
                    _tev.RunInNextCycle([=, this]() {
                            _messageGenerator.Feed(std::move(negotiationRequestCipher));
                        });
                }
            }
            else
            {
                /** Protocol negotiation message */
                Cipher::ChaCha20Poly1305::Decryptor decryptor{_auth->GetServerKey()};
                auto decryptedMessage = decryptor.Decrypt(message);
                std::string decryptedMessageStr(decryptedMessage.begin(), decryptedMessage.end());
                auto negotiationResponse = nlohmann::json::parse(decryptedMessageStr).get<Schema::IServer::ProtocolNegotiationResponse>();
                auto newResumptionKeyIndexStr = negotiationResponse.get_session_resumption_key_index();
                _newResumptionKeyIndex.emplace(
                    newResumptionKeyIndexStr.begin(), newResumptionKeyIndexStr.end());
                _newResumptionKey = Common::Base64::Decode<sizeof(Cipher::EcdhePsk::Psk)>(negotiationResponse.get_session_resumption_key());
                _handshakeComplete = true;
                _handshakePromise.Resolve();
            }
        }
        catch(...)
        {
            Close();
            /** DO NOT throw here. As this normally happens in another thread. */
        }
    }

    JS::Promise<std::optional<std::vector<std::uint8_t>>> ReceiveAsync() override
    {
        /** Receive: tx */
        auto messageOpt = co_await _messageGenerator.NextAsync();
        if (!messageOpt.has_value())
        {
            Close();
        }
        co_return messageOpt;
    }

    JS::Promise<void> WaitHandshakeAsync()
    {
        if (_closed)
        {
            throw std::runtime_error("Connection is closed");
        }
        if (_handshakeComplete)
        {
            JS::Promise<void> promise;
            promise.Resolve();
            return promise;
        }
        return _handshakePromise;
    }

    std::vector<uint8_t> GetResumptionKeyIndex() const
    {
        if (!_newResumptionKeyIndex.has_value())
        {
            throw std::runtime_error("No resumption key index available");
        }
        return _newResumptionKeyIndex.value();
    }

    Cipher::EcdhePsk::Psk GetResumptionKey() const
    {
        if (!_newResumptionKey.has_value())
        {
            throw std::runtime_error("No resumption key available");
        }
        return _newResumptionKey.value();
    }
private:
    Tev& _tev;
    std::shared_ptr<Cipher::IAuthenticationPeer> _auth{nullptr};
    JS::AsyncGenerator<std::vector<std::uint8_t>> _messageGenerator{};
    bool _closed{false};
    /** These are not the key and index used for this psk session. */
    std::optional<std::vector<uint8_t>> _newResumptionKeyIndex{std::nullopt};
    std::optional<Cipher::EcdhePsk::Psk> _newResumptionKey{std::nullopt};
    JS::Promise<void> _handshakePromise{};
    bool _handshakeComplete{false};

    void StartAuth()
    {
        auto message = _auth->GetNextMessage(std::nullopt);
        if (!message.has_value())
        {
            throw std::runtime_error("Failed to get the first authentication message");
        }
        /** 
         * Push the send to the next cycle to simulate the real interaction.
         * (Client & Server not on the same call stack)
         */
        _tev.RunInNextCycle([=, this](){
                _messageGenerator.Feed(message->Serialize());
            });
    }
};

class TestServer : public TUI::Network::IServer<void>
{
public:
    TestServer(Tev& tev, const std::string& username, const std::string& password)
        : _tev(tev), _username(username), _password(password)
    {
        RunTestAsync();
    }

    ~TestServer() override
    {
        Close();
    }
    TestServer(const TestServer&) = delete;
    TestServer& operator=(const TestServer&) = delete;
    TestServer(TestServer&&) = delete;
    TestServer& operator=(TestServer&&) = delete;

    void Close() override
    {
        if (_isClosed)
        {
            return;
        }
        _isClosed = true;
        _connectionGenerator.Finish();
    }

    bool IsClosed() const noexcept override
    {
        return _isClosed;
    }

    JS::Promise<std::optional<std::shared_ptr<Network::IConnection<void>>>> AcceptAsync() override
    {
        if (_isClosed)
        {
            throw std::runtime_error("Server is closed");
        }
        return _connectionGenerator.NextAsync();
    }

private:
    JS::Promise<void> RunTestAsync()
    {
        try
        {
            co_await DelayAsync(_tev, 100);
            auto connection = std::make_shared<TestConnection>(_tev, _username, _password);
            _connectionGenerator.Feed(connection);
            co_await connection->WaitHandshakeAsync();
            auto resumptionKeyIndex = connection->GetResumptionKeyIndex();
            auto resumptionKey = connection->GetResumptionKey();
            co_await DelayAsync(_tev, 100);
            connection->Close();
            co_await DelayAsync(_tev, 100);
            auto connection2 = std::make_shared<TestConnection>(_tev, resumptionKeyIndex, resumptionKey);
            _connectionGenerator.Feed(connection2);
            co_await connection2->WaitHandshakeAsync();
            co_await DelayAsync(_tev, 100);
            auto resumptionKeyIndex2 = connection2->GetResumptionKeyIndex();
            connection2->Close();
            /** Test wrong resumption key */
            try
            {
                co_await DelayAsync(_tev, 100);
                auto connection3 = std::make_shared<TestConnection>(_tev, resumptionKeyIndex2, Cipher::EcdhePsk::Psk{});
                _connectionGenerator.Feed(connection3);
                co_await connection3->WaitHandshakeAsync();
                AssertWithMessage(false, "Connection should not be established with wrong resumption key");
            }
            catch(...)
            {
            }
            /** Test wrong resumption index */
            try
            {
                co_await DelayAsync(_tev, 100);
                auto connection3 = std::make_shared<TestConnection>(_tev, std::vector<uint8_t>{0x01, 0x02, 0x03}, Cipher::EcdhePsk::Psk{});
                _connectionGenerator.Feed(connection3);
                co_await connection3->WaitHandshakeAsync();
                AssertWithMessage(false, "Connection should not be established with wrong resumption index");
            }
            catch(...)
            {
            }
            /** Test wrong username */
            try
            {
                co_await DelayAsync(_tev, 100);
                auto connection3 = std::make_shared<TestConnection>(_tev, "wrong_username", "wrong_password");
                _connectionGenerator.Feed(connection3);
                co_await connection3->WaitHandshakeAsync();
                AssertWithMessage(false, "Connection should not be established with wrong username");
            }
            catch(...)
            {
            }
            /** Test wrong password */
            try
            {
                co_await DelayAsync(_tev, 100);
                auto connection3 = std::make_shared<TestConnection>(_tev, _username, "wrong_password");
                _connectionGenerator.Feed(connection3);
                co_await connection3->WaitHandshakeAsync();
                AssertWithMessage(false, "Connection should not be established with wrong password");
            }
            catch(...)
            {
            }

            co_await DelayAsync(_tev, 100);
            Close();
        }
        catch(const std::exception& e)
        {
            std::cerr << "Error in TestServer: " << e.what() << std::endl;
            abort();
        }
    }

    Tev& _tev;
    std::string _username;
    std::string _password;
    bool _isClosed{false};
    JS::AsyncGenerator<std::shared_ptr<Network::IConnection<void>>> _connectionGenerator;
};

JS::Promise<void> HandleConnection(std::shared_ptr<Network::IConnection<CallerId>> connection)
{
    auto callerId = connection->GetId();
    std::cout << "Accepted connection, userId: " << 
        static_cast<std::string>(callerId.userId) <<
        ", connectionId: " << 
        static_cast<std::string>(callerId.connectionId) << 
        std::endl;
    while(true)
    {
        auto messageOpt = co_await connection->ReceiveAsync();
        if (!messageOpt.has_value())
        {
            std::cout << "Connection closed, userId: " << 
                static_cast<std::string>(callerId.userId) <<
                ", connectionId: " << 
                static_cast<std::string>(callerId.connectionId) << 
                std::endl;
            break;
        }
    }
}

JS::Promise<void> TestFlowAsync(Tev& tev)
{
    /** register test user */
    std::string username = "user";
    std::string password = "password";
    Common::Uuid userId{};
    auto registration = Cipher::Spake2p::Register(username, password);
    Schema::IServer::UserCredential userCredential;
    userCredential.set_w0(Common::Base64::Encode(registration.w0));
    userCredential.set_l(Common::Base64::Encode(registration.L));
    userCredential.set_salt(Common::Base64::Encode(registration.salt));
    auto userCredentialStr = static_cast<nlohmann::json>(userCredential).dump();
    auto testServer = std::make_shared<TestServer>(tev, username, password);
    auto secureSessionServer = SecureSession::Server::Create(
        tev,
        testServer,
        [=](const std::string& usernameIn) -> std::pair<std::string, Common::Uuid> {
            if (usernameIn != username)
            {
                throw std::runtime_error("Invalid username");
            }
            return {userCredentialStr, userId};
        });
    while(true)
    {
        auto connectionOpt = co_await secureSessionServer->AcceptAsync();
        if (!connectionOpt.has_value())
        {
            std::cout << "Server closed" << std::endl;
            break;
        }
        auto connection = std::move(connectionOpt.value());
        HandleConnection(connection);
    }
}

JS::Promise<void> TestAsync(Tev& tev)
{
    RunAsyncTest(TestFlowAsync(tev));
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    Tev tev{};
    TestAsync(tev);
    tev.MainLoop();

    return 0;
}

