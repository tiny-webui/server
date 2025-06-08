//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Message data = nlohmann::json::parse(jsonString);
//     ChatHistory data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <regex>

namespace TUI {
namespace Schema {
namespace Chat {
    using nlohmann::json;

    #ifndef NLOHMANN_UNTYPED_TUI_Schema_Chat_HELPER
    #define NLOHMANN_UNTYPED_TUI_Schema_Chat_HELPER
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

    enum class Type : int { IMAGE_URL, REFUSAL, TEXT };

    class MessageContent {
        public:
        MessageContent() = default;
        virtual ~MessageContent() = default;

        private:
        std::string data;
        Type type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const Type & get_type() const { return type; }
        Type & get_mutable_type() { return type; }
        void set_type(const Type & value) { this->type = value; }
    };

    enum class Role : int { ASSISTANT, DEVELOPER, USER };

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
        Role role;

        public:
        const std::vector<MessageContent> & get_content() const { return content; }
        std::vector<MessageContent> & get_mutable_content() { return content; }
        void set_content(const std::vector<MessageContent> & value) { this->content = value; }

        const Role & get_role() const { return role; }
        Role & get_mutable_role() { return role; }
        void set_role(const Role & value) { this->role = value; }
    };

    class ChatHistoryContent {
        public:
        ChatHistoryContent() = default;
        virtual ~ChatHistoryContent() = default;

        private:
        std::string data;
        Type type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const Type & get_type() const { return type; }
        Type & get_mutable_type() { return type; }
        void set_type(const Type & value) { this->type = value; }
    };

    /**
     * The format is based on OpenAI's chat completions but with several simplifications.
     * Namely, all data fields are called data instead of text/image_url/refusal as the type
     * already indicates the type of the data.
     * Options other than image_url is also removed in the image_url type.
     */
    class ChatHistoryElement {
        public:
        ChatHistoryElement() = default;
        virtual ~ChatHistoryElement() = default;

        private:
        std::vector<ChatHistoryContent> content;
        Role role;

        public:
        const std::vector<ChatHistoryContent> & get_content() const { return content; }
        std::vector<ChatHistoryContent> & get_mutable_content() { return content; }
        void set_content(const std::vector<ChatHistoryContent> & value) { this->content = value; }

        const Role & get_role() const { return role; }
        Role & get_mutable_role() { return role; }
        void set_role(const Role & value) { this->role = value; }
    };

    using ChatHistory = std::vector<ChatHistoryElement>;
}
}
}

namespace TUI {
namespace Schema {
namespace Chat {
    void from_json(const json & j, MessageContent & x);
    void to_json(json & j, const MessageContent & x);

    void from_json(const json & j, Message & x);
    void to_json(json & j, const Message & x);

    void from_json(const json & j, ChatHistoryContent & x);
    void to_json(json & j, const ChatHistoryContent & x);

    void from_json(const json & j, ChatHistoryElement & x);
    void to_json(json & j, const ChatHistoryElement & x);

    void from_json(const json & j, Type & x);
    void to_json(json & j, const Type & x);

    void from_json(const json & j, Role & x);
    void to_json(json & j, const Role & x);

    inline void from_json(const json & j, MessageContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<Type>());
    }

    inline void to_json(json & j, const MessageContent & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, Message& x) {
        x.set_content(j.at("content").get<std::vector<MessageContent>>());
        x.set_role(j.at("role").get<Role>());
    }

    inline void to_json(json & j, const Message & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, ChatHistoryContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<Type>());
    }

    inline void to_json(json & j, const ChatHistoryContent & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, ChatHistoryElement& x) {
        x.set_content(j.at("content").get<std::vector<ChatHistoryContent>>());
        x.set_role(j.at("role").get<Role>());
    }

    inline void to_json(json & j, const ChatHistoryElement & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, Type & x) {
        if (j == "image_url") x = Type::IMAGE_URL;
        else if (j == "refusal") x = Type::REFUSAL;
        else if (j == "text") x = Type::TEXT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Type & x) {
        switch (x) {
            case Type::IMAGE_URL: j = "image_url"; break;
            case Type::REFUSAL: j = "refusal"; break;
            case Type::TEXT: j = "text"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"Type\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, Role & x) {
        if (j == "assistant") x = Role::ASSISTANT;
        else if (j == "developer") x = Role::DEVELOPER;
        else if (j == "user") x = Role::USER;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Role & x) {
        switch (x) {
            case Role::ASSISTANT: j = "assistant"; break;
            case Role::DEVELOPER: j = "developer"; break;
            case Role::USER: j = "user"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"Role\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
}
}
