#include <sstream>
#include <iomanip>
#include "Utilities.h"

using namespace TUI::Common;

int64_t Utilities::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

int64_t Utilities::GetMonotonicTimestamp()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

static uint8_t HexCharToNumber(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    else if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    else if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    throw std::invalid_argument("Invalid hex character");
}

std::vector<uint8_t> Utilities::HexToBytes(const std::string& hex)
{
    if (hex.size() % 2 != 0)
    {
        throw std::invalid_argument("Hex string must have an even length");
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2)
    {
        uint8_t byte = (HexCharToNumber(hex[i]) << 4) | HexCharToNumber(hex[i + 1]);
        bytes.push_back(byte);
    }
    return bytes;
}

std::string Utilities::BytesToHex(const uint8_t* bytes, size_t size)
{
    std::ostringstream oss;
    for (size_t i = 0; i < size; ++i)
    {
        oss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

std::string Utilities::BytesToHex(const std::vector<uint8_t>& bytes)
{
    return BytesToHex(bytes.data(), bytes.size());
}
