//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     DeveloperMessageContent data = nlohmann::json::parse(jsonString);
//     DeveloperMessage data = nlohmann::json::parse(jsonString);
//     UserMessageContent data = nlohmann::json::parse(jsonString);
//     UserMessage data = nlohmann::json::parse(jsonString);
//     AssistantMessageContent data = nlohmann::json::parse(jsonString);
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

    enum class DeveloperMessageContentType : int { TEXT };

    /**
     * The format is based on OpenAI's chat completions but with several simplifications.
     * Namely, all data fields are called data instead of text/image_url/refusal as the type
     * already indicates the type of the data.
     * Options other than image_url is also removed in the image_url type.
     */
    class DeveloperMessageContent {
        public:
        DeveloperMessageContent() = default;
        virtual ~DeveloperMessageContent() = default;

        private:
        std::string data;
        DeveloperMessageContentType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const DeveloperMessageContentType & get_type() const { return type; }
        DeveloperMessageContentType & get_mutable_type() { return type; }
        void set_type(const DeveloperMessageContentType & value) { this->type = value; }
    };

    /**
     * The format is based on OpenAI's chat completions but with several simplifications.
     * Namely, all data fields are called data instead of text/image_url/refusal as the type
     * already indicates the type of the data.
     * Options other than image_url is also removed in the image_url type.
     */
    class DeveloperMessageContentClass {
        public:
        DeveloperMessageContentClass() = default;
        virtual ~DeveloperMessageContentClass() = default;

        private:
        std::string data;
        DeveloperMessageContentType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const DeveloperMessageContentType & get_type() const { return type; }
        DeveloperMessageContentType & get_mutable_type() { return type; }
        void set_type(const DeveloperMessageContentType & value) { this->type = value; }
    };

    enum class DeveloperMessageRole : int { DEVELOPER };

    class DeveloperMessage {
        public:
        DeveloperMessage() = default;
        virtual ~DeveloperMessage() = default;

        private:
        std::vector<DeveloperMessageContentClass> content;
        DeveloperMessageRole role;

        public:
        const std::vector<DeveloperMessageContentClass> & get_content() const { return content; }
        std::vector<DeveloperMessageContentClass> & get_mutable_content() { return content; }
        void set_content(const std::vector<DeveloperMessageContentClass> & value) { this->content = value; }

        const DeveloperMessageRole & get_role() const { return role; }
        DeveloperMessageRole & get_mutable_role() { return role; }
        void set_role(const DeveloperMessageRole & value) { this->role = value; }
    };

    enum class UserMessageContentType : int { IMAGE_URL, TEXT };

    class UserMessageContent {
        public:
        UserMessageContent() = default;
        virtual ~UserMessageContent() = default;

        private:
        std::string data;
        UserMessageContentType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const UserMessageContentType & get_type() const { return type; }
        UserMessageContentType & get_mutable_type() { return type; }
        void set_type(const UserMessageContentType & value) { this->type = value; }
    };

    class UserMessageContentClass {
        public:
        UserMessageContentClass() = default;
        virtual ~UserMessageContentClass() = default;

        private:
        std::string data;
        UserMessageContentType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const UserMessageContentType & get_type() const { return type; }
        UserMessageContentType & get_mutable_type() { return type; }
        void set_type(const UserMessageContentType & value) { this->type = value; }
    };

    enum class UserMessageRole : int { USER };

    class UserMessage {
        public:
        UserMessage() = default;
        virtual ~UserMessage() = default;

        private:
        std::vector<UserMessageContentClass> content;
        UserMessageRole role;

        public:
        const std::vector<UserMessageContentClass> & get_content() const { return content; }
        std::vector<UserMessageContentClass> & get_mutable_content() { return content; }
        void set_content(const std::vector<UserMessageContentClass> & value) { this->content = value; }

        const UserMessageRole & get_role() const { return role; }
        UserMessageRole & get_mutable_role() { return role; }
        void set_role(const UserMessageRole & value) { this->role = value; }
    };

    enum class AssistantMessageContentType : int { REFUSAL, TEXT };

    class AssistantMessageContent {
        public:
        AssistantMessageContent() = default;
        virtual ~AssistantMessageContent() = default;

        private:
        std::string data;
        AssistantMessageContentType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const AssistantMessageContentType & get_type() const { return type; }
        AssistantMessageContentType & get_mutable_type() { return type; }
        void set_type(const AssistantMessageContentType & value) { this->type = value; }
    };

    class AssistantMessageContentClass {
        public:
        AssistantMessageContentClass() = default;
        virtual ~AssistantMessageContentClass() = default;

        private:
        std::string data;
        AssistantMessageContentType type;

        public:
        const std::string & get_data() const { return data; }
        std::string & get_mutable_data() { return data; }
        void set_data(const std::string & value) { this->data = value; }

        const AssistantMessageContentType & get_type() const { return type; }
        AssistantMessageContentType & get_mutable_type() { return type; }
        void set_type(const AssistantMessageContentType & value) { this->type = value; }
    };

    enum class AssistantMessageRole : int { ASSISTANT };

    class AssistantMessage {
        public:
        AssistantMessage() = default;
        virtual ~AssistantMessage() = default;

        private:
        std::vector<AssistantMessageContentClass> content;
        AssistantMessageRole role;

        public:
        const std::vector<AssistantMessageContentClass> & get_content() const { return content; }
        std::vector<AssistantMessageContentClass> & get_mutable_content() { return content; }
        void set_content(const std::vector<AssistantMessageContentClass> & value) { this->content = value; }

        const AssistantMessageRole & get_role() const { return role; }
        AssistantMessageRole & get_mutable_role() { return role; }
        void set_role(const AssistantMessageRole & value) { this->role = value; }
    };

    enum class PurpleType : int { IMAGE_URL, REFUSAL, TEXT };

    /**
     * The format is based on OpenAI's chat completions but with several simplifications.
     * Namely, all data fields are called data instead of text/image_url/refusal as the type
     * already indicates the type of the data.
     * Options other than image_url is also removed in the image_url type.
     */
    class MessageContent {
        public:
        MessageContent() = default;
        virtual ~MessageContent() = default;

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

    enum class MessageRole : int { ASSISTANT, DEVELOPER, USER };

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
namespace Chat {
    void from_json(const json & j, DeveloperMessageContent & x);
    void to_json(json & j, const DeveloperMessageContent & x);

    void from_json(const json & j, DeveloperMessageContentClass & x);
    void to_json(json & j, const DeveloperMessageContentClass & x);

    void from_json(const json & j, DeveloperMessage & x);
    void to_json(json & j, const DeveloperMessage & x);

    void from_json(const json & j, UserMessageContent & x);
    void to_json(json & j, const UserMessageContent & x);

    void from_json(const json & j, UserMessageContentClass & x);
    void to_json(json & j, const UserMessageContentClass & x);

    void from_json(const json & j, UserMessage & x);
    void to_json(json & j, const UserMessage & x);

    void from_json(const json & j, AssistantMessageContent & x);
    void to_json(json & j, const AssistantMessageContent & x);

    void from_json(const json & j, AssistantMessageContentClass & x);
    void to_json(json & j, const AssistantMessageContentClass & x);

    void from_json(const json & j, AssistantMessage & x);
    void to_json(json & j, const AssistantMessage & x);

    void from_json(const json & j, MessageContent & x);
    void to_json(json & j, const MessageContent & x);

    void from_json(const json & j, Message & x);
    void to_json(json & j, const Message & x);

    void from_json(const json & j, DeveloperMessageContentType & x);
    void to_json(json & j, const DeveloperMessageContentType & x);

    void from_json(const json & j, DeveloperMessageRole & x);
    void to_json(json & j, const DeveloperMessageRole & x);

    void from_json(const json & j, UserMessageContentType & x);
    void to_json(json & j, const UserMessageContentType & x);

    void from_json(const json & j, UserMessageRole & x);
    void to_json(json & j, const UserMessageRole & x);

    void from_json(const json & j, AssistantMessageContentType & x);
    void to_json(json & j, const AssistantMessageContentType & x);

    void from_json(const json & j, AssistantMessageRole & x);
    void to_json(json & j, const AssistantMessageRole & x);

    void from_json(const json & j, PurpleType & x);
    void to_json(json & j, const PurpleType & x);

    void from_json(const json & j, MessageRole & x);
    void to_json(json & j, const MessageRole & x);

    inline void from_json(const json & j, DeveloperMessageContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<DeveloperMessageContentType>());
    }

    inline void to_json(json & j, const DeveloperMessageContent & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, DeveloperMessageContentClass& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<DeveloperMessageContentType>());
    }

    inline void to_json(json & j, const DeveloperMessageContentClass & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, DeveloperMessage& x) {
        x.set_content(j.at("content").get<std::vector<DeveloperMessageContentClass>>());
        x.set_role(j.at("role").get<DeveloperMessageRole>());
    }

    inline void to_json(json & j, const DeveloperMessage & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, UserMessageContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<UserMessageContentType>());
    }

    inline void to_json(json & j, const UserMessageContent & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, UserMessageContentClass& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<UserMessageContentType>());
    }

    inline void to_json(json & j, const UserMessageContentClass & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, UserMessage& x) {
        x.set_content(j.at("content").get<std::vector<UserMessageContentClass>>());
        x.set_role(j.at("role").get<UserMessageRole>());
    }

    inline void to_json(json & j, const UserMessage & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, AssistantMessageContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<AssistantMessageContentType>());
    }

    inline void to_json(json & j, const AssistantMessageContent & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, AssistantMessageContentClass& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<AssistantMessageContentType>());
    }

    inline void to_json(json & j, const AssistantMessageContentClass & x) {
        j = json::object();
        j["data"] = x.get_data();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, AssistantMessage& x) {
        x.set_content(j.at("content").get<std::vector<AssistantMessageContentClass>>());
        x.set_role(j.at("role").get<AssistantMessageRole>());
    }

    inline void to_json(json & j, const AssistantMessage & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, MessageContent& x) {
        x.set_data(j.at("data").get<std::string>());
        x.set_type(j.at("type").get<PurpleType>());
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

    inline void from_json(const json & j, DeveloperMessageContentType & x) {
        if (j == "text") x = DeveloperMessageContentType::TEXT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const DeveloperMessageContentType & x) {
        switch (x) {
            case DeveloperMessageContentType::TEXT: j = "text"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"DeveloperMessageContentType\": " + std::to_string(static_cast<int>(x)));
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

    inline void from_json(const json & j, UserMessageContentType & x) {
        if (j == "image_url") x = UserMessageContentType::IMAGE_URL;
        else if (j == "text") x = UserMessageContentType::TEXT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const UserMessageContentType & x) {
        switch (x) {
            case UserMessageContentType::IMAGE_URL: j = "image_url"; break;
            case UserMessageContentType::TEXT: j = "text"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"UserMessageContentType\": " + std::to_string(static_cast<int>(x)));
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

    inline void from_json(const json & j, AssistantMessageContentType & x) {
        if (j == "refusal") x = AssistantMessageContentType::REFUSAL;
        else if (j == "text") x = AssistantMessageContentType::TEXT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const AssistantMessageContentType & x) {
        switch (x) {
            case AssistantMessageContentType::REFUSAL: j = "refusal"; break;
            case AssistantMessageContentType::TEXT: j = "text"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"AssistantMessageContentType\": " + std::to_string(static_cast<int>(x)));
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

    inline void from_json(const json & j, PurpleType & x) {
        if (j == "image_url") x = PurpleType::IMAGE_URL;
        else if (j == "refusal") x = PurpleType::REFUSAL;
        else if (j == "text") x = PurpleType::TEXT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const PurpleType & x) {
        switch (x) {
            case PurpleType::IMAGE_URL: j = "image_url"; break;
            case PurpleType::REFUSAL: j = "refusal"; break;
            case PurpleType::TEXT: j = "text"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"PurpleType\": " + std::to_string(static_cast<int>(x)));
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
