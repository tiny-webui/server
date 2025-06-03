#include "CurlTypes.h"

using namespace TUI::Network::Http::CurlTypes;

CurlEnv::CurlEnv()
{
    auto code = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (code != CURLE_OK)
    {
        throw std::runtime_error("Curl error: " + std::string(curl_easy_strerror(code)));
    }
}

CurlEnv::~CurlEnv()
{
    curl_global_cleanup();
}

CurlSList::~CurlSList()
{
    if (_slist)
    {
        curl_slist_free_all(_slist);
    }
}

CurlSList::CurlSList(CurlSList&& other) noexcept
    : _slist(other._slist)
{
    other._slist = nullptr;
}

CurlSList& CurlSList::operator=(CurlSList&& other) noexcept
{
    if (this != &other)
    {
        if (_slist)
        {
            curl_slist_free_all(_slist);
        }
        _slist = other._slist;
        other._slist = nullptr;
    }
    return *this;
}

void CurlSList::Append(const std::string& data)
{
    auto new_slist = curl_slist_append(_slist, data.c_str());
    if (!new_slist)
    {
        throw std::runtime_error("Failed to append to CURLSList");
    }
    _slist = new_slist;
}

struct curl_slist* CurlSList::Get() const
{
    return _slist;
}

Curl::Curl()
{
    _curl = curl_easy_init();
    if (!_curl)
    {
        throw std::runtime_error("Failed to initialize CURL");
    }
}

Curl::Curl(CURL* curl)
{
    if (!curl)
    {
        throw std::invalid_argument("CURL pointer cannot be null");
    }
    _curl = curl;
}

Curl::~Curl()
{
    if (_curl)
    {
        curl_easy_cleanup(_curl);
    }
}

Curl::Curl(Curl&& other) noexcept
    : _curl(other._curl), _body(std::move(other._body)), _headers(std::move(other._headers))
{
    other._curl = nullptr;
}

Curl& Curl::operator=(Curl&& other) noexcept
{
    if (this != &other)
    {
        if (_curl)
        {
            curl_easy_cleanup(_curl);
        }
        _curl = other._curl;
        _body = std::move(other._body);
        _headers = std::move(other._headers);
        other._curl = nullptr;
    }
    return *this;
}

CURL* Curl::Get() const
{
    return _curl;
}

void Curl::SetBody(const std::string& body)
{
    /** Keep body alive during the request */
    _body = body;
    SetOpt(CURLOPT_POSTFIELDS, _body.c_str());
}

void Curl::SetHeaders(const std::map<std::string, std::string>& headers_map)
{
    if (_headers)
    {
        throw std::runtime_error("Headers already set for this CURL instance");
    }
    if (headers_map.empty())
    {
        return;
    }
    auto headers = std::make_unique<CurlSList>();
    for (const auto& header : headers_map)
    {
        headers->Append(header.first + ": " + header.second);
    }
    /** Keep headers alive during the request */
    _headers = std::move(headers);
    SetOpt(CURLOPT_HTTPHEADER, _headers->Get());
}

CurlM::CurlM()
{
    _curlm = curl_multi_init();
    if (!_curlm)
    {
        throw std::runtime_error("Failed to initialize CURLM");
    }
}

CurlM::~CurlM()
{
    if (_curlm)
    {
        curl_multi_cleanup(_curlm);
    }
}

CurlM::CurlM(CurlM&& other) noexcept
    : _curlm(other._curlm)
{
    other._curlm = nullptr;
}

CurlM& CurlM::operator=(CurlM&& other) noexcept
{
    if (this != &other)
    {
        if (_curlm)
        {
            curl_multi_cleanup(_curlm);
        }
        _curlm = other._curlm;
        other._curlm = nullptr;
    }
    return *this;
}

CURLM* CurlM::Get() const
{
    return _curlm;
}

void CurlM::AddCurl(const Curl& curl)
{
    auto code = curl_multi_add_handle(_curlm, curl.Get());
    if (code != CURLM_OK)
    {
        throw std::runtime_error("CURL Multi error: " + std::string(curl_multi_strerror(code)));
    }
}

void CurlM::RemoveCurl(const Curl& curl)
{
    auto code = curl_multi_remove_handle(_curlm, curl.Get());
    if (code != CURLM_OK)
    {
        throw std::runtime_error("CURL Multi error: " + std::string(curl_multi_strerror(code)));
    }
}

void CurlM::SocketAction(int fd, int events)
{
    int running_handles = 0;
    auto code = curl_multi_socket_action(_curlm, fd, events, &running_handles);
    if (code != CURLM_OK)
    {
        throw std::runtime_error("CURL Multi error: " + std::string(curl_multi_strerror(code)));
    }
}

CURLMsg* CurlM::InfoRead()
{
    int msgs_left = 0;
    return curl_multi_info_read(_curlm, &msgs_left);
}
