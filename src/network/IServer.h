#pragma once

#include "IConnection.h"

namespace TUI::Network
{
    template <typename Identity>
    class IServer
    {
    public:
        IServer() = default;
        virtual ~IServer() = default;

        IServer(const IServer&) = delete;
        IServer& operator=(const IServer&) = delete;
        IServer(IServer&&) = delete;
        IServer& operator=(IServer&&) = delete;

        virtual void Close() = 0;
        virtual JS::Promise<std::optional<std::shared_ptr<IConnection<Identity>>>> AcceptAsync() = 0;
    };

    template <>
    class IServer<void>
    {
    public:
        IServer() = default;
        virtual ~IServer() = default;

        IServer(const IServer&) = delete;
        IServer& operator=(const IServer&) = delete;
        IServer(IServer&&) = delete;
        IServer& operator=(IServer&&) = delete;

        virtual void Close() = 0;
        virtual JS::Promise<std::optional<std::shared_ptr<IConnection<void>>>> AcceptAsync() = 0;
    };
}
