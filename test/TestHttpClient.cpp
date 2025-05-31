#include <iostream>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include "network/HttpClient.h"

using namespace TUI::Network;

#define assert(expr, msg) \
    if (!(expr)) \
    { \
        std::cerr << "Assertion failed: " << msg << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        abort(); \
    }

static Tev tev{};

JS::Promise<void> TestGetAsync(std::shared_ptr<Http::Client> client)
{
    auto requestData = Http::RequestData{
        .url = "https://httpbin.org/get",
        .headers = {{"X-Test-Header", "hello from tui"}},
        .body = ""
    };
    auto response = co_await client->MakeRequestAsync(Http::Method::GET, requestData);
    std::cout << "Response: " << std::endl << response << std::endl;
}

JS::Promise<void> TestPostAsync(std::shared_ptr<Http::Client> client)
{
    auto requestData = Http::RequestData{
        .url = "https://httpbin.org/post",
        .headers = {
            {"Content-Type", "application/json"},
            {"X-Test-Header", "hello from tui"}
        },
        .body = R"({"key": "value"})"
    };
    auto response = co_await client->MakeRequestAsync(Http::Method::POST, requestData);
    std::cout << "Response: " << std::endl << response << std::endl;
}

#define RunAsyncTest(testFunc, client) \
    do \
    { \
        try \
        { \
            co_await testFunc(client); \
        } \
        catch(const std::exception& e) \
        { \
            assert(false, "Unhandled exception: " + std::string(e.what())); \
        } \
        catch(...) \
        { \
            assert(false, "Unhandled unknown exception"); \
        } \
    } while (0)

JS::Promise<void> TestAsync()
{
    auto client = Http::Client::Create(tev);
    RunAsyncTest(TestGetAsync, client);
    RunAsyncTest(TestPostAsync, client);
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    TestAsync();

    tev.MainLoop();

    return 0;
}
