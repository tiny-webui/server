 
/**
 * @file OpenAI.h
 * @brief OpenAI provider built using the same pattern as AzureOpenAI.
 */

#pragma once

#include "IProvider.h"
#include "Option.h"

namespace TUI::ApiProvider
{
	class OpenAI : public IProvider
	{
	public:
		OpenAI() = default;
		~OpenAI() override = default;

		nlohmann::json GetParams() const override;
		void Initialize(const nlohmann::json& params) override;
		Network::Http::RequestData FormatRequest(const Schema::IServer::LinearHistory& history, bool stream) const override;
		Schema::IServer::MessageContent ParseResponse(const std::string& response) const override;
		std::optional<Schema::IServer::MessageContent> ParseStreamResponse(const Network::Http::StreamResponse::Event& event) const override;

	private:
		struct Params
		{
			std::string url{"https://api.openai.com/v1/responses"};
			std::string apiKey;
			std::string model;
			double temperature{0.5};
			std::string reasoningEffort{""};
		};

		static Option::OptionList<Params> ParamsDefinition;

		Params _params;
	};
}
