#pragma once

#include <string>
#include <map>
#include <set>
#include <memory>
#include <curl/curl.h>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include <js-style-co-routine/AsyncGenerator.h>
#include "CurlTypes.h"

namespace TUI::Network::Http
{
    enum class Method
    {
        GET,
        POST,
    };

    struct RequestData
    {
        std::string url;
        std::map<std::string, std::string> headers;
        std::string body;
    };

    class Request
    {
        friend class Client;
    public:
        Request() = default;
        ~Request() = default;

        /** Awaitable implementation */
        bool await_ready() const;
        void await_suspend(std::coroutine_handle<> handle);
        std::string await_resume();
    private:
        struct State
        {
            JS::Promise<std::string> promise{};
            std::weak_ptr<CurlTypes::Curl> curl{};
            std::string response{};
        };

        std::shared_ptr<State> _state{std::make_shared<State>()};
        
        static size_t CurlWriteFunction(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept;
    };

    // class StreamRequest
    // {
    //     friend class Client;
    // public:
    //     ~StreamRequest();

    //     JS::Promise<std::string> NextAsync();
    // private:
    //     JS::AsyncGenerator<std::string> _generator;

    //     StreamRequest();
    // };

    class Client : public std::enable_shared_from_this<Client>
    {
    public:
        class RequestCancelledException : public std::exception
        {
        };

        static std::shared_ptr<Client> Create(Tev& tev);
        ~Client();

        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;
        Client(Client&&) = delete;
        Client& operator=(Client&&) = delete;

        Request MakeRequestAsync(Method method, const RequestData& data);

        // StreamRequest MakeStreamRequest(Method method, const RequestData& data);

        void CancelRequest(Request& request);

        // void CancelRequest(StreamRequest& request);
    private:
        Tev& _tev;
        CurlTypes::CurlEnv _curlEnv{};
        std::unique_ptr<CurlTypes::CurlM> _curlm{nullptr};
        Tev::TimeoutHandle _curlTimeoutHandle{0};
        /** 
         * The Client MUST live longer than Curl, While Request can outlive Client.
         * Thus Client holds a shared_ptr to Curl, while Request holds a weak_ptr.
         * Use CURL* as the key since the C callback uses this to identify the request.
         */
        std::map<CURL*, std::pair<std::shared_ptr<CurlTypes::Curl>, Request>> _requests{};

        Client(Tev& tev);
        static int CurlSocketFunction(CURL* easy, curl_socket_t s, int what, void* clientp, void* socketp) noexcept;
        static int CurlTimeoutFunction(CURLM* multi, long timeout_ms, void* clientp) noexcept;
        void SocketActionHandler(int fd, int events);
    };
}
