#include <iostream>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include <common/WorkerThread.h>
#include "Utility.h"

using namespace TUI::Common;

Tev tev{};

JS::Promise<void> DelayAsync(int ms)
{
    JS::Promise<void> promise;
    tev.SetTimeout([=]() mutable {
        promise.Resolve();
    }, ms);
    return promise;
}

JS::Promise<void> TestVoidSuccessAsync()
{
    WorkerThread workerThread{tev};
    co_await workerThread.ExecTaskAsync([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });
}

JS::Promise<void> TestSuccessAsync()
{
    WorkerThread workerThread{tev};
    std::string result = co_await workerThread.ExecTaskAsync([]() -> std::string {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        return "Hello from worker thread!";
    });
    AssertWithMessage(result == "Hello from worker thread!", "Wrong result");
}

JS::Promise<void> TestExceptionAsync()
{
    WorkerThread workerThread{tev};
    try
    {
        co_await workerThread.ExecTaskAsync([]() -> std::string {
            throw std::runtime_error("Test exception");
        });
        AssertWithMessage(false, "Expected exception not thrown");
    }
    catch (const std::runtime_error& e)
    {
        AssertWithMessage(std::string(e.what()) == "Test exception", "Wrong exception message");
    }
    catch (...)
    {
        AssertWithMessage(false, "Unexpected exception type");
    }
}

JS::Promise<void> TestQueueAsync()
{
    WorkerThread workerThread{tev};
    std::vector<JS::Promise<std::string>> promises;
    for (int i = 0; i < 10; ++i)
    {
        promises.push_back(std::move(workerThread.ExecTaskAsync([=]() -> std::string {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return "Task " + std::to_string(i);
        })));
    }
    auto results = co_await JS::Promise<std::string>::All(promises);
    AssertWithMessage(results.size() == 10, "Expected 10 results");
    for (int i = 0; i < 10; ++i)
    {
        AssertWithMessage(results[i] == "Task " + std::to_string(i), "Result mismatch for task " + std::to_string(i));
    }
}

JS::Promise<void> TestCloseAsync()
{
    WorkerThread workerThread{tev};
    auto promise = workerThread.ExecTaskAsync([]() -> std::string {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return "Task completed";
    });
    co_await DelayAsync(50);
    workerThread.Close();
    try
    {
        co_await promise;
        AssertWithMessage(false, "Expected exception not thrown after worker thread closed");
    }
    catch (const std::exception& e)
    {
        AssertWithMessage(std::string(e.what()) == "WorkerThread closed", "Wrong exception message");
    }
    catch (...)
    {
        AssertWithMessage(false, "Unexpected exception type");
    }
}

JS::Promise<void> TestAsync()
{
    RunAsyncTest(TestVoidSuccessAsync());
    RunAsyncTest(TestSuccessAsync());
    RunAsyncTest(TestExceptionAsync());
    RunAsyncTest(TestQueueAsync());
    RunAsyncTest(TestCloseAsync());
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    TestAsync();

    tev.MainLoop();
    
    return 0;
}

