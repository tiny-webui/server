#include "cipher/Counter.h"
#include <iostream>
#include "Utility.h"

using namespace TUI::Cipher;

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    Counter<2> c1;
    Counter<2> c2{std::vector<uint8_t>{0x01, 0x00}};

    AssertWithMessage(c1 < c2, "Expected c1 < c2");
    AssertWithMessage(c1 != c2, "Expected c1 != c2");

    c1++;
    AssertWithMessage(c1 == c2, "Expected c1 == c2 after incrementing c1");

    c1++;
    AssertWithMessage(c1 > c2, "Expected c1 > c2 after incrementing c1 again");

    Counter<2> c3{std::vector<uint8_t>{0xFF, 0xFF}};
    try
    {
        c3++;
        AssertWithMessage(false, "Expected overflow exception, but it was not thrown.");
    }
    catch(const std::overflow_error& e)
    {
    }

    Counter<2> c4 = c1;
    AssertWithMessage(c4 == c1, "Expected c4 to be equal to c1 after assignment");

    Counter<2> c5 = std::move(c1);
    AssertWithMessage(c5 == c4, "Expected c5 to be equal to c4 after move assignment");

    Counter<2>::ValueType bytes = static_cast<Counter<2>::ValueType>(c5);
    Counter<2> c6(bytes);
    AssertWithMessage(c6 == c5, "Expected c6 to be equal to c5 after constructing from bytes");

    return 0;
}


