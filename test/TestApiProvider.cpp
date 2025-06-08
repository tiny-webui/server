#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include <unistd.h>
#include "apiProvider/AzureOpenAI.h"
#include "network/HttpClient.h"
#include "network/HttpStreamResponseParser.h"

using namespace TUI;
using namespace TUI::Network;

static Tev tev{};
static std::filesystem::path configFilePath;
static std::filesystem::path historyFilePath;

static nlohmann::json LoadJsonFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return nlohmann::json::parse(content);
}

static Schema::Chat::ChatHistory LoadChatHistory(const std::filesystem::path& path)
{
    auto json = LoadJsonFile(path);
    return json.get<Schema::Chat::ChatHistory>();
}

JS::Promise<void> TestBulkChatAsync()
{
    auto client = Http::Client::Create(tev);
    ApiProvider::AzureOpenAI provider{};
    auto params = LoadJsonFile(configFilePath);
    provider.Initialize(params);
    auto history = LoadChatHistory(historyFilePath);
    auto requestData = provider.FormatRequest(history, false);
    auto response = co_await client->MakeRequest(Http::Method::POST, requestData).GetResponseAsync();
    auto message = provider.ParseResponse(response);
    std::cout << nlohmann::json(message).dump(4) << std::endl;
}

JS::Promise<void> TestStreamChatAsync()
{
    auto client = Http::Client::Create(tev);
    ApiProvider::AzureOpenAI provider{};
    auto params = LoadJsonFile(configFilePath);
    provider.Initialize(params);
    auto history = LoadChatHistory(historyFilePath);
    auto requestData = provider.FormatRequest(history, true);
    auto streamEventParser = Http::StreamResponse::Parser{};
    auto responseStream = client->MakeStreamRequest(Http::Method::POST, requestData).GetResponseStream();
    auto parser = Http::StreamResponse::AsyncParser(responseStream);
    auto eventStream = parser.Parse();
    while (true)
    {
        auto event = co_await eventStream.NextAsync();
        if (!event.has_value())
        {
            std::cout << std::endl;
            break;
        }
        auto message = provider.ParseStreamResponse(event.value());
        if (message.has_value())
        {
            std::cout << message.value().get_data();
            std::cout.flush();
        }
    }
}

#define testAssert(expr, msg) \
    if (!(expr)) \
    { \
        std::cerr << "Assertion failed: " << msg << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        abort(); \
    }

#define RunAsyncTest(testFunc) \
    do \
    { \
        try \
        { \
            std::cout << "Running test: " #testFunc << std::endl; \
            co_await testFunc(); \
            std::cout << "Test " #testFunc " completed successfully." << std::endl; \
        } \
        catch(const std::exception& e) \
        { \
            testAssert(false, "Unhandled exception: " + std::string(e.what())); \
        } \
        catch(...) \
        { \
            testAssert(false, "Unhandled unknown exception"); \
        } \
    } while (0)

static JS::Promise<void> TestAsync()
{
    RunAsyncTest(TestBulkChatAsync);
    RunAsyncTest(TestStreamChatAsync);
}

int main(int argc, char const *argv[])
{
    int opt = 0;
    while ((opt = getopt(argc, const_cast<char**>(argv), "c:h:")) != -1)
    {
        switch (opt)
        {
            case 'c':
                configFilePath = optarg;
                break;
            case 'h':
                historyFilePath = optarg;
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " -c <config_file> -h <history_file>" << std::endl;
                return 1;
        }
    }
    if (configFilePath.empty() || historyFilePath.empty())
    {
        std::cerr << "Both config file and history file must be specified." << std::endl;
        return 1;
    }

    TestAsync();

    tev.MainLoop();

    return 0;
}

