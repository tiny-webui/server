#include <cstring>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/param_build.h>
#include <openssl/core_names.h>
#include "EcdhePsk.h"

using namespace TUI::Cipher::EcdhePsk;

Client::Client(const Psk& psk, size_t keySize)
    : _psk(psk), _keySize(keySize)
{
    if (keySize == 0)
    {
        throw std::invalid_argument("Key size must be greater than 0");
    }
}

std::vector<uint8_t> Client::GetClientMessage(const std::vector<uint8_t>& keyIndex)
{
    auto marker = _stepChecker->CheckStep(Step::Init, Step::ClientMessage);
    int rc = 0;
    /** Generate key pair */
    {
        Openssl::EvpPkeyCtx keyGenCtx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr));
        rc = EVP_PKEY_keygen_init(keyGenCtx);
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
        _pkey = Openssl::EvpPkey(pkey);
    }
    std::vector<uint8_t> pubkey(PUBKEY_SIZE, 0);
    {
        size_t pubkeyLen = 0;
        rc = EVP_PKEY_get_raw_public_key(_pkey, nullptr, &pubkeyLen);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot get public key length");
        }
        if (pubkeyLen != PUBKEY_SIZE)
        {
            throw std::runtime_error("Invalid public key length");
        }
        rc = EVP_PKEY_get_raw_public_key(_pkey, pubkey.data(), &pubkeyLen);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot get public key");
        }
    }
    /** Generate nonce */
    std::vector<uint8_t> nonce(NONCE_SIZE, 0);
    {
        rc = RAND_priv_bytes(nonce.data(), NONCE_SIZE);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot generate nonce");
        }
    }
    /** Generate client message */
    std::vector<uint8_t> clientMessage(PUBKEY_SIZE + NONCE_SIZE + keyIndex.size(), 0);
    {
        std::copy(pubkey.begin(), pubkey.end(), clientMessage.begin());
        std::copy(nonce.begin(), nonce.end(), clientMessage.begin() + PUBKEY_SIZE);
        std::copy(keyIndex.begin(), keyIndex.end(), clientMessage.begin() + PUBKEY_SIZE + NONCE_SIZE);
    }
    _clientMessage = std::move(clientMessage);
    return _clientMessage.value();
}

void Client::TakeServerMessage(const std::vector<uint8_t>& serverMessage)
{
    auto marker = _stepChecker->CheckStep(Step::ClientMessage, Step::ServerMessage);
    if (serverMessage.size() != PUBKEY_SIZE + NONCE_SIZE)
    {
        throw std::runtime_error("Invalid server message size");
    }
    int rc = 0;
    /** Generate shared secret */
    std::vector<uint8_t> Z{};
    {
        Openssl::EvpPkey serverPubKey{EVP_PKEY_new_raw_public_key(
            EVP_PKEY_X25519, nullptr, serverMessage.data(), PUBKEY_SIZE)};
        Openssl::EvpPkeyCtx deriveCtx{EVP_PKEY_CTX_new(_pkey, nullptr)};
        rc = EVP_PKEY_derive_init(deriveCtx);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot init derive context");
        }
        rc = EVP_PKEY_derive_set_peer(deriveCtx, serverPubKey);
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
        Z.resize(keyLen);
        rc = EVP_PKEY_derive(deriveCtx, Z.data(), &keyLen);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot derive key");
        }
    }
    /** Generate prk */
    std::vector<uint8_t> prk(32, 0);
    {
        Openssl::EvpKdf kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
        Openssl::EvpKdfCtx kdfCtx(EVP_KDF_CTX_new(kdf));
        std::string info = "prk";
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_utf8_string(
                OSSL_KDF_PARAM_DIGEST, SN_sha256, strlen(SN_sha256)),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_KEY, Z.data(), Z.size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_SALT, _psk.data(), _psk.size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_INFO, const_cast<void*>(static_cast<const void*>(info.c_str())), info.length()),
            OSSL_PARAM_construct_end()
        };
        rc = EVP_KDF_derive(kdfCtx, prk.data(), prk.size(), params);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot derive PRK");
        }
    }
    /** Calculate transcript hash */
    std::array<uint8_t, 32> transcriptHash{};
    {
        std::vector<uint8_t> transcript(_clientMessage->size() + serverMessage.size(), 0);
        std::copy(_clientMessage->begin(), _clientMessage->end(), transcript.begin());
        std::copy(serverMessage.begin(), serverMessage.end(), transcript.begin() + _clientMessage->size());
        std::vector<uint8_t> md(EVP_MAX_MD_SIZE, 0);
        size_t mdLen = 0;
        rc = EVP_Q_digest(nullptr, "SHA256", nullptr,
            transcript.data(), transcript.size(), md.data(), &mdLen);
        if (rc != 1 || mdLen != 32)
        {
            throw std::runtime_error("Cannot calculate transcript hash");
        }
        std::copy(md.begin(), md.begin() + 32, transcriptHash.begin());
    }
    _transcriptHash = std::move(transcriptHash);
    /** Generate session keys */
    std::vector<uint8_t> clientKey(_keySize, 0);
    {
        Openssl::EvpKdf kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
        Openssl::EvpKdfCtx kdfCtx(EVP_KDF_CTX_new(kdf));
        std::string info = "client key";
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_utf8_string(
                OSSL_KDF_PARAM_DIGEST, SN_sha256, strlen(SN_sha256)),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_KEY, prk.data(), prk.size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_SALT, _transcriptHash->data(), _transcriptHash->size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_INFO, const_cast<void*>(static_cast<const void*>(info.c_str())), info.length()),
            OSSL_PARAM_construct_end()
        };
        rc = EVP_KDF_derive(kdfCtx, clientKey.data(), clientKey.size(), params);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot derive client key");
        }
    }
    _clientKey = std::move(clientKey);
    std::vector<uint8_t> serverKey(_keySize, 0);
    {
        Openssl::EvpKdf kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
        Openssl::EvpKdfCtx kdfCtx(EVP_KDF_CTX_new(kdf));
        std::string info = "server key";
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_utf8_string(
                OSSL_KDF_PARAM_DIGEST, SN_sha256, strlen(SN_sha256)),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_KEY, prk.data(), prk.size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_SALT, _transcriptHash->data(), _transcriptHash->size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_INFO, const_cast<void*>(static_cast<const void*>(info.c_str())), info.length()),
            OSSL_PARAM_construct_end()
        };
        rc = EVP_KDF_derive(kdfCtx, serverKey.data(), serverKey.size(), params);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot derive client key");
        }
    }
    _serverKey = std::move(serverKey);
}

std::vector<uint8_t> Client::GetClientKey()
{
    auto marker = _stepChecker->CheckStep(Step::ServerMessage, Step::ServerMessage);
    if (!_clientKey.has_value())
    {
        throw std::runtime_error("Client key is not available");
    }
    return _clientKey.value();
}

std::vector<uint8_t> Client::GetServerKey()
{
    auto marker = _stepChecker->CheckStep(Step::ServerMessage, Step::ServerMessage);
    if (!_serverKey.has_value())
    {
        throw std::runtime_error("Server key is not available");
    }
    return _serverKey.value();
}

std::array<uint8_t, 32> Client::GetTranscriptHash()
{
    auto marker = _stepChecker->CheckStep(Step::ServerMessage, Step::ServerMessage);
    if (!_transcriptHash.has_value())
    {
        throw std::runtime_error("Transcript hash is not available");
    }
    return _transcriptHash.value();
}

Server::Server(const Psk& psk, size_t keySize)
    : _psk(psk), _keySize(keySize)
{
    if (keySize == 0)
    {
        throw std::invalid_argument("Key size must be greater than 0");
    }
}

std::vector<uint8_t> Server::ParseKeyIndexFromClientMessage(const std::vector<uint8_t>& clientMessage)
{
    if (clientMessage.size() < PUBKEY_SIZE + NONCE_SIZE)
    {
        throw std::runtime_error("Invalid client message size");
    }
    return std::vector<uint8_t>(clientMessage.begin() + PUBKEY_SIZE + NONCE_SIZE, clientMessage.end());
}

std::vector<uint8_t> Server::GetServerMessage(const std::vector<uint8_t>& clientMessage)
{
    auto marker = _stepChecker->CheckStep(Step::Init, Step::ServerMessage);
    if (clientMessage.size() < PUBKEY_SIZE + NONCE_SIZE)
    {
        throw std::runtime_error("Invalid client message size");
    }
    int rc = 0;
    /** Generate key pair */
    Openssl::EvpPkey pkey{};
    {
        Openssl::EvpPkeyCtx keyGenCtx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr));
        rc = EVP_PKEY_keygen_init(keyGenCtx);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot init keygen context");
        }
        EVP_PKEY* pkeyRaw = nullptr;
        rc = EVP_PKEY_keygen(keyGenCtx, &pkeyRaw);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot generate key pair");
        }
        pkey = Openssl::EvpPkey(pkeyRaw);
    }
    std::vector<uint8_t> pubKey(PUBKEY_SIZE, 0);
    {
        size_t pubkeyLen = 0;
        rc = EVP_PKEY_get_raw_public_key(pkey, nullptr, &pubkeyLen);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot get public key length");
        }
        if (pubkeyLen != PUBKEY_SIZE)
        {
            throw std::runtime_error("Invalid public key length");
        }
        rc = EVP_PKEY_get_raw_public_key(pkey, pubKey.data(), &pubkeyLen);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot get public key");
        }
    }
    /** Generate nonce */
    std::vector<uint8_t> nonce(NONCE_SIZE, 0);
    {
        rc = RAND_priv_bytes(nonce.data(), NONCE_SIZE);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot generate nonce");
        }
    }
    /** Generate server message */
    std::vector<uint8_t> serverMessage(PUBKEY_SIZE + NONCE_SIZE, 0);
    {
        std::copy(pubKey.begin(), pubKey.end(), serverMessage.begin());
        std::copy(nonce.begin(), nonce.end(), serverMessage.begin() + PUBKEY_SIZE);
    }
    /** Calculate Z */
    std::vector<uint8_t> Z{};
    {
        Openssl::EvpPkey serverPubKey{EVP_PKEY_new_raw_public_key(
            EVP_PKEY_X25519, nullptr, clientMessage.data(), PUBKEY_SIZE)};
        Openssl::EvpPkeyCtx deriveCtx{EVP_PKEY_CTX_new(pkey, nullptr)};
        rc = EVP_PKEY_derive_init(deriveCtx);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot init derive context");
        }
        rc = EVP_PKEY_derive_set_peer(deriveCtx, serverPubKey);
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
        Z.resize(keyLen);
        rc = EVP_PKEY_derive(deriveCtx, Z.data(), &keyLen);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot derive key");
        }
    }
    /** Generate prk */
    std::vector<uint8_t> prk(32, 0);
    {
        Openssl::EvpKdf kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
        Openssl::EvpKdfCtx kdfCtx(EVP_KDF_CTX_new(kdf));
        std::string info = "prk";
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_utf8_string(
                OSSL_KDF_PARAM_DIGEST, SN_sha256, strlen(SN_sha256)),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_KEY, Z.data(), Z.size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_SALT, _psk.data(), _psk.size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_INFO, const_cast<void*>(static_cast<const void*>(info.c_str())), info.length()),
            OSSL_PARAM_construct_end()
        };
        rc = EVP_KDF_derive(kdfCtx, prk.data(), prk.size(), params);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot derive PRK");
        }
    }
    /** Calculate transcript hash */
    std::array<uint8_t, 32> transcriptHash{};
    {
        std::vector<uint8_t> transcript(clientMessage.size() + serverMessage.size(), 0);
        std::copy(clientMessage.begin(), clientMessage.end(), transcript.begin());
        std::copy(serverMessage.begin(), serverMessage.end(), transcript.begin() + clientMessage.size());
        std::vector<uint8_t> md(EVP_MAX_MD_SIZE, 0);
        size_t mdLen = 0;
        rc = EVP_Q_digest(nullptr, "SHA256", nullptr,
            transcript.data(), transcript.size(), md.data(), &mdLen);
        if (rc != 1 || mdLen != 32)
        {
            throw std::runtime_error("Cannot calculate transcript hash");
        }
        std::copy(md.begin(), md.begin() + 32, transcriptHash.begin());
    }
    _transcriptHash = std::move(transcriptHash);
    /** Generate session keys */
    std::vector<uint8_t> clientKey(_keySize, 0);
    {
        Openssl::EvpKdf kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
        Openssl::EvpKdfCtx kdfCtx(EVP_KDF_CTX_new(kdf));
        std::string info = "client key";
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_utf8_string(
                OSSL_KDF_PARAM_DIGEST, SN_sha256, strlen(SN_sha256)),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_KEY, prk.data(), prk.size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_SALT, _transcriptHash->data(), _transcriptHash->size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_INFO, const_cast<void*>(static_cast<const void*>(info.c_str())), info.length()),
            OSSL_PARAM_construct_end()
        };
        rc = EVP_KDF_derive(kdfCtx, clientKey.data(), clientKey.size(), params);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot derive client key");
        }
    }
    _clientKey = std::move(clientKey);
    std::vector<uint8_t> serverKey(_keySize, 0);
    {
        Openssl::EvpKdf kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
        Openssl::EvpKdfCtx kdfCtx(EVP_KDF_CTX_new(kdf));
        std::string info = "server key";
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_utf8_string(
                OSSL_KDF_PARAM_DIGEST, SN_sha256, strlen(SN_sha256)),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_KEY, prk.data(), prk.size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_SALT, _transcriptHash->data(), _transcriptHash->size()),
            OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_INFO, const_cast<void*>(static_cast<const void*>(info.c_str())), info.length()),
            OSSL_PARAM_construct_end()
        };
        rc = EVP_KDF_derive(kdfCtx, serverKey.data(), serverKey.size(), params);
        if (rc != 1)
        {
            throw std::runtime_error("Cannot derive client key");
        }
    }
    _serverKey = std::move(serverKey);
    return serverMessage;
}

std::vector<uint8_t> Server::GetClientKey()
{
    auto marker = _stepChecker->CheckStep(Step::ServerMessage, Step::ServerMessage);
    if (!_clientKey.has_value())
    {
        throw std::runtime_error("Client key is not available");
    }
    return _clientKey.value();
}

std::vector<uint8_t> Server::GetServerKey()
{
    auto marker = _stepChecker->CheckStep(Step::ServerMessage, Step::ServerMessage);
    if (!_serverKey.has_value())
    {
        throw std::runtime_error("Server key is not available");
    }
    return _serverKey.value();
}

std::array<uint8_t, 32> Server::GetTranscriptHash()
{
    auto marker = _stepChecker->CheckStep(Step::ServerMessage, Step::ServerMessage);
    if (!_transcriptHash.has_value())
    {
        throw std::runtime_error("Transcript hash is not available");
    }
    return _transcriptHash.value();
}

