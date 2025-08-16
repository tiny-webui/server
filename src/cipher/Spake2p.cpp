#include <stdexcept>
#include <vector>
#include <sodium/randombytes.h>
#include <sodium/crypto_pwhash_argon2id.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_kdf_hkdf_sha256.h>
#include "Spake2p.h"
#include "Ed25519.h"
#include "ChaCha20Poly1305.h"

using namespace TUI::Cipher;
using namespace TUI::Cipher::Spake2p;

static_assert(ARGON2ID_LANES == 1, "SPAKE2p only supports 1 lane for now");
static_assert(sizeof(RegistrationResult::salt) == crypto_pwhash_argon2id_SALTBYTES, "Salt size mismatch");
static_assert(sizeof(RegistrationResult::w0) == Ed25519::Scalar::size, "w0 size mismatch");
static_assert(sizeof(RegistrationResult::L) == Ed25519::Point::size, "L size mismatch");
static_assert(crypto_generichash_BYTES == crypto_kdf_hkdf_sha256_KEYBYTES,
              "Hash size mismatch for HKDF expansion");

static constexpr std::string_view HASH_CONTEXT = "TUI";
static constexpr std::string_view ID_VERIFIER = "tui-server";

static std::pair<std::array<uint8_t, sizeof(RegistrationResult::w0)>, std::array<uint8_t, sizeof(RegistrationResult::w0)>>
DeriveW0W1(
    const std::string& username, const std::string& password,
    const std::array<uint8_t, sizeof(RegistrationResult::salt)>& salt)
{
    int rc = 0;
    /** len(password) | password | len(username) | username | len(ID_VERIFIER) | ID_VERIFIER */
    std::vector<uint8_t> keyMaterial{};
    {
        uint16_t passwordLen = static_cast<uint16_t>(password.size());
        keyMaterial.insert(
            keyMaterial.end(),
            reinterpret_cast<uint8_t*>(&passwordLen),
            reinterpret_cast<uint8_t*>(&passwordLen) + sizeof(passwordLen));
        keyMaterial.insert(keyMaterial.end(), password.begin(), password.end());
        uint16_t usernameLen = static_cast<uint16_t>(username.size());
        keyMaterial.insert(
            keyMaterial.end(),
            reinterpret_cast<uint8_t*>(&usernameLen),
            reinterpret_cast<uint8_t*>(&usernameLen) + sizeof(usernameLen));
        keyMaterial.insert(keyMaterial.end(), username.begin(), username.end());
        uint16_t idVerifierLen = static_cast<uint16_t>(ID_VERIFIER.size());
        keyMaterial.insert(
            keyMaterial.end(),
            reinterpret_cast<uint8_t*>(&idVerifierLen),
            reinterpret_cast<uint8_t*>(&idVerifierLen) + sizeof(idVerifierLen));
        keyMaterial.insert(keyMaterial.end(), ID_VERIFIER.begin(), ID_VERIFIER.end());
    }
    std::array<uint8_t, Ed25519::Scalar::keyMaterialSize> w0s{};
    std::array<uint8_t, Ed25519::Scalar::keyMaterialSize> w1s{};
    {
        std::array<uint8_t, Ed25519::Scalar::keyMaterialSize * 2> key{};
        /** Lane defaults to 1 with libsodium */
        rc = crypto_pwhash_argon2id(
            key.data(), key.size(),
            reinterpret_cast<const char*>(keyMaterial.data()), keyMaterial.size(),
            salt.data(),
            ARGON2ID_ITERATIONS,
            ARGON2ID_MEM_COST_BYTES,
            crypto_pwhash_argon2id_ALG_ARGON2ID13);
        if (rc != 0)
        {
            throw std::runtime_error("Failed to derive key using Argon2ID");
        }
        std::copy(key.begin(), key.begin() + Ed25519::Scalar::keyMaterialSize, w0s.begin());
        std::copy(key.begin() + Ed25519::Scalar::keyMaterialSize, key.end(), w1s.begin());
    }
    auto w0 = Ed25519::Scalar::Reduce(w0s);
    auto w1 = Ed25519::Scalar::Reduce(w1s);
    return {w0.Dump(), w1.Dump()};
}

static std::array<uint8_t, crypto_generichash_BYTES> GetTranscriptHash(
    const std::string& context,
    const std::string& idProver,
    const std::string& idVerifier,
    const Ed25519::Point& X,
    const Ed25519::Point& Y,
    const Ed25519::Point& Z,
    const Ed25519::Point& V,
    const Ed25519::Scalar& w0)
{
    auto XBytes = X.Dump();
    auto YBytes = Y.Dump();
    auto ZBytes = Z.Dump();
    auto VBytes = V.Dump();
    auto w0Bytes = w0.Dump();
    std::vector<uint8_t> transcript;
    transcript.reserve(
        context.size() + 
        idProver.size() + idVerifier.size() +
        M_BYTES.size() + N_BYTES.size() + 
        XBytes.size() + YBytes.size() + 
        ZBytes.size() + VBytes.size() + 
        w0Bytes.size());
    transcript.insert(transcript.end(), context.begin(), context.end());
    transcript.insert(transcript.end(), idProver.begin(), idProver.end());
    transcript.insert(transcript.end(), idVerifier.begin(), idVerifier.end());
    transcript.insert(transcript.end(), M_BYTES.begin(), M_BYTES.end());
    transcript.insert(transcript.end(), N_BYTES.begin(), N_BYTES.end());
    transcript.insert(transcript.end(), XBytes.begin(), XBytes.end());
    transcript.insert(transcript.end(), YBytes.begin(), YBytes.end());
    transcript.insert(transcript.end(), ZBytes.begin(), ZBytes.end());
    transcript.insert(transcript.end(), VBytes.begin(), VBytes.end());
    transcript.insert(transcript.end(), w0Bytes.begin(), w0Bytes.end());
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

RegistrationResult Spake2p::Register(const std::string& username, const std::string& password)
{
    std::array<uint8_t, 16> salt;
    randombytes_buf(salt.data(), salt.size());
    auto [w0Bytes, w1Bytes] = DeriveW0W1(username, password, salt);
    Ed25519::Scalar w1(w1Bytes);
    auto L = w1.GetPubKey();
    RegistrationResult result{};
    result.w0 = w0Bytes;
    result.L = L.Dump();
    result.salt = salt;
    return result;
}

/** Client */

Client::Client(
    const std::string& username,
    const std::string& password,
    const std::map<HandshakeMessage::Type, std::vector<uint8_t>>& additionalElements)
    : _username(username), _password(password), _firstMessageAdditionalElements(additionalElements)
{
    if (username.empty() || password.empty())
    {
        throw std::invalid_argument("Username and password cannot be empty");
    }
    for (const auto& element : _firstMessageAdditionalElements)
    {
        if (element.first == HandshakeMessage::Type::KeyIndex || element.first == HandshakeMessage::Type::CipherMessage)
        {
            throw std::invalid_argument("KeyIndex and CipherMessage elements are reserved for protocol use");
        }
    }
}

HandshakeMessage::Message Client::RetrieveSalt()
{
    auto marker = _stepChecker->CheckStep(Step::Init, Step::RetrieveSalt);
    /** DO NOT include the null termination */
    std::vector<uint8_t> keyIndex(_username.begin(), _username.end());
    _firstMessageAdditionalElements.emplace(
        HandshakeMessage::Type::KeyIndex, std::move(keyIndex));
    HandshakeMessage::Message message{std::move(_firstMessageAdditionalElements)};
    _firstMessageAdditionalElements.clear();
    return message;
}

HandshakeMessage::Message Client::GetShareP(
    const HandshakeMessage::Message& serverMessage)
{
    auto marker = _stepChecker->CheckStep(Step::RetrieveSalt, Step::ShareP);
    auto cipherMessageOpt = serverMessage.GetElement(HandshakeMessage::Type::CipherMessage);
    if (!cipherMessageOpt.has_value())
    {
        throw std::runtime_error("Server message is missing CipherMessage element");
    }
    auto cipherMessage = std::move(cipherMessageOpt.value());
    if (cipherMessage.size() != sizeof(RegistrationResult::salt))
    {
        throw std::runtime_error("Invalid server message size");
    }
    std::array<uint8_t, sizeof(RegistrationResult::salt)> salt{};
    std::copy(cipherMessage.begin(), cipherMessage.end(), salt.begin());
    /** Recalculate w0 and w1 */
    auto [w0, w1] = DeriveW0W1(_username, _password, salt);
    _password.clear();
    _w0 = Ed25519::Scalar(w0);
    _w1 = Ed25519::Scalar(w1);
    /** Calculate X */
    _x = Ed25519::Scalar::Generate();
    auto M = Ed25519::Point(M_BYTES);
    _X = _x->GetPubKey() + _w0.value() * M;
    auto shareP = _X.value().Dump();
    /** Assemble handshake message */
    std::vector<uint8_t> cipherMessageToSend(shareP.size(), 0);
    std::copy(shareP.begin(), shareP.end(), cipherMessageToSend.begin());
    HandshakeMessage::Message message{
        {{HandshakeMessage::Type::CipherMessage, std::move(cipherMessageToSend)}}};
    return message;
}

HandshakeMessage::Message Client::GetConfirmP(
    const HandshakeMessage::Message& serverMessage)
{
    auto marker = _stepChecker->CheckStep(Step::ShareP, Step::ConfirmP);
    auto cipherMessageOpt = serverMessage.GetElement(HandshakeMessage::Type::CipherMessage);
    if (!cipherMessageOpt.has_value())
    {
        throw std::runtime_error("Server message is missing CipherMessage element");
    }
    auto cipherMessage = std::move(cipherMessageOpt.value());
    if (cipherMessage.size() < Ed25519::Point::size)
    {
        throw std::runtime_error("Invalid server message size");
    }
    std::array<uint8_t, Ed25519::Point::size> YBytes;
    std::copy(cipherMessage.begin(), cipherMessage.begin() + Ed25519::Point::size, YBytes.begin());
    Ed25519::Point Y(YBytes);
    /** Calculate Z and V */
    auto h = Ed25519::GetCofactor();
    Ed25519::Point N(N_BYTES);
    auto Z = h * (_x.value() * (Y - _w0.value() * N));
    auto V = h * (_w1.value() * (Y - _w0.value() * N));
    /** Calculate prk = Hash(TT) */
    auto prk = GetTranscriptHash(
        std::string(HASH_CONTEXT),
        _username, std::string(ID_VERIFIER),
        _X.value(), Y, Z, V, _w0.value());
    /** Expand keys */
    _clientKey = HkdfExpendKey(prk, "client key");
    _serverKey = HkdfExpendKey(prk, "server key");
    auto confirmPKey = HkdfExpendKey(prk, "confirmP key");
    auto confirmVKey = HkdfExpendKey(prk, "confirmV key");
    /** Decrypt confirm V */
    ChaCha20Poly1305::Decryptor decryptor{confirmVKey};
    auto decryptedShareP = decryptor.Decrypt(cipherMessage.data() + Ed25519::Point::size, cipherMessage.size() - Ed25519::Point::size);
    if (decryptedShareP.size() != Ed25519::Point::size)
    {
        throw std::runtime_error("Invalid decrypted shareP size");
    }
    std::array<uint8_t, Ed25519::Point::size> decryptedSharePBytes;
    std::copy(decryptedShareP.begin(), decryptedShareP.end(), decryptedSharePBytes.begin());
    Ed25519::Point decryptedX(decryptedSharePBytes);
    if (decryptedX != _X.value())
    {
        throw std::runtime_error("ShareP ConfirmV mismatch");
    }
    /** Calculate confirmP = MAC(K_confirmP, shareV) */
    ChaCha20Poly1305::Encryptor encryptor{confirmPKey};
    auto confirmP = encryptor.Encrypt(Y.Dump());
    /** Assemble the handshake message */
    std::vector<uint8_t> cipherMessageToSend(confirmP.size(), 0);
    std::copy(confirmP.begin(), confirmP.end(), cipherMessageToSend.begin());
    HandshakeMessage::Message message{
        {{HandshakeMessage::Type::CipherMessage, std::move(cipherMessageToSend)}}};
    return message;
}

std::optional<HandshakeMessage::Message> Client::GetNextMessage(
    const std::optional<HandshakeMessage::Message>& peerMessage)
{
    switch (_stepChecker->GetCurrentStep())
    {
        case Step::Init:
            if (peerMessage.has_value())
            {
                throw std::runtime_error("No peer message expected");
            }
            return RetrieveSalt();
        case Step::RetrieveSalt:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            return GetShareP(peerMessage.value());
        case Step::ShareP:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            return GetConfirmP(peerMessage.value());
        default:
            throw std::runtime_error("Invalid step in Client handshake");
    }
}

bool Client::IsHandshakeComplete()
{
    return _stepChecker->GetCurrentStep() == Step::ConfirmP;
}

std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> Client::GetClientKey()
{
    auto marker = _stepChecker->CheckStep(Step::ConfirmP, Step::ConfirmP);
    return _clientKey;
}

std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> Client::GetServerKey()
{
    auto marker = _stepChecker->CheckStep(Step::ConfirmP, Step::ConfirmP);
    return _serverKey;
}

/** Server */

Server::Server(std::function<RegistrationResult(const std::string&)> getUserRegistration)
    : _getUserRegistration(std::move(getUserRegistration))
{
    if (!_getUserRegistration)
    {
        throw std::invalid_argument("getUserRegistration function cannot be null");
    }
}

HandshakeMessage::Message Server::RetrieveSalt(
    const HandshakeMessage::Message& clientMessage)
{
    auto marker = _stepChecker->CheckStep(Step::Init, Step::RetrieveSalt);
    auto keyIndexOpt = clientMessage.GetElement(HandshakeMessage::Type::KeyIndex);
    if (!keyIndexOpt.has_value())
    {
        throw std::runtime_error("Client message is missing KeyIndex element");
    }
    {
        std::string username(keyIndexOpt.value().begin(), keyIndexOpt.value().end());
        _username = std::move(username);
    }
    _registrationResult = _getUserRegistration(_username);
    std::vector<uint8_t> salt(_registrationResult->salt.begin(), _registrationResult->salt.end());
    HandshakeMessage::Message message{
        {{HandshakeMessage::Type::CipherMessage, salt}}};
    return message;
}

HandshakeMessage::Message Server::GetShareVConfirmV(
    const HandshakeMessage::Message& clientMessage)
{
    auto marker = _stepChecker->CheckStep(Step::RetrieveSalt, Step::ShareVConfirmV);
    auto cipherMessageOpt = clientMessage.GetElement(HandshakeMessage::Type::CipherMessage);
    if (!cipherMessageOpt.has_value())
    {
        throw std::runtime_error("Client message is missing CipherMessage element");
    }
    auto cipherMessage = std::move(cipherMessageOpt.value());
    if (cipherMessage.size() != Ed25519::Point::size)
    {
        throw std::runtime_error("Invalid client message size");
    }
    std::array<uint8_t, Ed25519::Point::size> X_Bytes;
    std::copy(cipherMessage.begin(), cipherMessage.end(), X_Bytes.begin());
    Ed25519::Point X(X_Bytes);
    /** Calculate confirm V (Y) */
    auto y = Ed25519::Scalar::Generate();
    Ed25519::Point N(N_BYTES);
    Ed25519::Scalar w0(_registrationResult->w0);
    _Y = y.GetPubKey() + w0 * N;
    /** Calculate Z and V */
    auto h = Ed25519::GetCofactor();
    Ed25519::Point M(M_BYTES);
    Ed25519::Point L(_registrationResult->L);
    auto Z = h * (y * (X - w0 * M));
    auto V = h * (y * L);

    /** Calculate prk = Hash(TT) */
    auto prk = GetTranscriptHash(
        std::string(HASH_CONTEXT),
        _username, std::string(ID_VERIFIER),
        X, _Y.value(), Z, V, w0);
    /** Expand keys */
    _clientKey = HkdfExpendKey(prk, "client key");
    _serverKey = HkdfExpendKey(prk, "server key");
    _confirmPKey = HkdfExpendKey(prk, "confirmP key");
    auto confirmVkey = HkdfExpendKey(prk, "confirmV key");
    /** Calculate confirmV = MAC(K_configmV, shareP) */
    ChaCha20Poly1305::Encryptor encryptor{confirmVkey};
    auto confirmV = encryptor.Encrypt(X.Dump());
    /** Assemble the handshake message */
    auto shareV = _Y.value().Dump();
    std::vector<uint8_t> cipherMessageToSend(shareV.size() + confirmV.size(), 0);
    std::copy(shareV.begin(), shareV.end(), cipherMessageToSend.begin());
    std::copy(confirmV.begin(), confirmV.end(), cipherMessageToSend.begin() + shareV.size());
    HandshakeMessage::Message message{
        {{HandshakeMessage::Type::CipherMessage, std::move(cipherMessageToSend)}}};
    return message;
}

void Server::TakeConfirmP(const HandshakeMessage::Message& clientMessage)
{
    auto marker = _stepChecker->CheckStep(Step::ShareVConfirmV, Step::ConfirmP);
    auto cipherMessageOpt = clientMessage.GetElement(HandshakeMessage::Type::CipherMessage);
    if (!cipherMessageOpt.has_value())
    {
        throw std::runtime_error("Client message is missing CipherMessage element");
    }
    auto cipherMessage = std::move(cipherMessageOpt.value());
    ChaCha20Poly1305::Decryptor decryptor{_confirmPKey};
    auto decryptedYBytes =  decryptor.Decrypt(cipherMessage);
    if (decryptedYBytes.size() != Ed25519::Point::size)
    {
        throw std::runtime_error("Invalid decrypted shareV size");
    }
    std::array<uint8_t, Ed25519::Point::size> YBytes;
    std::copy(decryptedYBytes.begin(), decryptedYBytes.end(), YBytes.begin());
    Ed25519::Point Y(YBytes);
    if (Y != _Y.value())
    {
        throw std::runtime_error("Invalid confirm P from client");
    }
}

std::optional<HandshakeMessage::Message> Server::GetNextMessage(
    const std::optional<HandshakeMessage::Message>& peerMessage)
{
    switch (_stepChecker->GetCurrentStep())
    {
        case Step::Init:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            return RetrieveSalt(peerMessage.value());
        case Step::RetrieveSalt:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            return GetShareVConfirmV(peerMessage.value());
        case Step::ShareVConfirmV:
            if (!peerMessage.has_value())
            {
                throw std::runtime_error("Peer message is required");
            }
            TakeConfirmP(peerMessage.value());
            return std::nullopt; // Handshake complete
        default:
            throw std::runtime_error("Invalid step in Server handshake");
    }
}

bool Server::IsHandshakeComplete()
{
    return _stepChecker->GetCurrentStep() == Step::ConfirmP;
}

std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> Server::GetClientKey()
{
    auto marker = _stepChecker->CheckStep(Step::ConfirmP, Step::ConfirmP);
    return _clientKey;
}

std::array<uint8_t, IAuthenticationPeer::KEY_SIZE> Server::GetServerKey()
{
    auto marker = _stepChecker->CheckStep(Step::ConfirmP, Step::ConfirmP);
    return _serverKey;
}

