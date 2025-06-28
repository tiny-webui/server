//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Request data = nlohmann::json::parse(jsonString);
//     Response data = nlohmann::json::parse(jsonString);
//     ErrorResponse data = nlohmann::json::parse(jsonString);
//     StreamEndResponse data = nlohmann::json::parse(jsonString);
//     ErrorCode data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <regex>

namespace TUI {
namespace Schema {
namespace Rpc {
    using nlohmann::json;

    #ifndef NLOHMANN_UNTYPED_TUI_Schema_Rpc_HELPER
    #define NLOHMANN_UNTYPED_TUI_Schema_Rpc_HELPER
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

    /**
     * There are three types of interactions:
     * 1. Request: request + response / error
     * 2. Stream request: request + stream responses + end response / error
     * 3. Post: request
     */
    class Request {
        public:
        Request() = default;
        virtual ~Request() = default;

        private:
        double id;
        std::string method;
        nlohmann::json params;

        public:
        const double & get_id() const { return id; }
        double & get_mutable_id() { return id; }
        void set_id(const double & value) { this->id = value; }

        const std::string & get_method() const { return method; }
        std::string & get_mutable_method() { return method; }
        void set_method(const std::string & value) { this->method = value; }

        const nlohmann::json & get_params() const { return params; }
        nlohmann::json & get_mutable_params() { return params; }
        void set_params(const nlohmann::json & value) { this->params = value; }
    };

    class Response {
        public:
        Response() = default;
        virtual ~Response() = default;

        private:
        double id;
        nlohmann::json result;

        public:
        const double & get_id() const { return id; }
        double & get_mutable_id() { return id; }
        void set_id(const double & value) { this->id = value; }

        const nlohmann::json & get_result() const { return result; }
        nlohmann::json & get_mutable_result() { return result; }
        void set_result(const nlohmann::json & value) { this->result = value; }
    };

    class Error {
        public:
        Error() = default;
        virtual ~Error() = default;

        private:
        double code;
        std::string message;

        public:
        const double & get_code() const { return code; }
        double & get_mutable_code() { return code; }
        void set_code(const double & value) { this->code = value; }

        const std::string & get_message() const { return message; }
        std::string & get_mutable_message() { return message; }
        void set_message(const std::string & value) { this->message = value; }
    };

    class ErrorResponse {
        public:
        ErrorResponse() = default;
        virtual ~ErrorResponse() = default;

        private:
        Error error;
        double id;

        public:
        const Error & get_error() const { return error; }
        Error & get_mutable_error() { return error; }
        void set_error(const Error & value) { this->error = value; }

        const double & get_id() const { return id; }
        double & get_mutable_id() { return id; }
        void set_id(const double & value) { this->id = value; }
    };

    class StreamEndResponse {
        public:
        StreamEndResponse() = default;
        virtual ~StreamEndResponse() = default;

        private:
        bool end;
        double id;
        nlohmann::json result;

        public:
        const bool & get_end() const { return end; }
        bool & get_mutable_end() { return end; }
        void set_end(const bool & value) { this->end = value; }

        const double & get_id() const { return id; }
        double & get_mutable_id() { return id; }
        void set_id(const double & value) { this->id = value; }

        const nlohmann::json & get_result() const { return result; }
        nlohmann::json & get_mutable_result() { return result; }
        void set_result(const nlohmann::json & value) { this->result = value; }
    };

    struct ErrorCode
    {
        static constexpr double NOT_MODIFIED = 304;
        static constexpr double BAD_REQUEST = 400;
        static constexpr double UNAUTHORIZED = 401;
        static constexpr double NOT_FOUND = 404;
        static constexpr double CONFLICT = 409;
        static constexpr double LOCKED = 423;
        static constexpr double INTERNAL_SERVER_ERROR = 500;
        static constexpr double NOT_IMPLEMENTED = 501;
        static constexpr double BAD_GATEWAY = 502;
    };

    class Exception : public std::runtime_error
    {
    public:
        Exception(double code, const std::string& message)
            : std::runtime_error(message), _code(code)
        {
        }
        double get_code() const noexcept
        {
            return _code;
        }
    private:
        double _code;
    };
}
}
}

namespace TUI {
namespace Schema {
namespace Rpc {
    void from_json(const json & j, Request & x);
    void to_json(json & j, const Request & x);

    void from_json(const json & j, Response & x);
    void to_json(json & j, const Response & x);

    void from_json(const json & j, Error & x);
    void to_json(json & j, const Error & x);

    void from_json(const json & j, ErrorResponse & x);
    void to_json(json & j, const ErrorResponse & x);

    void from_json(const json & j, StreamEndResponse & x);
    void to_json(json & j, const StreamEndResponse & x);

    inline void from_json(const json & j, Request& x) {
        x.set_id(j.at("id").get<double>());
        x.set_method(j.at("method").get<std::string>());
        x.set_params(get_untyped(j, "params"));
    }

    inline void to_json(json & j, const Request & x) {
        j = json::object();
        j["id"] = x.get_id();
        j["method"] = x.get_method();
        j["params"] = x.get_params();
    }

    inline void from_json(const json & j, Response& x) {
        x.set_id(j.at("id").get<double>());
        x.set_result(get_untyped(j, "result"));
    }

    inline void to_json(json & j, const Response & x) {
        j = json::object();
        j["id"] = x.get_id();
        j["result"] = x.get_result();
    }

    inline void from_json(const json & j, Error& x) {
        x.set_code(j.at("code").get<double>());
        x.set_message(j.at("message").get<std::string>());
    }

    inline void to_json(json & j, const Error & x) {
        j = json::object();
        j["code"] = x.get_code();
        j["message"] = x.get_message();
    }

    inline void from_json(const json & j, ErrorResponse& x) {
        x.set_error(j.at("error").get<Error>());
        x.set_id(j.at("id").get<double>());
    }

    inline void to_json(json & j, const ErrorResponse & x) {
        j = json::object();
        j["error"] = x.get_error();
        j["id"] = x.get_id();
    }

    inline void from_json(const json & j, StreamEndResponse& x) {
        x.set_end(j.at("end").get<bool>());
        x.set_id(j.at("id").get<double>());
        x.set_result(get_untyped(j, "result"));
    }

    inline void to_json(json & j, const StreamEndResponse & x) {
        j = json::object();
        j["end"] = x.get_end();
        j["id"] = x.get_id();
        j["result"] = x.get_result();
    }
}
}
}
