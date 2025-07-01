#pragma once

#include <cstdint>
#include <array>
#include <stdexcept>
#include <algorithm>

namespace TUI::Cipher
{
    template <std::size_t N>
    class BigUint
    {
        static_assert(N > 0, "BigUint size must be greater than 0");
    public:
        constexpr BigUint() = default;
        constexpr BigUint(const std::array<uint8_t, N>& bytes) : _bytes(bytes) {}
        template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value && sizeof(T) <= N>>
        constexpr BigUint(T value)
        {
            size_t i = 0;
            for (i = 0; i < sizeof(T); i++)
            {
                _bytes[i] = static_cast<uint8_t>(value & 0xFF);
                value >>= 8;
            }
            for (; i < N; i++)
            {
                _bytes[i] = 0; // Fill remaining bytes with zero
            }
        }
        
        constexpr BigUint(const BigUint<N>& other) = default;
        constexpr BigUint(BigUint<N>&& other) noexcept = default;
        constexpr BigUint<N>& operator=(const BigUint<N>& other) = default;
        constexpr BigUint<N>& operator=(BigUint<N>&& other) noexcept = default;
        
        constexpr ~BigUint() = default;

        constexpr const std::array<uint8_t, N>& Get() const
        {
            return _bytes;
        }

        template <typename Iterator>
        constexpr BigUint(Iterator begin, Iterator end)
        {
            std::size_t distance = std::distance(begin, end);
            if (distance != N)
            {
                throw std::invalid_argument("Iterator range size must match BigUint size");
            }
            std::size_t i = 0;
            for (auto it = begin; it != end; ++it, ++i)
            {
                _bytes[i] = static_cast<uint8_t>(*it);
            }
        }

        BigUint<N>& operator++()
        {
            for (std::size_t i = 0; i < N; ++i)
            {
                if (++_bytes[i] != 0)
                {
                    break;
                }
                if (i == N - 1)
                {
                    throw std::overflow_error("BigUint overflow");
                }
            }
            return *this;
        }

        BigUint<N> operator++(int)
        {
            BigUint<N> temp = *this;
            ++(*this);
            return temp;
        }
        
        friend bool operator==(const BigUint<N>& lhs, const BigUint<N>& rhs)
        {
            return lhs._bytes == rhs._bytes;
        }

        friend bool operator!=(const BigUint<N>& lhs, const BigUint<N>& rhs)
        {
            return !(lhs == rhs);
        }

        friend bool operator<(const BigUint<N>& lhs, const BigUint<N>& rhs)
        {
            return std::lexicographical_compare(lhs._bytes.begin(), lhs._bytes.end(),
                                                rhs._bytes.begin(), rhs._bytes.end());
        }

        friend bool operator<=(const BigUint<N>& lhs, const BigUint<N>& rhs)
        {
            return !(rhs < lhs);
        }

        friend bool operator>(const BigUint<N>& lhs, const BigUint<N>& rhs)
        {
            return rhs < lhs;
        }

        friend bool operator>=(const BigUint<N>& lhs, const BigUint<N>& rhs)
        {
            return !(lhs < rhs);
        }

        static constexpr std::size_t size() noexcept
        {
            return N;
        }
    private:
        std::array<uint8_t, N> _bytes;
    };
}

