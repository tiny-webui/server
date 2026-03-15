#include <iostream>
#include <string>
#include <variant>
#include <nlohmann/json.hpp>
#include "schema/IServer.h"
#include "Utility.h"

using namespace TUI::Schema::IServer;
using json = nlohmann::json;

static void TestChatMessageRoundTrip()
{
    json input = R"({
        "role": "user",
        "content": [
            {"type": "text", "data": "Hello, world!"}
        ]
    })"_json;

    Message msg = input.get<Message>();
    AssertWithMessage(std::holds_alternative<ChatMessage>(msg), "Should be ChatMessage");

    auto& chat = std::get<ChatMessage>(msg);
    AssertWithMessage(chat.get_role() == ChatMessageRole::USER, "Role should be user");
    AssertWithMessage(chat.get_content().size() == 1, "Should have 1 content item");
    AssertWithMessage(chat.get_content()[0].get_data() == "Hello, world!", "Content data mismatch");
    AssertWithMessage(chat.get_content()[0].get_type() == MessageContentType::TEXT, "Content type should be text");

    json output = msg;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestChatMessageAssistantRole()
{
    json input = R"({
        "role": "assistant",
        "content": [
            {"type": "text", "data": "I can help with that."},
            {"type": "refusal", "data": "I cannot do that."}
        ]
    })"_json;

    Message msg = input.get<Message>();
    AssertWithMessage(std::holds_alternative<ChatMessage>(msg), "Should be ChatMessage");

    auto& chat = std::get<ChatMessage>(msg);
    AssertWithMessage(chat.get_role() == ChatMessageRole::ASSISTANT, "Role should be assistant");
    AssertWithMessage(chat.get_content().size() == 2, "Should have 2 content items");
    AssertWithMessage(chat.get_content()[1].get_type() == MessageContentType::REFUSAL, "Second item should be refusal");

    json output = msg;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestFunctionCallMessageRoundTrip()
{
    json input = R"({
        "type": "function_call",
        "call_id": "call_123",
        "name": "get_weather",
        "arguments": "{\"city\": \"London\"}"
    })"_json;

    Message msg = input.get<Message>();
    AssertWithMessage(std::holds_alternative<FunctionCallMessage>(msg), "Should be FunctionCallMessage");

    auto& fc = std::get<FunctionCallMessage>(msg);
    AssertWithMessage(fc.get_call_id() == "call_123", "call_id mismatch");
    AssertWithMessage(fc.get_name() == "get_weather", "name mismatch");
    AssertWithMessage(fc.get_arguments() == "{\"city\": \"London\"}", "arguments mismatch");
    AssertWithMessage(fc.get_type() == FunctionCallMessageType::FUNCTION_CALL, "type mismatch");

    json output = msg;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestFunctionCallMessageWithExtra()
{
    json input = R"({
        "type": "function_call",
        "call_id": "call_456",
        "name": "search",
        "arguments": "{}",
        "extra": {"id": "fc_1", "status": "completed"}
    })"_json;

    Message msg = input.get<Message>();
    AssertWithMessage(std::holds_alternative<FunctionCallMessage>(msg), "Should be FunctionCallMessage");

    auto& fc = std::get<FunctionCallMessage>(msg);
    AssertWithMessage(fc.get_extra().has_value(), "extra should have a value");
    AssertWithMessage(fc.get_extra()->is_object(), "extra should be an object");
    AssertWithMessage((*fc.get_extra())["id"] == "fc_1", "extra id mismatch");

    json output = msg;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestFunctionCallOutputMessageRoundTrip()
{
    json input = R"({
        "type": "function_call_output",
        "call_id": "call_123",
        "output": [
            {"type": "text", "data": "Sunny, 22°C"}
        ]
    })"_json;

    Message msg = input.get<Message>();
    AssertWithMessage(std::holds_alternative<FunctionCallOutputMessage>(msg), "Should be FunctionCallOutputMessage");

    auto& fco = std::get<FunctionCallOutputMessage>(msg);
    AssertWithMessage(fco.get_call_id() == "call_123", "call_id mismatch");
    AssertWithMessage(fco.get_output().size() == 1, "Should have 1 output item");
    AssertWithMessage(fco.get_output()[0].get_data() == "Sunny, 22°C", "Output data mismatch");

    json output = msg;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestFunctionCallOutputMessageWithExtra()
{
    json input = R"({
        "type": "function_call_output",
        "call_id": "call_789",
        "extra": {"provider_id": "x"},
        "output": [
            {"type": "text", "data": "result"}
        ]
    })"_json;

    Message msg = input.get<Message>();
    AssertWithMessage(std::holds_alternative<FunctionCallOutputMessage>(msg), "Should be FunctionCallOutputMessage");

    auto& fco = std::get<FunctionCallOutputMessage>(msg);
    AssertWithMessage(fco.get_extra().has_value(), "extra should have a value");
    AssertWithMessage(fco.get_extra()->is_object(), "extra should be an object");
    AssertWithMessage((*fco.get_extra())["provider_id"] == "x", "extra provider_id mismatch");

    json output = msg;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestMessageArrayRoundTrip()
{
    json input = R"([
        {
            "role": "user",
            "content": [{"type": "text", "data": "What is the weather?"}]
        },
        {
            "type": "function_call",
            "call_id": "call_1",
            "name": "get_weather",
            "arguments": "{}"
        },
        {
            "type": "function_call_output",
            "call_id": "call_1",
            "output": [{"type": "text", "data": "Rainy"}]
        },
        {
            "role": "assistant",
            "content": [{"type": "text", "data": "It is rainy."}]
        }
    ])"_json;

    auto messages = input.get<std::vector<Message>>();
    AssertWithMessage(messages.size() == 4, "Should have 4 messages");
    AssertWithMessage(std::holds_alternative<ChatMessage>(messages[0]), "messages[0] should be ChatMessage");
    AssertWithMessage(std::holds_alternative<FunctionCallMessage>(messages[1]), "messages[1] should be FunctionCallMessage");
    AssertWithMessage(std::holds_alternative<FunctionCallOutputMessage>(messages[2]), "messages[2] should be FunctionCallOutputMessage");
    AssertWithMessage(std::holds_alternative<ChatMessage>(messages[3]), "messages[3] should be ChatMessage");

    json output = messages;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestLinearHistoryRoundTrip()
{
    json input = R"([
        {
            "role": "developer",
            "content": [{"type": "text", "data": "You are a helpful assistant."}]
        },
        {
            "role": "user",
            "content": [{"type": "text", "data": "Hi"}]
        }
    ])"_json;

    LinearHistory history = input.get<LinearHistory>();
    AssertWithMessage(history.size() == 2, "Should have 2 messages");
    AssertWithMessage(std::holds_alternative<ChatMessage>(history[0]), "First should be ChatMessage");

    auto& dev = std::get<ChatMessage>(history[0]);
    AssertWithMessage(dev.get_role() == ChatMessageRole::DEVELOPER, "Role should be developer");

    json output = history;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestMessageNodeRoundTrip()
{
    json input = R"({
        "children": ["child_1"],
        "id": "node_1",
        "message": {
            "role": "user",
            "content": [{"type": "text", "data": "Hello"}]
        },
        "parent": "root",
        "timestamp": 1234567890.0
    })"_json;

    MessageNode node = input.get<MessageNode>();
    AssertWithMessage(node.get_id() == "node_1", "id mismatch");
    AssertWithMessage(node.get_children().size() == 1, "Should have 1 child");
    AssertWithMessage(node.get_parent().has_value(), "Should have parent");
    AssertWithMessage(node.get_parent().value() == "root", "parent mismatch");
    AssertWithMessage(std::holds_alternative<ChatMessage>(node.get_message()), "message should be ChatMessage");

    json output = node;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestMessageNodeWithFunctionCall()
{
    json input = R"({
        "children": [],
        "id": "node_2",
        "message": {
            "type": "function_call",
            "call_id": "c1",
            "name": "foo",
            "arguments": "{}"
        },
        "timestamp": 100.0
    })"_json;

    MessageNode node = input.get<MessageNode>();
    AssertWithMessage(std::holds_alternative<FunctionCallMessage>(node.get_message()), "message should be FunctionCallMessage");
    AssertWithMessage(!node.get_parent().has_value(), "Should not have parent");

    json output = node;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestInvalidMessageThrows()
{
    json invalid = R"({"unknown_field": 123})"_json;

    bool threw = false;
    try
    {
        [[maybe_unused]] auto msg = invalid.get<Message>();
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    AssertWithMessage(threw, "Deserializing invalid JSON should throw");
}

static void TestChatCompletionParamsRoundTrip()
{
    json input = R"({
        "id": "chat_1",
        "messages": [
            {"role": "user", "content": [{"type": "text", "data": "Hi"}]},
            {"type": "function_call", "call_id": "c1", "name": "greet", "arguments": "{}"},
            {"type": "function_call_output", "call_id": "c1", "output": [{"type": "text", "data": "Hello!"}]}
        ],
        "modelId": "gpt-4",
        "parent": "prev_node"
    })"_json;

    ChatCompletionParams params = input.get<ChatCompletionParams>();
    AssertWithMessage(params.get_id() == "chat_1", "id mismatch");
    AssertWithMessage(params.get_messages().size() == 3, "Should have 3 messages");
    AssertWithMessage(params.get_model_id() == "gpt-4", "model_id mismatch");
    AssertWithMessage(params.get_parent().has_value(), "Should have parent");

    json output = params;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestImageUrlContentType()
{
    json input = R"({
        "role": "user",
        "content": [
            {"type": "image_url", "data": "https://example.com/img.png"},
            {"type": "text", "data": "What is in this image?"}
        ]
    })"_json;

    Message msg = input.get<Message>();
    auto& chat = std::get<ChatMessage>(msg);
    AssertWithMessage(chat.get_content()[0].get_type() == MessageContentType::IMAGE_URL, "First content should be image_url");

    json output = msg;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestTreeHistoryRoundTrip()
{
    json input = R"({
        "nodes": {
            "n1": {
                "children": ["n2"],
                "id": "n1",
                "message": {"role": "user", "content": [{"type": "text", "data": "Hi"}]},
                "timestamp": 1.0
            },
            "n2": {
                "children": [],
                "id": "n2",
                "message": {"role": "assistant", "content": [{"type": "text", "data": "Hello!"}]},
                "parent": "n1",
                "timestamp": 2.0
            }
        }
    })"_json;

    TreeHistory history = input.get<TreeHistory>();
    AssertWithMessage(history.get_nodes().size() == 2, "Should have 2 nodes");
    AssertWithMessage(history.get_nodes().at("n1").get_children().size() == 1, "n1 should have 1 child");
    AssertWithMessage(!history.get_nodes().at("n1").get_parent().has_value(), "n1 should not have parent");
    AssertWithMessage(history.get_nodes().at("n2").get_parent().value() == "n1", "n2 parent should be n1");

    json output = history;
    AssertWithMessage(output == input, "Round-trip should produce identical JSON");
}

static void TestChatCompletionSegmentStringVariant()
{
    json strInput = R"("hello stream chunk")"_json;

    ChatCompletionSegment seg = strInput.get<ChatCompletionSegment>();
    AssertWithMessage(std::holds_alternative<std::string>(seg), "Should be string variant");
    AssertWithMessage(std::get<std::string>(seg) == "hello stream chunk", "String value mismatch");

    json output = seg;
    AssertWithMessage(output == strInput, "Round-trip should produce identical JSON");
}

static void TestChatCompletionSegmentObjectVariant()
{
    json objInput = R"({
        "event": "function_call_start",
        "data": {
            "type": "function_call",
            "call_id": "c1",
            "name": "test_fn",
            "arguments": "{}",
            "extra": {"id": "x", "status": "in_progress"}
        }
    })"_json;

    ChatCompletionSegment seg = objInput.get<ChatCompletionSegment>();
    AssertWithMessage(std::holds_alternative<ChatCompletionSegmentClass>(seg), "Should be ChatCompletionSegmentClass variant");

    auto& cls = std::get<ChatCompletionSegmentClass>(seg);
    AssertWithMessage(cls.get_event() == Event::FUNCTION_CALL_START, "Event should be function_call_start");
    AssertWithMessage(cls.get_data().has_value(), "Should have data");
    AssertWithMessage(cls.get_data()->get_name() == "test_fn", "data name mismatch");

    json output = seg;
    AssertWithMessage(output == objInput, "Round-trip should produce identical JSON");
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        {"ChatMessageRoundTrip", TestChatMessageRoundTrip},
        {"ChatMessageAssistantRole", TestChatMessageAssistantRole},
        {"FunctionCallMessageRoundTrip", TestFunctionCallMessageRoundTrip},
        {"FunctionCallMessageWithExtra", TestFunctionCallMessageWithExtra},
        {"FunctionCallOutputMessageRoundTrip", TestFunctionCallOutputMessageRoundTrip},
        {"FunctionCallOutputMessageWithExtra", TestFunctionCallOutputMessageWithExtra},
        {"MessageArrayRoundTrip", TestMessageArrayRoundTrip},
        {"LinearHistoryRoundTrip", TestLinearHistoryRoundTrip},
        {"MessageNodeRoundTrip", TestMessageNodeRoundTrip},
        {"MessageNodeWithFunctionCall", TestMessageNodeWithFunctionCall},
        {"InvalidMessageThrows", TestInvalidMessageThrows},
        {"ChatCompletionParamsRoundTrip", TestChatCompletionParamsRoundTrip},
        {"ImageUrlContentType", TestImageUrlContentType},
        {"TreeHistoryRoundTrip", TestTreeHistoryRoundTrip},
        {"ChatCompletionSegmentStringVariant", TestChatCompletionSegmentStringVariant},
        {"ChatCompletionSegmentObjectVariant", TestChatCompletionSegmentObjectVariant},
    };

    int passed = 0;
    int failed = 0;
    for (auto& [name, fn] : tests)
    {
        try
        {
            std::cout << "Running: " << name << "... ";
            fn();
            std::cout << "PASS" << std::endl;
            passed++;
        }
        catch (const std::exception& e)
        {
            std::cout << "FAIL: " << e.what() << std::endl;
            failed++;
        }
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed, " << (passed + failed) << " total." << std::endl;
    return failed > 0 ? 1 : 0;
}
