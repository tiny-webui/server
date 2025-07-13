#pragma once

#include <chrono>
#include <stdint.h>

namespace TUI::Common::Utilities
{
    int64_t GetTimestamp();
    int64_t GetMonotonicTimestamp();

    std::vector<uint8_t> HexToBytes(const std::string& hex);
    template<size_t N>
    std::array<uint8_t, N> HexToBytes(const std::string& hex)
    {
        auto bytesVector = HexToBytes(hex);
        if (bytesVector.size() != N)
        {
            throw std::runtime_error("Data size does not match expected size");
        }
        std::array<uint8_t, N> bytesArray{};
        std::copy(bytesVector.begin(), bytesVector.end(), bytesArray.begin());
        return bytesArray;
    }

    std::string BytesToHex(const uint8_t* bytes, size_t size);
    std::string BytesToHex(const std::vector<uint8_t>& bytes);
    template <size_t N>
    std::string BytesToHex(const std::array<uint8_t, N>& bytes)
    {
        return BytesToHex(bytes.data(), N);
    }
}