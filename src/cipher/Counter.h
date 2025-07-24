#pragma once

#include <cstddef>
#include <array>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace TUI::Cipher
{
    /**
     * @brief A little endian counter.
     * 
     * @tparam N Number of bytes in the counter.
     */
    template <size_t N>
    class Counter
    {
    public:
        using ValueType = std::array<uint8_t, N>;

        Counter() = default;

        Counter(const uint8_t* data, size_t size)
        {
            if (size != N)
            {
                throw std::invalid_argument("Size must be " + std::to_string(N));
            }
            std::copy(data, data + N, _value.begin());
        }

        explicit Counter(const ValueType& value):
            _value(value)
        {
        }

        explicit Counter(const std::vector<uint8_t>& value)
            : Counter(value.data(), value.size())
        {
        }

        Counter& operator=(const Counter& other) = default;
        Counter& operator=(Counter&& other) noexcept = default;
        Counter(const Counter& other) = default;
        Counter(Counter&& other) noexcept = default;
        ~Counter() = default;

        Counter& operator=(const ValueType& value)
        {
            _value = value;
            return *this;
        }

        Counter& operator=(const std::vector<uint8_t>& value)
        {
            if (value.size() != N)
            {
                throw std::invalid_argument("Size must be " + std::to_string(N));
            }
            _value = ValueType{};
            std::copy(value.begin(), value.end(), _value.begin());
            return *this;
        }

        explicit operator const ValueType&() const
        {
            return _value;
        }

        Counter& operator++()
        {
            for (size_t i = 0; i < N; ++i)
            {
                if (++_value[i] != 0)
                {
                    return *this;
                }
            }
            throw std::overflow_error("Counter overflow");
        }

        Counter operator++(int)
        {
            Counter temp(*this);
            ++(*this);
            return temp;
        }

        auto operator<=>(const Counter& other) const
        {
            for (size_t i = N; i > 0; --i)
            {
                if (_value[i - 1] < other._value[i - 1])
                {
                    return std::strong_ordering::less;
                }
                if (_value[i - 1] > other._value[i - 1])
                {
                    return std::strong_ordering::greater;
                }
            }
            return std::strong_ordering::equal;
        }

        bool operator==(const Counter& other) const
        {
            return _value == other._value;
        }

        const ValueType& GetBytes() const
        {
            return _value;
        }

    private:
        ValueType _value{};
    };
}
