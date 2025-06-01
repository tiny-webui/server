#include "LwsTypes.h"
#include <string>
#include <stdexcept>

using namespace TUI::Network::WebSocket::LwsTypes;

/** Context */

Context::Context(const struct lws_context_creation_info& info)
{
    _context = lws_create_context(&info);
    if (!_context)
    {
        throw std::runtime_error("Failed to create LWS context");
    }
}

Context::~Context()
{
    if (_context)
    {
        lws_context_destroy(_context);
    }
}

Context::Context(Context&& other) noexcept
    : _context(other._context)
{
    other._context = nullptr;
}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        if (_context)
        {
            lws_context_destroy(_context);
        }
        _context = other._context;
        other._context = nullptr;
    }
    return *this;
}

struct lws_context* Context::Get() const
{
    return _context;
}

void Context::CancelService() const
{
    if (!_context)
    {
        throw std::runtime_error("LWS context is not initialized");
    }
    lws_cancel_service(_context);
}
