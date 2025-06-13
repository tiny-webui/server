#include <iostream>
#include "common/Uuid.h"
#include "Utility.h"

using namespace TUI::Common;

void TestConstructor()
{
    Uuid uuid{};
    AssertWithMessage(uuid != nullptr, "Generated UUID should not be null");
    Uuid uuid2{nullptr};
    AssertWithMessage(uuid2 == nullptr, "UUID initialized with nullptr should be null");
    Uuid uuid3{uuid};
    AssertWithMessage(uuid3 == uuid, "Copy constructor should create an equal UUID");
    Uuid uuid4{std::move(uuid3)};
    AssertWithMessage(uuid4 == uuid, "Move constructor should create an equal UUID");
    Uuid uuid5{"550e8400-e29b-41d4-a716-446655440000"};
    AssertWithMessage(uuid5 != nullptr, "UUID from string should not be null");
}

void TestOperator()
{
    Uuid uuid1{"550e8400-e29b-41d4-a716-446655440000"};
    Uuid uuid2{"550e8400-e29b-41d4-a716-446655440001"};
    AssertWithMessage(uuid1 != uuid2, "UUIDs should not be equal");
    AssertWithMessage(uuid1 < uuid2, "UUID1 should be less than UUID2");
    AssertWithMessage(uuid1 <= uuid2, "UUID1 should be less than or equal to UUID2");
    AssertWithMessage(uuid2 > uuid1, "UUID2 should be greater than UUID1");
    AssertWithMessage(uuid2 >= uuid1, "UUID2 should be greater than or equal to UUID1");
    AssertWithMessage(uuid1 == uuid1, "UUID should be equal to itself");
    AssertWithMessage(uuid1 != nullptr, "UUID should not be null");
    AssertWithMessage(uuid2 != nullptr, "UUID should not be null");
}

void TestConversion()
{
    Uuid uuid{};
    std::string uuidStr = static_cast<std::string>(uuid);
    Uuid uuid2{uuidStr};
    AssertWithMessage(uuid == uuid2, "UUID should be equal after conversion to string and back");
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    RunTest(TestConstructor());
    RunTest(TestOperator());
    RunTest(TestConversion());

    return 0;
}


