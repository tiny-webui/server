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
        })
};

nlohmann::json AzureOpenAI::GetParams() const
{
    return ParamsDefinition.ToJson();
}

void AzureOpenAI::Initialize(const nlohmann::json& params)
{
    _params = ParamsDefinition.Parse(params);
}

RequestData AzureOpenAI::FormatRequest(const Schema::IServer::LinearHistory& history, bool stream) const
{
    RequestData data{};
    data.url = _params.url;
    data.headers = {
        {"Content-Type", "application/json"},
        {"api-key", _params.apiKey}
    };
    nlohmann::json body = {
        {"messages", nlohmann::json::array()},
        {"temperature", 0.5},
        {"stream", stream}
    };
    for (const auto& message : history)
    {
        auto messageJson = nlohmann::json::object();
        messageJson["content"] = nlohmann::json::array();
        if (message.get_role() == Schema::IServer::Role::DEVELOPER)
        {
            messageJson["role"] = "system";
        }
        else if(message.get_role() == Schema::IServer::Role::USER)
        {
            messageJson["role"] = "user";
        }
        else if(message.get_role() == Schema::IServer::Role::ASSISTANT)
        {
            messageJson["role"] = "assistant";
        }
        else
        {
            continue;
        }
        for (const auto& content : message.get_content())
        {
            if (content.get_type() == Schema::IServer::Type::TEXT || content.get_type() == Schema::IServer::Type::REFUSAL)
            {
                /** Azure open ai does not seem to have a special REFUSAL message type */
                messageJson["content"].push_back({
                    {"type", "text"},
                    {"text", content.get_data()}
                });
            }
            else if (content.get_type() == Schema::IServer::Type::IMAGE_URL)
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
    data.body = body.dump();
    return data;
}

Schema::IServer::MessageContent AzureOpenAI::ParseResponse(const std::string& responseString) const
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
    const auto& message = choice.get_message();
    Schema::IServer::MessageContent content;
    content.set_type(message.get_refusal() ? Schema::IServer::Type::REFUSAL : Schema::IServer::Type::TEXT);
    content.set_data(message.get_content());
    return content;
}

std::optional<Schema::IServer::MessageContent> AzureOpenAI::ParseStreamResponse(const StreamResponse::Event& event) const
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
    if (delta.get_content().empty())
    {
        return std::nullopt;
    }
    Schema::IServer::MessageContent content{};
    content.set_type(Schema::IServer::Type::TEXT);
    content.set_data(delta.get_content());
    return content;
}
