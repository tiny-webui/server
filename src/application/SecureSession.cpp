#include <nlohmann/json.hpp>
#include "SecureSession.h"
#include "cipher/IAuthenticationPeer.h"
#include "cipher/Spake2p.h"
#include "cipher/EcdhePsk.h"
#include "cipher/HandshakeMessage.h"
#include "schema/IServer.h"
#include "common/Utilities.h"
#include "common/UniqueTypes.h"

using namespace TUI;
using namespace TUI::Network;
using namespace TUI::Application;
using namespace TUI::Application::SecureSession;

/** Session */

Connection::Connection(
    std::shared_ptr<IConnection<void>> connection,
    const CallerId& callerId,
    Cipher::ChaCha20Poly1305::Encryptor encryptor,
    Cipher::ChaCha20Poly1305::Decryptor decryptor,
    bool turnOffEncryption,
    std::function<void(CallerId)> onClose)
    : _connection(std::move(connection)), _callerId(callerId), _encryptor(std::move(encryptor)),
      _decryptor(std::move(decryptor)), _turnOffEncryption(turnOffEncryption), _onClose(std::move(onClose))
{
    if (_connection == nullptr)
    {
        throw std::invalid_argument("Connection cannot be null");
    }
    if (_onClose == nullptr)
    {
        throw std::invalid_argument("onClose callback cannot be null");
    }
}

Connection::~Connection()
{
    Close();
}

void Connection::Close()
{
    if (_closed)
    {
        return;
    }
    _closed = true;
    _connection->Close();
    _onClose(_callerId);
}

bool Connection::IsClosed() const noexcept
{
    return _closed;
}

void Connection::Send(std::vector<std::uint8_t> message)
{
    if (IsClosed())
    {
        throw std::runtime_error("Connection is closed");
    }
    if (!_turnOffEncryption)
    {
        message = _encryptor.Encrypt(message);
    }
    _connection->Send(std::move(message));
}

JS::Promise<std::optional<std::vector<std::uint8_t>>> Connection::ReceiveAsync()
{
    if (IsClosed())
    {
        throw std::runtime_error("Connection is closed");
    }
    auto dataOpt = co_await _connection->ReceiveAsync();
    if (!dataOpt.has_value())
    {
        Close();
        co_return std::nullopt;
    }
    auto data = std::move(dataOpt.value());
    if (!_turnOffEncryption)
    {
        data = _decryptor.Decrypt(data);
    }
    co_return std::move(data);
}

CallerId Connection::GetId() const
{
    return _callerId;
}

/** Server */

std::shared_ptr<IServer<CallerId>> Server::Create(
    Tev& tev,
    std::shared_ptr<IServer<void>> server,
    GetUserCredentialFunc getUserCredential)
{
    return std::shared_ptr<Server>(new Server(tev, server, getUserCredential));
}

Server::Server(
    Tev& tev,
    std::shared_ptr<IServer<void>> server,
    GetUserCredentialFunc getUserCredential)
    : _tev(tev), _server(server), _getUserCredential(getUserCredential)
{
    if (server == nullptr || getUserCredential == nullptr)
    {
        throw std::invalid_argument("Server and getUserCredential cannot be null");
    }
    /** Fire the raw connection handle loop */
    HandleRawConnections();
}

Server::~Server()
{
    Close();
}

void Server::Close()
{
    if (_closed)
    {
        return;
    }
    _closed = true;
    /** Clear all resumption timeouts */
    _resumptionKeyTimeouts.clear();
    /** Clear all resumption keys so the timeouts do not get set during close */
    _sessionResumptionKeys.clear();
    /** 
     * Explicitly closing all connections. 
     * Closeing the server should do so too. 
     */
    for (auto item = _connections.begin(); item != _connections.end();)
    {
        /** Erase first to avoid recursive callback. */
        item->second->Close();
        item = _connections.erase(item);
    }
    if (_server)
    {
        _server->Close();
    }
    _connectionGenerator.Finish();
}

bool Server::IsClosed() const noexcept
{
    return _closed;
}

JS::Promise<std::optional<std::shared_ptr<IConnection<CallerId>>>> Server::AcceptAsync()
{
    if (IsClosed())
    {
        throw std::runtime_error("Server is closed");
    }
    return _connectionGenerator.NextAsync();
}

JS::Promise<void> Server::HandleRawConnections()
{
    while (true)
    {
        auto connectionOpt = co_await _server->AcceptAsync();
        if (!connectionOpt.has_value())
        {
            break;
        }
        auto connection = connectionOpt.value();
        /** Fire handshake session */
        HandleHandshakeAsync(connection);
    }
    Close();
}

JS::Promise<void> Server::HandleHandshakeAsync(std::shared_ptr<IConnection<void>> connection)
{
    /** 
     * 10s handshake timeout 
     * This will not leak on exit. 
     * As the connection will be closed when the server is closed.
     * And the receive loop will break and clear the timeout.
     */
    
    std::optional<std::string> usernameOpt;
    try
    {
        auto handshakeTimeout = _tev.SetTimeout(
            [connection]() {
                connection->Close();
            }, AUTH_TIMEOUT_MS);
        std::shared_ptr<Cipher::IAuthenticationPeer> auth{nullptr};
        /** 
         * This will generate both the userId and the connectionId.'
         * The userId should be replaced by the real userId after authentication.
         */
        CallerId callerId{};
        while (true)
        {
            auto dataOpt = co_await connection->ReceiveAsync();
            if (!dataOpt.has_value())
            {
                throw std::runtime_error("Connection closed before handshake completion");
            }
            auto data = std::move(dataOpt.value());
            if (auth == nullptr || !auth->IsHandshakeComplete())
            {
                /** authentication messages */
                Cipher::HandshakeMessage message{data};
                if (auth == nullptr)
                {
                    /** First message, create auth from the ProtocolType */
                    auto protocolTypeElementOpt = message.GetElement(Cipher::HandshakeMessage::Type::ProtocolType);
                    if (!protocolTypeElementOpt.has_value())
                    {
                        throw std::runtime_error("ProtocolType element is missing in the handshake message");
                    }
                    auto protocolTypeElement = std::move(protocolTypeElementOpt.value());
                    if (protocolTypeElement.size() != sizeof(ProtocolType))
                    {
                        throw std::runtime_error("Invalid ProtocolType element size");
                    }
                    auto protocolType = static_cast<ProtocolType>(protocolTypeElement[0]);
                    switch (protocolType)
                    {
                    case ProtocolType::Password:
                        auth = std::make_shared<Cipher::Spake2p::Server>([this, &callerId, &usernameOpt](const std::string& username) -> Cipher::Spake2p::RegistrationResult {
                            Cipher::Spake2p::RegistrationResult result;
                            auto pairOpt = _getUserCredential(username);
                            if (!pairOpt.has_value())
                            {
                                /** invalid username. However, we need to trick the attacker to do the computational heavy handshake. */
                                result = _fakeCredentialGenerator.GetFakeCredential(username);
                            }
                            else
                            {
                                /** valid username */
                                if (_bruteForceLimiter.IsBlocked(username))
                                {
                                    /** 
                                     * The current username is under attack. 
                                     * We need to provide the correct salt to the attacker to not leaking this is a valid username.
                                     * But the other credentials should be fake.
                                     */
                                    result = _fakeCredentialGenerator.GetFakeCredential(username);
                                    auto pair = std::move(pairOpt.value());
                                    auto userCredential = nlohmann::json::parse(pair.first).get<Schema::IServer::UserCredential>();
                                    result.salt = Common::Utilities::HexToBytes<sizeof(result.salt)>(userCredential.get_salt());
                                }
                                else
                                {
                                    auto pair = std::move(pairOpt.value());
                                    auto userCredential = nlohmann::json::parse(pair.first).get<Schema::IServer::UserCredential>();
                                    result.w0 = Common::Utilities::HexToBytes<sizeof(result.w0)>(userCredential.get_w0());
                                    result.L = Common::Utilities::HexToBytes<sizeof(result.L)>(userCredential.get_l());
                                    result.salt = Common::Utilities::HexToBytes<sizeof(result.salt)>(userCredential.get_salt());
                                    callerId.userId = pair.second;
                                }
                                usernameOpt = username;
                            }
                            return result;
                        });
                        break;
                    case ProtocolType::Psk:
                        auth = std::make_shared<Cipher::EcdhePsk::Server>([this, &callerId](const std::vector<uint8_t>& keyIndex) -> Cipher::EcdhePsk::Psk {
                            auto keyIndexStr = std::string(keyIndex.begin(), keyIndex.end());
                            auto timeoutItem = _resumptionKeyTimeouts.find(keyIndexStr);
                            if (timeoutItem != _resumptionKeyTimeouts.end())
                            {
                                _resumptionKeyTimeouts.erase(timeoutItem);
                            }
                            auto item = _sessionResumptionKeys.find(keyIndexStr);
                            if (item == _sessionResumptionKeys.end())
                            {
                                throw std::runtime_error("PSK not found for the given key index");
                            }
                            auto pair = std::move(item->second);
                            _sessionResumptionKeys.erase(item);
                            callerId = pair.second;
                            return pair.first;
                        });
                        break;
                    default:
                        throw std::runtime_error("Unknown protocol type");
                    }
                }
                auto replyOpt = auth->GetNextMessage(message);
                if (replyOpt.has_value())
                {
                    auto replyData = replyOpt->Serialize();
                    connection->Send(std::move(replyData));
                }
            }
            else
            {
                /** 
                 * There are cases where the caller id is not set to a valid value when the username is not valid or blocked.
                 * But it is almost impossible to reach here by passing the handshake with a fake credential.
                 * So no further checks are implemented.
                 */
                /** authentication pass. This message would be the protocol negotiation message */
                Cipher::ChaCha20Poly1305::Decryptor decryptor{auth->GetClientKey()};
                auto plainTextBytes = decryptor.Decrypt(data);
                std::string plainText(plainTextBytes.begin(), plainTextBytes.end());
                auto negotiationRequest = nlohmann::json::parse(plainText).get<Schema::IServer::ProtocolNegotiationRequest>();
                /** Close any session with the same callerId (in case of a valid resumption) */
                auto item = _connections.find(callerId);
                if (item != _connections.end())
                {
                    auto oldConnection = item->second;
                    _connections.erase(item);
                    oldConnection->Close();
                }
                /** Regenerate connection id */
                callerId.connectionId = Common::Uuid{};
                /** Generate resumption keys */
                Common::Uuid resumptionKeyIndex{};
                auto resumptionKey = Cipher::EcdhePsk::GeneratePsk();
                Schema::IServer::ProtocolNegotiationResponse negotiationResponse;
                negotiationResponse.set_session_resumption_key_index(static_cast<std::string>(resumptionKeyIndex));
                negotiationResponse.set_session_resumption_key(Common::Utilities::BytesToHex(resumptionKey));
                negotiationResponse.set_was_under_attack(false);
                if (usernameOpt.has_value())
                {
                    bool wasUnderAttack = _bruteForceLimiter.LogValidLogin(usernameOpt.value());
                    negotiationResponse.set_was_under_attack(wasUnderAttack);
                    /** Avoid following exceptions to trigger the invalid login log */
                    usernameOpt.reset();
                }
                _sessionResumptionKeys.emplace(
                    static_cast<std::string>(resumptionKeyIndex),
                    std::make_pair(resumptionKey, callerId));
                /** Reply negotiation. This will always be encrypted no matter how turnOffEncryption is set */
                auto negotiationResponseStr = static_cast<nlohmann::json>(negotiationResponse).dump();
                std::vector<uint8_t> negotiationResponseBytes(negotiationResponseStr.begin(), negotiationResponseStr.end());
                Cipher::ChaCha20Poly1305::Encryptor encryptor{auth->GetServerKey()};
                auto negotiationResponseCipher = encryptor.Encrypt(negotiationResponseBytes);
                connection->Send(std::move(negotiationResponseCipher));
                /** Create the secure connection */
                std::weak_ptr<Server> weakThis = shared_from_this();
                auto secureConnection = std::make_shared<Connection>(
                    connection,
                    callerId,
                    encryptor,
                    decryptor,
                    negotiationRequest.get_turn_off_encryption(),
                    [weakThis, resumptionKeyIndex](CallerId id) {
                        auto self = weakThis.lock();
                        if (!self)
                        {
                            return;
                        }
                        self->_connections.erase(id);
                        /** If the resumption key exists, set a timeout */
                        auto resumptionKeyIndexStr = static_cast<std::string>(resumptionKeyIndex);
                        auto item = self->_sessionResumptionKeys.find(resumptionKeyIndexStr);
                        if (item == self->_sessionResumptionKeys.end())
                        {
                            return;
                        }
                        auto resumptionKeyTimeout = self->_tev.SetTimeout(
                            [weakThis, resumptionKeyIndexStr]() {
                                auto self = weakThis.lock();
                                if (!self)
                                {
                                    return;
                                }
                                self->_resumptionKeyTimeouts.erase(resumptionKeyIndexStr);
                                self->_sessionResumptionKeys.erase(resumptionKeyIndexStr);
                            }, RESUMPTION_KEY_TIMEOUT_MS);
                        self->_resumptionKeyTimeouts.emplace(
                            resumptionKeyIndexStr,
                            std::move(resumptionKeyTimeout));
                    });
                _connections.emplace(callerId, secureConnection);
                /** Add the connection to the generator */
                _connectionGenerator.Feed(secureConnection);
                break;
            }
        }
    }
    catch(...)
    {
        if (usernameOpt.has_value())
        {
            /** Invalid login attempt for this username */
            _bruteForceLimiter.LogInvalidLogin(usernameOpt.value());
        }
        /** Under any error, close the connection */
        connection->Close();
    }
}
