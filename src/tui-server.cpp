#include <filesystem>
#include <iostream>
#include <sstream>
#include <memory>

#include <signal.h>
#include <unistd.h>

#include <js-style-co-routine/Promise.h>
#include <tev-cpp/Tev.h>

#include "application/SecureSession.h"
#include "application/Service.h"
#include "common/TevInjectionQueue.h"
#include "database/Database.h"
#include "network/IServer.h"
#include "network/WebSocketServer.h"

using namespace TUI;

struct AppParams
{
    std::optional<std::filesystem::path> dbPath{std::nullopt};
    std::optional<std::string> unixSocketPath{std::nullopt};
    std::optional<std::string> address{std::nullopt};
    std::optional<uint16_t> port{std::nullopt};

    static AppParams Parse(int argc, char const *argv[])
    {
        int opt = -1;
        AppParams params{};
        while ((opt = getopt(argc, const_cast<char**>(argv), "d:u:a:p:")) != -1)
        {
            switch (opt)
            {
            case 'd':
                params.dbPath = std::filesystem::path(optarg);
                break;
            case 'u':
                params.unixSocketPath = std::string(optarg);
                break;
            case 'a':
                params.address = std::string(optarg);
                break;
            case 'p':
                params.port = static_cast<uint16_t>(std::stoi(optarg));
                break;
            default:
                break;
            }
        }
        return params;
    }

    void Check() const
    {
        if (!dbPath.has_value())
        {
            throw std::invalid_argument("Database path is required");
        }
        if (!unixSocketPath.has_value() && (!address.has_value() || !port.has_value()))
        {
            throw std::invalid_argument("Either unix socket path or address and port must be provided");
        }
    }

    std::string getHelp(const std::string& programName) const
    {
        std::ostringstream oss;
        oss << "Usage: " << std::endl 
            << programName << std::endl
            << "    -d <database_path>" << std::endl
            << "    -u <unix_socket_path> | -a <address> -p <port>" << std::endl;
        return oss.str();
    }
};

struct App
{
    Tev tev{};
    std::shared_ptr<Application::Service> service{nullptr};
    std::shared_ptr<Common::TevInjectionQueue<int>> signalQueue{nullptr};
};

static JS::Promise<void> MainNoexceptAsync(AppParams params);
static JS::Promise<void> MainAsync(AppParams params);
static void SignalHandler(int sig);

App gApp{};

int main(int argc, char const *argv[])
{
    auto params = AppParams::Parse(argc, argv);
    try
    {
        params.Check();
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error parsing arguments: " << e.what() << std::endl;
        std::cerr << params.getHelp(argv[0]) << std::endl;
        return 1;
    }

    MainNoexceptAsync(std::move(params));
    gApp.tev.MainLoop();

    return 0;
}

static JS::Promise<void> MainNoexceptAsync(AppParams params)
{
    try
    {
        co_await MainAsync(std::move(params));
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error in MainAsync: " << e.what() << std::endl;
        abort();
    }
    catch (...)
    {
        std::cerr << "Unknown error occurred in MainAsync" << std::endl;
        abort();
    }
}

static JS::Promise<void> MainAsync(AppParams params)
{
    auto database = co_await Database::Database::CreateAsync(gApp.tev, params.dbPath.value());
    std::shared_ptr<Network::IServer<void>> webSocketServer{nullptr};
    if (params.unixSocketPath.has_value())
    {
        webSocketServer = Network::WebSocket::Server::Create(
            gApp.tev, params.unixSocketPath.value());
    }
    else
    {
        webSocketServer = Network::WebSocket::Server::Create(
            gApp.tev, params.address.value(), params.port.value());
    }
    auto secureSessionServer = Application::SecureSession::Server::Create(
        gApp.tev, std::move(webSocketServer), 
        [=](const std::string& username) -> std::optional<std::pair<std::string, Common::Uuid>> {
            try
            {
                auto userId = database->GetUserId(username);
                auto credential = database->GetUserCredential(userId);
                return std::make_pair(credential, userId);
            }
            catch(...)
            {
                return std::nullopt;
            }
        });
    gApp.service = std::make_shared<Application::Service>(
        gApp.tev, std::move(secureSessionServer), std::move(database),
        [](const std::string& errorMessage) {
            std::cerr << "Server critical error: " << errorMessage << std::endl;
            abort();
        });
    gApp.signalQueue = Common::TevInjectionQueue<int>::Create(
        gApp.tev,
        [](int&& sig) {
            std::cout << "Signal received: " << sig << std::endl;
            gApp.service->Close();
            gApp.signalQueue->Close();
        },
        [](){
            std::cerr << "Signal queue error" << std::endl;
            abort();
        });
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
}

static void SignalHandler(int sig)
{
    if (gApp.signalQueue)
    {
        gApp.signalQueue->Inject(sig);
    }
}
