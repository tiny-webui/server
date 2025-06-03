#include "HttpStreamResponseParser.h"

using namespace TUI::Network::Http::StreamResponse;

std::vector<Event> Parser::Feed(const std::string& response)
{
    _buffer += response;
    std::vector<Event> events;
    while (true)
    {
        /** Cut the event out */
        size_t delimiterLength = 0;
        size_t end = _buffer.find("\r\n\r\n");
        if (end != std::string::npos)
        {
            delimiterLength = 4;
        }
        else
        {
            end = _buffer.find("\n\n");
            if (end == std::string::npos)
            {
                break;
            }
            delimiterLength = 2;
        }
        std::string eventData = _buffer.substr(0, end);
        _buffer.erase(0, end + delimiterLength);

        auto event = ParseEvent(eventData);
        if (event.has_value())
        {
            events.push_back(std::move(event.value()));
        }
    }
    return events;
}

std::optional<Event> Parser::End()
{
    auto event = ParseEvent(_buffer);
    _buffer.clear();
    return event;
}

std::optional<Event> Parser::ParseEvent(const std::string& eventData) const
{
    auto event = std::make_optional<Event>();

    size_t start = 0;
    while (start < eventData.length())
    {
        size_t delimiterLength = 0;
        size_t end = eventData.find("\r\n", start);
        if (end != std::string::npos)
        {
            delimiterLength = 2;
        }
        else
        {
            end = eventData.find("\n", start);
            if (end == std::string::npos)
            {
                /** Take the whole line */
                end = eventData.length();
                delimiterLength = 0;
            }
            delimiterLength = 1;
        }
        auto line = eventData.substr(start, end - start);
        start = end + delimiterLength;

        auto colonPos = line.find(":");
        std::string fieldName = "";
        std::string fieldValue = "";
        if (colonPos == 0)
        {
            /** Skip line starting with : */
            continue;
        }
        else if(colonPos == std::string::npos)
        {
            /** Whole line is the field and value is "" */
            fieldName = line;
        }
        else
        {
            fieldName = line.substr(0, colonPos);
            fieldValue = line.substr(colonPos + 1);
        }

        if (fieldName == "event")
        {
            event->type = fieldValue;
        }
        else if(fieldName == "data")
        {
            if (fieldValue.length() > 0 && fieldValue[0] == ' ')
            {
                /** Remove leading space */
                fieldValue.erase(0, 1);
            }
            if (!event->value.has_value())
            {
                event->value = fieldValue;
            }
            else
            {
                /** Append to existing value if it already exists */
                event->value = event->value.value() + "\n" + fieldValue;
            }
        }
        else if(fieldName == "id")
        {
            event->id = fieldValue;
        }
        else if(fieldName == "retry")
        {
            try
            {
                event->retry = std::stoi(fieldValue);
            }
            catch(...)
            {
            }
        }
    }

    if (event->type.has_value() || event->value.has_value() || event->id.has_value() || event->retry.has_value())
    {
        return event;
    }

    /** No valid event found */
    return std::nullopt;
}
