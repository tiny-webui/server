#include <iostream>
#include "cipher/BigUint.h"
#include "Utility.h"

using namespace TUI::Cipher;

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    BigUint<16> a(10u), b(20u);

    a++;
    ++a;
    AssertWithMessage(a == BigUint<16>(12u), "BigUint increment failed");
    AssertWithMessage(a != b, "BigUint equality check failed");
    AssertWithMessage(a < b, "BigUint less than check failed");
    AssertWithMessage(a <= b, "BigUint less than or equal check failed");
    AssertWithMessage(b > a, "BigUint greater than check failed");
    AssertWithMessage(b >= a, "BigUint greater than or equal check failed");

    BigUint<1> c(static_cast<uint8_t>(0xFF));
    try
    {
        c++;
        AssertWithMessage(false, "BigUint overflow did not throw");
    }
    catch(const std::overflow_error&)
    {
    }

    return 0;
}

