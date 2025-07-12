#include "cipher/Ed25519.h"
#include "Utility.h"

using namespace TUI::Cipher::Ed25519;

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    {
        /** This is not how ed25519 is intended for. Just using this for testing. */
        auto a = Scalar::Generate();
        auto b = Scalar::Generate();
        auto A = a.GetPubKey();
        auto B = b.GetPubKey();
        AssertWithMessage(a*B == b*A, "a*B should equal b*A");
    }
    {
        auto a = Scalar::Generate();
        auto b = Scalar::Generate();
        auto C = Point::Generate();
        auto D = (a + b) * C;
        auto E = a * C + b * C;
        AssertWithMessage(D == E, "D should equal E"); 
    }
    {
        auto a = Scalar::Generate();
        auto B = Point::Generate();
        auto C = Point::Generate();
        auto D = a * (B + C);
        auto E = a * B + a * C;
        AssertWithMessage(D == E, "D should equal E");
    }
    return 0;
}

