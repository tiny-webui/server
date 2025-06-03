#include <iostream>
#include <tev-cpp/Tev.h>
#include <sys/eventfd.h>
#include <signal.h>
#include "network/WebSocketServer.h"

using namespace TUI::Network;

static Tev tev{};
static std::shared_ptr<WebSocket::Server> server{nullptr};
static int exitFd = -1;

std::vector<std::uint8_t> StringToBytes(const std::string_view& str)
{
    return std::vector<std::uint8_t>(str.begin(), str.end());
}

JS::Promise<void> ReceiveMessageAsync(std::shared_ptr<WebSocket::Connection> connection)
{
    while (true)
    {
        auto message = co_await connection->ReceiveAsync();
        if (!message.has_value())
        {
            std::cout << "Connection closed." << std::endl;
            break;
        }
        std::cout << "Received message: " << std::string(message->begin(), message->end()) << std::endl;
    }
}

JS::Promise<void> TestAsync()
{
    server = WebSocket::Server::Create(tev, "127.0.0.1", 12345);
    while (true)
    {
        auto connection = co_await server->AcceptAsync();
        if (!connection.has_value())
        {
            std::cout << "Server closed." << std::endl;
            break;
        }
        std::cout << "New connection accepted" << std::endl;
        ReceiveMessageAsync(connection.value());
        connection.value()->Send(StringToBytes("Hello from server!"));
    }
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    exitFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (exitFd == -1)
    {
        std::cerr << "Failed to create exit eventfd" << std::endl;
        return 1;
    }

    tev.SetReadHandler(exitFd, [=](){
        eventfd_t value = 0;
        eventfd_read(exitFd, &value);
        tev.SetReadHandler(exitFd, nullptr);
        close(exitFd);
        std::cout << "Closing server..." << std::endl;
        if (server)
        {
            server->Close();
            server.reset();
        }
    });

    signal(SIGINT, [](int) {
        eventfd_t value = 1;
        eventfd_write(exitFd, value);
    });
    signal(SIGTERM, [](int) {
        eventfd_t value = 1;
        eventfd_write(exitFd, value);
    });

    TestAsync().Catch([](const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    });

    tev.MainLoop();

    return 0;
}

