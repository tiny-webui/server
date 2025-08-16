#include <cstring>
#include <sodium/randombytes.h>
#include <sodium/crypto_kx.h>
#include <sodium/crypto_scalarmult.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_kdf_hkdf_sha256.h>
#include "EcdhePsk.h"
#include "ChaCha20Poly1305.h"

using namespace TUI::Cipher;
using namespace TUI::Cipher::EcdhePsk;

static_assert(IAuthenticationPeer::KEY_SIZE == sizeof(ChaCha20Poly1305::Key), "Key size mismatch");
static_assert(PUBKEY_SIZE == crypto_kx_PUBLICKEYBYTES, "Public key size mismatch");
static_assert(PRIKEY_SIZE == crypto_kx_SECRETKEYBYTES, "Private key size mismatch");
static_assert(PUBKEY_SIZE == crypto_scalarmult_BYTES, "Public key size mismatch for scalar multiplication");
static_assert(HASH_SIZE == crypto_generichash_BYTES, "Hash size mismatch");

static std::array<uint8_t, crypto_generichash_BYTES> GetTranscriptHash(
    const HandshakeMessage::Message& clientMessage,
    const HandshakeMessage::Message& serverMessage)
{
    auto clientMessageBytes = clientMessage.Serialize();
    auto serverMessageBytes = serverMessage.Serialize();
    std::vector<uint8_t> transcript(clientMessageBytes.size() + serverMessageBytes.size(), 0);
    std::copy(clientMessageBytes.begin(), clientMessageBytes.end(), transcript.begin());
    std::copy(serverMessageBytes.begin(), serverMessageBytes.end(), transcript.begin() + clientMessageBytes.size());
    std::array<uint8_t, crypto_generichash_BYTES> transcriptHash{};
    int rc = crypto_generichash(
        transcriptHash.data(), transcriptHash.size(),
        transcript.data(), transcript.size(),
        nullptr, 0);
    if (rc != 0)
    {
        throw std::runtime_error("Cannot calculate transcript hash");
    }
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
 * @return std::vector<uint8_t> 
 */
template<typename KeyMaterialContainer, typename SaltContainer>
static std::array<uint8_t, crypto_kdf_hkdf_sha256_KEYBYTES> HkdfExtractKey(
    const KeyMaterialContainer& keyMaterial,
    const SaltContainer& salt)
{
    static_assert(
        std::is_same_v<typename KeyMaterialContainer::value_type, uint8_t> &&
        std::is_same_v<typename SaltContainer::value_type, uint8_t>,
        "Key and salt containers must contain uint8_t elements"
    );
    std::array<uint8_t, crypto_kdf_hkdf_sha256_KEYBYTES> prk{};
    int rc = crypto_kdf_hkdf_sha256_extract(
        prk.data(),
        salt.data(), salt.size(),
        keyMaterial.data(), keyMaterial.size());
    if (rc != 0)
    {
        throw std::runtime_error("Cannot extract key");
    }
    return prk;
}

static std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> HkdfExpendKey(
    const std::array<uint8_t, crypto_kdf_hkdf_sha256_KEYBYTES>& prk,
    const std::string& info)
{
    std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> key{};
    int rc = crypto_kdf_hkdf_sha256_expand(
        key.data(), key.size(),
        info.data(), info.size(),
        prk.data());
    if (rc != 0)
    {
        throw std::runtime_error("Cannot expand key");
    }
    return key;
}

/** Static */

Psk EcdhePsk::GeneratePsk()
{
    Psk psk{};
    randombytes_buf(psk.data(), psk.size());
    return psk;
}

/** Client */

Client::Client(
    const Psk& psk,
    const std::vector<uint8_t>& keyIndex,
    const std::map<HandshakeMessage::Type, std::vector<uint8_t>>& additionalElements)
    : _psk(psk)
{
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

HandshakeMessage::Message Client::GetClientMessage()
{
    auto marker = _stepChecker->CheckStep(Step::Init, Step::ClientMessage);
    /** Generate key pair */
    std::array<uint8_t, PUBKEY_SIZE> pubKey{};
    crypto_kx_keypair(pubKey.data(), _priKey.data());
    /** Generate nonce */
    std::array<uint8_t, NONCE_SIZE> nonce{};
    randombytes_buf(nonce.data(), NONCE_SIZE);
    /** Generate client message */
    std::vector<uint8_t> clientMessage(PUBKEY_SIZE + NONCE_SIZE, 0);
    {
        std::copy(pubKey.begin(), pubKey.end(), clientMessage.begin());
        std::copy(nonce.begin(), nonce.end(), clientMessage.begin() + PUBKEY_SIZE);
    }
    /** Assemble handshake message */
    _firstMessageAdditionalElements.emplace(
        HandshakeMessage::Type::CipherMessage, std::move(clientMessage));
    _clientMessage = HandshakeMessage::Message(_firstMessageAdditionalElements);
    return _clientMessage.value();
}

HandshakeMessage::Message Client::TakeServerMessage(
    const HandshakeMessage::Message& handshakeMessage)
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
    std::array<uint8_t, PUBKEY_SIZE> Z{};
    int rc = crypto_scalarmult(Z.data(), _priKey.data(), serverMessage.data());
    if (rc != 0)
    {
        throw std::runtime_error("Cannot generate shared secret");
    }
    /** Calculate transcript hash */
    _transcriptHash = GetTranscriptHash(_clientMessage.value(), handshakeMessage);
    /** Generate prk */
    std::vector<uint8_t> keyMaterial(Z.size() + _psk.size(), 0);
    std::copy(Z.begin(), Z.end(), keyMaterial.begin());
    std::copy(_psk.begin(), _psk.end(), keyMaterial.begin() + Z.size());
    auto prk = HkdfExtractKey(keyMaterial, _transcriptHash);
    /** Generate confirmation keys and session keys */
    auto clientConfirmKey = HkdfExpendKey(prk, "client confirm key");
    _serverConfirmKey = HkdfExpendKey(prk, "server confirm key");
    _clientKey = HkdfExpendKey(prk, "client key");
    _serverKey = HkdfExpendKey(prk, "server key");
    /** Generate client confirmation message */
    std::vector<uint8_t> clientConfirmMessage{};
    {
        ChaCha20Poly1305::Encryptor encryptor{clientConfirmKey};
        clientConfirmMessage = encryptor.Encrypt(_transcriptHash);
    }
    /** Assemble handshake message */
    HandshakeMessage::Message clientConfirmation{
        {{HandshakeMessage::Type::CipherMessage, std::move(clientConfirmMessage)}}};
    return clientConfirmation;
}

void Client::TakeServerConfirmation(
    const HandshakeMessage::Message& handshakeMessage)
{
    auto marker = _stepChecker->CheckStep(Step::ServerMessage, Step::ServerConfirmation);
    auto serverConfirmOpt = handshakeMessage.GetElement(HandshakeMessage::Type::CipherMessage);
    if (!serverConfirmOpt.has_value())
    {
        throw std::runtime_error("Server confirmation is missing CipherMessage element");
    }
    auto serverConfirm = serverConfirmOpt.value();
    ChaCha20Poly1305::Decryptor decryptor{_serverConfirmKey};
    auto decryptedHash = decryptor.Decrypt(serverConfirm);
    if (!std::equal(
        decryptedHash.begin(), decryptedHash.end(),
        _transcriptHash.begin(), _transcriptHash.end()))
    {
        throw std::runtime_error("Server confirmation does not match transcript hash");
    }
}

std::optional<HandshakeMessage::Message> Client::GetNextMessage(
    const std::optional<HandshakeMessage::Message>& peerMessage)
{
    switch (this->_stepChecker->GetCurrentStep())
    {
        case Step::Init:
            if (peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is unexpected");
            }
            return GetClientMessage();
        case Step::ClientMessage:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            return TakeServerMessage(peerMessage.value());
        case Step::ServerMessage:
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

std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> Client::GetClientKey()
{
    auto marker = _stepChecker->CheckStep(Step::ServerConfirmation, Step::ServerConfirmation);
    return _clientKey;
}

std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> Client::GetServerKey()
{
    auto marker = _stepChecker->CheckStep(Step::ServerConfirmation, Step::ServerConfirmation);
    return _serverKey;
}

/** Server */

Server::Server(
    std::function<Psk(const std::vector<uint8_t>&)> getPsk)
    : _getPsk(getPsk)
{
    if (getPsk == nullptr)
    {
        throw std::invalid_argument("getPsk function cannot be null");
    }
}

HandshakeMessage::Message Server::TakeClientMessage(
    const HandshakeMessage::Message& handshakeMessage)
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
    std::array<uint8_t, PUBKEY_SIZE> pubKey{};
    std::array<uint8_t, PRIKEY_SIZE> priKey{};
    crypto_kx_keypair(pubKey.data(), priKey.data());
    /** Generate nonce */
    std::array<uint8_t, NONCE_SIZE> nonce{};
    randombytes_buf(nonce.data(), NONCE_SIZE);
    /** Generate server message */
    std::vector<uint8_t> cipherMessage(PUBKEY_SIZE + NONCE_SIZE, 0);
    {
        std::copy(pubKey.begin(), pubKey.end(), cipherMessage.begin());
        std::copy(nonce.begin(), nonce.end(), cipherMessage.begin() + PUBKEY_SIZE);
    }
    /** Assemble handshake message */
    HandshakeMessage::Message serverMessage{
        {{HandshakeMessage::Type::CipherMessage, std::move(cipherMessage)}}}; 
    /** Calculate Z */
    std::array<uint8_t, PUBKEY_SIZE> Z{};
    int rc = crypto_scalarmult(Z.data(), priKey.data(), clientMessage.data());
    if (rc != 0)
    {
        throw std::runtime_error("Cannot generate shared secret");
    }
    /** Calculate transcript hash */
    _transcriptHash = GetTranscriptHash(handshakeMessage, serverMessage);
    /** Generate prk */
    std::vector<uint8_t> keyMaterial(Z.size() + psk.size(), 0);
    std::copy(Z.begin(), Z.end(), keyMaterial.begin());
    std::copy(psk.begin(), psk.end(), keyMaterial.begin() + Z.size());
    auto prk = HkdfExtractKey(keyMaterial, _transcriptHash);
    /** Generate confirmation keys and session keys */
    _clientConfirmKey = HkdfExpendKey(prk, "client confirm key");
    _serverConfirmKey = HkdfExpendKey(prk, "server confirm key");
    _clientKey = HkdfExpendKey(prk, "client key");
    _serverKey = HkdfExpendKey(prk, "server key");
    return serverMessage;
}

HandshakeMessage::Message Server::TakeClientConfirmation(
    const HandshakeMessage::Message& handshakeMessage)
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
        ChaCha20Poly1305::Decryptor decryptor{_clientConfirmKey};
        auto decryptedHash = decryptor.Decrypt(clientConfirm);
        if (!std::equal(
            decryptedHash.begin(), decryptedHash.end(),
            _transcriptHash.begin(), _transcriptHash.end()))
        {
            throw std::runtime_error("Client confirmation does not match transcript hash");
        }
    }
    /** Generate server confirm */
    std::vector<uint8_t> cipherMessage{};
    {
        ChaCha20Poly1305::Encryptor encryptor{_serverConfirmKey};
        cipherMessage = encryptor.Encrypt(_transcriptHash);
    }
    /** Assemble handshake message */
    HandshakeMessage::Message serverConfirmation{
        {{HandshakeMessage::Type::CipherMessage, std::move(cipherMessage)}}};
    return serverConfirmation;
}

std::optional<HandshakeMessage::Message> Server::GetNextMessage(
    const std::optional<HandshakeMessage::Message>& peerMessage)
{
    switch (this->_stepChecker->GetCurrentStep())
    {
        case Step::Init:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            return TakeClientMessage(peerMessage.value());
        case Step::ClientMessage:
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

std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> Server::GetClientKey()
{
    auto marker = _stepChecker->CheckStep(Step::ClientConfirmation, Step::ClientConfirmation);
    return _clientKey;
}

std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> Server::GetServerKey()
{
    auto marker = _stepChecker->CheckStep(Step::ClientConfirmation, Step::ClientConfirmation);
    return _serverKey;
}

