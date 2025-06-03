#include <iostream>
#include <random>
#include <algorithm>
#include "network/HttpStreamResponseParser.h"

using namespace TUI::Network::Http::StreamResponse;

static const std::string testData = R"(
event: message
data: Hello, world!

: this is a comment



event: message
data: This is a second message
data: This is a continuation of the second message

id: 12345
retry: 1000
event: message
data: This is a third message

data: This is a message without an event type

)";

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    Parser parser;

    /** Split test data into random pieces to feed the parser */
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 10);
    size_t pieceCount = dis(gen);
    dis = std::uniform_int_distribution<>(0, testData.length());
    std::vector<size_t> splitPoints;
    for (size_t i = 0; i < pieceCount - 1; i++)
    {
        splitPoints.push_back(dis(gen));
    }
    splitPoints.push_back(testData.length());
    std::sort(splitPoints.begin(), splitPoints.end());
    std::vector<std::string> pieces;
    size_t lastPos = 0;
    for (size_t pos : splitPoints)
    {
        pieces.push_back(testData.substr(lastPos, pos - lastPos));
        lastPos = pos;
    }

    std::vector<Event> allEvents;
    for (const auto& piece : pieces)
    {
        auto events = parser.Feed(piece);
        allEvents.insert(allEvents.end(), events.begin(), events.end());
    }
    auto endEvent = parser.End();
    if (endEvent.has_value())
    {
        allEvents.push_back(endEvent.value());
    }

    for (const auto& event: allEvents)
    {
        std::cout << "Event:" << std::endl;
        if (event.type.has_value())
        {
            std::cout << "  Type: " << event.type.value() << std::endl;
        }
        if (event.value.has_value())
        {
            std::cout << "  Value: " << event.value.value() << std::endl;
        }
        if (event.id.has_value())
        {
            std::cout << "  ID: " << event.id.value() << std::endl;
        }
        if (event.retry.has_value())
        {
            std::cout << "  Retry: " << event.retry.value() << std::endl;
        }
        std::cout << std::endl;
    }

    return 0;
}

