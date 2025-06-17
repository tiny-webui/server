#pragma once

#include <vector>
#include <js-style-co-routine/Promise.h>

namespace TUI::Network
{
    template<typename Identity>
    class IConnection
    {
    public:
        IConnection() = default;
        virtual ~IConnection() = default;    

        IConnection(const IConnection&) = delete;
        IConnection& operator=(const IConnection&) = delete;
        IConnection(IConnection&&) = delete;
        IConnection& operator=(IConnection&&) = delete;

        virtual void Close() = 0;
        virtual bool IsClosed() const noexcept = 0;
        virtual void Send(std::vector<std::uint8_t> message) = 0;
        virtual JS::Promise<std::optional<std::vector<std::uint8_t>>> ReceiveAsync() = 0;
        virtual Identity GetId() const = 0;
    };

    template<>
    class IConnection<void>
    {
    public:
        IConnection() = default;
        virtual ~IConnection() = default;

        IConnection(const IConnection&) = delete;
        IConnection& operator=(const IConnection&) = delete;
        IConnection(IConnection&&) = delete;
        IConnection& operator=(IConnection&&) = delete;

        virtual void Close() = 0;
        virtual void Send(std::vector<std::uint8_t> message) = 0;
        virtual JS::Promise<std::optional<std::vector<std::uint8_t>>> ReceiveAsync() = 0;
        virtual bool IsClosed() const noexcept = 0;
    };
}
