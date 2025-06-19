//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     SetMetadataParams data = nlohmann::json::parse(jsonString);
//     GetMetadataParams data = nlohmann::json::parse(jsonString);
//     GetMetadataResult data = nlohmann::json::parse(jsonString);
//     DeleteMetadataParams data = nlohmann::json::parse(jsonString);
//     Message data = nlohmann::json::parse(jsonString);
//     MessageNode data = nlohmann::json::parse(jsonString);
//     LinearHistory data = nlohmann::json::parse(jsonString);
//     TreeHistory data = nlohmann::json::parse(jsonString);
//     GetChatListParams data = nlohmann::json::parse(jsonString);
//     GetChatListResult data = nlohmann::json::parse(jsonString);
//     ChatCompletionParams data = nlohmann::json::parse(jsonString);
//     ChatCompletionInfo data = nlohmann::json::parse(jsonString);
//     ExecuteGenerationTaskParams data = nlohmann::json::parse(jsonString);
//     GetModelListParams data = nlohmann::json::parse(jsonString);
//     GetModelListResult data = nlohmann::json::parse(jsonString);
//     ProviderParams data = nlohmann::json::parse(jsonString);
//     NewModelParams data = nlohmann::json::parse(jsonString);
//     ModifyModelParams data = nlohmann::json::parse(jsonString);

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
namespace IServer {
    using nlohmann::json;

    #ifndef NLOHMANN_UNTYPED_TUI_Schema_IServer_HELPER
    #define NLOHMANN_UNTYPED_TUI_Schema_IServer_HELPER
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

    #ifndef NLOHMANN_OPTIONAL_TUI_Schema_IServer_HELPER
    #define NLOHMANN_OPTIONAL_TUI_Schema_IServer_HELPER
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

    /**
     * Design principles:
     * 1. No backward compatibility is required. As the server and client's version are always
     * in sync.
     * 2. Simplicity is preferred.
     */
    class SetMetadataParams {
        public:
        SetMetadataParams() = default;
        virtual ~SetMetadataParams() = default;

        private:
        std::map<std::string, nlohmann::json> entries;
        std::vector<std::string> path;

        public:
        const std::map<std::string, nlohmann::json> & get_entries() const { return entries; }
        std::map<std::string, nlohmann::json> & get_mutable_entries() { return entries; }
        void set_entries(const std::map<std::string, nlohmann::json> & value) { this->entries = value; }

        /**
         * For example, ['global'] means global metadata,
         * ['user'] means current user's metadata,
         * ['chat', '123'] means chat metadata for chat with id 123.
         * ['model', 'abc'] means model metadata for model with id abc.
         */
        const std::vector<std::string> & get_path() const { return path; }
        std::vector<std::string> & get_mutable_path() { return path; }
        void set_path(const std::vector<std::string> & value) { this->path = value; }
    };

    class GetMetadataParams {
        public:
        GetMetadataParams() = default;
        virtual ~GetMetadataParams() = default;

        private:
        std::vector<std::string> keys;
        std::vector<std::string> path;

        public:
        const std::vector<std::string> & get_keys() const { return keys; }
        std::vector<std::string> & get_mutable_keys() { return keys; }
        void set_keys(const std::vector<std::string> & value) { this->keys = value; }

        const std::vector<std::string> & get_path() const { return path; }
        std::vector<std::string> & get_mutable_path() { return path; }
        void set_path(const std::vector<std::string> & value) { this->path = value; }
    };

    class DeleteMetadataParams {
        public:
        DeleteMetadataParams() = default;
        virtual ~DeleteMetadataParams() = default;

        private:
        std::vector<std::string> keys;
        std::vector<std::string> path;

        public:
        const std::vector<std::string> & get_keys() const { return keys; }
        std::vector<std::string> & get_mutable_keys() { return keys; }
        void set_keys(const std::vector<std::string> & value) { this->keys = value; }

        const std::vector<std::string> & get_path() const { return path; }
        std::vector<std::string> & get_mutable_path() { return path; }
        void set_path(const std::vector<std::string> & value) { this->path = value; }
    };

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

    class MessageNode {
        public:
        MessageNode() = default;
        virtual ~MessageNode() = default;

        private:
        std::vector<std::string> children;
        std::string id;
        Message message;
        std::optional<std::string> parent;
        double timestamp;

        public:
        const std::vector<std::string> & get_children() const { return children; }
        std::vector<std::string> & get_mutable_children() { return children; }
        void set_children(const std::vector<std::string> & value) { this->children = value; }

        const std::string & get_id() const { return id; }
        std::string & get_mutable_id() { return id; }
        void set_id(const std::string & value) { this->id = value; }

        const Message & get_message() const { return message; }
        Message & get_mutable_message() { return message; }
        void set_message(const Message & value) { this->message = value; }

        std::optional<std::string> get_parent() const { return parent; }
        void set_parent(std::optional<std::string> value) { this->parent = value; }

        const double & get_timestamp() const { return timestamp; }
        double & get_mutable_timestamp() { return timestamp; }
        void set_timestamp(const double & value) { this->timestamp = value; }
    };

    class TreeHistory {
        public:
        TreeHistory() = default;
        virtual ~TreeHistory() = default;

        private:
        std::string head;
        std::map<std::string, MessageNode> nodes;

        public:
        const std::string & get_head() const { return head; }
        std::string & get_mutable_head() { return head; }
        void set_head(const std::string & value) { this->head = value; }

        const std::map<std::string, MessageNode> & get_nodes() const { return nodes; }
        std::map<std::string, MessageNode> & get_mutable_nodes() { return nodes; }
        void set_nodes(const std::map<std::string, MessageNode> & value) { this->nodes = value; }
    };

    class GetChatListParams {
        public:
        GetChatListParams() = default;
        virtual ~GetChatListParams() = default;

        private:
        std::optional<std::vector<std::string>> meta_data_keys;
        double quantity;
        double start;

        public:
        /**
         * If no key is specified, no metadata will be returned.
         */
        std::optional<std::vector<std::string>> get_meta_data_keys() const { return meta_data_keys; }
        void set_meta_data_keys(std::optional<std::vector<std::string>> value) { this->meta_data_keys = value; }

        const double & get_quantity() const { return quantity; }
        double & get_mutable_quantity() { return quantity; }
        void set_quantity(const double & value) { this->quantity = value; }

        const double & get_start() const { return start; }
        double & get_mutable_start() { return start; }
        void set_start(const double & value) { this->start = value; }
    };

    class List {
        public:
        List() = default;
        virtual ~List() = default;

        private:
        std::string id;
        std::optional<std::map<std::string, nlohmann::json>> metadata;

        public:
        const std::string & get_id() const { return id; }
        std::string & get_mutable_id() { return id; }
        void set_id(const std::string & value) { this->id = value; }

        std::optional<std::map<std::string, nlohmann::json>> get_metadata() const { return metadata; }
        void set_metadata(std::optional<std::map<std::string, nlohmann::json>> value) { this->metadata = value; }
    };

    class GetChatListResult {
        public:
        GetChatListResult() = default;
        virtual ~GetChatListResult() = default;

        private:
        std::vector<List> list;

        public:
        /**
         * This list will be in the reverse order of creation
         */
        const std::vector<List> & get_list() const { return list; }
        std::vector<List> & get_mutable_list() { return list; }
        void set_list(const std::vector<List> & value) { this->list = value; }
    };

    class ChatCompletionParams {
        public:
        ChatCompletionParams() = default;
        virtual ~ChatCompletionParams() = default;

        private:
        std::string id;
        std::string model_id;
        std::optional<std::string> parent;
        Message user_message;

        public:
        const std::string & get_id() const { return id; }
        std::string & get_mutable_id() { return id; }
        void set_id(const std::string & value) { this->id = value; }

        const std::string & get_model_id() const { return model_id; }
        std::string & get_mutable_model_id() { return model_id; }
        void set_model_id(const std::string & value) { this->model_id = value; }

        /**
         * If parent is not present, treat as a new root.
         */
        std::optional<std::string> get_parent() const { return parent; }
        void set_parent(std::optional<std::string> value) { this->parent = value; }

        const Message & get_user_message() const { return user_message; }
        Message & get_mutable_user_message() { return user_message; }
        void set_user_message(const Message & value) { this->user_message = value; }
    };

    class ChatCompletionInfo {
        public:
        ChatCompletionInfo() = default;
        virtual ~ChatCompletionInfo() = default;

        private:
        std::string assistant_message_id;
        std::string user_message_id;

        public:
        const std::string & get_assistant_message_id() const { return assistant_message_id; }
        std::string & get_mutable_assistant_message_id() { return assistant_message_id; }
        void set_assistant_message_id(const std::string & value) { this->assistant_message_id = value; }

        const std::string & get_user_message_id() const { return user_message_id; }
        std::string & get_mutable_user_message_id() { return user_message_id; }
        void set_user_message_id(const std::string & value) { this->user_message_id = value; }
    };

    class ExecuteGenerationTaskParams {
        public:
        ExecuteGenerationTaskParams() = default;
        virtual ~ExecuteGenerationTaskParams() = default;

        private:
        std::string model_id;
        std::string prompt_template_name;
        std::vector<std::string> prompt_template_params;

        public:
        const std::string & get_model_id() const { return model_id; }
        std::string & get_mutable_model_id() { return model_id; }
        void set_model_id(const std::string & value) { this->model_id = value; }

        const std::string & get_prompt_template_name() const { return prompt_template_name; }
        std::string & get_mutable_prompt_template_name() { return prompt_template_name; }
        void set_prompt_template_name(const std::string & value) { this->prompt_template_name = value; }

        const std::vector<std::string> & get_prompt_template_params() const { return prompt_template_params; }
        std::vector<std::string> & get_mutable_prompt_template_params() { return prompt_template_params; }
        void set_prompt_template_params(const std::vector<std::string> & value) { this->prompt_template_params = value; }
    };

    class GetModelListParams {
        public:
        GetModelListParams() = default;
        virtual ~GetModelListParams() = default;

        private:
        std::optional<std::vector<std::string>> metadata_keys;

        public:
        std::optional<std::vector<std::string>> get_metadata_keys() const { return metadata_keys; }
        void set_metadata_keys(std::optional<std::vector<std::string>> value) { this->metadata_keys = value; }
    };

    class GetModelListResultElement {
        public:
        GetModelListResultElement() = default;
        virtual ~GetModelListResultElement() = default;

        private:
        std::string id;
        std::optional<std::map<std::string, nlohmann::json>> metadata;
        std::string provider_name;

        public:
        const std::string & get_id() const { return id; }
        std::string & get_mutable_id() { return id; }
        void set_id(const std::string & value) { this->id = value; }

        std::optional<std::map<std::string, nlohmann::json>> get_metadata() const { return metadata; }
        void set_metadata(std::optional<std::map<std::string, nlohmann::json>> value) { this->metadata = value; }

        const std::string & get_provider_name() const { return provider_name; }
        std::string & get_mutable_provider_name() { return provider_name; }
        void set_provider_name(const std::string & value) { this->provider_name = value; }
    };

    class NewModelParams {
        public:
        NewModelParams() = default;
        virtual ~NewModelParams() = default;

        private:
        std::string provider_name;
        nlohmann::json provider_params;

        public:
        const std::string & get_provider_name() const { return provider_name; }
        std::string & get_mutable_provider_name() { return provider_name; }
        void set_provider_name(const std::string & value) { this->provider_name = value; }

        const nlohmann::json & get_provider_params() const { return provider_params; }
        nlohmann::json & get_mutable_provider_params() { return provider_params; }
        void set_provider_params(const nlohmann::json & value) { this->provider_params = value; }
    };

    class ModifyModelParams {
        public:
        ModifyModelParams() = default;
        virtual ~ModifyModelParams() = default;

        private:
        std::string id;
        nlohmann::json provider_params;

        public:
        const std::string & get_id() const { return id; }
        std::string & get_mutable_id() { return id; }
        void set_id(const std::string & value) { this->id = value; }

        const nlohmann::json & get_provider_params() const { return provider_params; }
        nlohmann::json & get_mutable_provider_params() { return provider_params; }
        void set_provider_params(const nlohmann::json & value) { this->provider_params = value; }
    };

    using GetMetadataResult = std::map<std::string, nlohmann::json>;
    using LinearHistory = std::vector<Message>;
    using GetModelListResult = std::vector<GetModelListResultElement>;
    using ProviderParams = nlohmann::json;
}
}
}

namespace TUI {
namespace Schema {
namespace IServer {
    void from_json(const json & j, SetMetadataParams & x);
    void to_json(json & j, const SetMetadataParams & x);

    void from_json(const json & j, GetMetadataParams & x);
    void to_json(json & j, const GetMetadataParams & x);

    void from_json(const json & j, DeleteMetadataParams & x);
    void to_json(json & j, const DeleteMetadataParams & x);

    void from_json(const json & j, MessageContent & x);
    void to_json(json & j, const MessageContent & x);

    void from_json(const json & j, Message & x);
    void to_json(json & j, const Message & x);

    void from_json(const json & j, MessageNode & x);
    void to_json(json & j, const MessageNode & x);

    void from_json(const json & j, TreeHistory & x);
    void to_json(json & j, const TreeHistory & x);

    void from_json(const json & j, GetChatListParams & x);
    void to_json(json & j, const GetChatListParams & x);

    void from_json(const json & j, List & x);
    void to_json(json & j, const List & x);

    void from_json(const json & j, GetChatListResult & x);
    void to_json(json & j, const GetChatListResult & x);

    void from_json(const json & j, ChatCompletionParams & x);
    void to_json(json & j, const ChatCompletionParams & x);

    void from_json(const json & j, ChatCompletionInfo & x);
    void to_json(json & j, const ChatCompletionInfo & x);

    void from_json(const json & j, ExecuteGenerationTaskParams & x);
    void to_json(json & j, const ExecuteGenerationTaskParams & x);

    void from_json(const json & j, GetModelListParams & x);
    void to_json(json & j, const GetModelListParams & x);

    void from_json(const json & j, GetModelListResultElement & x);
    void to_json(json & j, const GetModelListResultElement & x);

    void from_json(const json & j, NewModelParams & x);
    void to_json(json & j, const NewModelParams & x);

    void from_json(const json & j, ModifyModelParams & x);
    void to_json(json & j, const ModifyModelParams & x);

    void from_json(const json & j, Type & x);
    void to_json(json & j, const Type & x);

    void from_json(const json & j, Role & x);
    void to_json(json & j, const Role & x);

    inline void from_json(const json & j, SetMetadataParams& x) {
        x.set_entries(j.at("entries").get<std::map<std::string, nlohmann::json>>());
        x.set_path(j.at("path").get<std::vector<std::string>>());
    }

    inline void to_json(json & j, const SetMetadataParams & x) {
        j = json::object();
        j["entries"] = x.get_entries();
        j["path"] = x.get_path();
    }

    inline void from_json(const json & j, GetMetadataParams& x) {
        x.set_keys(j.at("keys").get<std::vector<std::string>>());
        x.set_path(j.at("path").get<std::vector<std::string>>());
    }

    inline void to_json(json & j, const GetMetadataParams & x) {
        j = json::object();
        j["keys"] = x.get_keys();
        j["path"] = x.get_path();
    }

    inline void from_json(const json & j, DeleteMetadataParams& x) {
        x.set_keys(j.at("keys").get<std::vector<std::string>>());
        x.set_path(j.at("path").get<std::vector<std::string>>());
    }

    inline void to_json(json & j, const DeleteMetadataParams & x) {
        j = json::object();
        j["keys"] = x.get_keys();
        j["path"] = x.get_path();
    }

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

    inline void from_json(const json & j, MessageNode& x) {
        x.set_children(j.at("children").get<std::vector<std::string>>());
        x.set_id(j.at("id").get<std::string>());
        x.set_message(j.at("message").get<Message>());
        x.set_parent(get_stack_optional<std::string>(j, "parent"));
        x.set_timestamp(j.at("timestamp").get<double>());
    }

    inline void to_json(json & j, const MessageNode & x) {
        j = json::object();
        j["children"] = x.get_children();
        j["id"] = x.get_id();
        j["message"] = x.get_message();
        if (x.get_parent()) {
            j["parent"] = x.get_parent();
        }
        j["timestamp"] = x.get_timestamp();
    }

    inline void from_json(const json & j, TreeHistory& x) {
        x.set_head(j.at("head").get<std::string>());
        x.set_nodes(j.at("nodes").get<std::map<std::string, MessageNode>>());
    }

    inline void to_json(json & j, const TreeHistory & x) {
        j = json::object();
        j["head"] = x.get_head();
        j["nodes"] = x.get_nodes();
    }

    inline void from_json(const json & j, GetChatListParams& x) {
        x.set_meta_data_keys(get_stack_optional<std::vector<std::string>>(j, "metaDataKeys"));
        x.set_quantity(j.at("quantity").get<double>());
        x.set_start(j.at("start").get<double>());
    }

    inline void to_json(json & j, const GetChatListParams & x) {
        j = json::object();
        if (x.get_meta_data_keys()) {
            j["metaDataKeys"] = x.get_meta_data_keys();
        }
        j["quantity"] = x.get_quantity();
        j["start"] = x.get_start();
    }

    inline void from_json(const json & j, List& x) {
        x.set_id(j.at("id").get<std::string>());
        x.set_metadata(get_stack_optional<std::map<std::string, nlohmann::json>>(j, "metadata"));
    }

    inline void to_json(json & j, const List & x) {
        j = json::object();
        j["id"] = x.get_id();
        if (x.get_metadata()) {
            j["metadata"] = x.get_metadata();
        }
    }

    inline void from_json(const json & j, GetChatListResult& x) {
        x.set_list(j.at("list").get<std::vector<List>>());
    }

    inline void to_json(json & j, const GetChatListResult & x) {
        j = json::object();
        j["list"] = x.get_list();
    }

    inline void from_json(const json & j, ChatCompletionParams& x) {
        x.set_id(j.at("id").get<std::string>());
        x.set_model_id(j.at("modelId").get<std::string>());
        x.set_parent(get_stack_optional<std::string>(j, "parent"));
        x.set_user_message(j.at("userMessage").get<Message>());
    }

    inline void to_json(json & j, const ChatCompletionParams & x) {
        j = json::object();
        j["id"] = x.get_id();
        j["modelId"] = x.get_model_id();
        if (x.get_parent()) {
            j["parent"] = x.get_parent();
        }
        j["userMessage"] = x.get_user_message();
    }

    inline void from_json(const json & j, ChatCompletionInfo& x) {
        x.set_assistant_message_id(j.at("assistantMessageId").get<std::string>());
        x.set_user_message_id(j.at("userMessageId").get<std::string>());
    }

    inline void to_json(json & j, const ChatCompletionInfo & x) {
        j = json::object();
        j["assistantMessageId"] = x.get_assistant_message_id();
        j["userMessageId"] = x.get_user_message_id();
    }

    inline void from_json(const json & j, ExecuteGenerationTaskParams& x) {
        x.set_model_id(j.at("modelId").get<std::string>());
        x.set_prompt_template_name(j.at("promptTemplateName").get<std::string>());
        x.set_prompt_template_params(j.at("promptTemplateParams").get<std::vector<std::string>>());
    }

    inline void to_json(json & j, const ExecuteGenerationTaskParams & x) {
        j = json::object();
        j["modelId"] = x.get_model_id();
        j["promptTemplateName"] = x.get_prompt_template_name();
        j["promptTemplateParams"] = x.get_prompt_template_params();
    }

    inline void from_json(const json & j, GetModelListParams& x) {
        x.set_metadata_keys(get_stack_optional<std::vector<std::string>>(j, "metadataKeys"));
    }

    inline void to_json(json & j, const GetModelListParams & x) {
        j = json::object();
        if (x.get_metadata_keys()) {
            j["metadataKeys"] = x.get_metadata_keys();
        }
    }

    inline void from_json(const json & j, GetModelListResultElement& x) {
        x.set_id(j.at("id").get<std::string>());
        x.set_metadata(get_stack_optional<std::map<std::string, nlohmann::json>>(j, "metadata"));
        x.set_provider_name(j.at("providerName").get<std::string>());
    }

    inline void to_json(json & j, const GetModelListResultElement & x) {
        j = json::object();
        j["id"] = x.get_id();
        if (x.get_metadata()) {
            j["metadata"] = x.get_metadata();
        }
        j["providerName"] = x.get_provider_name();
    }

    inline void from_json(const json & j, NewModelParams& x) {
        x.set_provider_name(j.at("providerName").get<std::string>());
        x.set_provider_params(get_untyped(j, "providerParams"));
    }

    inline void to_json(json & j, const NewModelParams & x) {
        j = json::object();
        j["providerName"] = x.get_provider_name();
        j["providerParams"] = x.get_provider_params();
    }

    inline void from_json(const json & j, ModifyModelParams& x) {
        x.set_id(j.at("id").get<std::string>());
        x.set_provider_params(get_untyped(j, "providerParams"));
    }

    inline void to_json(json & j, const ModifyModelParams & x) {
        j = json::object();
        j["id"] = x.get_id();
        j["providerParams"] = x.get_provider_params();
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
