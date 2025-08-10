#include "Base64.h"
#include <stdexcept>

using namespace TUI::Common;

std::string Base64::Encode(const uint8_t* data, size_t size)
{
    /** Base64URL char set */
    static constexpr char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    /** Calculate the encoded length w/o padding */
    size_t encodedLength = (size * 4 + 2) / 3;
    std::string encoded;
    encoded.reserve(encodedLength);
    size_t j = 0;
    /** Aligned to the MSB */
    uint16_t buffer = 0;
    size_t bitsLeft = 0;
    for(size_t i = 0; i < encodedLength; i++)
    {
        if (bitsLeft < 6)
        {
            if (j < size)
            {
                uint8_t byte = data[j++];
                buffer = buffer | (static_cast<uint16_t>(byte) << (8 - bitsLeft));
                bitsLeft += 8;
            }
        }
        /** First 6 bits is the index */
        int index = buffer >> 10;
        encoded.push_back(base64Chars[index]);
        buffer <<= 6;
        bitsLeft -= 6;
    }
    return encoded;
}

std::string Base64::Encode(const std::vector<std::uint8_t>& data)
{
    return Encode(data.data(), data.size());
}

static uint8_t Base64CharToValue(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return static_cast<uint8_t>(c - 'A');
    }
    else if (c >= 'a' && c <= 'z')
    {
        return static_cast<uint8_t>(c - 'a' + 26);
    }
    else if (c >= '0' && c <= '9')
    {
        return static_cast<uint8_t>(c - '0' + 52);
    }
    else if (c == '-' || c == '+')
    {
        return 62;
    }
    else if (c == '_' || c == '/' || c == ',')
    {
        return 63;
    }
    /** DO NOT pass the padding in here!!! */
    throw std::invalid_argument("Invalid Base64 character: " + std::string(1, c));
}

std::vector<std::uint8_t> Base64::Decode(const std::string& base64String)
{
    /** Calculate the max output length */
    size_t maxOutputLength = (base64String.size() * 3 + 3) / 4;
    std::vector<std::uint8_t> decoded;
    decoded.reserve(maxOutputLength);
    /** Aligned to MSB */
    uint16_t buffer = 0;
    size_t bitsLeft = 0;
    size_t j = 0;
    for (const char& c : base64String)
    {
        if (c == '=')
        {
            break;
        }
        uint8_t value = Base64CharToValue(c);
        buffer = buffer | (static_cast<uint16_t>(value) << (10 - bitsLeft));
        bitsLeft += 6;
        while (bitsLeft >= 8)
        {
            uint8_t byte = static_cast<uint8_t>(buffer >> 8);
            decoded.push_back(byte);
            buffer <<= 8;
            bitsLeft -= 8;
            j++;
        }
    }
    decoded.resize(j);
    return decoded;
}
