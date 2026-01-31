#pragma once

#include <memory>
#include <js-style-co-routine/Promise.h>
#include <js-style-co-routine/AsyncGenerator.h>
#include <tev-cpp/Tev.h>
#include "TwsTypes.h"
#include "common/UniqueTypes.h"
#include "IConnection.h"
#include "IServer.h"

namespace TUI::Network::WebSocket
{
    class Server : public IServer<void>, public std::enable_shared_from_this<Server> 
    {
    public:
        static std::shared_ptr<IServer<void>> Create(Tev& tev, const std::string& address, int port);
        static std::shared_ptr<IServer<void>> Create(Tev& tev, const std::string& unixSocketPath);    

        ~Server() override;
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        Server(Server&&) = delete;
        Server& operator=(Server&&) = delete;

        void Close() override;
        bool IsClosed() const noexcept override;
        JS::Promise<std::optional<std::shared_ptr<IConnection<void>>>> AcceptAsync() override;
    private:

        class Connection : public IConnection<void>, public std::enable_shared_from_this<Connection>
        {
            friend class Server;
        public:
            ~Connection() override;
            Connection(const Connection&) = delete;
            Connection& operator=(const Connection&) = delete;
            Connection(Connection&&) = delete;
            Connection& operator=(Connection&&) = delete;

            void Close() override;
            bool IsClosed() const noexcept override;
            void Send(std::vector<std::uint8_t> message) override;
            JS::Promise<std::optional<std::vector<std::uint8_t>>> ReceiveAsync() override;
        private:
            explicit Connection(std::unique_ptr<TwsTypes::TwsConnection> twsConnection);

            std::unique_ptr<TwsTypes::TwsConnection> _twsConnection;
            JS::AsyncGenerator<std::vector<std::uint8_t>> _receiveGenerator{};
        };

        Server(Tev& tev, const std::string& address, int port, bool addressIsUds = false);

        std::unique_ptr<TwsTypes::Tws> _tws;
        JS::AsyncGenerator<std::shared_ptr<IConnection<void>>> _connectionGenerator{};
    };
}
