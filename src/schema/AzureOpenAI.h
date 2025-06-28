//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     BulkResponse data = nlohmann::json::parse(jsonString);
//     StreamResponse data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <regex>

#ifndef NLOHMANN_OPT_HELPER
#define NLOHMANN_OPT_HELPER
namespace nlohmann {
    template <typename T>
    struct adl_serializer<std::shared_ptr<T>> {
        static void to_json(json & j, const std::shared_ptr<T> & opt) {
            if (!opt) j = nullptr; else j = *opt;
        }

        static std::shared_ptr<T> from_json(const json & j) {
            if (j.is_null()) return std::make_shared<T>(); else return std::make_shared<T>(j.get<T>());
        }
    };
    template <typename T>
    struct adl_serializer<std::optional<T>> {
        static void to_json(json & j, const std::optional<T> & opt) {
            if (!opt) j = nullptr; else j = *opt;
        }

        static std::optional<T> from_json(const json & j) {
            if (j.is_null()) return std::make_optional<T>(); else return std::make_optional<T>(j.get<T>());
        }
    };
}
#endif

namespace TUI {
namespace Schema {
namespace AzureOpenAI {
    using nlohmann::json;

    #ifndef NLOHMANN_UNTYPED_TUI_Schema_AzureOpenAI_HELPER
    #define NLOHMANN_UNTYPED_TUI_Schema_AzureOpenAI_HELPER
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

    #ifndef NLOHMANN_OPTIONAL_TUI_Schema_AzureOpenAI_HELPER
    #define NLOHMANN_OPTIONAL_TUI_Schema_AzureOpenAI_HELPER
    template <typename T>
    inline std::shared_ptr<T> get_heap_optional(const json & j, const char * property) {
        auto it = j.find(property);
        if (it != j.end() && !it->is_null()) {
            return j.at(property).get<std::shared_ptr<T>>();
        }
        return std::shared_ptr<T>();
    }

    template <typename T>
    inline std::shared_ptr<T> get_heap_optional(const json & j, std::string property) {
        return get_heap_optional<T>(j, property.data());
    }
    template <typename T>
    inline std::optional<T> get_stack_optional(const json & j, const char * property) {
        auto it = j.find(property);
        if (it != j.end() && !it->is_null()) {
            return j.at(property).get<std::optional<T>>();
        }
        return std::optional<T>();
    }

    template <typename T>
    inline std::optional<T> get_stack_optional(const json & j, std::string property) {
        return get_stack_optional<T>(j, property.data());
    }
    #endif

    enum class Role : int { ASSISTANT };

    class Message {
        public:
        Message() = default;
        virtual ~Message() = default;

        private:
        std::string content;
        std::optional<bool> refusal;
        Role role;

        public:
        const std::string & get_content() const { return content; }
        std::string & get_mutable_content() { return content; }
        void set_content(const std::string & value) { this->content = value; }

        std::optional<bool> get_refusal() const { return refusal; }
        void set_refusal(std::optional<bool> value) { this->refusal = value; }

        const Role & get_role() const { return role; }
        Role & get_mutable_role() { return role; }
        void set_role(const Role & value) { this->role = value; }
    };

    class BulkResponseChoice {
        public:
        BulkResponseChoice() = default;
        virtual ~BulkResponseChoice() = default;

        private:
        std::string finish_reason;
        double index;
        Message message;

        public:
        const std::string & get_finish_reason() const { return finish_reason; }
        std::string & get_mutable_finish_reason() { return finish_reason; }
        void set_finish_reason(const std::string & value) { this->finish_reason = value; }

        const double & get_index() const { return index; }
        double & get_mutable_index() { return index; }
        void set_index(const double & value) { this->index = value; }

        const Message & get_message() const { return message; }
        Message & get_mutable_message() { return message; }
        void set_message(const Message & value) { this->message = value; }
    };

    class BulkResponse {
        public:
        BulkResponse() = default;
        virtual ~BulkResponse() = default;

        private:
        std::vector<BulkResponseChoice> choices;

        public:
        const std::vector<BulkResponseChoice> & get_choices() const { return choices; }
        std::vector<BulkResponseChoice> & get_mutable_choices() { return choices; }
        void set_choices(const std::vector<BulkResponseChoice> & value) { this->choices = value; }
    };

    class Delta {
        public:
        Delta() = default;
        virtual ~Delta() = default;

        private:
        std::optional<std::string> content;

        public:
        std::optional<std::string> get_content() const { return content; }
        void set_content(std::optional<std::string> value) { this->content = value; }
    };

    class StreamResponseChoice {
        public:
        StreamResponseChoice() = default;
        virtual ~StreamResponseChoice() = default;

        private:
        Delta delta;

        public:
        const Delta & get_delta() const { return delta; }
        Delta & get_mutable_delta() { return delta; }
        void set_delta(const Delta & value) { this->delta = value; }
    };

    class StreamResponse {
        public:
        StreamResponse() = default;
        virtual ~StreamResponse() = default;

        private:
        std::vector<StreamResponseChoice> choices;

        public:
        const std::vector<StreamResponseChoice> & get_choices() const { return choices; }
        std::vector<StreamResponseChoice> & get_mutable_choices() { return choices; }
        void set_choices(const std::vector<StreamResponseChoice> & value) { this->choices = value; }
    };
}
}
}

namespace TUI {
namespace Schema {
namespace AzureOpenAI {
    void from_json(const json & j, Message & x);
    void to_json(json & j, const Message & x);

    void from_json(const json & j, BulkResponseChoice & x);
    void to_json(json & j, const BulkResponseChoice & x);

    void from_json(const json & j, BulkResponse & x);
    void to_json(json & j, const BulkResponse & x);

    void from_json(const json & j, Delta & x);
    void to_json(json & j, const Delta & x);

    void from_json(const json & j, StreamResponseChoice & x);
    void to_json(json & j, const StreamResponseChoice & x);

    void from_json(const json & j, StreamResponse & x);
    void to_json(json & j, const StreamResponse & x);

    void from_json(const json & j, Role & x);
    void to_json(json & j, const Role & x);

    inline void from_json(const json & j, Message& x) {
        x.set_content(j.at("content").get<std::string>());
        x.set_refusal(get_stack_optional<bool>(j, "refusal"));
        x.set_role(j.at("role").get<Role>());
    }

    inline void to_json(json & j, const Message & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["refusal"] = x.get_refusal();
        j["role"] = x.get_role();
    }

    inline void from_json(const json & j, BulkResponseChoice& x) {
        x.set_finish_reason(j.at("finish_reason").get<std::string>());
        x.set_index(j.at("index").get<double>());
        x.set_message(j.at("message").get<Message>());
    }

    inline void to_json(json & j, const BulkResponseChoice & x) {
        j = json::object();
        j["finish_reason"] = x.get_finish_reason();
        j["index"] = x.get_index();
        j["message"] = x.get_message();
    }

    inline void from_json(const json & j, BulkResponse& x) {
        x.set_choices(j.at("choices").get<std::vector<BulkResponseChoice>>());
    }

    inline void to_json(json & j, const BulkResponse & x) {
        j = json::object();
        j["choices"] = x.get_choices();
    }

    inline void from_json(const json & j, Delta& x) {
        x.set_content(get_stack_optional<std::string>(j, "content"));
    }

    inline void to_json(json & j, const Delta & x) {
        j = json::object();
        if (x.get_content()) {
            j["content"] = x.get_content();
        }
    }

    inline void from_json(const json & j, StreamResponseChoice& x) {
        x.set_delta(j.at("delta").get<Delta>());
    }

    inline void to_json(json & j, const StreamResponseChoice & x) {
        j = json::object();
        j["delta"] = x.get_delta();
    }

    inline void from_json(const json & j, StreamResponse& x) {
        x.set_choices(j.at("choices").get<std::vector<StreamResponseChoice>>());
    }

    inline void to_json(json & j, const StreamResponse & x) {
        j = json::object();
        j["choices"] = x.get_choices();
    }

    inline void from_json(const json & j, Role & x) {
        if (j == "assistant") x = Role::ASSISTANT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Role & x) {
        switch (x) {
            case Role::ASSISTANT: j = "assistant"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"Role\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
}
}
