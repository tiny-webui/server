#include <filesystem>
#include <iostream>
#include <sstream>
#include <memory>
#include <fstream>

#include <signal.h>
#include <unistd.h>

#include <js-style-co-routine/Promise.h>
#include <tev-cpp/Tev.h>

#include "application/SecureSession.h"
#include "application/Service.h"
#include "common/TevInjectionQueue.h"
#include "common/Base64.h"
#include "database/Database.h"
#include "network/IServer.h"
#include "network/WebSocketServer.h"

using namespace TUI;

struct AppParams
{
    std::optional<std::filesystem::path> dbPath{std::nullopt};
    std::optional<std::filesystem::path> configPath{std::nullopt};

    static AppParams Parse(int argc, char const *argv[])
    {
        int opt = -1;
        AppParams params{};
        while ((opt = getopt(argc, const_cast<char**>(argv), "d:c:")) != -1)
        {
            switch (opt)
            {
            case 'd':
                params.dbPath = std::filesystem::path(optarg);
                break;
            case 'c':
                params.configPath = std::filesystem::path(optarg);
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
    }

    std::string getHelp(const std::string& programName) const
    {
        std::ostringstream oss;
        oss << "Usage: " << std::endl 
            << programName << std::endl
            << "    -d <database_path>" << std::endl
            << "    [-c <config_path>]" << std::endl;
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
static JS::Promise<void> PrepareTestDatabaseAsync(AppParams params);
static JS::Promise<void> MainAsync(AppParams params);
static void SignalHandler(int sig);

static constexpr std::string_view serverAddress = "127.0.0.1";
static constexpr uint16_t serverPort = 12345;
static constexpr std::string_view testUsername = "test@example.com";
static constexpr std::string_view testPassword = "password";
static App gApp{};

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
        co_await PrepareTestDatabaseAsync(params);
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

static JS::Promise<void> PrepareTestDatabaseAsync(AppParams params)
{
    if (std::filesystem::exists(params.dbPath.value()))
    {
        std::filesystem::remove(params.dbPath.value());
    }
    if (std::filesystem::exists(params.dbPath->string() + "-wal"))
    {
        std::filesystem::remove(params.dbPath->string() + "-wal");
    }
    if (std::filesystem::exists(params.dbPath->string() + "-shm"))
    {
        std::filesystem::remove(params.dbPath->string() + "-shm");
    }

    auto database = co_await Database::Database::CreateAsync(gApp.tev, params.dbPath.value());
    /** Register a test user */
    auto registration = Cipher::Spake2p::Register(std::string(testUsername), std::string(testPassword));
    Schema::IServer::UserCredential userCredential;
    userCredential.set_w0(Common::Base64::Encode(registration.w0));
    userCredential.set_l(Common::Base64::Encode(registration.L));
    userCredential.set_salt(Common::Base64::Encode(registration.salt));
    Schema::IServer::UserAdminSettings adminSettings;
    using RoleType = std::remove_reference_t<decltype(adminSettings.get_mutable_role())>;
    adminSettings.set_role(RoleType::ADMIN);
    co_await database->CreateUserAsync(
        std::string(testUsername),
        static_cast<nlohmann::json>(adminSettings).dump(),
        static_cast<nlohmann::json>(userCredential).dump());
    /** Create a new model if config path is provided */
    if (params.configPath.has_value())
    {
        if (!std::filesystem::exists(params.configPath.value()))
        {
            throw std::invalid_argument("Config path does not exist");
        }
        std::ifstream configFile(params.configPath->string());
        if (!configFile.is_open())
        {
            throw std::runtime_error("Failed to open config file");
        }
        std::stringstream buffer;
        buffer << configFile.rdbuf();
        configFile.close();
        std::string configContent = buffer.str();
        co_await database->CreateModelAsync(configContent);
    }
}

static JS::Promise<void> MainAsync(AppParams params)
{
    auto database = co_await Database::Database::CreateAsync(gApp.tev, params.dbPath.value());
    std::shared_ptr<Network::IServer<void>> webSocketServer{nullptr};
    webSocketServer = Network::WebSocket::Server::Create(
        gApp.tev, std::string(serverAddress), serverPort);
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
