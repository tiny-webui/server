#pragma once

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <stdexcept>

namespace TUI::Cipher::Openssl
{
    class EvpCipherCtx
    {
    public:
        EvpCipherCtx()
        {
            _ctx = EVP_CIPHER_CTX_new();
            if (!_ctx)
            {
                throw std::runtime_error("Failed to create EVP_CIPHER_CTX");
            }
        }

        ~EvpCipherCtx()
        {
            if (_ctx)
            {
                EVP_CIPHER_CTX_free(_ctx);
            }
        }

        EvpCipherCtx(const EvpCipherCtx&) = delete;
        EvpCipherCtx& operator=(const EvpCipherCtx&) = delete;
        EvpCipherCtx(EvpCipherCtx&& other) noexcept
            : _ctx(other._ctx)
        {
            other._ctx = nullptr;
        }
        EvpCipherCtx& operator=(EvpCipherCtx&& other) noexcept
        {
            if (this != &other)
            {
                if (_ctx)
                {
                    EVP_CIPHER_CTX_free(_ctx);
                }
                _ctx = other._ctx;
                other._ctx = nullptr;
            }
            return *this;
        }

        operator EVP_CIPHER_CTX*() const
        {
            return _ctx;
        }

    private:
        EVP_CIPHER_CTX* _ctx;
    };

    class EvpPkeyCtx
    {
    public:
        EvpPkeyCtx() = default;
        explicit EvpPkeyCtx(EVP_PKEY_CTX* ctx)
        {
            if (!ctx)
            {
                throw std::runtime_error("Failed to create EVP_PKEY_CTX from nullptr");
            }
            _ctx = ctx;
        }

        ~EvpPkeyCtx()
        {
            if (_ctx)
            {
                EVP_PKEY_CTX_free(_ctx);
            }
        }

        EvpPkeyCtx(const EvpPkeyCtx&) = delete;
        EvpPkeyCtx& operator=(const EvpPkeyCtx&) = delete;
        EvpPkeyCtx(EvpPkeyCtx&& other) noexcept
            : _ctx(other._ctx)
        {
            other._ctx = nullptr;
        }
        EvpPkeyCtx& operator=(EvpPkeyCtx&& other) noexcept
        {
            if (this != &other)
            {
                if (_ctx)
                {
                    EVP_PKEY_CTX_free(_ctx);
                }
                _ctx = other._ctx;
                other._ctx = nullptr;
            }
            return *this;
        }

        operator EVP_PKEY_CTX*() const
        {
            return _ctx;
        }

    private:
        EVP_PKEY_CTX* _ctx{nullptr};
    };

    class EvpPkey
    {
    public:
        EvpPkey() = default;
        explicit EvpPkey(EVP_PKEY* key)
            : _key(key)
        {
            if (!_key)
            {
                throw std::runtime_error("Failed to create EVP_PKEY");
            }
        }

        ~EvpPkey()
        {
            if (_key)
            {
                EVP_PKEY_free(_key);
            }
        }

        EvpPkey(const EvpPkey&) = delete;
        EvpPkey& operator=(const EvpPkey&) = delete;
        EvpPkey(EvpPkey&& other) noexcept
            : _key(other._key)
        {
            other._key = nullptr;
        }
        EvpPkey& operator=(EvpPkey&& other) noexcept
        {
            if (this != &other)
            {
                if (_key)
                {
                    EVP_PKEY_free(_key);
                }
                _key = other._key;
                other._key = nullptr;
            }
            return *this;
        }

        operator EVP_PKEY*() const
        {
            return _key;
        }
    private:
        EVP_PKEY* _key{nullptr};
    };

    class EvpKdf
    {
    public:
        EvpKdf() = default;
        explicit EvpKdf(EVP_KDF* kdf)
        {
            if (!kdf)
            {
                throw std::runtime_error("Failed to create EVP_KDF");
            }
            _kdf = kdf;
        }

        ~EvpKdf()
        {
            if (_kdf)
            {
                EVP_KDF_free(_kdf);
            }
        }

        EvpKdf(const EvpKdf&) = delete;
        EvpKdf& operator=(const EvpKdf&) = delete;
        EvpKdf(EvpKdf&& other) noexcept
            : _kdf(other._kdf)
        {
            other._kdf = nullptr;
        }
        EvpKdf& operator=(EvpKdf&& other) noexcept
        {
            if (this != &other)
            {
                if (_kdf)
                {
                    EVP_KDF_free(_kdf);
                }
                _kdf = other._kdf;
                other._kdf = nullptr;
            }
            return *this;
        }

        operator EVP_KDF*() const
        {
            return _kdf;
        }
    private:
        EVP_KDF* _kdf{nullptr};
    };

    class EvpKdfCtx
    {
    public:
        EvpKdfCtx() = default;
        explicit EvpKdfCtx(EVP_KDF_CTX* ctx)
        {
            if (!ctx)
            {
                throw std::runtime_error("Failed to create EVP_KDF_CTX");
            }
            _ctx = ctx;
        }

        ~EvpKdfCtx()
        {
            if (_ctx)
            {
                EVP_KDF_CTX_free(_ctx);
            }
        }

        EvpKdfCtx(const EvpKdfCtx&) = delete;
        EvpKdfCtx& operator=(const EvpKdfCtx&) = delete;
        EvpKdfCtx(EvpKdfCtx&& other) noexcept
            : _ctx(other._ctx)
        {
            other._ctx = nullptr;
        }
        EvpKdfCtx& operator=(EvpKdfCtx&& other) noexcept
        {
            if (this != &other)
            {
                if (_ctx)
                {
                    EVP_KDF_CTX_free(_ctx);
                }
                _ctx = other._ctx;
                other._ctx = nullptr;
            }
            return *this;
        }

        operator EVP_KDF_CTX*() const
        {
            return _ctx;
        }
    private:
        EVP_KDF_CTX* _ctx{nullptr};
    };

    class BigNum
    {
    public:
        BigNum()
        {
            _bn = BN_new();
            if (!_bn)
            {
                throw std::runtime_error("Failed to create BIGNUM");
            }
        }

        BigNum(uint8_t* data, size_t len)
        {
            _bn = BN_new();
            if (!_bn)
            {
                throw std::runtime_error("Failed to create BIGNUM");
            }
            if (BN_bin2bn(data, static_cast<int>(len), _bn) == nullptr)
            {
                BN_free(_bn);
                throw std::runtime_error("Failed to initialize BIGNUM from binary data");
            }
        }

        explicit BigNum(unsigned long value)
        {
            _bn = BN_new();
            if (!_bn)
            {
                throw std::runtime_error("Failed to create BIGNUM");
            }
            if (BN_set_word(_bn, value) == 0)
            {
                BN_free(_bn);
                throw std::runtime_error("Failed to set BIGNUM from word");
            }
        }

        ~BigNum()
        {
            if (_bn)
            {
                BN_free(_bn);
            }
        }

        BigNum(const BigNum& other)
            : _bn(BN_new())
        {
            if (!_bn)
            {
                throw std::runtime_error("Failed to create BIGNUM");
            }
            if (BN_copy(_bn, other._bn) == nullptr)
            {
                BN_free(_bn);
                throw std::runtime_error("Failed to copy BIGNUM");
            }
        }
        BigNum& operator=(const BigNum& other)
        {
            if (this != &other)
            {
                if (_bn)
                {
                    BN_free(_bn);
                }
                _bn = BN_new();
                if (!_bn)
                {
                    throw std::runtime_error("Failed to create BIGNUM");
                }
                if (BN_copy(_bn, other._bn) == nullptr)
                {
                    BN_free(_bn);
                    throw std::runtime_error("Failed to copy BIGNUM");
                }
            }
            return *this;
        }
        BigNum(BigNum&& other) noexcept
            : _bn(other._bn)
        {
            other._bn = nullptr;
        }
        BigNum& operator=(BigNum&& other) noexcept
        {
            if (this != &other)
            {
                if (_bn)
                {
                    BN_free(_bn);
                }
                _bn = other._bn;
                other._bn = nullptr;
            }
            return *this;
        }

        operator BIGNUM*() const
        {
            return _bn;
        }
        
        BigNum operator+(const BigNum& other) const
        {
            BigNum result{};
            if (BN_add(result._bn, _bn, other._bn) == 0)
            {
                throw std::runtime_error("Failed to add BIGNUMs");
            }
            return result;
        }

        std::strong_ordering operator<=>(const BigNum& other) const
        {
            int cmp = BN_cmp(_bn, other._bn);
            if (cmp < 0)
            {
                return std::strong_ordering::less;
            }
            else if (cmp > 0)
            {
                return std::strong_ordering::greater;
            }
            return std::strong_ordering::equal;
        }

        bool operator==(const BigNum& other) const
        {
            return BN_cmp(_bn, other._bn) == 0;
        }

    private:
        BIGNUM* _bn{nullptr};
    };

    class BnCtx
    {
    public:
        BnCtx()
        {
            _ctx = BN_CTX_new();
            if (!_ctx)
            {
                throw std::runtime_error("Failed to create BN_CTX");
            }
        }

        ~BnCtx()
        {
            if (_ctx)
            {
                BN_CTX_free(_ctx);
            }
        }

        BnCtx(const BnCtx&) = delete;
        BnCtx& operator=(const BnCtx&) = delete;
        BnCtx(BnCtx&& other) noexcept
            : _ctx(other._ctx)
        {
            other._ctx = nullptr;
        }
        BnCtx& operator=(BnCtx&& other) noexcept
        {
            if (this != &other)
            {
                if (_ctx)
                {
                    BN_CTX_free(_ctx);
                }
                _ctx = other._ctx;
                other._ctx = nullptr;
            }
            return *this;
        }

        operator BN_CTX*() const
        {
            return _ctx;
        }
    private:
        BN_CTX* _ctx{nullptr};
    };
}

