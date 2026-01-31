#pragma once

#include <tiny-websocket/tws.h>
#include <functional>
#include <string>
#include <memory>
#include <unordered_map>

namespace TUI::Network::WebSocket::TwsTypes
{
    struct EventLoopCallbacks
    {
        std::function<void(int, const std::function<void()>&)> setReadHandler;
        std::function<void(int, const std::function<void()>&)> setWriteHandler;
        std::function<tws_timeout_handle_t(uint64_t, const std::function<void()>&)> setTimeout;
        std::function<void(tws_timeout_handle_t)> clearTimeout;
    };

    class TwsConnection;

    class Tws
    {
    public:
        Tws();
        ~Tws();

        Tws(const Tws&) = delete;
        Tws& operator=(const Tws&) = delete;
        Tws(Tws&&) = delete;
        Tws& operator=(Tws&&) = delete;

        void ConfigAppendSubprotocol(const std::string& subprotocol);
        void ConfigSetAddressFamily(tws_address_family_t address_family);
        void ConfigSetHost(const std::string& host);
        void ConfigSetPort(uint16_t port);
        void ConfigSetPath(const std::string& path);

        void SetEventLoopCallbacks(const EventLoopCallbacks& callbacks);

        void SetOnClosedCallback(const std::function<void(tws_close_reason_t)>& callback);
        void SetOnConnectionCallback(const std::function<void(std::unique_ptr<TwsConnection>)>& callback);

        void Open();

        bool operator==(std::nullptr_t) const noexcept;
    private:
        static void OnClosedBridge(tws_close_reason_t reason, tws_ctx_t ctx) noexcept;
        static void OnConnectionBridge(tws_conn_handle_t conn_handle, tws_ctx_t ctx) noexcept;

        tws_handle_t _handle{nullptr};
        std::function<void(tws_close_reason_t)> _onClosedCallback{nullptr};
        std::function<void(std::unique_ptr<TwsConnection>)> _onConnectionCallback{nullptr};

        std::shared_ptr<EventLoopCallbacks> _eventLoopCallbacks{nullptr};
    };

    class TwsConnection
    {
        friend class Tws;
    public:
        ~TwsConnection();

        TwsConnection(const TwsConnection&) = delete;
        TwsConnection& operator=(const TwsConnection&) = delete;
        TwsConnection(TwsConnection&&) = delete;
        TwsConnection& operator=(TwsConnection&&) = delete;

        std::string GetSubProtocol();
        void SetOnMessageCallback(const std::function<void(bool, const void*, size_t)>& callback);
        void SetOnClosedCallback(const std::function<void(tws_close_reason_t)>& callback);

        void SendMessage(const std::string& message);
        void SendMessage(const std::vector<uint8_t>& message);

        bool operator==(std::nullptr_t) const noexcept;
    private:
        explicit TwsConnection(tws_conn_handle_t handle, std::shared_ptr<EventLoopCallbacks> eventLoopCallbacks);

        static void OnMessageBridge(bool is_binary, const void* data, size_t data_size, tws_ctx_t ctx) noexcept;
        static void OnClosedBridge(tws_close_reason_t reason, tws_ctx_t ctx) noexcept;

        tws_conn_handle_t _handle;
        std::shared_ptr<EventLoopCallbacks> _eventLoopCallbacks;

        std::function<void(bool, const void*, size_t)> _onMessageCallback{nullptr};
        std::function<void(tws_close_reason_t)> _onClosedCallback{nullptr};
    };
}
