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

RequestData OpenAI::FormatRequest(const Schema::IServer::LinearHistory& history, bool stream, const std::optional<std::vector<Schema::IServer::Tool>>& tools) const
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

	if (tools.has_value() && !tools.value().empty())
	{
		body["tools"] = nlohmann::json::array();
		for (const auto& tool : tools.value())
		{
			body["tools"].push_back({
				{"type", "function"},
				{"name", tool.get_name()},
				{"description", tool.get_description()},
				{"parameters", tool.get_parameters()}
			});
		}
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
					messageJson["content"].push_back({
						{"type", chatMessage.get_role() == Schema::IServer::ChatMessageRole::ASSISTANT ? "output_text" : "input_text"},
						{"text", content.get_data()}
					});
				}
				else if (content.get_type() == Schema::IServer::MessageContentType::IMAGE_URL)
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
		else if (std::holds_alternative<Schema::IServer::FunctionCallMessage>(message))
		{
			const auto& functionCall = std::get<Schema::IServer::FunctionCallMessage>(message);
			auto functionCallJson = nlohmann::json::object();
			if (functionCall.get_extra().has_value())
			{
				functionCallJson = functionCall.get_extra().value();
			}
			functionCallJson["type"] = "function_call";
			functionCallJson["call_id"] = functionCall.get_call_id();
			functionCallJson["name"] = functionCall.get_name();
			functionCallJson["arguments"] = functionCall.get_arguments();
			body["input"].push_back(functionCallJson);
		}
		else if (std::holds_alternative<Schema::IServer::FunctionCallOutputMessage>(message))
		{
			const auto& functionCallOutput = std::get<Schema::IServer::FunctionCallOutputMessage>(message);
			auto functionCallOutputJson = nlohmann::json::object();
			if (functionCallOutput.get_extra().has_value())
			{
				functionCallOutputJson = functionCallOutput.get_extra().value();
			}
			functionCallOutputJson["type"] = "function_call_output";
			functionCallOutputJson["call_id"] = functionCallOutput.get_call_id();
			std::string outputText;
			for (const auto& content : functionCallOutput.get_output())
			{
				if (content.get_type() == Schema::IServer::MessageContentType::TEXT)
				{
					outputText += content.get_data();
				}
			}
			functionCallOutputJson["output"] = outputText;
			body["input"].push_back(functionCallOutputJson);
		}
		else
		{
			throw std::runtime_error("Unknown message type in history");
		}

		
	}
	data.body = body.dump();
	return data;
}


Schema::IServer::LinearHistory OpenAI::ParseResponse(const std::string& responseString) const
{
	Schema::IServer::LinearHistory results{};

	auto json = nlohmann::json::parse(responseString);
	if (!json.contains("output") || !json.at("output").is_array())
	{
		throw std::invalid_argument("Invalid response: missing output array");
	}

	for (const auto& item : json.at("output"))
	{
		auto typeIt = item.find("type");
		if (typeIt == item.end())
		{
			continue;
		}
		auto itemType = typeIt->get<std::string>();

		if (itemType == "message")
		{
			if (!item.contains("content") || !item.at("content").is_array())
			{
				continue;
			}
			Schema::IServer::ChatMessage chatMessage;
			chatMessage.set_role(Schema::IServer::ChatMessageRole::ASSISTANT);
			std::vector<Schema::IServer::MessageContent> contents;
			for (const auto& content : item.at("content"))
			{
				auto contentTypeIt = content.find("type");
				if (contentTypeIt == content.end())
				{
					continue;
				}
				auto contentType = contentTypeIt->get<std::string>();
				if (content.contains("text") && (contentType == "output_text" || contentType == "text"))
				{
					Schema::IServer::MessageContent messageContent;
					messageContent.set_type(Schema::IServer::MessageContentType::TEXT);
					messageContent.set_data(content.at("text").get<std::string>());
					contents.push_back(std::move(messageContent));
				}
				else if (contentType == "refusal" && content.contains("refusal"))
				{
					Schema::IServer::MessageContent messageContent;
					messageContent.set_type(Schema::IServer::MessageContentType::REFUSAL);
					messageContent.set_data(content.at("refusal").get<std::string>());
					contents.push_back(std::move(messageContent));
				}
			}
			if (!contents.empty())
			{
				chatMessage.set_content(contents);
				results.push_back(std::move(chatMessage));
			}
		}
		else if (itemType == "function_call")
		{
			Schema::IServer::FunctionCallMessage functionCall;
			functionCall.set_type(Schema::IServer::FunctionCallMessageType::FUNCTION_CALL);
			functionCall.set_call_id(item.at("call_id").get<std::string>());
			functionCall.set_name(item.at("name").get<std::string>());
			functionCall.set_arguments(item.at("arguments").get<std::string>());
			functionCall.set_extra(std::make_optional<nlohmann::json>(item));
			results.push_back(std::move(functionCall));
		}
	}

	if (results.empty())
	{
		throw std::runtime_error("No output in response");
	}

	return results;
}

std::optional<Schema::IServer::ChatCompletionSegment> OpenAI::ParseStreamResponse(const StreamResponse::Event& event) const
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

	if (eventType == "response.output_text.delta")
	{
		try
		{
			auto json = nlohmann::json::parse(valueString);
			if (json.contains("delta") && json.at("delta").is_string())
			{
			    return json.at("delta").get<std::string>();
			}
		}
		catch(...)
		{
			return std::nullopt;
		}
	}
	else if (eventType == "response.output_item.added")
	{
		try
		{
			auto json = nlohmann::json::parse(valueString);
			if (json.contains("item") && json.at("item").contains("type")
				&& json.at("item").at("type").get<std::string>() == "function_call")
			{
				const auto& item = json.at("item");
				Schema::IServer::FunctionCallMessage functionCall;
				functionCall.set_type(Schema::IServer::FunctionCallMessageType::FUNCTION_CALL);
				functionCall.set_call_id(item.at("call_id").get<std::string>());
				functionCall.set_name(item.at("name").get<std::string>());
				functionCall.set_arguments(item.value("arguments", ""));
				Schema::IServer::ChatCompletionSegmentClass segment;
				segment.set_event(Schema::IServer::Event::FUNCTION_CALL_START);
				segment.set_data(std::move(functionCall));
				return segment;
			}
		}
		catch (...)
		{
			return std::nullopt;
		}
	}
	else if (eventType == "response.function_call_arguments.done")
	{
		try
		{
			auto json = nlohmann::json::parse(valueString);
			Schema::IServer::FunctionCallMessage functionCall;
			functionCall.set_type(Schema::IServer::FunctionCallMessageType::FUNCTION_CALL);
			functionCall.set_call_id(json.value("call_id", ""));
			functionCall.set_name(json.value("name", ""));
			functionCall.set_arguments(json.value("arguments", ""));
			Schema::IServer::ChatCompletionSegmentClass segment;
			segment.set_event(Schema::IServer::Event::FUNCTION_CALL_END);
			segment.set_data(std::move(functionCall));
			return segment;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}
	else if (eventType == "response.completed" || valueString == "[DONE]")
	{
		return std::nullopt;
	}
	else if (eventType == "response.failed")
	{
		std::string errorMsg = "Response failed";
		try
		{
			auto json = nlohmann::json::parse(valueString);
			if (json.contains("response") && json.at("response").contains("error")
				&& json.at("response").at("error").contains("message"))
			{
				errorMsg = json.at("response").at("error").at("message").get<std::string>();
			}
		}
		catch (...) {}
		throw std::runtime_error(errorMsg);
	}

	return std::nullopt;
}
