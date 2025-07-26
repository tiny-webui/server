#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <stdexcept>

/**
 * This will try to decode most base64 variants.
 * And will encode into the base64url non-padding variant.
 */

namespace TUI::Common::Base64
{
    std::string Encode(const uint8_t* data, size_t size);
    std::string Encode(const std::vector<std::uint8_t>& data);
    template<size_t N>
    std::string Encode(const std::array<uint8_t, N>& data)
    {
        return Encode(data.data(), data.size());
    }
    /**
     * @brief You SHOULD NEVER trust the decoded data.
     *     Check the output thoroughly before any use.
     * 
     * @param base64String 
     * @return std::vector<std::uint8_t> 
     */
    std::vector<std::uint8_t> Decode(const std::string& base64String);
    template<size_t N>
    std::array<std::uint8_t, N> Decode(const std::string& base64String)
    {
        auto bytesVector = Decode(base64String);
        if (bytesVector.size() != N)
        {
            throw std::invalid_argument("Data size does not match expected size");
        }
        std::array<std::uint8_t, N> bytesArray{};
        std::copy(bytesVector.begin(), bytesVector.end(), bytesArray.begin());
        return bytesArray;
    }
}
