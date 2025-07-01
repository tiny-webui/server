#include "Aes256Gcm.h"

using namespace TUI::Cipher::Aes256Gcm;

Encryptor::Encryptor(const Key& key)
{
    if (EVP_EncryptInit_ex(_ctx, EVP_aes_256_gcm(), nullptr, key.data(), nullptr) != 1)
    {
        throw std::runtime_error("Failed to initialize encryption context");
    }
    if (EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_IVLEN, Iv::size(), nullptr) != 1)
    {
        throw std::runtime_error("Failed to set IV length");
    }
}

std::vector<uint8_t> Encryptor::Encrypt(const std::vector<uint8_t>& plainText)
{
    /** update iv */
    _iv++;
    int rc = EVP_EncryptInit_ex(_ctx, nullptr, nullptr, nullptr, _iv.Get().data());
    if (rc != 1)
    {
        throw std::runtime_error("Failed to update IV");
    }
    std::vector<uint8_t> cipherText(Iv::size() + plainText.size() + sizeof(Tag), 0);
    std::copy(_iv.Get().begin(), _iv.Get().end(), cipherText.begin());
    int totalLen = 0;
    int outLen = 0;
    rc = EVP_EncryptUpdate(
        _ctx,
        cipherText.data() + Iv::size(),
        &outLen,
        plainText.data(),
        plainText.size());
    if (rc != 1)
    {
        throw std::runtime_error("Failed to encrypt data");
    }
    totalLen += outLen;
    rc = EVP_EncryptFinal_ex(_ctx, cipherText.data() + Iv::size() + outLen, &outLen);
    if (rc != 1)
    {
        throw std::runtime_error("Failed to finalize encryption");
    }
    totalLen += outLen;
    if (static_cast<size_t>(totalLen) != plainText.size())
    {
        throw std::runtime_error("Encryption output size mismatch");
    }
    rc = EVP_CIPHER_CTX_ctrl(
        _ctx, EVP_CTRL_GCM_GET_TAG, sizeof(Tag), cipherText.data() + Iv::size() + totalLen);
    if (rc != 1)
    {
        throw std::runtime_error("Failed to get GCM tag");
    }
    return cipherText;
}

Decryptor::Decryptor(const Key& key)
{
    if (EVP_DecryptInit_ex(_ctx, EVP_aes_256_gcm(), nullptr, key.data(), nullptr) != 1)
    {
        throw std::runtime_error("Failed to initialize decryption context");
    }
    if (EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_IVLEN, Iv::size(), nullptr) != 1)
    {
        throw std::runtime_error("Failed to set IV length for decryption");
    }
}

std::vector<uint8_t> Decryptor::Decrypt(const std::vector<uint8_t>& cipherText)
{
    if (cipherText.size() < Iv::size() + sizeof(Tag))
    {
        throw std::invalid_argument("Cipher text is too short");
    }
    Iv iv{cipherText.begin(), cipherText.begin() + Iv::size()};
    if (_lastIv.has_value() && iv <= _lastIv.value())
    {
        throw std::runtime_error("IV replay detected");
    }
    _lastIv = iv;
    int rc = EVP_DecryptInit_ex(_ctx, nullptr, nullptr, nullptr, iv.Get().data());
    if (rc != 1)
    {
        throw std::runtime_error("Failed to update IV");
    }
    std::vector<uint8_t> plainText(cipherText.size() - Iv::size() - sizeof(Tag), 0);
    int totalLen = 0;
    int outLen = 0;
    rc = EVP_DecryptUpdate(
        _ctx,
        plainText.data(),
        &outLen,
        cipherText.data() + Iv::size(),
        static_cast<int>(cipherText.size() - Iv::size() - sizeof(Tag)));
    if (rc != 1)
    {
        throw std::runtime_error("Failed to decrypt data");
    }
    totalLen += outLen;
    Tag tag{};
    std::copy(
        cipherText.data() + cipherText.size() - sizeof(Tag),
        cipherText.data() + cipherText.size(),
        tag.begin());
    rc = EVP_CIPHER_CTX_ctrl(
        _ctx, EVP_CTRL_GCM_SET_TAG, sizeof(Tag), static_cast<void*>(tag.data()));
    if (rc != 1)
    {
        throw std::runtime_error("Failed to set GCM tag for decryption");
    }
    rc = EVP_DecryptFinal_ex(_ctx, plainText.data() + totalLen, &outLen);
    if (rc != 1)
    {
        throw std::runtime_error("Failed to finalize decryption");
    }
    totalLen += outLen;
    if (static_cast<size_t>(totalLen) != plainText.size())
    {
        throw std::runtime_error("Decryption output size mismatch");
    }
    return plainText;
}

