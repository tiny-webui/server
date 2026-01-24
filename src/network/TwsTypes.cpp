#include "TwsTypes.h"

#include <stdexcept>

using namespace TUI::Network::WebSocket::TwsTypes;

namespace
{

extern "C"
int OnSetReadHandlerBridge(int fd, void (*handler)(void*), void* handler_ctx, tws_ctx_t ctx) noexcept
{
    EventLoopCallbacks* eventLoop = static_cast<EventLoopCallbacks*>(ctx.ptr);
    if (eventLoop->setReadHandler)
    {
        try
        {
            if (handler == nullptr)
            {
                eventLoop->setReadHandler(fd, nullptr);
                return 0;
            }
            eventLoop->setReadHandler(fd, [handler, handler_ctx]() {
                if (handler)
                {
                    handler(handler_ctx);
                }
            });
        }
        catch(...)
        {
            return -1;
        }
        return 0;
    }
    return -1;
}

extern "C"
int OnSetWriteHandlerBridge(int fd, void (*handler)(void*), void* handler_ctx, tws_ctx_t ctx) noexcept
{
    EventLoopCallbacks* eventLoop = static_cast<EventLoopCallbacks*>(ctx.ptr);
    if (eventLoop->setWriteHandler)
    {
        try
        {
            if (handler == nullptr)
            {
                eventLoop->setWriteHandler(fd, nullptr);
                return 0;
            }
            eventLoop->setWriteHandler(fd, [handler, handler_ctx]() {
                if (handler)
                {
                    handler(handler_ctx);
                }
            });
        }
        catch(...)
        {
            return -1;
        }
        return 0;
    }
    return -1;
}

extern "C"
tws_timeout_handle_t OnSetTimeoutBridge(uint64_t timeout_ms, void (*handler)(void*), void* handler_ctx, tws_ctx_t ctx) noexcept
{
    EventLoopCallbacks* eventLoop = static_cast<EventLoopCallbacks*>(ctx.ptr);
    if (eventLoop->setTimeout)
    {
        try
        {
            return eventLoop->setTimeout(timeout_ms, [handler, handler_ctx]() {
                if (handler)
                {
                    handler(handler_ctx);
                }
            });
        }
        catch(...)
        {
            tws_timeout_handle_t nullHandle{};
            return nullHandle;
        }
    }
    tws_timeout_handle_t nullHandle{};
    return nullHandle;
}

extern "C"
void OnClearTimeoutBridge(tws_timeout_handle_t timeout_handle, tws_ctx_t ctx) noexcept
{
    EventLoopCallbacks* eventLoop = static_cast<EventLoopCallbacks*>(ctx.ptr);
    if (eventLoop->clearTimeout)
    {
        try
        {
            eventLoop->clearTimeout(timeout_handle);
        }
        catch(...)
        {
            /** Ignored */
        }
    }
}

}

Tws::Tws()
    : _handle(tws_create())
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("Failed to create TWS instance");
    }
    tws_ctx_t ctx{};
    ctx.ptr = this;
    int rc = tws_set_callbacks_ctx(_handle, ctx);
    if (rc != 0)
    {
        tws_close(_handle);
        throw std::runtime_error("Failed to set callbacks context");
    }
    rc = tws_set_on_closed_callback(_handle, &Tws::OnClosedBridge);
    if (rc != 0)
    {
        tws_close(_handle);
        throw std::runtime_error("Failed to set on_closed callback");
    }
}

Tws::~Tws()
{
    if (_handle != nullptr)
    {
        tws_close(_handle);
    }
}

void Tws::ConfigAppendSubprotocol(const std::string& subprotocol)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS instance is closed");
    }
    int rc = tws_config_append_subprotocol(_handle, subprotocol.c_str());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to append subprotocol");
    }
}

void Tws::ConfigSetAddressFamily(tws_address_family_t address_family)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS instance is closed");
    }
    int rc = tws_config_set_address_family(_handle, address_family);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set address family");
    }
}

void Tws::ConfigSetHost(const std::string& host)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS instance is closed");
    }
    int rc = tws_config_set_host(_handle, host.c_str());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set host");
    }
}

void Tws::ConfigSetPort(uint16_t port)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS instance is closed");
    }
    int rc = tws_config_set_port(_handle, port);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set port");
    }
}

void Tws::ConfigSetPath(const std::string& path)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS instance is closed");
    }
    int rc = tws_config_set_path(_handle, path.c_str());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set path");
    }
}

void Tws::SetEventLoopCallbacks(const EventLoopCallbacks& callbacks)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS instance is closed");
    }
    _eventLoopCallbacks = std::make_shared<EventLoopCallbacks>(callbacks);
    tws_ctx_t ctx{};
    ctx.ptr = _eventLoopCallbacks.get();
    int rc = tws_set_event_loop_ctx(_handle, ctx);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set event loop context");
    }
    rc = tws_set_on_set_read_handler_callback(_handle, &OnSetReadHandlerBridge);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set on_set_read_handler callback");
    }
    rc = tws_set_on_set_write_handler_callback(_handle, &OnSetWriteHandlerBridge);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set on_set_write_handler callback");
    }
    rc = tws_set_on_set_timeout_callback(_handle, &OnSetTimeoutBridge);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set on_set_timeout callback");
    }
    rc = tws_set_on_clear_timeout_callback(_handle, &OnClearTimeoutBridge);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set on_clear_timeout callback");
    }
}

void Tws::SetOnClosedCallback(const std::function<void(tws_close_reason_t)>& callback)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS instance is closed");
    }
    _onClosedCallback = callback;
}

extern "C"
void Tws::OnClosedBridge(tws_close_reason_t reason, tws_ctx_t ctx) noexcept
{
    Tws* self = static_cast<Tws*>(ctx.ptr);
    self->_handle = nullptr;
    /** Block closed by close reason to avoid calling the callback in destructor. */
    if (self->_onClosedCallback && reason != TWS_CLOSED_BY_CLOSE)
    {
        try
        {
            self->_onClosedCallback(reason);
        }
        catch(...)
        {
            /** Ignored */
        }
    }
}

void Tws::SetOnConnectionCallback(const std::function<void(std::unique_ptr<TwsConnection>)>& callback)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS instance is closed");
    }
    _onConnectionCallback = callback;
    int rc = tws_set_on_connection_callback(_handle, &Tws::OnConnectionBridge);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set on_connection callback");
    }
}

extern "C"
void Tws::OnConnectionBridge(tws_conn_handle_t conn_handle, tws_ctx_t ctx) noexcept
{
    Tws* self = static_cast<Tws*>(ctx.ptr);
    if (self->_onConnectionCallback)
    {
        try
        {
            auto connection = std::unique_ptr<TwsConnection>(new TwsConnection(conn_handle, self->_eventLoopCallbacks));
            self->_onConnectionCallback(std::move(connection));
        }
        catch(...)
        {
            /** Ignored */
        }
    }
}

void Tws::Open()
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS instance not created");
    }
    int rc = tws_open(_handle);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to open TWS instance");
    }
}

bool Tws::operator==(std::nullptr_t) const noexcept
{
    return _handle == nullptr;
}

/** Tws Connection */

TwsConnection::TwsConnection(tws_conn_handle_t handle, std::shared_ptr<EventLoopCallbacks> eventLoopCallbacks)
    : _handle(handle), _eventLoopCallbacks(eventLoopCallbacks)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("Invalid TWS connection handle");
    }
    if (_eventLoopCallbacks == nullptr)
    {
        throw std::runtime_error("Event loop callbacks not set for TWS connection");
    }
    tws_ctx_t ctx{};
    ctx.ptr = this;
    int rc = tws_conn_set_callback_ctx(_handle, ctx);
    if (rc != 0)
    {
        tws_conn_close(_handle);
        throw std::runtime_error("Failed to set connection callbacks context");
    }
    rc = tws_conn_set_on_closed_callback(_handle, &TwsConnection::OnClosedBridge);
    if (rc != 0)
    {
        tws_conn_close(_handle);
        throw std::runtime_error("Failed to set connection on_closed callback");
    }
}

TwsConnection::~TwsConnection()
{
    if (_handle != nullptr)
    {
        tws_conn_close(_handle);
    }
}

std::string TwsConnection::GetSubProtocol()
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS connection is closed");
    }
    const char* subprotocol = tws_conn_get_subprotocol(_handle);
    if (subprotocol == nullptr)
    {
        return "";
    }
    return std::string(subprotocol);
}

void TwsConnection::SetOnMessageCallback(const std::function<void(bool, const void*, size_t)>& callback)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS connection is closed");
    }
    _onMessageCallback = callback;
    int rc = tws_conn_set_on_message_callback(_handle, &TwsConnection::OnMessageBridge);
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set on_message callback");
    }
}

extern "C"
void TwsConnection::OnMessageBridge(bool is_binary, const void* data, size_t data_size, tws_ctx_t ctx) noexcept
{
    TwsConnection* self = static_cast<TwsConnection*>(ctx.ptr);
    if (self->_onMessageCallback)
    {
        try
        {
            self->_onMessageCallback(is_binary, data, data_size);
        }
        catch(...)
        {
            /** Ignored */
        }
    }
}

void TwsConnection::SetOnClosedCallback(const std::function<void(tws_close_reason_t)>& callback)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS connection is closed");
    }
    _onClosedCallback = callback;
}

extern "C"
void TwsConnection::OnClosedBridge(tws_close_reason_t reason, tws_ctx_t ctx) noexcept
{
    TwsConnection* self = static_cast<TwsConnection*>(ctx.ptr);
    self->_handle = nullptr;
    /** Block closed by close reason to avoid calling the callback in destructor. */
    if (self->_onClosedCallback && reason != TWS_CLOSED_BY_CLOSE)
    {
        try
        {
            self->_onClosedCallback(reason);
        }
        catch(...)
        {
            /** Ignored */
        }
    }
}

void TwsConnection::SendMessage(const std::string& message)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS connection is closed");
    }
    int rc = tws_conn_send(_handle, false, message.data(), message.size());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to send text message");
    }
}

void TwsConnection::SendMessage(const std::vector<uint8_t>& message)
{
    if (_handle == nullptr)
    {
        throw std::runtime_error("TWS connection is closed");
    }
    int rc = tws_conn_send(_handle, true, message.data(), message.size());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to send binary message");
    }
}

bool TwsConnection::operator==(std::nullptr_t) const noexcept
{
    return _handle == nullptr;
}
