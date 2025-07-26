#include "common/Tlv.h"
#include "Utility.h"

using namespace TUI::Common;

enum class TestType : uint8_t
{
    A = 0,
    B = 1,
    C = 2,
};

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    Tlv<TestType> tlv;

    std::vector<uint8_t> elementA = {0x01, 0x02, 0x03};
    std::vector<uint8_t> elementB = {0x04, 0x05, 0x06};
    tlv.SetElement(TestType::A, elementA);
    tlv.SetElement(TestType::B, elementB);
    
    auto bytes = tlv.Serialize();

    auto parsedTlv = Tlv<TestType>::Parse(bytes);

    auto elementAOpt = parsedTlv.GetElement(TestType::A);
    AssertWithMessage(elementAOpt.has_value(), "Element A should be present");
    AssertWithMessage(elementAOpt.value() == elementA, "Element A should match the original value");
    auto elementBOpt = parsedTlv.GetElement(TestType::B);
    AssertWithMessage(elementBOpt.has_value(), "Element B should be present");
    AssertWithMessage(elementBOpt.value() == elementB, "Element B should match the original value");
    auto elementCOpt = parsedTlv.GetElement(TestType::C);
    AssertWithMessage(!elementCOpt.has_value(), "Element C should not be present");

    return 0;
}

