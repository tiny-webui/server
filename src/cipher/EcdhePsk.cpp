#include <cstring>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/param_build.h>
#include <openssl/core_names.h>
#include "EcdhePsk.h"
#include "XChaCha20Poly1305.h"

using namespace TUI::Cipher;
using namespace TUI::Cipher::EcdhePsk;

static Openssl::EvpPkey GeneratePriKey()
{
    Openssl::EvpPkeyCtx keyGenCtx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr));
    int rc = EVP_PKEY_keygen_init(keyGenCtx);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot init keygen context");
    }
    EVP_PKEY* pkey = nullptr;
    rc = EVP_PKEY_keygen(keyGenCtx, &pkey);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot generate key pair");
    }
    return Openssl::EvpPkey(pkey);
}

static std::vector<uint8_t> GetRawPubKeyFromPriKey(const Openssl::EvpPkey& priKey)
{
    size_t pubKeyLen = 0;
    int rc = EVP_PKEY_get_raw_public_key(priKey, nullptr, &pubKeyLen);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot get public key length");
    }
    if (pubKeyLen != PUBKEY_SIZE)
    {
        throw std::runtime_error("Invalid public key length");
    }
    std::vector<uint8_t> pubKey(PUBKEY_SIZE, 0);
    rc = EVP_PKEY_get_raw_public_key(priKey, pubKey.data(), &pubKeyLen);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot get public key");
    }
    return pubKey;
}

static std::vector<uint8_t> GenerateNonce(size_t size)
{
    std::vector<uint8_t> nonce(size, 0);
    int rc = RAND_priv_bytes(nonce.data(), size);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot generate nonce");
    }
    return nonce;
}

static std::vector<uint8_t> GenerateZ(
    const Openssl::EvpPkey& priKey,
    const Openssl::EvpPkey& pubKey)
{
    Openssl::EvpPkeyCtx deriveCtx{EVP_PKEY_CTX_new(priKey, nullptr)};
    int rc = EVP_PKEY_derive_init(deriveCtx);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot init derive context");
    }
    rc = EVP_PKEY_derive_set_peer(deriveCtx, pubKey);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot set peer public key");
    }
    size_t keyLen = 0;
    rc = EVP_PKEY_derive(deriveCtx, nullptr, &keyLen);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot derive key length");
    }
    std::vector<uint8_t> Z(keyLen, 0);
    rc = EVP_PKEY_derive(deriveCtx, Z.data(), &keyLen);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot derive key");
    }
    return Z;
}

static std::array<uint8_t, 32> GetTranscriptHash(
    const HandshakeMessage& clientMessage,
    const HandshakeMessage& serverMessage)
{
    auto clientMessageBytes = clientMessage.Serialize();
    auto serverMessageBytes = serverMessage.Serialize();
    std::vector<uint8_t> transcript(clientMessageBytes.size() + serverMessageBytes.size(), 0);
    std::copy(clientMessageBytes.begin(), clientMessageBytes.end(), transcript.begin());
    std::copy(serverMessageBytes.begin(), serverMessageBytes.end(), transcript.begin() + clientMessageBytes.size());
    std::vector<uint8_t> md(EVP_MAX_MD_SIZE, 0);
    size_t mdLen = 0;
    int rc = EVP_Q_digest(nullptr, "SHA256", nullptr,
        transcript.data(), transcript.size(), md.data(), &mdLen);
    if (rc != 1 || mdLen != 32)
    {
        throw std::runtime_error("Cannot calculate transcript hash");
    }
    std::array<uint8_t, 32> transcriptHash{};
    std::copy(md.begin(), md.begin() + 32, transcriptHash.begin());
    return transcriptHash;
}

/**
 * @brief 
 * 
 * @attention Not using reference for key materials because 
 *     the OSSL option does not take const pointer. 
 *     There is risk of modification.
 * 
 * @param key 
 * @param salt 
 * @param info 
 * @param keySize 
 * @return std::vector<uint8_t> 
 */
template<typename KeyContainer, typename SaltContainer>
static std::vector<uint8_t> GenerateKey(
    KeyContainer key, 
    SaltContainer salt,
    std::string info,
    size_t keySize)
{
    static_assert(
        std::is_same_v<typename KeyContainer::value_type, uint8_t> &&
        std::is_same_v<typename SaltContainer::value_type, uint8_t>,
        "Key and salt containers must contain uint8_t elements"
    );

    std::vector<uint8_t> generatedKey(keySize, 0);
    Openssl::EvpKdf kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
    Openssl::EvpKdfCtx kdfCtx(EVP_KDF_CTX_new(kdf));
    std::string mdName = SN_sha256;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(
            OSSL_KDF_PARAM_DIGEST, mdName.data(), mdName.size()),
        OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_KEY, key.data(), key.size()),
        OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_SALT, salt.data(), salt.size()),
        OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_INFO, const_cast<char*>(info.c_str()), info.length()),
        OSSL_PARAM_construct_end()
    };
    int rc = EVP_KDF_derive(kdfCtx, generatedKey.data(), generatedKey.size(), params);
    if (rc != 1)
    {
        throw std::runtime_error("Cannot derive " + info);
    }
    return generatedKey;
}

/** Client */

Client::Client(
    const Psk& psk,
    const std::vector<uint8_t>& keyIndex,
    const std::unordered_map<HandshakeMessage::Type, std::vector<uint8_t>>& additionalElements,
    size_t keySize)
    : _psk(psk), _keySize(keySize)
{
    if (keySize == 0)
    {
        throw std::invalid_argument("Key size must be greater than 0");
    }
    _firstMessageAdditionalElements.emplace(HandshakeMessage::Type::KeyIndex, keyIndex);
    for (const auto& element : additionalElements)
    {
        if (element.first == HandshakeMessage::Type::KeyIndex || element.first == HandshakeMessage::Type::CipherMessage)
        {
            throw std::invalid_argument("KeyIndex and CipherMessage elements are reserved for protocol use");
        }
        _firstMessageAdditionalElements.emplace(element.first, element.second);
    }
}

HandshakeMessage Client::GetClientMessage()
{
    auto marker = _stepChecker->CheckStep(Step::Init, Step::ClientMessage);
    /** Generate key pair */
    _pkey = GeneratePriKey();
    auto pubkey = GetRawPubKeyFromPriKey(_pkey);
    /** Generate nonce */
    auto nonce = GenerateNonce(NONCE_SIZE);
    /** Generate client message */
    std::vector<uint8_t> clientMessage(PUBKEY_SIZE + NONCE_SIZE, 0);
    {
        std::copy(pubkey.begin(), pubkey.end(), clientMessage.begin());
        std::copy(nonce.begin(), nonce.end(), clientMessage.begin() + PUBKEY_SIZE);
    }
    /** Assemble handshake message */
    _firstMessageAdditionalElements.emplace(
        HandshakeMessage::Type::CipherMessage, std::move(clientMessage));
    _clientMessage = HandshakeMessage(_firstMessageAdditionalElements);
    return _clientMessage.value();
}

HandshakeMessage Client::TakeServerMessage(const HandshakeMessage& handshakeMessage)
{
    auto marker = _stepChecker->CheckStep(Step::ClientMessage, Step::ServerMessage);
    auto serverMessageOpt = handshakeMessage.GetElement(HandshakeMessage::Type::CipherMessage);
    if (!serverMessageOpt.has_value())
    {
        throw std::runtime_error("Server message is missing CipherMessage element");
    }
    auto serverMessage = serverMessageOpt.value();
    if (serverMessage.size() != PUBKEY_SIZE + NONCE_SIZE)
    {
        throw std::runtime_error("Invalid server message size");
    }
    /** Generate shared secret */
    Openssl::EvpPkey serverPubKey{EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, nullptr, serverMessage.data(), PUBKEY_SIZE)};
    auto Z = GenerateZ(_pkey, serverPubKey);
    /** Generate prk */
    auto prk = GenerateKey(Z, _psk, "prk", 32);
    /** Calculate transcript hash */
    _transcriptHash = GetTranscriptHash(_clientMessage.value(), handshakeMessage);
    /** Generate confirmation keys and session keys */
    auto clientConfirmKey = GenerateKey(prk, _transcriptHash.value(), "client confirm key", _keySize);
    _serverConfirmKey = GenerateKey(prk, _transcriptHash.value(), "server confirm key", _keySize);
    _clientKey = GenerateKey(prk, _transcriptHash.value(), "client key", _keySize);
    _serverKey = GenerateKey(prk, _transcriptHash.value(), "server key", _keySize);
    /** Generate client confirmation message */
    std::vector<uint8_t> clientConfirmMessage{};
    {
        XChaCha20Poly1305::Encryptor encryptor{clientConfirmKey};
        clientConfirmMessage = encryptor.Encrypt(_transcriptHash.value());
    }
    /** Assemble handshake message */
    HandshakeMessage clientConfirmation{
        {{HandshakeMessage::Type::CipherMessage, std::move(clientConfirmMessage)}}};
    return clientConfirmation;
}

void Client::TakeServerConfirmation(const HandshakeMessage& handshakeMessage)
{
    auto marker = _stepChecker->CheckStep(Step::ServerMessage, Step::ServerConfirmation);
    auto serverConfirmOpt = handshakeMessage.GetElement(HandshakeMessage::Type::CipherMessage);
    if (!serverConfirmOpt.has_value())
    {
        throw std::runtime_error("Server confirmation is missing CipherMessage element");
    }
    auto serverConfirm = serverConfirmOpt.value();
    if (!_serverConfirmKey.has_value())
    {
        throw std::runtime_error("Server confirmation key is not available");
    }
    XChaCha20Poly1305::Decryptor decryptor{_serverConfirmKey.value()};
    auto decryptedHash = decryptor.Decrypt(serverConfirm);
    if (!_transcriptHash.has_value())
    {
        throw std::runtime_error("Transcript hash is not available");
    }
    if (!std::equal(
        decryptedHash.begin(), decryptedHash.end(),
        _transcriptHash->begin(), _transcriptHash->end()))
    {
        throw std::runtime_error("Server confirmation does not match transcript hash");
    }
}

std::optional<HandshakeMessage> Client::GetNextMessage(
    const std::optional<HandshakeMessage>& peerMessage)
{
    switch (_numCalls++)
    {
        case 0:
            if (peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is unexpected");
            }
            return GetClientMessage();
        case 1:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            return TakeServerMessage(peerMessage.value());
        case 2:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            TakeServerConfirmation(peerMessage.value());
            return std::nullopt;
        default:
            throw std::runtime_error("Exceeding max call count");
    }
}

bool Client::IsHandshakeComplete()
{
    return _stepChecker->GetCurrentStep() == Step::ServerConfirmation;
}

std::vector<uint8_t> Client::GetClientKey()
{
    auto marker = _stepChecker->CheckStep(Step::ServerConfirmation, Step::ServerConfirmation);
    if (!_clientKey.has_value())
    {
        throw std::runtime_error("Client key is not available");
    }
    return _clientKey.value();
}

std::vector<uint8_t> Client::GetServerKey()
{
    auto marker = _stepChecker->CheckStep(Step::ServerConfirmation, Step::ServerConfirmation);
    if (!_serverKey.has_value())
    {
        throw std::runtime_error("Server key is not available");
    }
    return _serverKey.value();
}

/** Server */

Server::Server(
    std::function<Psk(const std::vector<uint8_t>&)> getPsk,
    size_t keySize)
    : _getPsk(getPsk), _keySize(keySize)
{
    if (getPsk == nullptr)
    {
        throw std::invalid_argument("getPsk function cannot be null");
    }
    if (keySize == 0)
    {
        throw std::invalid_argument("Key size must be greater than 0");
    }
}

HandshakeMessage Server::TakeClientMessage(const HandshakeMessage& handshakeMessage)
{
    auto marker = _stepChecker->CheckStep(Step::Init, Step::ClientMessage);
    auto keyIndexOpt = handshakeMessage.GetElement(HandshakeMessage::Type::KeyIndex);
    if (!keyIndexOpt.has_value())
    {
        throw std::runtime_error("Handshake message is missing KeyIndex element");
    }
    auto psk = _getPsk(keyIndexOpt.value());
    auto clientMessageOpt = handshakeMessage.GetElement(HandshakeMessage::Type::CipherMessage);
    if (!clientMessageOpt.has_value())
    {
        throw std::runtime_error("Handshake message is missing CipherMessage element");
    }
    auto clientMessage = clientMessageOpt.value();
    if (clientMessage.size() != PUBKEY_SIZE + NONCE_SIZE)
    {
        throw std::runtime_error("Invalid client message size");
    }
    /** Generate key pair */
    auto priKey = GeneratePriKey();
    auto pubKey = GetRawPubKeyFromPriKey(priKey);
    /** Generate nonce */
    auto nonce = GenerateNonce(NONCE_SIZE);
    /** Generate server message */
    std::vector<uint8_t> cipherMessage(PUBKEY_SIZE + NONCE_SIZE, 0);
    {
        std::copy(pubKey.begin(), pubKey.end(), cipherMessage.begin());
        std::copy(nonce.begin(), nonce.end(), cipherMessage.begin() + PUBKEY_SIZE);
    }
    /** Assemble handshake message */
    HandshakeMessage serverMessage{
        {{HandshakeMessage::Type::CipherMessage, std::move(cipherMessage)}}}; 
    /** Calculate Z */
    Openssl::EvpPkey clientPubKey{EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, nullptr, clientMessage.data(), PUBKEY_SIZE)};
    auto Z = GenerateZ(priKey, clientPubKey);
    /** Generate prk */
    auto prk = GenerateKey(Z, psk, "prk", 32);
    /** Calculate transcript hash */
    _transcriptHash = GetTranscriptHash(handshakeMessage, serverMessage);
    /** Generate confirmation keys and session keys */
    _serverConfirmKey = GenerateKey(prk, _transcriptHash.value(), "server confirm key", _keySize);
    _clientConfirmKey = GenerateKey(prk, _transcriptHash.value(), "client confirm key", _keySize);
    _clientKey = GenerateKey(prk, _transcriptHash.value(), "client key", _keySize);
    _serverKey = GenerateKey(prk, _transcriptHash.value(), "server key", _keySize);
    return serverMessage;
}

HandshakeMessage Server::TakeClientConfirmation(const HandshakeMessage& handshakeMessage)
{
    auto marker = _stepChecker->CheckStep(Step::ClientMessage, Step::ClientConfirmation);
    /** validate client confirm */
    {
        auto clientConfirmOpt = handshakeMessage.GetElement(HandshakeMessage::Type::CipherMessage);
        if (!clientConfirmOpt.has_value())
        {
            throw std::runtime_error("Client confirmation is missing CipherMessage element");
        }
        auto clientConfirm = clientConfirmOpt.value();
        if (!_clientConfirmKey.has_value())
        {
            throw std::runtime_error("Client confirmation key is not available");
        }
        XChaCha20Poly1305::Decryptor decryptor{_clientConfirmKey.value()};
        auto decryptedHash = decryptor.Decrypt(clientConfirm);
        if (!_transcriptHash.has_value())
        {
            throw std::runtime_error("Transcript hash is not available");
        }
        if (!std::equal(
            decryptedHash.begin(), decryptedHash.end(),
            _transcriptHash->begin(), _transcriptHash->end()))
        {
            throw std::runtime_error("Client confirmation does not match transcript hash");
        }
    }
    /** Generate server confirm */
    std::vector<uint8_t> cipherMessage{};
    {
        if (!_serverConfirmKey.has_value())
        {
            throw std::runtime_error("Server confirmation key is not available");
        }
        XChaCha20Poly1305::Encryptor encryptor{_serverConfirmKey.value()};
        cipherMessage = encryptor.Encrypt(_transcriptHash.value());
    }
    /** Assemble handshake message */
    HandshakeMessage serverConfirmation{
        {{HandshakeMessage::Type::CipherMessage, std::move(cipherMessage)}}};
    return serverConfirmation;
}

std::optional<HandshakeMessage> Server::GetNextMessage(
    const std::optional<HandshakeMessage>& peerMessage)
{
    switch (_numCalls++)
    {
        case 0:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            return TakeClientMessage(peerMessage.value());
        case 1:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            return TakeClientConfirmation(peerMessage.value());
        default:
            throw std::runtime_error("Exceeding max call count");
    }
}

bool Server::IsHandshakeComplete()
{
    return _stepChecker->GetCurrentStep() == Step::ClientConfirmation;
}

std::vector<uint8_t> Server::GetClientKey()
{
    auto marker = _stepChecker->CheckStep(Step::ClientConfirmation, Step::ClientConfirmation);
    if (!_clientKey.has_value())
    {
        throw std::runtime_error("Client key is not available");
    }
    return _clientKey.value();
}

std::vector<uint8_t> Server::GetServerKey()
{
    auto marker = _stepChecker->CheckStep(Step::ClientConfirmation, Step::ClientConfirmation);
    if (!_serverKey.has_value())
    {
        throw std::runtime_error("Server key is not available");
    }
    return _serverKey.value();
}

