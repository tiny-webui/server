#pragma once

#include <nlohmann/json.hpp>
#include <vector>
#include <optional>
#include <string>
#include "Option.h"
#include "network/HttpClient.h"
#include "network/HttpStreamResponseParser.h"
#include "schema/Chat.h"

namespace TUI::ApiProvider
{
    /**
     * An api provider provides interfaces to construct requests and decodes responses.
     * It does not handle network communication directly.
     * The api provider should also provide information on its initialization parameters in a standard manner.
     */
    class IProvider
    {
    public:
        virtual ~IProvider() = default;
        virtual nlohmann::json GetParams() const = 0;
        virtual void Initialize(nlohmann::json params) = 0;
        virtual Network::Http::RequestData FormatRequest(const Schema::Chat::ChatHistory& history, bool stream) const = 0;
        virtual Schema::Chat::AssistantMessageContent ParseResponse(const std::string& response) const = 0;
        virtual std::optional<Schema::Chat::AssistantMessageContent> ParseStreamResponse(const Network::Http::StreamResponse::Event& event) const = 0;
    };
}
