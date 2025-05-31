#pragma once

#include <string>
#include <map>
#include <memory>
#include <curl/curl.h>

template<CURLINFO info> struct CurlInfoType;
template<> struct CurlInfoType<CURLINFO_RESPONSE_CODE> { using type = long; };

namespace TUI::Network::Http::CurlTypes
{
    class CurlEnv
    {
    public:
        CurlEnv();
        ~CurlEnv();
    };

    class CurlSList
    {
    public:
        CurlSList() = default;
        ~CurlSList();
        void Append(const std::string& data);
        struct curl_slist* Get() const;
    private:
        struct curl_slist* _slist{nullptr};
    };

    class Curl
    {
    public:
        Curl();
        Curl(CURL* curl);
        ~Curl();
        CURL* Get() const;

        template<typename... Args>
        void SetOpt(CURLoption option, Args&&... args)
        {
            auto code = curl_easy_setopt(_curl, option, std::forward<Args>(args)...);
            if (code != CURLE_OK)
            {
                std::string errorMsg = "CURL error: " + std::string(curl_easy_strerror(code));
                throw std::runtime_error(errorMsg);
            }
        }

        template<CURLINFO info>
        auto GetInfo() -> typename CurlInfoType<info>::type
        {
            using T = typename CurlInfoType<info>::type;
            T value{};
            auto code = curl_easy_getinfo(_curl, info, &value);
            if (code != CURLE_OK)
            {
                std::string errorMsg = "CURL getinfo error: " + std::string(curl_easy_strerror(code));
                throw std::runtime_error(errorMsg);
            }
            return value;
        }

        void SetBody(const std::string& body);
        void SetHeaders(const std::map<std::string, std::string>& headers);
    private:
        CURL* _curl{nullptr};
        std::string _body{};
        std::unique_ptr<CurlSList> _headers{nullptr};
    };

    class CurlM
    {
    public:
        CurlM();
        ~CurlM();
        CURLM* Get() const;

        template<typename... Args>
        void SetOpt(CURLMoption option, Args&&... args)
        {
            auto code = curl_multi_setopt(_curlm, option, std::forward<Args>(args)...);
            if (code != CURLM_OK)
            {
                std::string errorMsg = "CURL Multi error: " + std::string(curl_multi_strerror(code));
                throw std::runtime_error(errorMsg);
            }
        }

        void AddCurl(const Curl& curl);
        void RemoveCurl(const Curl& curl);
        void SocketAction(int fd, int events);
        CURLMsg* InfoRead();
    private:
        CURLM* _curlm{nullptr};
    };
}
