#pragma once

#include <libwebsockets.h>

namespace TUI::Network::WebSocket::LwsTypes
{
    class Context
    {
    public:
        Context(const struct lws_context_creation_info& info);
        ~Context();

        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&) noexcept;
        Context& operator=(Context&&) noexcept; 

        struct lws_context* Get() const;
        void CancelService() const;
    private:
        struct lws_context* _context{nullptr};
    };
}
