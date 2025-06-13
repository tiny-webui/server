#include <iostream>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include "network/HttpClient.h"
#include "Utility.h"

using namespace TUI::Network;

static Tev tev{};

JS::Promise<void> DelayAsync(int ms)
{
    JS::Promise<void> promise;
    tev.SetTimeout([=]() mutable {
        promise.Resolve();
    }, ms);
    return promise;
}

JS::Promise<void> TestGetAsync(std::shared_ptr<Http::Client> client)
{
    auto requestData = Http::RequestData{
        .url = "https://httpbin.org/get",
        .headers = {{"X-Test-Header", "hello from tui"}},
        .body = ""
    };
    auto response = co_await client->MakeRequest(Http::Method::GET, requestData).GetResponseAsync();
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
    auto response = co_await client->MakeRequest(Http::Method::POST, requestData).GetResponseAsync();
    std::cout << "Response: " << std::endl << response;
    std::cout << std::endl;
}

JS::Promise<void> TestCancelImmediatelyAsync(std::shared_ptr<Http::Client> client)
{
    auto requestData = Http::RequestData{
        .url = "https://httpbin.org/delay/5",
        .headers = {{"X-Test-Header", "hello from tui"}},
        .body = ""
    };
    auto request = client->MakeRequest(Http::Method::GET, requestData);
    auto responsePromise = request.GetResponseAsync();
    
    // Cancel the request immediately
    client->CancelRequest(request);
    
    try
    {
        co_await responsePromise;
    }
    catch (const Http::Client::RequestCancelledException&)
    {
        std::cout << "Request was cancelled successfully." << std::endl;
    }
}

JS::Promise<void> TestCancelAsync(std::shared_ptr<Http::Client> client)
{
    auto requestData = Http::RequestData{
        .url = "https://httpbin.org/delay/5",
        .headers = {{"X-Test-Header", "hello from tui"}},
        .body = ""
    };
    auto request = client->MakeRequest(Http::Method::GET, requestData);
    auto responsePromise = request.GetResponseAsync();

    // Cancel the request after a short delay
    co_await DelayAsync(10);
    client->CancelRequest(request);

    try
    {
        co_await responsePromise;
    }
    catch (const Http::Client::RequestCancelledException&)
    {
        std::cout << "Request was cancelled successfully." << std::endl;
    }
}

JS::Promise<void> TestStreamAsync(std::shared_ptr<Http::Client> client)
{
    auto requestData = Http::RequestData{
        .url = "https://httpbin.org/stream/5",
        .headers = {{"X-Test-Header", "hello from tui"}},
        .body = ""
    };
    auto responseStream = client->MakeStreamRequest(Http::Method::GET, requestData).GetResponseStream();
    
    while(true)
    {
        auto next = co_await responseStream.NextAsync();
        if (!next.has_value())
        {
            break;
        }
        std::cout << "Streamed data: " << std::endl << *next;
    }
    std::cout << "Stream completed." << std::endl;
}

JS::Promise<void> TestStreamCancelAsync(std::shared_ptr<Http::Client> client)
{
    auto requestData = Http::RequestData{
        .url = "https://httpbin.org/stream/5",
        .headers = {{"X-Test-Header", "hello from tui"}},
        .body = ""
    };
    auto streamRequest = client->MakeStreamRequest(Http::Method::GET, requestData);
    
    // Cancel the stream after a short delay
    co_await DelayAsync(10);
    client->CancelRequest(streamRequest);

    auto responseStream = streamRequest.GetResponseStream();
    try
    {
        while (true)
        {
            auto next = co_await responseStream.NextAsync();
            if (!next.has_value())
            {
                break;
            }
            std::cout << "Streamed data: " << std::endl << *next;
        }
        std::cout << "Stream completed." << std::endl;
    }
    catch (const Http::Client::RequestCancelledException&)
    {
        std::cout << "Stream was cancelled successfully." << std::endl;
    }
}

JS::Promise<void> TestAsync()
{
    auto client = Http::Client::Create(tev);
    RunAsyncTest(TestGetAsync(client));
    RunAsyncTest(TestPostAsync(client));
    RunAsyncTest(TestCancelImmediatelyAsync(client));
    RunAsyncTest(TestCancelAsync(client));
    RunAsyncTest(TestStreamAsync(client));
    RunAsyncTest(TestStreamCancelAsync(client));
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    TestAsync();

    tev.MainLoop();

    return 0;
}
