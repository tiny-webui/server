#include "WebSocketServer.h"

#include <cstring>

using namespace TUI::Network;
using namespace TUI::Network::WebSocket;

/** Connection */

Server::Connection::Connection(std::unique_ptr<TwsTypes::TwsConnection> twsConnection)
    : _twsConnection(std::move(twsConnection))
{
    _twsConnection->SetOnMessageCallback([this](bool isBinary, const void* data, size_t size) {
        if (!isBinary)
        {
            /** Text messages are ignored */
            return;
        }
        std::vector<std::uint8_t> message(size);
        std::memcpy(message.data(), data, size);
        _receiveGenerator.Feed(std::move(message));
    });
    _twsConnection->SetOnClosedCallback([this](tws_close_reason_t reason) {
        (void)reason;
        _receiveGenerator.Finish();
    });
}

Server::Connection::~Connection()
{
    try
    {
        Close();
    }
    catch(...)
    {
        /** Ignored */
    }
}

void Server::Connection::Close()
{
    if (IsClosed())
    {
        return;
    }
    /** This will not trigger the on closed callback */
    _twsConnection.reset();
    _receiveGenerator.Finish();
}

bool Server::Connection::IsClosed() const noexcept
{
    return _twsConnection == nullptr || *_twsConnection == nullptr;
}

void Server::Connection::Send(std::vector<std::uint8_t> message)
{
    _twsConnection->SendMessage(std::move(message));
}

JS::Promise<std::optional<std::vector<std::uint8_t>>> Server::Connection::ReceiveAsync()
{
    return _receiveGenerator.NextAsync();
}

/** Server */

std::shared_ptr<IServer<void>> Server::Create(Tev& tev, const std::string& address, int port)
{
    return std::shared_ptr<Server>(new Server(tev, address, port));
}

std::shared_ptr<IServer<void>> Server::Create(Tev& tev, const std::string& unixSocketPath)
{
    return std::shared_ptr<Server>(new Server(tev, unixSocketPath, -1, true));
}

Server::Server(Tev& tev, const std::string& address, int port, bool addressIsUds)
    : _tws(std::make_unique<TwsTypes::Tws>())
{
    if (addressIsUds)
    {
        _tws->ConfigSetAddressFamily(tws_address_family_t::TWS_AF_UNIX);
        _tws->ConfigSetHost(address);
    }
    else
    {
        _tws->ConfigSetAddressFamily(tws_address_family_t::TWS_AF_INET);
        _tws->ConfigSetHost(address);
        _tws->ConfigSetPort(static_cast<uint16_t>(port));
    }

    TwsTypes::EventLoopCallbacks callbacks{};
    auto readHandlers = std::make_shared<std::map<int, Tev::FdHandler>>();
    callbacks.setReadHandler = [&tev, readHandlers](int fd, const std::function<void()>& handler) {
        if (handler == nullptr)
        {
            readHandlers->erase(fd);
            return;
        }
        auto handle = tev.SetReadHandler(fd, handler);
        (*readHandlers)[fd] = std::move(handle);
    };
    auto writeHandlers = std::make_shared<std::map<int, Tev::FdHandler>>();
    callbacks.setWriteHandler = [&tev, writeHandlers](int fd, const std::function<void()>& handler) {
        if (handler == nullptr)
        {
            writeHandlers->erase(fd);
            return;
        }
        auto handle = tev.SetWriteHandler(fd, handler);
        (*writeHandlers)[fd] = std::move(handle);
    };
    callbacks.setTimeout = [&tev](uint64_t timeoutMs, const std::function<void()>& handler) -> tws_timeout_handle_t {
        /** Entrust tws to handle the lifecycle of the timeout. */
        Tev::Timeout* handle = new Tev::Timeout(tev.SetTimeout(handler, timeoutMs));
        tws_timeout_handle_t twsHandle{};
        twsHandle.ptr = handle;
        return twsHandle;
    };
    callbacks.clearTimeout = [&tev](tws_timeout_handle_t timeoutHandle) {
        Tev::Timeout* handle = static_cast<Tev::Timeout*>(timeoutHandle.ptr);
        if (handle != nullptr)
        {
            delete handle;
        }
    };
    _tws->SetEventLoopCallbacks(callbacks);

    _tws->SetOnConnectionCallback([this](std::unique_ptr<TwsTypes::TwsConnection> twsConnection) {
        auto connection = std::shared_ptr<Connection>(new Connection(std::move(twsConnection)));
        this->_connectionGenerator.Feed(std::move(connection));
    });
    _tws->SetOnClosedCallback([this](tws_close_reason_t reason) {
        (void)reason;
        this->_connectionGenerator.Finish();
    });

    _tws->Open();
}

Server::~Server()
{
    try
    {
        Close();
    }
    catch(...)
    {
        /** Ignored */
    }
}

void Server::Close()
{
    if (IsClosed())
    {
        return;
    }
    /** This will not trigger the onClosed callback */
    _tws.reset();
    _connectionGenerator.Finish();
}

bool Server::IsClosed() const noexcept
{
    return _tws == nullptr || *_tws == nullptr;
}

JS::Promise<std::optional<std::shared_ptr<IConnection<void>>>> Server::AcceptAsync()
{
    return _connectionGenerator.NextAsync();
}
