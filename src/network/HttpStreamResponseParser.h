#pragma once

#include <string>
#include <vector>
#include <optional>

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
}
