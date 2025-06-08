#pragma once

#include <string>
#include <vector>
#include <optional>
#include <js-style-co-routine/AsyncGenerator.h>

namespace TUI::Network::Http::StreamResponse
{
    struct Event
    {
        std::optional<std::string> type;
        std::optional<std::string> value;
        std::optional<std::string> id;
        std::optional<int> retry;
    };

    class Parser
    {
    public:
        Parser() = default;
        ~Parser() = default;

        std::vector<Event> Feed(const std::string& response);
        std::optional<Event> End();
    private:
        std::string _buffer{};
        std::optional<Event> ParseEvent(const std::string& eventData) const;
    };

    class AsyncParser
    {
    public:
        AsyncParser(JS::AsyncGenerator<std::string> responseStream);
        ~AsyncParser() = default;

        JS::AsyncGenerator<Event> Parse();
    private:
        JS::AsyncGenerator<std::string> _responseStream;
        Parser _parser{};
    };
}
