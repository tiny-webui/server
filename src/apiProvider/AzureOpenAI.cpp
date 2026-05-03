#include "AzureOpenAI.h"
#include "schema/AzureOpenAI.h"

using namespace TUI;
using namespace TUI::ApiProvider;
using namespace TUI::ApiProvider::Option;
using namespace TUI::Network::Http;


OptionList<AzureOpenAI::Params> AzureOpenAI::ParamsDefinition = {
    CreateOption<AzureOpenAI::Params, StringOption<AzureOpenAI::Params>>(
        "url",
        false,
        [](AzureOpenAI::Params& params, std::string value){
            params.url = std::move(value);
        }),
    CreateOption<AzureOpenAI::Params, StringOption<AzureOpenAI::Params>>(
        "apiKey",
        false,
        [](AzureOpenAI::Params& params, std::string value){
            params.apiKey = std::move(value);
        }),
    CreateOption<AzureOpenAI::Params, NumberFromRangeOption<AzureOpenAI::Params>>(
        "temperature",
        true,
        [](AzureOpenAI::Params& params, double value){
            params.temperature = value;
        },
        0.0,
        2.0,
        0.5),
    CreateOption<AzureOpenAI::Params, StringOption<AzureOpenAI::Params>>(
        "model",
        true,
        [](AzureOpenAI::Params& params, std::string value){
            params.model = value;
        },
        "")
};

nlohmann::json AzureOpenAI::GetParams() const
{
    return ParamsDefinition.ToJson();
}

void AzureOpenAI::Initialize(const nlohmann::json& params)
{
    _params = ParamsDefinition.Parse(params);
}

RequestData AzureOpenAI::FormatRequest(const Schema::IServer::LinearHistory& history, bool stream, const std::optional<std::vector<Schema::IServer::Tool>>& /*tools*/) const
{
    RequestData data{};
    data.url = _params.url;
    data.headers = {
        {"Content-Type", "application/json"},
        {"api-key", _params.apiKey}
    };
    nlohmann::json body = {
        {"messages", nlohmann::json::array()},
        {"temperature", _params.temperature},
        {"stream", stream}
    };
    if (!_params.model.empty())
    {
        body["model"] = _params.model;
    }
    for (const auto& message : history)
    {
        if (std::holds_alternative<Schema::IServer::ChatMessage>(message))
        {
            const auto& chatMessage = std::get<Schema::IServer::ChatMessage>(message);
            auto messageJson = nlohmann::json::object();
            messageJson["content"] = nlohmann::json::array();
            if (chatMessage.get_role() == Schema::IServer::ChatMessageRole::DEVELOPER)
            {
                messageJson["role"] = "system";
            }
            else if(chatMessage.get_role() == Schema::IServer::ChatMessageRole::USER)
            {
                messageJson["role"] = "user";
            }
            else if(chatMessage.get_role() == Schema::IServer::ChatMessageRole::ASSISTANT)
            {
                messageJson["role"] = "assistant";
            }
            else
            {
                continue;
            }
            for (const auto& content : chatMessage.get_content())
            {
                if (content.get_type() == Schema::IServer::MessageContentType::TEXT || content.get_type() == Schema::IServer::MessageContentType::REFUSAL)
                {
                    /** Azure open ai does not seem to have a special REFUSAL message type */
                    messageJson["content"].push_back({
                        {"type", "text"},
                        {"text", content.get_data()}
                    });
                }
                else if (content.get_type() == Schema::IServer::MessageContentType::IMAGE_URL)
                {
                    messageJson["content"].push_back({
                        {"type", "image_url"},
                        {"image_url", {
                            {"url", content.get_data()}
                        }}
                    });
                }
                else
                {
                    continue;
                }
            }
            body["messages"].push_back(messageJson);
        }
        else if (std::holds_alternative<Schema::IServer::FunctionCallMessage>(message)
            || std::holds_alternative<Schema::IServer::FunctionCallOutputMessage>(message))
        {
            throw std::runtime_error("AzureOpenAI does not support function call messages");
        }
    }
    data.body = body.dump();
    return data;
}

Schema::IServer::LinearHistory AzureOpenAI::ParseResponse(const std::string& responseString) const
{
    Schema::AzureOpenAI::BulkResponse response;
    try
    {
        response = nlohmann::json::parse(responseString).get<Schema::AzureOpenAI::BulkResponse>();
    }
    catch(const std::exception& e)
    {
        throw std::invalid_argument("Invalid response: " + std::string(e.what()));
    }
    if (response.get_choices().empty())
    {
        throw std::runtime_error("No choices in response");
    }
    const auto& choice = response.get_choices().front();
    const auto& msg = choice.get_message();
    Schema::IServer::MessageContent content;
    content.set_type(msg.get_refusal() ? Schema::IServer::MessageContentType::REFUSAL : Schema::IServer::MessageContentType::TEXT);
    content.set_data(msg.get_content());
    Schema::IServer::ChatMessage chatMessage;
    chatMessage.set_role(Schema::IServer::ChatMessageRole::ASSISTANT);
    chatMessage.set_content({content});
    return Schema::IServer::LinearHistory{std::move(chatMessage)};
}

std::optional<Schema::IServer::ChatCompletionSegment> AzureOpenAI::ParseStreamResponse(const StreamResponse::Event& event) const
{
    if (!event.value.has_value())
    {
        return std::nullopt;
    }
    std::string valueString = event.value.value();
    if (valueString == "[DONE]")
    {
        return std::nullopt;
    }
    Schema::AzureOpenAI::StreamResponse response;
    try
    {
        response = nlohmann::json::parse(valueString).get<Schema::AzureOpenAI::StreamResponse>();
    }
    catch(...)
    {
        return std::nullopt;
    }
    if (response.get_choices().empty())
    {
        return std::nullopt;
    }
    const auto& choice = response.get_choices().front();
    const auto& delta = choice.get_delta();
    if (!delta.get_content().has_value())
    {
        return std::nullopt;
    }
    if (delta.get_content().value().empty())
    {
        return std::nullopt;
    }
    return delta.get_content().value();
}
