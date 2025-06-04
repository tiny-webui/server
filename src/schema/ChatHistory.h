//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     DeveloperMessage data = nlohmann::json::parse(jsonString);
//     UserMessage data = nlohmann::json::parse(jsonString);
//     AssistantMessage data = nlohmann::json::parse(jsonString);
//     Message data = nlohmann::json::parse(jsonString);
//     ChatHistory data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <regex>

namespace TUI {
namespace Schema {
namespace ChatHistory {
    using nlohmann::json;

    #ifndef NLOHMANN_UNTYPED_TUI_Schema_ChatHistory_HELPER
    #define NLOHMANN_UNTYPED_TUI_Schema_ChatHistory_HELPER
    inline json get_untyped(const json & j, const char * property) {
        if (j.find(property) != j.end()) {
            return j.at(property).get<json>();
        }
        return json();
    }

    inline json get_untyped(const json & j, std::string property) {
        return get_untyped(j, property.data());
    }
    #endif

    enum class PurpleType : int { TEXT };

    class DeveloperMessageContent {
        public:
        DeveloperMessageContent() = default;
        virtual ~DeveloperMessageContent() = default;

        private:
        std::string data;
        PurpleType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const PurpleType & get_type() const { return type; }
        PurpleType & get_mutable_type() { return type; }
        void set_type(const PurpleType & value) { this->type = value; }
    };

    enum class DeveloperMessageRole : int { DEVELOPER };

    /**
     * The format is based on OpenAI's chat completions but with several simplifications.
     * Namely, all data fields are called data instead of text/image_url/refusal as the type
     * already indicates the type of the data.
     * Options other than image_url is also removed in the image_url type.
     */
    class DeveloperMessage {
        public:
        DeveloperMessage() = default;
        virtual ~DeveloperMessage() = default;

        private:
        std::vector<DeveloperMessageContent> content;
        DeveloperMessageRole role;

        public:
        const std::vector<DeveloperMessageContent> & get_content() const { return content; }
        std::vector<DeveloperMessageContent> & get_mutable_content() { return content; }
        void set_content(const std::vector<DeveloperMessageContent> & value) { this->content = value; }

        const DeveloperMessageRole & get_role() const { return role; }
        DeveloperMessageRole & get_mutable_role() { return role; }
        void set_role(const DeveloperMessageRole & value) { this->role = value; }
    };

    enum class FluffyType : int { IMAGE_URL, TEXT };

    class UserMessageContent {
        public:
        UserMessageContent() = default;
        virtual ~UserMessageContent() = default;

        private:
        std::string data;
        FluffyType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const FluffyType & get_type() const { return type; }
        FluffyType & get_mutable_type() { return type; }
        void set_type(const FluffyType & value) { this->type = value; }
    };

    enum class UserMessageRole : int { USER };

    class UserMessage {
        public:
        UserMessage() = default;
        virtual ~UserMessage() = default;

        private:
        std::vector<UserMessageContent> content;
        UserMessageRole role;

        public:
        const std::vector<UserMessageContent> & get_content() const { return content; }
        std::vector<UserMessageContent> & get_mutable_content() { return content; }
        void set_content(const std::vector<UserMessageContent> & value) { this->content = value; }

        const UserMessageRole & get_role() const { return role; }
        UserMessageRole & get_mutable_role() { return role; }
        void set_role(const UserMessageRole & value) { this->role = value; }
    };

    enum class TentacledType : int { REFUSAL, TEXT };

    class AssistantMessageContent {
        public:
        AssistantMessageContent() = default;
        virtual ~AssistantMessageContent() = default;

        private:
        std::string data;
        TentacledType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const TentacledType & get_type() const { return type; }
        TentacledType & get_mutable_type() { return type; }
        void set_type(const TentacledType & value) { this->type = value; }
    };

    enum class AssistantMessageRole : int { ASSISTANT };

    class AssistantMessage {
        public:
        AssistantMessage() = default;
        virtual ~AssistantMessage() = default;

        private:
        std::vector<AssistantMessageContent> content;
        AssistantMessageRole role;

        public:
        const std::vector<AssistantMessageContent> & get_content() const { return content; }
        std::vector<AssistantMessageContent> & get_mutable_content() { return content; }
        void set_content(const std::vector<AssistantMessageContent> & value) { this->content = value; }

        const AssistantMessageRole & get_role() const { return role; }
        AssistantMessageRole & get_mutable_role() { return role; }
        void set_role(const AssistantMessageRole & value) { this->role = value; }
    };

    enum class StickyType : int { IMAGE_URL, REFUSAL, TEXT };

    class MessageContent {
        public:
        MessageContent() = default;
        virtual ~MessageContent() = default;

        private:
        std::string data;
        StickyType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const StickyType & get_type() const { return type; }
        StickyType & get_mutable_type() { return type; }
        void set_type(const StickyType & value) { this->type = value; }
    };

    enum class MessageRole : int { ASSISTANT, DEVELOPER, USER };

    /**
     * The format is based on OpenAI's chat completions but with several simplifications.
     * Namely, all data fields are called data instead of text/image_url/refusal as the type
     * already indicates the type of the data.
     * Options other than image_url is also removed in the image_url type.
     */
    class Message {
        public:
        Message() = default;
        virtual ~Message() = default;

        private:
        std::vector<MessageContent> content;
        MessageRole role;

        public:
        const std::vector<MessageContent> & get_content() const { return content; }
        std::vector<MessageContent> & get_mutable_content() { return content; }
        void set_content(const std::vector<MessageContent> & value) { this->content = value; }

        const MessageRole & get_role() const { return role; }
        MessageRole & get_mutable_role() { return role; }
        void set_role(const MessageRole & value) { this->role = value; }
    };

    using ChatHistory = std::vector<Message>;
}
}
}

namespace TUI {
namespace Schema {
namespace ChatHistory {
    void from_json(const json & j, DeveloperMessageContent & x);
    void to_json(json & j, const DeveloperMessageContent & x);

    void from_json(const json & j, DeveloperMessage & x);
    void to_json(json & j, const DeveloperMessage & x);

    void from_json(const json & j, UserMessageContent & x);
    void to_json(json & j, const UserMessageContent & x);

    void from_json(const json & j, UserMessage & x);
    void to_json(json & j, const UserMessage & x);

    void from_json(const json & j, AssistantMessageContent & x);
    void to_json(json & j, const AssistantMessageContent & x);

    void from_json(const json & j, AssistantMessage & x);
    void to_json(json & j, const AssistantMessage & x);

    void from_json(const json & j, MessageContent & x);
    void to_json(json & j, const MessageContent & x);

    void from_json(const json & j, Message & x);
    void to_json(json & j, const Message & x);

    void from_json(const json & j, PurpleType & x);
    void to_json(json & j, const PurpleType & x);

    void from_json(const json & j, DeveloperMessageRole & x);
    void to_json(json & j, const DeveloperMessageRole & x);

    void from_json(const json & j, FluffyType & x);
    void to_json(json & j, const FluffyType & x);

    void from_json(const json & j, UserMessageRole & x);
    void to_json(json & j, const UserMessageRole & x);

    void from_json(const json & j, TentacledType & x);
    void to_json(json & j, const TentacledType & x);

    void from_json(const json & j, AssistantMessageRole & x);
    void to_json(json & j, const AssistantMessageRole & x);

    void from_json(const json & j, StickyType & x);
    void to_json(json & j, const StickyType & x);

    void from_json(const json & j, MessageRole & x);
    void to_json(json & j, const MessageRole & x);

    inline void from_json(const json & j, DeveloperMessageContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<PurpleType>());
    }

    inline void to_json(json & j, const DeveloperMessageContent & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, DeveloperMessage& x) {
        x.set_content(j.at("content").get<std::vector<DeveloperMessageContent>>());
        x.set_role(j.at("role").get<DeveloperMessageRole>());
    }

    inline void to_json(json & j, const DeveloperMessage & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, UserMessageContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<FluffyType>());
    }

    inline void to_json(json & j, const UserMessageContent & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, UserMessage& x) {
        x.set_content(j.at("content").get<std::vector<UserMessageContent>>());
        x.set_role(j.at("role").get<UserMessageRole>());
    }

    inline void to_json(json & j, const UserMessage & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, AssistantMessageContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<TentacledType>());
    }

    inline void to_json(json & j, const AssistantMessageContent & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, AssistantMessage& x) {
        x.set_content(j.at("content").get<std::vector<AssistantMessageContent>>());
        x.set_role(j.at("role").get<AssistantMessageRole>());
    }

    inline void to_json(json & j, const AssistantMessage & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, MessageContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<StickyType>());
    }

    inline void to_json(json & j, const MessageContent & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, Message& x) {
        x.set_content(j.at("content").get<std::vector<MessageContent>>());
        x.set_role(j.at("role").get<MessageRole>());
    }

    inline void to_json(json & j, const Message & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, PurpleType & x) {
        if (j == "text") x = PurpleType::TEXT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const PurpleType & x) {
        switch (x) {
            case PurpleType::TEXT: j = "text"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"PurpleType\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, DeveloperMessageRole & x) {
        if (j == "developer") x = DeveloperMessageRole::DEVELOPER;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const DeveloperMessageRole & x) {
        switch (x) {
            case DeveloperMessageRole::DEVELOPER: j = "developer"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"DeveloperMessageRole\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, FluffyType & x) {
        if (j == "image_url") x = FluffyType::IMAGE_URL;
        else if (j == "text") x = FluffyType::TEXT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const FluffyType & x) {
        switch (x) {
            case FluffyType::IMAGE_URL: j = "image_url"; break;
            case FluffyType::TEXT: j = "text"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"FluffyType\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, UserMessageRole & x) {
        if (j == "user") x = UserMessageRole::USER;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const UserMessageRole & x) {
        switch (x) {
            case UserMessageRole::USER: j = "user"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"UserMessageRole\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, TentacledType & x) {
        if (j == "refusal") x = TentacledType::REFUSAL;
        else if (j == "text") x = TentacledType::TEXT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const TentacledType & x) {
        switch (x) {
            case TentacledType::REFUSAL: j = "refusal"; break;
            case TentacledType::TEXT: j = "text"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"TentacledType\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, AssistantMessageRole & x) {
        if (j == "assistant") x = AssistantMessageRole::ASSISTANT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const AssistantMessageRole & x) {
        switch (x) {
            case AssistantMessageRole::ASSISTANT: j = "assistant"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"AssistantMessageRole\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, StickyType & x) {
        if (j == "image_url") x = StickyType::IMAGE_URL;
        else if (j == "refusal") x = StickyType::REFUSAL;
        else if (j == "text") x = StickyType::TEXT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const StickyType & x) {
        switch (x) {
            case StickyType::IMAGE_URL: j = "image_url"; break;
            case StickyType::REFUSAL: j = "refusal"; break;
            case StickyType::TEXT: j = "text"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"StickyType\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, MessageRole & x) {
        if (j == "assistant") x = MessageRole::ASSISTANT;
        else if (j == "developer") x = MessageRole::DEVELOPER;
        else if (j == "user") x = MessageRole::USER;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const MessageRole & x) {
        switch (x) {
            case MessageRole::ASSISTANT: j = "assistant"; break;
            case MessageRole::DEVELOPER: j = "developer"; break;
            case MessageRole::USER: j = "user"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"MessageRole\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
}
}
