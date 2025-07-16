#include "HttpClient.h"
#include <functional>

using namespace TUI::Network;
using namespace TUI::Network::Http::CurlTypes;

/** Request */

JS::Promise<std::string> Http::Request::GetResponseAsync() const
{
    return _state->promise;
}

extern "C" size_t Http::Request::CurlWriteFunction(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept
{
    try
    {
        auto state = static_cast<Http::Request::State*>(userdata);
        size_t total_size = size * nmemb;
        state->response.append(ptr, total_size);
        return total_size;
    }
    catch(...)
    {
        /** @todo log */
        return CURLE_WRITE_ERROR;
    }
}

/** Stream request */

JS::AsyncGenerator<std::string> Http::StreamRequest::GetResponseStream() const
{
    return _state->generator;
}

extern "C" size_t Http::StreamRequest::CurlWriteFunction(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept
{
    try
    {
        auto state = static_cast<Http::StreamRequest::State*>(userdata);
        size_t total_size = size * nmemb;
        state->generator.Feed(std::string(ptr, total_size));
        return total_size;
    }
    catch(...)
    {
        /** @todo log */
        return CURLE_WRITE_ERROR;
    }
}

/** Client */

std::shared_ptr<Http::Client> Http::Client::Create(Tev& tev)
{
    /** The constructor is invisible to std::make_shared */
    return std::shared_ptr<Http::Client>(new Client(tev));
}

Http::Client::Client(Tev& tev)
    : _tev(tev)
{
    _curlm = std::make_unique<CurlM>();
    _curlm->SetOpt(CURLMOPT_SOCKETDATA, static_cast<void*>(this));
    _curlm->SetOpt(CURLMOPT_SOCKETFUNCTION, &Client::CurlSocketFunction);
    _curlm->SetOpt(CURLMOPT_TIMERDATA, static_cast<void*>(this));
    _curlm->SetOpt(CURLMOPT_TIMERFUNCTION, &Client::CurlTimeoutFunction);
}

Http::Client::~Client()
{
    _curlTimeout.Clear();
    for (auto&& request : _requests)
    {
        request.second.second._state->promise.Reject("Http client closed");
    }
    for (auto&& streamRequest : _streamRequests)
    {
        streamRequest.second.second._state->generator.Reject("Http client closed");
    }
}

Http::Request Http::Client::MakeRequest(Http::Method method, const Http::RequestData& data)
{
    Http::Request request{};
    auto curl = std::make_shared<Curl>();
    curl->SetOpt(CURLOPT_URL, data.url.c_str());
    if (method == Http::Method::POST)
    {
        curl->SetOpt(CURLOPT_POST, 1L);
        curl->SetBody(data.body);
    }
    curl->SetHeaders(data.headers);
    /** @todo set more request options */
    /** Write directly to request._state's raw pointer */
    curl->SetOpt(CURLOPT_WRITEDATA, request._state.get());
    curl->SetOpt(CURLOPT_WRITEFUNCTION, &Request::CurlWriteFunction);

    request._state->curl = curl;
    _curlm->AddCurl(*curl);
    _requests[curl->Get()] = {curl, request};
    return request;
}

void Http::Client::CancelRequest(Http::Request& request)
{
    auto curl = request._state->curl.lock();
    if (!curl)
    {
        /** Request is invalid */
        return;
    }
    auto item = _requests.find(curl->Get());
    if (item == _requests.end())
    {
        request._state->promise.Reject("Request not found");
        return;
    }
    _curlm->RemoveCurl(*item->second.first);
    _requests.erase(item);
    request._state->promise.Reject(std::make_exception_ptr(RequestCancelledException()));
}

Http::StreamRequest Http::Client::MakeStreamRequest(Http::Method method, const Http::RequestData& data)
{
    Http::StreamRequest request{};
    auto curl = std::make_shared<Curl>();
    curl->SetOpt(CURLOPT_URL, data.url.c_str());
    if (method == Http::Method::POST)
    {
        curl->SetOpt(CURLOPT_POST, 1L);
        curl->SetBody(data.body);
    }
    curl->SetHeaders(data.headers);
    /** @todo set more request options */
    /** Write directly to request._state's raw pointer */
    curl->SetOpt(CURLOPT_WRITEDATA, request._state.get());
    curl->SetOpt(CURLOPT_WRITEFUNCTION, &StreamRequest::CurlWriteFunction);

    request._state->curl = curl;
    _curlm->AddCurl(*curl);
    _streamRequests[curl->Get()] = {curl, request};
    return request;
}

void Http::Client::CancelRequest(Http::StreamRequest& request)
{
    auto curl = request._state->curl.lock();
    if (!curl)
    {
        /** Request is invalid */
        return;
    }
    auto item = _streamRequests.find(curl->Get());
    if (item == _streamRequests.end())
    {
        request._state->generator.Reject("Request not found");
        return;
    }
    _curlm->RemoveCurl(*item->second.first);
    _streamRequests.erase(item);
    request._state->generator.Reject(std::make_exception_ptr(RequestCancelledException()));
}

extern "C" int Http::Client::CurlSocketFunction(CURL*, curl_socket_t s, int what, void* clientp, void*) noexcept
{
    Client* client = static_cast<Client*>(clientp);
    try
    {
        /**
         * Use insert_or_assign to avoid the old handler being cleared.
         * Is should just be an update. Which will be the case if the handler is move assigned.
         */
        switch (what)
        {
        case CURL_POLL_IN:
            client->_readHandlers.insert_or_assign(s, client->_tev.SetReadHandler(s, 
                std::bind(&Client::SocketActionHandler, client, s, 0)));
            client->_writeHandlers.erase(s);
            break;
        case CURL_POLL_OUT:
            client->_readHandlers.erase(s);
            client->_writeHandlers.insert_or_assign(s, client->_tev.SetWriteHandler(s, 
                std::bind(&Client::SocketActionHandler, client, s, 0)));
            break;
        case CURL_POLL_INOUT:
            client->_readHandlers.insert_or_assign(s, client->_tev.SetReadHandler(s, 
                std::bind(&Client::SocketActionHandler, client, s, 0)));
            client->_writeHandlers.insert_or_assign(s, client->_tev.SetWriteHandler(s, 
                std::bind(&Client::SocketActionHandler, client, s, 0)));
            break;
        case CURL_POLL_REMOVE:
            client->_readHandlers.erase(s);
            client->_writeHandlers.erase(s);
            break;
        }
    }
    catch(const std::exception& e)
    {
        return -1;
    }
    return 0;
}

extern "C" int Http::Client::CurlTimeoutFunction(CURLM*, long timeout_ms, void* clientp) noexcept
{
    Client* client = static_cast<Client*>(clientp);
    try
    {
        client->_curlTimeout.Clear();
        if (timeout_ms >= 0)
        {
            client->_curlTimeout = client->_tev.SetTimeout(
                std::bind(&Client::SocketActionHandler, client, CURL_SOCKET_TIMEOUT, CURL_SOCKET_TIMEOUT),
                timeout_ms);
        }
        return 0;
    }
    catch(...)
    {
        /** @todo log */
        client->_curlTimeout.Clear();
        return -1;
    }
}

void Http::Client::SocketActionHandler(int fd, int events)
{
    /** 
     * This function access content of this after calling a callback (promise resolve/reject)
     * The callback may destroy this object, so we need to keep a shared_ptr to it. 
     */
    auto self = shared_from_this();
    _curlm->SocketAction(fd, events);
    CURLMsg* msg = nullptr;
    while((msg = _curlm->InfoRead()) != nullptr)
    {
        switch (msg->msg)
        {
        case CURLMSG_DONE: {
            {
                /** Find the Curl and request first */
                auto item = _requests.find(msg->easy_handle);
                if (item != _requests.end())
                {
                    auto [curl, request] = std::move(item->second);
                    _requests.erase(item);
                    _curlm->RemoveCurl(*curl);
                    if (msg->data.result != CURLE_OK)
                    {
                        
                        request._state->promise.Reject(std::string(curl_easy_strerror(msg->data.result)));
                        break;
                    }
                    /** Get the http code */
                    auto httpCode = curl->GetInfo<CURLINFO_HTTP_CODE>();
                    if (httpCode < 200 || httpCode >= 300)
                    {
                        request._state->promise.Reject("HTTP error: " + std::to_string(httpCode));
                        break;
                    }
                    request._state->promise.Resolve(std::move(request._state->response));
                    break;
                }
            }
            {
                auto item = _streamRequests.find(msg->easy_handle);
                if (item != _streamRequests.end())
                {
                    auto [curl, streamRequest] = std::move(item->second);
                    _streamRequests.erase(item);
                    _curlm->RemoveCurl(*curl);
                    if (msg->data.result != CURLE_OK)
                    {
                        streamRequest._state->generator.Reject(std::string(curl_easy_strerror(msg->data.result)));
                        break;
                    }
                    /** Get the http code */
                    auto httpCode = curl->GetInfo<CURLINFO_HTTP_CODE>();
                    if (httpCode < 200 || httpCode >= 300)
                    {
                        streamRequest._state->generator.Reject("HTTP error: " + std::to_string(httpCode));
                        break;
                    }
                    streamRequest._state->generator.Finish();
                    break;
                }
            }
            /** Wild CURL*, manage it with a temporary Curl */
            Curl curl(msg->easy_handle);
            _curlm->RemoveCurl(curl);
        } break;
        default:
            break;
        }
    }
}
