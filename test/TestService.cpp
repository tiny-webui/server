#include <iostream>
#include <filesystem>
#include <signal.h>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include <common/TevInjectionQueue.h>
#include "application/Service.h"
#include "network/IServer.h"
#include "network/WebSocketServer.h"
#include "database/Database.h"
#include "Utility.h"

using namespace TUI;

std::shared_ptr<Application::Service> gService{nullptr};
std::shared_ptr<Common::TevInjectionQueue<int>> gSignalQueue{nullptr};

class TestConnection : public Network::IConnection<Application::CallerId>
{
public:
    TestConnection(Application::CallerId id, std::shared_ptr<Network::IConnection<void>> connection)
        : _id(id), _connection(std::move(connection))
    {
    }

    ~TestConnection() override
    {
        Close();
    }

    void Close() override
    {
        _connection->Close();
    }

    bool IsClosed() const noexcept override
    {
        return _connection->IsClosed();
    }

    void Send(std::vector<std::uint8_t> message) override
    {
        _connection->Send(std::move(message));
    }

    JS::Promise<std::optional<std::vector<std::uint8_t>>> ReceiveAsync() override
    {
        return _connection->ReceiveAsync();
    }

    Application::CallerId GetId() const override
    {
        return _id;
    }

private:
    Application::CallerId _id;
    std::shared_ptr<Network::IConnection<void>> _connection;
};

class TestServer : public Network::IServer<Application::CallerId>
{
public:
    TestServer(Tev& tev, const std::string& address, int port, const Common::Uuid& userId)
        : _userId(userId), _server(Network::WebSocket::Server::Create(tev, address, port))
    {
    }

    ~TestServer() override
    {
        Close();
    }

    void Close() override
    {
        _server->Close();
    }

    bool IsClosed() const noexcept override
    {
        return _server->IsClosed();
    }

    JS::Promise<std::optional<std::shared_ptr<Network::IConnection<Application::CallerId>>>> AcceptAsync() override
    {
        auto rawConnection = co_await _server->AcceptAsync();
        std::optional<std::shared_ptr<Network::IConnection<Application::CallerId>>> result{std::nullopt};
        if (!rawConnection.has_value())
        {
            co_return result;
        }
        Application::CallerId callerId{_userId, Common::Uuid{}};
        std::shared_ptr<Network::IConnection<Application::CallerId>> connection = 
            std::make_shared<TestConnection>(callerId, rawConnection.value());
        result = connection;
        co_return result;
    }

private:
    Common::Uuid _userId;
    std::shared_ptr<Network::IServer<void>> _server;
};

static void SignalHandler(int sig)
{
    gSignalQueue->Inject(sig);
}

JS::Promise<void> MainAsync(Tev& tev, std::string dbPath)
{
    /** Delete the old database if any */
    if (std::filesystem::exists(dbPath))
    {
        std::filesystem::remove(dbPath);
    }
    if (std::filesystem::exists(dbPath + "-wal"))
    {
        std::filesystem::remove(dbPath + "-wal");
    }
    if (std::filesystem::exists(dbPath + "-shm"))
    {
        std::filesystem::remove(dbPath + "-shm");
    }

    auto database = co_await Database::Database::CreateAsync(tev, dbPath);
    Common::Uuid userId{};
    {
        /** Create a default user in the database. THIS IS ONLY FOR TESTING */
        Schema::IServer::UserAdminSettings settings{};
        settings.set_role(Schema::IServer::UserAdminSettingsRole::ADMIN);
        auto id = co_await database->CreateUserAsync("test", (static_cast<nlohmann::json>(settings)).dump(), "{}");
        userId = id;
    }

    auto server = std::make_shared<TestServer>(tev, "127.0.0.1", 12345, userId);

    gService = std::make_shared<Application::Service>(
        server, database,
        [](const std::string& errorMessage) {
            std::cerr << "Critical error: " << errorMessage << std::endl;
            abort();
        });

    gSignalQueue = Common::TevInjectionQueue<int>::Create(
        tev,
        [](int&& sig){
            std::cout << "Signal received: " << sig << std::endl;
            gService->Close();
            gSignalQueue->Close();
        },
        [](){
            std::cerr << "Signal queue error" << std::endl;
            abort();
        });

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
}

JS::Promise<void> TestAsync(Tev& tev, std::string dbPath)
{
    RunAsyncTest(MainAsync(tev, dbPath));
}

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <database_path>" << std::endl;
        return 1;
    }
    std::string dbPath = argv[1];

    Tev tev{};

    TestAsync(tev, dbPath);

    tev.MainLoop();

    return 0;
}

