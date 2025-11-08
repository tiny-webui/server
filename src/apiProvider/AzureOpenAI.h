#pragma once

#include "IProvider.h"
#include "Option.h"

namespace TUI::ApiProvider
{
    class AzureOpenAI : public IProvider
    {
    public:
        AzureOpenAI() = default;
        ~AzureOpenAI() override = default;
        nlohmann::json GetParams() const override;
        void Initialize(const nlohmann::json& params) override;
        Network::Http::RequestData FormatRequest(const Schema::IServer::LinearHistory& history, bool stream) const override;
        Schema::IServer::MessageContent ParseResponse(const std::string& response) const override;
        std::optional<Schema::IServer::MessageContent> ParseStreamResponse(const Network::Http::StreamResponse::Event& event) const override;
    private:
        struct Params
        {
            /** The full url. This contains the deployment name and api version. */
            std::string url;
            std::string apiKey;
            std::string model{""};
            double temperature{0.5};
        };

        static Option::OptionList<Params> ParamsDefinition;

        Params _params;        
    };
}

