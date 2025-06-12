#include "WebSocketServer.h"
#include <sys/eventfd.h>

using namespace TUI::Common;
using namespace TUI::Network;
using namespace TUI::Network::WebSocket;

/**
 * Libwebsockets is a pain in the a*s to integrate with a custom event loop.
 * So we just start a new thread to let it do its thing.
 * Be careful with thread safety in this file.
 * And DO NOT expose this complexity outside of this file.
 * 
 * Main thread -> lws thread
 * Use lws_cancel_service() to get a LWS_CALLBACK_EVENT_WAIT_CANCELLED callback 
 *     (Great naming. Not actually canceling anything)
 * lws thread -> main thread
 * Use eventfd to notify the event loop.
 */

/** Connection */

Server::Connection::Connection(std::uint64_t id, std::shared_ptr<Server> server)
    : _id(id), _server(server)
{
}

Server::Connection::~Connection()
{
    Close();
}

void Server::Connection::Close()
{
    auto server = _server.lock();
    if (!server)
    {
        return;
    }
    server->CloseConnection(_id);
}

void Server::Connection::Send(std::vector<std::uint8_t> message)
{
    auto server = _server.lock();
    if (!server)
    {
        return;
    }
    server->SendMessage(_id, std::move(message));
}

JS::Promise<std::optional<std::vector<std::uint8_t>>> Server::Connection::ReceiveAsync()
{
    return _receiveGenerator.NextAsync();
}

/** Server */

std::shared_ptr<Server> Server::Create(Tev& tev, const std::string& address, int port)
{
    return std::shared_ptr<Server>(new Server(tev, address, port));
}

std::shared_ptr<Server> Server::Create(Tev& tev, const std::string& unixSocketPath)
{
    return std::shared_ptr<Server>(new Server(tev, unixSocketPath, -1, true));
}

Server::Server(Tev& tev, const std::string& address, int port, bool addressIsUds)
    : _tev(tev)
{
    struct lws_protocols protocol{};
    /** Does not matter */
    protocol.name = "default";
    protocol.callback = &LwsCallback;
    protocol.per_session_data_size = sizeof(_connectionIdSeed);
    protocol.rx_buffer_size = Server::LWS_BUFFER_SIZE;
    /** Does not matter */
    protocol.id = 0;
    protocol.user = static_cast<void*>(this);
    /** 0: same as rx */
    protocol.tx_packet_size = 0;

    _protocols.push_back(protocol);
    _protocols.push_back(LWS_PROTOCOL_LIST_TERM);

    struct lws_context_creation_info info{};
    info.iface = address.c_str();
    if (!addressIsUds)
    {
        info.port = port;
    }
    info.protocols = _protocols.data();
    info.pt_serv_buf_size = LWS_BUFFER_SIZE;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;
    if (addressIsUds)
    {
        info.options |= LWS_SERVER_OPTION_UNIX_SOCK;
    }

    _context = std::make_unique<LwsTypes::Context>(info);

    _rxEventFd = Unique::Fd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
    if (_rxEventFd == -1)
    {
        throw std::runtime_error("Failed to create eventfd for tx");
    }

    _lwsThread = std::thread(std::bind(&Server::LwsThreadFunc, this));
    if (!_lwsThread.joinable())
    {
        throw std::runtime_error("Failed to create LWS thread");
    }

    _tev.SetReadHandler(_rxEventFd, std::bind(&Server::ITCRxCallback, this));
}

Server::~Server()
{
    CloseInternal();
}

void Server::Close()
{
    CloseInternal();
}

void Server::CloseInternal(bool closedByLws)
{
    if (_closed)
    {
        return;
    }
    _closed = true;
    if (_rxEventFd != -1)
    {
        _tev.SetReadHandler(_rxEventFd, nullptr);
        _rxEventFd = Unique::Fd(-1);
    }
    if (!closedByLws)
    {
        SendMessageToLwsThread(
            ITCType::CLOSE_SERVER,
            nullptr
        );
    }
    if (_lwsThread.joinable())
    {
        _lwsThread.join();
    }
    _connectionGenerator.Finish();
}

JS::Promise<std::optional<std::shared_ptr<IConnection<void>>>> Server::AcceptAsync()
{
    return _connectionGenerator.NextAsync();
}

void Server::CloseConnection(std::uint64_t id, bool closedByPeer)
{
    auto self = shared_from_this();
    auto it = _connections.find(id);
    if (it == _connections.end())
    {
        return;
    }
    auto connection = it->second;
    _connections.erase(it);
    if (!closedByPeer)
    {
        SendMessageToLwsThread(
            ITCType::DISCONNECT,
            std::make_unique<ITCDataConnectionId>(id)
        );
    }
    connection->_receiveGenerator.Finish();
}

void Server::SendMessage(std::uint64_t id, std::vector<std::uint8_t> message)
{
    auto item = _connections.find(id);
    if (item == _connections.end())
    {
        return;
    }
    SendMessageToLwsThread(
        ITCType::SEND_MESSAGE,
        std::make_unique<ITCDataMessage>(id, std::move(message))
    );
}

void Server::ITCRxCallback()
{
    auto self = shared_from_this();
    /** Read from eventfd to clear the event */
    eventfd_t value;
    eventfd_read(_rxEventFd, &value);
    while (true)
    {
        struct ITCMessage msg;
        {
            std::lock_guard<std::mutex> lock(_rxMutex);
            if (_rxQueue.empty())
            {
                break;
            }
            msg = std::move(_rxQueue.front());
            _rxQueue.pop_front();
        }
        switch (msg.type)
        {
        case ITCType::CONNECTION_ACCEPTED:{
            auto data = std::unique_ptr<ITCDataConnectionId>(
                static_cast<ITCDataConnectionId*>(msg.data.release())
            );
            auto connection = std::shared_ptr<Connection>(new Connection(data->id, shared_from_this()));
            _connections[data->id] = connection;
            _connectionGenerator.Feed(connection);
        } break;
        case ITCType::CONNECTION_DISCONNECTED:{
            auto data = std::unique_ptr<ITCDataConnectionDisconnected>(
                static_cast<ITCDataConnectionDisconnected*>(msg.data.release())
            );
            CloseConnection(data->id, true);
        } break;
        case ITCType::MESSAGE_RECEIVED:{
            auto data = std::unique_ptr<ITCDataMessage>(
                static_cast<ITCDataMessage*>(msg.data.release())
            );
            auto it = _connections.find(data->id);
            if (it == _connections.end())
            {
                break;
            }
            it->second->_receiveGenerator.Feed(std::move(data->message));
        } break;
        case ITCType::SERVER_CLOSED: {
            CloseInternal(true);
            /** DO NOT handle any more messages */
            return;
        } break;
        default:
            break;
        }
    }
}

void Server::SendMessageToLwsThread(ITCType type, std::unique_ptr<IITCData> data)
{
    std::lock_guard<std::mutex> lock(_txMutex);
    _txQueue.push_back(ITCMessage{type, std::move(data)});
    _context->CancelService();
}

/** Danger zone. Functions in another thread */

void Server::LwsThreadFunc()
{
    int rc = 0;
    while (rc >= 0 && !_lwsThreadShouldExit)
    {
        rc = lws_service(_context->Get(), 0);
    }
    if (rc < 0)
    {
        SendMessageToMainThread(
            ITCType::SERVER_CLOSED,
            nullptr
        );
    }
}

extern "C" int Server::LwsCallback(
    struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) noexcept
{
    (void)in;
    (void)len;

    try
    {
        if (!wsi)
        {
            return 0;
        }
        auto protocol = lws_get_protocol(wsi);
        if (!protocol)
        {
            return 0;
        }
        auto server = static_cast<Server*>(protocol->user);
        /** Be careful, this can be nullptr */
        auto pId = static_cast<std::uint64_t*>(user);

        switch (reason)
        {
        case LWS_CALLBACK_EVENT_WAIT_CANCELLED:{
            /** wsi and server exists for this callback. */
            while (true)
            {
                ITCMessage msg;
                {
                    std::lock_guard<std::mutex> lock(server->_txMutex);
                    if (server->_txQueue.empty())
                    {
                        break;
                    }
                    msg = std::move(server->_txQueue.front());
                    server->_txQueue.pop_front();
                }
                switch (msg.type)
                {
                case ITCType::SEND_MESSAGE: {
                    auto data = std::unique_ptr<ITCDataMessage>(
                        static_cast<ITCDataMessage*>(msg.data.release())
                    );
                    auto it = server->_lwsConnections.find(data->id);
                    if (it == server->_lwsConnections.end())
                    {
                        break;
                    }
                    auto& connection = it->second;
                    connection->txQueue.push(std::move(data->message));
                    lws_callback_on_writable(connection->wsi);
                } break;
                case ITCType::DISCONNECT: {
                    auto data = std::unique_ptr<ITCDataConnectionId>(
                        static_cast<ITCDataConnectionId*>(msg.data.release())
                    );
                    auto it = server->_lwsConnections.find(data->id);
                    if (it == server->_lwsConnections.end())
                    {
                        /** This is normal if this is a redundant message */
                        break;
                    }
                    auto connection = std::move(it->second);
                    server->_lwsConnections.erase(it);
                    lws_close_reason(it->second->wsi, LWS_CLOSE_STATUS_NORMAL, nullptr, 0);
                } break;
                case ITCType::CLOSE_SERVER: {
                    /** Signal the server thread to exit */
                    server->_lwsThreadShouldExit = true;
                } break;
                default:
                    break;
                }
            }
        } break;
        case LWS_CALLBACK_ESTABLISHED: {
            if (!pId)
            {
                throw std::runtime_error("LWS_CALLBACK_ESTABLISHED without user data");
            }
            auto connectionId = server->_connectionIdSeed++;
            *pId = connectionId;
            auto connection = std::make_unique<LwsConnection>(connectionId, wsi);
            server->SendMessageToMainThread(
                ITCType::CONNECTION_ACCEPTED,
                std::make_unique<ITCDataConnectionId>(connectionId)
            );
            server->_lwsConnections[connectionId] = std::move(connection);
        } break;
        case LWS_CALLBACK_CLOSED: {
            if (!pId)
            {
                throw std::runtime_error("LWS_CALLBACK_CLOSED without user data");
            }
            auto connectionId = *pId;
            auto it = server->_lwsConnections.find(connectionId);
            if (it != server->_lwsConnections.end())
            {
                server->_lwsConnections.erase(it);
                server->SendMessageToMainThread(
                    ITCType::CONNECTION_DISCONNECTED,
                    std::make_unique<ITCDataConnectionDisconnected>(connectionId, "Connection closed")
                );
            }
        } break;
        case LWS_CALLBACK_RECEIVE: {
            if (!pId)
            {
                throw std::runtime_error("LWS_CALLBACK_RECEIVE without user data");
            }
            auto id = *pId;
            auto it = server->_lwsConnections.find(id);
            if (it == server->_lwsConnections.end())
            {
                lws_close_reason(it->second->wsi, LWS_CLOSE_STATUS_NORMAL, nullptr, 0);
                return -1;
            }
            if (!lws_frame_is_binary(wsi))
            {
                /** We only support binary messages */
                break;
            }
            auto& connection = it->second;
            connection->rxBuffer.insert(
                connection->rxBuffer.end(),
                static_cast<const std::uint8_t*>(in),
                static_cast<const std::uint8_t*>(in) + len
            );
            if (lws_is_final_fragment(wsi))
            {
                server->SendMessageToMainThread(
                    ITCType::MESSAGE_RECEIVED,
                    std::make_unique<ITCDataMessage>(id, std::move(connection->rxBuffer))
                );
                connection->rxBuffer.clear();
            }
        } break;
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            if (!pId)
            {
                throw std::runtime_error("LWS_CALLBACK_SERVER_WRITEABLE without user data");
            }
            auto id = *pId;
            auto it = server->_lwsConnections.find(id);
            if (it == server->_lwsConnections.end())
            {
                break;
            }
            while (!it->second->txQueue.empty())
            {
                auto message = std::move(it->second->txQueue.front());
                it->second->txQueue.pop();
                /** Prepend LWS_PRE bytes to the message */
                message.insert(message.begin(), LWS_PRE, 0);
                /** We only transmit binary data */
                int written = lws_write(
                    wsi,
                    message.data() + LWS_PRE,
                    message.size() - LWS_PRE,
                    LWS_WRITE_BINARY);
                if (written < static_cast<int>(message.size()) - LWS_PRE)
                {
                    return -1;
                }
            }
        } break;
        default:
            break;
        }
    }
    catch(...)
    {
        /** @todo log */
        return -1;
    }
    return 0;
}

void Server::SendMessageToMainThread(ITCType type, std::unique_ptr<IITCData> data)
{
    std::lock_guard<std::mutex> lock(_rxMutex);
    _rxQueue.push_back(ITCMessage{type, std::move(data)});
    if (_rxEventFd != -1)
    {
        eventfd_write(_rxEventFd, 1);
    }
}
