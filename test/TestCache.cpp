#include <iostream>
#include "common/Cache.h"
#include "Utility.h"

using namespace TUI::Common;

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    Cache<int, int> cache(3);
    cache.Update(1, 10);
    cache.Update(2, 20);
    cache.Update(3, 30);
    cache.Update(4, 40);
    auto value1 = cache.TryGet(1);
    AssertWithMessage(value1.has_value() == false, "Cache should not contain key 1 after eviction");
    auto value2 = cache.TryGet(2);
    AssertWithMessage(value2.has_value() && value2.value() == 20, "Cache should contain key 2 with value 20");
    cache.Update(5, 50);
    auto value3 = cache.TryGet(3);
    AssertWithMessage(value3.has_value() == false, "Cache should not contain key 3 after eviction");
    cache.Update(4, 4);
    auto value4 = cache.TryGet(4);
    AssertWithMessage(value4.has_value() && value4.value() == 4, "Cache should contain key 4 with updated value 4");

    return 0;
}

