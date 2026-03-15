#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include <unistd.h>
#include "apiProvider/Factory.h"
#include "network/HttpClient.h"
#include "network/HttpStreamResponseParser.h"
#include "Utility.h"

using namespace TUI;
using namespace TUI::Network;

static Tev tev{};
static std::filesystem::path configFilePath;

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

static Schema::IServer::LinearHistory GetChatHistory()
{
    auto json = nlohmann::json::parse(R"([
        {
            "role": "developer",
            "content": [
                {"type": "text", "data": "You are a helpful assistant."}
            ]
        },
        {
            "role": "user",
            "content": [
                {"type": "text", "data": "What is the capital of France?"}
            ]
        },
        {
            "role": "assistant",
            "content": [
                {"type": "text", "data": "Paris"}
            ]
        },
        {
            "role": "user",
            "content": [
                {"type": "text", "data": "What's the second largest city of France?"}
            ]
        }
    ])");
    return json.get<Schema::IServer::LinearHistory>();
}

static Schema::IServer::LinearHistory GetToolCallHistory()
{
    auto json = nlohmann::json::parse(R"([
        {
            "role": "user",
            "content": [
                {"type": "text", "data": "What is the current room temperature?"}
            ]
        }
    ])");
    return json.get<Schema::IServer::LinearHistory>();
}

static std::vector<Schema::IServer::Tool> GetTools()
{
    Schema::IServer::Tool tool;
    tool.set_name("get_room_temperature");
    tool.set_description("Get the current room temperature.");
    tool.set_parameters(nlohmann::json::parse(R"({
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
    })"));
    return {tool};
}

static std::string ExecuteFunctionCall(const Schema::IServer::FunctionCallMessage& fcm)
{
    if (fcm.get_name() == "get_room_temperature")
    {
        return R"({"temperature": "22.5°C"})";
    }
    return R"({"error": "unknown function"})";
}

static void AppendFunctionCallOutput(
    Schema::IServer::LinearHistory& history,
    const Schema::IServer::FunctionCallMessage& fcm)
{
    // Add the function call itself to history
    history.push_back(fcm);

    // Execute and add the output
    auto result = ExecuteFunctionCall(fcm);
    Schema::IServer::FunctionCallOutputMessage fcom;
    fcom.set_type(Schema::IServer::FunctionCallOutputMessageType::FUNCTION_CALL_OUTPUT);
    fcom.set_call_id(fcm.get_call_id());
    Schema::IServer::MessageContent content;
    content.set_type(Schema::IServer::MessageContentType::TEXT);
    content.set_data(std::move(result));
    fcom.set_output({content});
    history.push_back(std::move(fcom));
}

JS::Promise<void> TestBulkChatAsync()
{
    auto client = Http::Client::Create(tev);
    auto params = LoadJsonFile(configFilePath);
    auto provider = ApiProvider::Factory::CreateProvider(
        params["providerName"].get<std::string>(),
        params["providerParams"]);
    auto history = GetChatHistory();
    auto requestData = provider->FormatRequest(history, false);
    auto response = co_await client->MakeRequest(Http::Method::POST, requestData).GetResponseAsync();
    auto result = provider->ParseResponse(response);
    std::cout << nlohmann::json(result).dump(4) << std::endl;
}

JS::Promise<void> TestStreamChatAsync()
{
    auto client = Http::Client::Create(tev);
    auto params = LoadJsonFile(configFilePath);
    auto provider = ApiProvider::Factory::CreateProvider(
        params["providerName"].get<std::string>(),
        params["providerParams"]);
    auto history = GetChatHistory();
    auto requestData = provider->FormatRequest(history, true);
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
        auto segment = provider->ParseStreamResponse(event.value());
        if (segment.has_value())
        {
            if (std::holds_alternative<std::string>(segment.value()))
            {
                std::cout << std::get<std::string>(segment.value());
                std::cout.flush();
            }
        }
    }
}

JS::Promise<void> TestBulkToolCallAsync()
{
    auto client = Http::Client::Create(tev);
    auto params = LoadJsonFile(configFilePath);
    auto provider = ApiProvider::Factory::CreateProvider(
        params["providerName"].get<std::string>(),
        params["providerParams"]);
    auto history = GetToolCallHistory();
    auto tools = GetTools();

    while (true)
    {
        auto requestData = provider->FormatRequest(history, false, tools);
        auto response = co_await client->MakeRequest(Http::Method::POST, requestData).GetResponseAsync();
        auto result = provider->ParseResponse(response);

        bool hasFunctionCall = false;
        for (const auto& msg : result)
        {
            if (std::holds_alternative<Schema::IServer::FunctionCallMessage>(msg))
            {
                hasFunctionCall = true;
                const auto& fcm = std::get<Schema::IServer::FunctionCallMessage>(msg);
                std::cout << "[tool call] " << fcm.get_name() << "(" << fcm.get_arguments() << ")" << std::endl;
                AppendFunctionCallOutput(history, fcm);
            }
            else if (std::holds_alternative<Schema::IServer::ChatMessage>(msg))
            {
                const auto& chatMsg = std::get<Schema::IServer::ChatMessage>(msg);
                for (const auto& content : chatMsg.get_content())
                {
                    std::cout << content.get_data();
                }
                std::cout << std::endl;
            }
        }
        if (!hasFunctionCall)
        {
            break;
        }
    }
}

JS::Promise<void> TestStreamToolCallAsync()
{
    auto client = Http::Client::Create(tev);
    auto params = LoadJsonFile(configFilePath);
    auto provider = ApiProvider::Factory::CreateProvider(
        params["providerName"].get<std::string>(),
        params["providerParams"]);
    auto history = GetToolCallHistory();
    auto tools = GetTools();

    while (true)
    {
        auto requestData = provider->FormatRequest(history, true, tools);
        auto responseStream = client->MakeStreamRequest(Http::Method::POST, requestData).GetResponseStream();
        auto parser = Http::StreamResponse::AsyncParser(responseStream);
        auto eventStream = parser.Parse();

        bool hasFunctionCall = false;
        std::map<int, Schema::IServer::FunctionCallMessage> pendingCalls;
        while (true)
        {
            auto event = co_await eventStream.NextAsync();
            if (!event.has_value())
            {
                break;
            }
            auto segment = provider->ParseStreamResponse(event.value());
            if (!segment.has_value())
            {
                continue;
            }
            if (std::holds_alternative<std::string>(segment.value()))
            {
                std::cout << std::get<std::string>(segment.value());
                std::cout.flush();
            }
            else
            {
                const auto& segClass = std::get<Schema::IServer::ChatCompletionSegmentClass>(segment.value());
                if (segClass.get_event() == Schema::IServer::Event::FUNCTION_CALL_START)
                {
                    auto fcm = segClass.get_data().value();
                    std::cout << "[tool call start] " << fcm.get_name() << std::endl;
                    pendingCalls[static_cast<int>(pendingCalls.size())] = std::move(fcm);
                }
                else if (segClass.get_event() == Schema::IServer::Event::FUNCTION_CALL_END)
                {
                    hasFunctionCall = true;
                    auto endFcm = segClass.get_data().value();
                    // Find the pending call and fill in arguments
                    for (auto& [idx, pending] : pendingCalls)
                    {
                        if (pending.get_arguments().empty())
                        {
                            pending.set_arguments(endFcm.get_arguments());
                            std::cout << "[tool call end] " << pending.get_name() << "(" << pending.get_arguments() << ")" << std::endl;
                            AppendFunctionCallOutput(history, pending);
                            break;
                        }
                    }
                }
            }
        }
        std::cout << std::endl;
        if (!hasFunctionCall)
        {
            break;
        }
    }
}

static JS::Promise<void> TestAsync()
{
    RunAsyncTest(TestBulkChatAsync());
    RunAsyncTest(TestStreamChatAsync());
    RunAsyncTest(TestBulkToolCallAsync());
    RunAsyncTest(TestStreamToolCallAsync());
}

int main(int argc, char const *argv[])
{
    int opt = 0;
    while ((opt = getopt(argc, const_cast<char**>(argv), "c:")) != -1)
    {
        switch (opt)
        {
            case 'c':
                configFilePath = optarg;
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " -c <config_file>" << std::endl;
                return 1;
        }
    }
    if (configFilePath.empty())
    {
        std::cerr << "Usage: " << argv[0] << " -c <config_file>" << std::endl;
        return 1;
    }

    TestAsync();

    tev.MainLoop();

    return 0;
}

