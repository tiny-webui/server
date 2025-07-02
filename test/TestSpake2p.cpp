#include <iostream>
#include <iomanip>
#include "cipher/Spake2p.h"

using namespace TUI::Cipher;
using namespace TUI::Cipher::Spake2p;

static void DumpHex(const std::string& label, const uint8_t* data, size_t size)
{
    std::cout << label << ": " << std::endl;
    for (size_t i = 0; i < size; ++i)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]) << " ";
        if (i % 16 == 15) // Print 16 bytes per line
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

    auto result = Spake2p::Register("user", "password");
    DumpHex("w0", result.w0.data(), result.w0.size());
    DumpHex("w1", result.w1.data(), result.w1.size());
    DumpHex("salt", result.salt.data(), result.salt.size());
    return 0;
}
