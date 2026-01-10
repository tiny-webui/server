#include <unordered_set>
#include <cctype>
#include "OpenAI.h"

using namespace TUI;
using namespace TUI::ApiProvider;
using namespace TUI::ApiProvider::Option;
using namespace TUI::Network::Http;

OptionList<OpenAI::Params> OpenAI::ParamsDefinition = {
	CreateOption<OpenAI::Params, StringOption<OpenAI::Params>>(
		"url",
		true,
		[](OpenAI::Params& params, std::string value){
			params.url = std::move(value);
		},
		"https://api.openai.com/v1/responses"),
	CreateOption<OpenAI::Params, StringOption<OpenAI::Params>>(
		"apiKey",
		false,
		[](OpenAI::Params& params, std::string value){
			params.apiKey = std::move(value);
		}),
	CreateOption<OpenAI::Params, StringOption<OpenAI::Params>>(
		"model",
		false,
		[](OpenAI::Params& params, std::string value){
			params.model = std::move(value);
		}),
	CreateOption<OpenAI::Params, NumberFromRangeOption<OpenAI::Params>>(
		"temperature",
		true,
		[](OpenAI::Params& params, double value){
			params.temperature = value;
		},
		0.0,
		2.0,
		0.5),
	CreateOption<OpenAI::Params, StringOption<OpenAI::Params>>(
		"reasoningEffort",
		true,
		[](OpenAI::Params& params, std::string value){
			static const std::unordered_set<std::string> valid{"none", "low", "medium", "high", ""};
			if (valid.find(value) == valid.end())
			{
				throw std::invalid_argument("Invalid reasoningEffort value");
			}
			params.reasoningEffort = std::move(value);
		},
		"")
};

nlohmann::json OpenAI::GetParams() const
{
	return ParamsDefinition.ToJson();
}

void OpenAI::Initialize(const nlohmann::json& params)
{
	_params = ParamsDefinition.Parse(params);
}

RequestData OpenAI::FormatRequest(const Schema::IServer::LinearHistory& history, bool stream) const
{
	RequestData data{};
	data.url = _params.url;
	data.headers = {
		{"Content-Type", "application/json"},
		{"Authorization", "Bearer " + _params.apiKey}
	};

	nlohmann::json body = {
		{"model", _params.model},
		{"input", nlohmann::json::array()},
		{"temperature", _params.temperature},
		{"stream", stream}
	};

	if (!_params.reasoningEffort.empty())
	{
		body["reasoning"] = {
			{"effort", _params.reasoningEffort}
		};
	}

	for (const auto& message : history)
	{
		auto messageJson = nlohmann::json::object();
		messageJson["content"] = nlohmann::json::array();
		if (message.get_role() == Schema::IServer::MessageRole::DEVELOPER)
		{
			messageJson["role"] = "system";
		}
		else if(message.get_role() == Schema::IServer::MessageRole::USER)
		{
			messageJson["role"] = "user";
		}
		else if(message.get_role() == Schema::IServer::MessageRole::ASSISTANT)
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
				messageJson["content"].push_back({
					{"type", message.get_role() == Schema::IServer::MessageRole::ASSISTANT ? "output_text" : "input_text"},
					{"text", content.get_data()}
				});
			}
			else if (content.get_type() == Schema::IServer::Type::IMAGE_URL)
			{
				messageJson["content"].push_back({
					{"type", "input_image"},
					{"image_url", content.get_data()}
				});
			}
			else
			{
				continue;
			}
		}
		body["input"].push_back(messageJson);
	}
	data.body = body.dump();
	return data;
}


Schema::IServer::MessageContent OpenAI::ParseResponse(const std::string& responseString) const
{
	auto json = nlohmann::json::parse(responseString);
	if (!json.contains("output") || !json.at("output").is_array())
	{
		throw std::invalid_argument("Invalid response: missing output array");
	}

	std::string text{};
	for (const auto& item : json.at("output"))
	{
		if (!item.contains("content") || !item.at("content").is_array())
		{
			continue;
		}
		for (const auto& content : item.at("content"))
		{
			auto typeIt = content.find("type");
			if (typeIt == content.end())
			{
				continue;
			}
			auto typeStr = typeIt->get<std::string>();
			if (content.contains("text") && (typeStr == "output_text" || typeStr == "text"))
			{
				text += content.at("text").get<std::string>();
			}
		}
	}

	if (text.empty())
	{
		throw std::runtime_error("No output text in response");
	}

	Schema::IServer::MessageContent content;
	content.set_type(Schema::IServer::Type::TEXT);
	content.set_data(std::move(text));
	return content;
}

std::optional<Schema::IServer::MessageContent> OpenAI::ParseStreamResponse(const StreamResponse::Event& event) const
{
	if (!event.value.has_value())
	{
		return std::nullopt;
	}

	auto trimLeading = [](std::string s){
		while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
		{
			s.erase(s.begin());
		}
		return s;
	};

	const std::string eventType = trimLeading(event.type.value_or(""));
	const std::string valueString = event.value.value();

	if (eventType == "response.completed" || valueString == "[DONE]")
	{
		return std::nullopt;
	}

	if (eventType == "response.output_text.delta")
	{
		try
		{
			auto json = nlohmann::json::parse(valueString);
			if (json.contains("delta") && json.at("delta").is_string())
			{
				Schema::IServer::MessageContent content{};
				content.set_type(Schema::IServer::Type::TEXT);
				content.set_data(json.at("delta").get<std::string>());
				return content;
			}
		}
		catch(...)
		{
			return std::nullopt;
		}
	}

	return std::nullopt;
}
