#include <iostream>
#include "application/RegistrationTlvType.h"
#include "cipher/Spake2p.h"
#include "common/Base64.h"
#include "common/Tlv.h"

/**
 * This is only for testing.
 * DO NOT use this for real registration.
 * At least not on a machine where your credential might be leaked by the shell history.
 */

using namespace TUI;

int main(int argc, char const *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password>" << std::endl;
        std::cerr << "Please DO NOT use a real username and password in this test." << std::endl;
        return 1;
    }

    std::string username = argv[1];
    std::string password = argv[2];

    auto result = Cipher::Spake2p::Register(username, password);
    Common::Tlv<Application::RegisterTlvType> tlv;
    tlv.SetElement(Application::RegisterTlvType::Username, username);
    tlv.SetElement(Application::RegisterTlvType::Salt, result.salt);
    tlv.SetElement(Application::RegisterTlvType::w0, result.w0);
    tlv.SetElement(Application::RegisterTlvType::L, result.L);
    auto serializedData = tlv.Serialize();
    auto encodedData = Common::Base64::Encode(serializedData);

    std::cout << "Registration string:" << std::endl << encodedData << std::endl;

    return 0;
}

