#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <sys/random.h>
#include <unistd.h>
#include "common/Base64.h"
#include "Utility.h"
#include <iomanip>
#include <optional>

using namespace TUI::Common::Base64;

static std::vector<std::uint8_t> GetRandomLengthRandomBytes(size_t minLength, size_t maxLength)
{
    size_t value = 0;
    if (getrandom(&value, sizeof(value), 0) != static_cast<ssize_t>(sizeof(value)))
    {
        throw std::runtime_error("Failed to get random bytes");
    }
    /** This is biased. But we are not doing a crypto operation. */
    size_t length = minLength + (value % (maxLength - minLength + 1));
    /** This might be too long. But that's your call. */
    std::vector<std::uint8_t> data(length);
    if (getrandom(data.data(), data.size(), 0) != static_cast<ssize_t>(data.size()))
    {
        throw std::runtime_error("Failed to get random bytes");
    }
    return data;
}

static void DumpBytes(const std::string& tag, const std::vector<std::uint8_t>& data)
{
    std::cout << tag << ": " << std::dec << data.size() << " bytes" << std::endl;
    for (const auto& byte : data)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte) << " ";
        if (((&byte - data.data() + 1) % 16) == 0)
        {
            std::cout << std::endl;
        }
    }
    std::cout << std::dec << std::endl;
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    std::optional<std::vector<uint8_t>> data;
    std::optional<std::string> encoded;
    std::optional<std::vector<uint8_t>> decoded;
    try
    {
        for (int i = 0; i < 100000; i++)
        {
            data.reset();
            encoded.reset();
            decoded.reset();
            data = GetRandomLengthRandomBytes(0, 1000);
            encoded = Encode(data.value());
            decoded = Decode(encoded.value());
            if (data != decoded)
            {
                throw std::runtime_error("Decoded data does not match the original data");
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << '\n';
        if (data.has_value())
        {
            DumpBytes("Original Data", data.value());
        }
        if (encoded.has_value())
        {
            std::cout << "Encoded Data:" << std::endl << encoded.value() << std::endl;
        }
        if (decoded.has_value())
        {
            DumpBytes("Decoded Data", decoded.value());
        }
        return 1;
    }

    return 0;
}

