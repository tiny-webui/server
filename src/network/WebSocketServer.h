#pragma once

#include <memory>
#include <unordered_map>
#include <thread>
#include <deque>
#include <js-style-co-routine/Promise.h>
#include <js-style-co-routine/AsyncGenerator.h>
#include <tev-cpp/Tev.h>
#include "LwsTypes.h"
#include "common/UniqueTypes.h"
#include "IConnection.h"
#include "IServer.h"

namespace TUI::Network::WebSocket
{
    class Server : public IServer<void>, public std::enable_shared_from_this<Server> 
    {
    public:
        static std::shared_ptr<Server> Create(Tev& tev, const std::string& address, int port);
        static std::shared_ptr<Server> Create(Tev& tev, const std::string& unixSocketPath);    

        ~Server() override;
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        Server(Server&&) = delete;
        Server& operator=(Server&&) = delete;

        void Close() override;
        JS::Promise<std::optional<std::shared_ptr<IConnection<void>>>> AcceptAsync() override;

        /** DO NOT call these directly. You do not have the id do to so. */
        void CloseConnection(std::uint64_t id, bool closedByPeer = false);
        void SendMessage(std::uint64_t id, std::vector<std::uint8_t> message);
    private:
        /** 1MiB */
        static constexpr size_t LWS_BUFFER_SIZE = 1 * 1024 * 1024; 

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
            void Send(std::vector<std::uint8_t> message) override;
            JS::Promise<std::optional<std::vector<std::uint8_t>>> ReceiveAsync() override;
        private:
            Connection(std::uint64_t id, std::shared_ptr<Server> server);
            std::uint64_t _id;
            std::weak_ptr<Server> _server;
            JS::AsyncGenerator<std::vector<std::uint8_t>> _receiveGenerator{};
        };

        struct LwsConnection
        {
            std::uint64_t id;
            struct lws* wsi;
            std::queue<std::vector<std::uint8_t>> txQueue;
            std::vector<std::uint8_t> rxBuffer;
        };

        /** Inter-thread communication messages */
        enum class ITCType
        {
            /** For tx */
            SEND_MESSAGE,
            DISCONNECT,
            CLOSE_SERVER,
            /** For rx */
            CONNECTION_ACCEPTED,
            CONNECTION_DISCONNECTED,
            MESSAGE_RECEIVED,
            SERVER_CLOSED,
        };

        struct IITCData
        {
            virtual ~IITCData() = default;
        };

        struct ITCDataMessage : public IITCData
        {
            explicit ITCDataMessage(std::uint64_t id, const std::vector<std::uint8_t>& message)
                : id(id), message(message)
            {
            }
            uint64_t id;
            std::vector<std::uint8_t> message;
        };

        struct ITCDataConnectionId : public IITCData
        {
            explicit ITCDataConnectionId(std::uint64_t id) : id(id) {}
            uint64_t id;
        };

        struct ITCDataConnectionDisconnected : public IITCData
        {
            explicit ITCDataConnectionDisconnected(std::uint64_t id, const std::string& reason) 
                : id(id), reason(reason)
            {
            }
            uint64_t id;
            std::string reason;
        };

        struct ITCMessage
        {
            ITCType type;
            std::unique_ptr<IITCData> data;
        };

        Tev& _tev;
        std::vector<struct lws_protocols> _protocols{};         /** lws thread only */
        std::unique_ptr<LwsTypes::Context> _context{nullptr};   /** lws thread only */
        std::uint64_t _connectionIdSeed{0};                     /** lws thread only */
        std::unordered_map<std::uint64_t, std::shared_ptr<Connection>> _connections{};
        JS::AsyncGenerator<std::shared_ptr<IConnection<void>>> _connectionGenerator{};
        std::unordered_map<std::uint64_t, std::unique_ptr<LwsConnection>> _lwsConnections{}; /** lws thread only */
        std::thread _lwsThread{};
        bool _lwsThreadShouldExit{false};                       /** lws thread only */
        bool _closed{false};
        std::mutex _txMutex{};                                  /** Both threads */
        std::deque<ITCMessage> _txQueue{};                      /** Both threads */
        Common::Unique::Fd _rxEventFd{-1};                        /** Both threads */
        std::mutex _rxMutex{};                                  /** Both threads */
        std::deque<ITCMessage> _rxQueue{};                      /** Both threads */

        /** Functions */
        static int LwsCallback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) noexcept;
        Server(Tev& tev, const std::string& address, int port, bool addressIsUds = false);
        void LwsThreadFunc();
        void ITCRxCallback();
        void SendMessageToMainThread(ITCType type, std::unique_ptr<IITCData> data = nullptr);
        void SendMessageToLwsThread(ITCType type, std::unique_ptr<IITCData> data = nullptr);
        void CloseInternal(bool closedByLws = false);
    };
}
