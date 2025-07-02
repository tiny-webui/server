#pragma once

#include <list>
#include <memory>
#include <unordered_map>
#include <functional>
#include <js-style-co-routine/AsyncGenerator.h>
#include <js-style-co-routine/Promise.h>
#include <nlohmann/json.hpp>
#include "network/IServer.h"
#include "schema/IServer.h"
#include "schema/Rpc.h"

namespace TUI::Rpc
{
    template <typename Identity>
    class RpcServer
    {
    public:
        using RequestHandler = std::function<JS::Promise<nlohmann::json>(Identity, nlohmann::json)>;
        using StreamRequestHandler = std::function<JS::AsyncGenerator<nlohmann::json, nlohmann::json>(Identity, nlohmann::json)>;
        using NotificationHandler = std::function<void(Identity, nlohmann::json)>;
        using NewConnectionHandler = std::function<void(Identity)>;
        using ConnectionClosedHandler = std::function<void(Identity)>;
        using CriticalErrorHandler = std::function<void(const std::string&)>;
        /**
         * @brief Construct a new Rpc Server object
         * 
         * @param server The rpc server will take ownership of the server object
         * @param requestHandlers 
         * @param streamRequestHandlers 
         * @param newConnectionHandler 
         * @param connectionClosedHandler This will not be called if the connection is closed by the user
         * @param criticalErrorHandler 
         */
        RpcServer(
            std::shared_ptr<Network::IServer<Identity>> server,
            std::unordered_map<std::string, RequestHandler> requestHandlers,
            std::unordered_map<std::string, StreamRequestHandler> streamRequestHandlers,
            std::unordered_map<std::string, NotificationHandler> notificationHandlers,
            NewConnectionHandler newConnectionHandler,
            ConnectionClosedHandler connectionClosedHandler,
            CriticalErrorHandler criticalErrorHandler)
            : _server(std::move(server)),
              _requestHandlers(std::move(requestHandlers)),
              _streamRequestHandlers(std::move(streamRequestHandlers)),
              _notificationHandlers(std::move(notificationHandlers)),
              _newConnectionHandler(std::move(newConnectionHandler)),
              _connectionClosedHandler(std::move(connectionClosedHandler)),
              _criticalErrorHandler(std::move(criticalErrorHandler))
        {
            if (_criticalErrorHandler == nullptr)
            {
                throw std::invalid_argument("Critical error handler cannot be null");
            }
            /** Fire the server async handler */
            HandleServerAsync();
        }

        RpcServer(const RpcServer&) = delete;
        RpcServer& operator=(const RpcServer&) = delete;
        RpcServer(RpcServer&&) = delete;
        RpcServer& operator=(RpcServer&&) = delete;

        ~RpcServer()
        {
            Close();
        }

        void CloseConnection(std::function<bool(const Identity&)> condition)
        {
            std::list<std::shared_ptr<Network::IConnection<Identity>>> connectionsToClose;
            for (const auto& [id, connection] : _connections)
            {
                if (condition(id))
                {
                    connectionsToClose.push_back(connection);
                }
            }
            for (const auto& connection : connectionsToClose)
            {
                /** Avoid calling closed callback by removing it first */
                _connections.erase(connection->GetId());
                connection->Close();
            }
        }

        void CloseConnection(const Identity& id)
        {
            auto item = _connections.find(id);
            if (item != _connections.end())
            {
                /** Avoid calling closed callback by removing it first */
                auto connection = item->second;
                _connections.erase(item);
                connection->Close();
            }
        }

        size_t CountConnection(std::function<bool(const Identity&)> condition) const
        {
            size_t count = 0;
            for (const auto& [id, connection] : _connections)
            {
                if (condition(id))
                {
                    ++count;
                }
            }
            return count;
        }

        void Close()
        {
            CloseInternal();
        }

    private:
        std::shared_ptr<Network::IServer<Identity>> _server;
        std::unordered_map<std::string, RequestHandler> _requestHandlers;
        std::unordered_map<std::string, StreamRequestHandler> _streamRequestHandlers;
        std::unordered_map<std::string, NotificationHandler> _notificationHandlers;
        NewConnectionHandler _newConnectionHandler;
        ConnectionClosedHandler _connectionClosedHandler;
        CriticalErrorHandler _criticalErrorHandler;
        std::unordered_map<Identity, std::shared_ptr<Network::IConnection<Identity>>> _connections{};
        bool _closed{false};

        JS::Promise<void> HandleServerAsync()
        {
            try
            {
                while(true)
                {
                    auto connection = co_await _server->AcceptAsync();
                    if (!connection.has_value())
                    {
                        CloseInternal(false, "Server closed");
                        co_return;
                    }
                    /** Fire the connection async handler */
                    HandleConnectionAsync(connection.value());
                }
            }
            catch(const std::exception& e)
            {
                CloseInternal(false, e.what());
                co_return;
            }
            catch(...)
            {
                CloseInternal(false, "Unknown error");
                co_return;
            }
        }

        JS::Promise<void> HandleConnectionAsync(
            std::shared_ptr<Network::IConnection<Identity>> connection)
        {
            auto id = connection->GetId();
            _connections[id] = connection;
            if (_newConnectionHandler)
            {
                _newConnectionHandler(id);
            }
            try
            {
                while(true)
                {
                    auto message = co_await connection->ReceiveAsync();
                    if (!message.has_value())
                    {
                        HandleClosedConnection(id);
                        co_return;
                    }
                    HandleRequestAsync(connection, std::move(message.value()));
                }
            }
            catch(...)
            {
                HandleClosedConnection(id);
                co_return;
            }
        }

        JS::Promise<void> HandleRequestAsync(
            std::weak_ptr<Network::IConnection<Identity>> connection,
            std::vector<std::uint8_t> message) noexcept
        {
            std::optional<Identity> id;
            {
                auto conn = connection.lock();
                if (!conn)
                {
                    co_return;
                }
                id = conn->GetId();
            }

            /** 
             * In this application, the connection will always receive full JSON messages.
             * Thus, sticky or incomplete messages are not a concern.
             */
            std::optional<Schema::Rpc::Request> request;
            try
            {
                request = nlohmann::json::parse(message.begin(), message.end()).get<Schema::Rpc::Request>();
            }
            catch(...)
            {
                /** Invalid message, silently ignored */
                co_return;
            }

            auto& method = request->get_method();
            {
                auto item = _requestHandlers.find(method);
                if (item != _requestHandlers.end())
                {
                    try
                    {
                        auto result = co_await item->second(id.value(), request->get_params());
                        TrySendResponse(connection, request->get_id(), result);
                    }
                    catch(const Schema::Rpc::Exception& e)
                    {
                        TrySendError(connection, request->get_id(), e.get_code(), e.what()); 
                    }
                    catch(const std::exception& e)
                    {
                        TrySendError(connection, request->get_id(), -1, e.what());
                    }
                    catch(...)
                    {
                        TrySendError(connection, request->get_id(), -1, "Unkown error");
                    }
                    co_return;
                }
            }
            {
                auto item = _streamRequestHandlers.find(method);
                if (item != _streamRequestHandlers.end())
                {
                    try
                    {
                        auto stream = item->second(id.value(), request->get_params());
                        auto streamId = request->get_id();
                        while (true)
                        {
                            auto result = co_await stream.NextAsync();
                            if (!result.has_value())
                            {
                                auto returnValue = stream.GetReturnValue();
                                TrySendStreamEndResponse(connection, streamId, returnValue);
                                break;
                            }
                            TrySendResponse(connection, streamId, result.value());
                        }
                    }
                    catch(const Schema::Rpc::Exception& e)
                    {
                        TrySendError(connection, request->get_id(), e.get_code(), e.what());
                    }
                    catch(const std::exception& e)
                    {
                        TrySendError(connection, request->get_id(), -1, e.what());
                    }
                    catch(...)
                    {
                        TrySendError(connection, request->get_id(), -1, "Unkown error");
                    }
                    co_return;
                }
            }
            {
                auto item = _notificationHandlers.find(method);
                if (item != _notificationHandlers.end())
                {
                    try
                    {
                        item->second(id.value(), request->get_params());
                    }
                    catch(...)
                    {
                        /** Silently ignored */
                    }
                    co_return;
                }
            }
        }

        void CloseInternal(bool closedByUser = true, const std::string& reason = std::string{})
        {
            if (_closed)
            {
                return;
            }
            _closed = true;
            /** 
             * Async requests will not end now.
             * They will end with their own triggers.
             */
            /** Explicitly close all connections to end their async handlers */
            std::vector<std::shared_ptr<Network::IConnection<Identity>>> connectionsToClose;
            connectionsToClose.reserve(_connections.size());
            for (const auto& item : _connections)
            {
                connectionsToClose.push_back(item.second);
            }
            _connections.clear();
            for (auto& connection : connectionsToClose)
            {
                connection->Close();
            }
            /** Explicitly close the server to end its async handler */
            _server->Close();
            _server.reset();
            /** Call the critical error handler */
            if (!closedByUser && _criticalErrorHandler)
            {
                _criticalErrorHandler(reason);
            }
        }

        void HandleClosedConnection(const Identity& id)
        {
            auto item = _connections.find(id);
            if (item == _connections.end())
            {
                return;
            }
            _connections.erase(item);
            if (_connectionClosedHandler)
            {
                _connectionClosedHandler(id);
            }
        }

        void TrySendResponse(
            std::weak_ptr<Network::IConnection<Identity>> conenction, double id, const nlohmann::json& result) const noexcept
        {
            try
            {
                Schema::Rpc::Response response{};
                response.set_id(id);
                response.set_result(result);
                nlohmann::json responseJson{};
                Schema::Rpc::to_json(responseJson, response);
                auto responseStr = responseJson.dump();
                std::vector<std::uint8_t> responseData(responseStr.begin(), responseStr.end());
                auto conn = conenction.lock();
                if (conn && !conn->IsClosed())
                {
                    conn->Send(std::move(responseData));
                }
            }
            catch(...)
            {
                /** @todo log */
                /** Ignored */
            }
        }

        void TrySendStreamEndResponse(
            std::weak_ptr<Network::IConnection<Identity>> connection, double id, const nlohmann::json& result) const noexcept
        {
            try
            {
                Schema::Rpc::StreamEndResponse response{};
                response.set_id(id);
                response.set_end(true);
                response.set_result(result);
                nlohmann::json responseJson{};
                Schema::Rpc::to_json(responseJson, response);
                auto responseStr = responseJson.dump();
                std::vector<std::uint8_t> responseData(responseStr.begin(), responseStr.end());
                auto conn = connection.lock();
                if (conn && !conn->IsClosed())
                {
                    conn->Send(std::move(responseData));
                }
            }
            catch(...)
            {
                /** @todo log */
                /** Ignored */
            }
        }

        void TrySendError(
            std::weak_ptr<Network::IConnection<Identity>> connection, double id, double code, const std::string& message) const noexcept
        {
            try
            {
                Schema::Rpc::ErrorResponse response{};
                response.set_id(id);
                Schema::Rpc::Error error{};
                error.set_code(code);
                error.set_message(message);
                response.set_error(error);
                nlohmann::json responseJson{};
                Schema::Rpc::to_json(responseJson, response);
                auto responseStr = responseJson.dump();
                std::vector<std::uint8_t> responseData(responseStr.begin(), responseStr.end());
                auto conn = connection.lock();
                if (conn && !conn->IsClosed())
                {
                    conn->Send(std::move(responseData));
                }
            }
            catch(...)
            {
                /** @todo log */
                /** Ignored */
            }
        }
    };
}
