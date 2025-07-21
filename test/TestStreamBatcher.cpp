#include <iostream>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include <js-style-co-routine/AsyncGenerator.h>
#include "common/StreamBatcher.h"
#include "Utility.h"

using namespace TUI::Common;

JS::Promise<void> DelayAsync(Tev& tev, uint64_t delay_ms)
{
    JS::Promise<void> promise{};
    auto timeout = tev.SetTimeout([&promise]() {
        promise.Resolve();
    }, delay_ms);
    co_await promise;
}

JS::AsyncGenerator<int, int> GenerateNumbers(Tev& tev, int n, uint64_t interval_ms)
{
    for (int i = 0; i < n; ++i)
    {
        co_await DelayAsync(tev, interval_ms);
        co_yield i;
    }
    co_return 12345;
}

JS::Promise<void> TestBatchingAsync(Tev& tev)
{
    auto generator = GenerateNumbers(tev, 10, 100);
    auto batcher = StreamBatcher::BatchStream(tev, generator, 600);
    int batches = 0;
    std::list<int> data{};
    while(true)
    {
        auto batch = co_await batcher.NextAsync();
        if (!batch.has_value())
        {
            break;
        }
        batches++;
        for (const auto& item : batch.value())
        {
            data.push_back(item);
        }
    }
    AssertWithMessage(batches == 2, "Expected 2 batches, got " + std::to_string(batches));
    AssertWithMessage(data.size() == 10, "Expected 10 items in total, got " + std::to_string(data.size()));
    for (int i = 0; i < 10; ++i)
    {
        AssertWithMessage(data.front() == i, "Expected item " + std::to_string(i) + ", got " + std::to_string(data.front()));
        data.pop_front();
    }
    auto result = batcher.GetReturnValue();
    AssertWithMessage(result == 12345, "Expected return value 0, got " + std::to_string(result));
}

JS::Promise<void> TestNoBatchingAsync(Tev& tev)
{
    auto generator = GenerateNumbers(tev, 10, 300);
    auto batcher = StreamBatcher::BatchStream(tev, generator, 100);
    int batches = 0;
    std::list<int> data{};
    while(true)
    {
        auto batch = co_await batcher.NextAsync();
        if (!batch.has_value())
        {
            break;
        }
        batches++;
        for (const auto& item : batch.value())
        {
            data.push_back(item);
        }
    }
    AssertWithMessage(batches == 10, "Expected 10 batches, got " + std::to_string(batches));
    AssertWithMessage(data.size() == 10, "Expected 10 items in total, got " + std::to_string(data.size()));
    for (int i = 0; i < 10; ++i)
    {
        AssertWithMessage(data.front() == i, "Expected item " + std::to_string(i) + ", got " + std::to_string(data.front()));
        data.pop_front();
    }
    auto result = batcher.GetReturnValue();
    AssertWithMessage(result == 12345, "Expected return value 0, got " + std::to_string(result));
}

JS::AsyncGenerator<int, int> GenerateNumbersAndThrow(Tev& tev, int n, uint64_t interval_ms)
{
    for (int i = 0; i < n; ++i)
    {
        co_await DelayAsync(tev, interval_ms);
        co_yield i;
    }
    throw std::runtime_error("Test exception");
}

JS::Promise<void> TestExceptionAsync(Tev& tev)
{
    auto generator = GenerateNumbersAndThrow(tev, 10, 100);
    auto batcher = StreamBatcher::BatchStream(tev, generator, 600);
    int batches = 0;
    std::list<int> data{};
    try
    {
        while(true)
        {
            auto batch = co_await batcher.NextAsync();
            if (!batch.has_value())
            {
                break;
            }
            batches++;
            for (const auto& item : batch.value())
            {
                data.push_back(item);
            }
        }
        AssertWithMessage(false, "Expected exception to be thrown, but it was not.");
    }
    catch (const std::exception& e)
    {
        AssertWithMessage(std::string(e.what()) == "Test exception", "Expected 'Test exception', got '" + std::string(e.what()) + "'");
    }
    AssertWithMessage(batches == 2, "Expected 2 batches, got " + std::to_string(batches));
    AssertWithMessage(data.size() == 10, "Expected 10 items in total, got " + std::to_string(data.size()));
}

JS::Promise<void> TestAsync(Tev& tev)
{
    RunAsyncTest(TestBatchingAsync(tev));
    RunAsyncTest(TestNoBatchingAsync(tev));
    RunAsyncTest(TestExceptionAsync(tev));
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    Tev tev{};

    TestAsync(tev);

    tev.MainLoop();

    return 0;
}


