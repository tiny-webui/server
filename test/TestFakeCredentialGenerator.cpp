#include <iostream>
#include "cipher/FakeCredentialGenerator.h"
#include "Utility.h"

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    TUI::Cipher::FakeCredentialGenerator generator(2);
    auto user1Cred1 = generator.GetFakeCredential("user1");
    auto user1Cred2 = generator.GetFakeCredential("user1");
    AssertWithMessage(user1Cred1.salt == user1Cred2.salt, "Salt should be the same for the same username");
    auto user2Cred1 = generator.GetFakeCredential("user2");
    AssertWithMessage(user1Cred1.salt != user2Cred1.salt, "Salt should be different for different usernames");
    /** Push user1 out of the cache */
    generator.GetFakeCredential("user3");
    auto user1Cred3 = generator.GetFakeCredential("user1");
    AssertWithMessage(user1Cred1.salt == user1Cred3.salt, "Salt should be the same for the same username after eviction");

    return 0;
}

