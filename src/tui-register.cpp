#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <optional>
#include <fcntl.h>
#include <unistd.h>

#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include <nlohmann/json.hpp>

#include "application/RegistrationTlvType.h"
#include "database/Database.h"
#include "cipher/Spake2p.h"
#include "common/Base64.h"
#include "common/Tlv.h"
#include "common/Utf8.h"
#include "common/Uuid.h"
#include "schema/IServer.h"

using namespace TUI;

struct AppParams
{
    std::optional<std::filesystem::path> dbPath{std::nullopt};
    std::optional<std::string> registerString{std::nullopt};

    static AppParams Parse(int argc, char const *argv[])
    {
        int opt = -1;
        AppParams params{};
        while ((opt = getopt(argc, const_cast<char**>(argv), "d:r:")) != -1)
        {
            switch (opt)
            {
            case 'd':
                params.dbPath = std::filesystem::path(optarg);
                break;
            case 'r':
                params.registerString = std::string(optarg);
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
        if (!registerString.has_value())
        {
            throw std::invalid_argument("Register string is required");
        }
    }

    std::string getHelp(const std::string& programName) const
    {
        std::ostringstream oss;
        oss << "Usage: " << std::endl 
            << programName << std::endl
            << "    -d <database_path>" << std::endl
            << "    -r <register_string>" << std::endl;
        return oss.str();
    }
};

static JS::Promise<void> MainNoexceptAsync(Tev& tev, AppParams params);
static JS::Promise<void> MainAsync(Tev& tev, AppParams params);
static JS::Promise<std::string> ReadLineAsync(Tev& tev);

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

    Tev tev;
    MainNoexceptAsync(tev, std::move(params));
    tev.MainLoop();

    return 0;
}

static JS::Promise<void> MainNoexceptAsync(Tev& tev, AppParams params)
{
    try
    {
        co_await MainAsync(tev, std::move(params));
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        exit(1);
    }
    catch (...)
    {
        std::cerr << "Unknown error occurred" << std::endl;
        exit(1);
    }
}

static JS::Promise<void> MainAsync(Tev& tev, AppParams params)
{
    /** Parser the registration string to user info and credential */
    auto bytes = Common::Base64::Decode(params.registerString.value());
    auto tlv = Common::Tlv<Application::RegisterTlvType>::Parse(bytes);
    auto usernameOpt = tlv.GetElement(Application::RegisterTlvType::Username);
    if (!usernameOpt.has_value())
    {
        throw std::invalid_argument("Username is required");
    }
    std::string username(usernameOpt.value().begin(), usernameOpt.value().end());
    if (!Common::Utf8::IsValid(username))
    {
        throw std::invalid_argument("Username must be a valid UTF-8 string");
    }
    auto saltOpt = tlv.GetElement(Application::RegisterTlvType::Salt);
    if (!saltOpt.has_value())
    {
        throw std::invalid_argument("Salt is required");
    }
    if (saltOpt.value().size() != sizeof(Cipher::Spake2p::RegistrationResult::salt))
    {
        throw std::invalid_argument("Invalid salt size");
    }
    auto w0Opt = tlv.GetElement(Application::RegisterTlvType::w0);
    if (!w0Opt.has_value())
    {
        throw std::invalid_argument("w0 is required");
    }
    if (w0Opt.value().size() != sizeof(Cipher::Spake2p::RegistrationResult::w0))
    {
        throw std::invalid_argument("Invalid w0 size");
    }
    auto LOpt = tlv.GetElement(Application::RegisterTlvType::L);
    if (!LOpt.has_value())
    {
        throw std::invalid_argument("L is required");
    }
    if (LOpt.value().size() != sizeof(Cipher::Spake2p::RegistrationResult::L))
    {
        throw std::invalid_argument("Invalid L size");
    }
    std::string publicMetadata{};
    auto publicMetadataOpt = tlv.GetElement(Application::RegisterTlvType::PublicMetadata);
    if (publicMetadataOpt.has_value())
    {
        publicMetadata = std::string(publicMetadataOpt.value().begin(), publicMetadataOpt.value().end());
        if (!Common::Utf8::IsValid(publicMetadata))
        {
            throw std::invalid_argument("Public metadata must be a valid UTF-8 string");
        }
        auto publicMetadataObj = nlohmann::json::parse(publicMetadata);
        if (!publicMetadataObj.is_object())
        {
            throw std::invalid_argument("Public metadata must be a JSON object");
        }
        /** condense the json string */
        publicMetadata = publicMetadataObj.dump();
    }
    Schema::IServer::UserCredential credential;
    credential.set_salt(Common::Base64::Encode(saltOpt.value()));
    credential.set_w0(Common::Base64::Encode(w0Opt.value()));
    credential.set_l(Common::Base64::Encode(LOpt.value()));
    
    auto database = co_await Database::Database::CreateAsync(tev, params.dbPath.value());
    /**
     * Check if:
     * 1. There is an existing user. If so, this is a password reset.
     * 2. There is an existing admin user. If not, this is the first admin registration. This can happen if:
     *     a. The server was just created and there is no user.
     *     b. The last admin deleted themselves.
     * If neither is true, reject the registration.
     */
    auto userList = database->ListUser();
    std::optional<Common::Uuid> existingUserId{ std::nullopt };
    bool hasAdmin = false;
    for (const auto& user : userList)
    {
        if (user.userName == username)
        {
            existingUserId = user.id;
        }
        auto adminSettingsObj = nlohmann::json::parse(user.adminSettings);
        Schema::IServer::UserAdminSettings adminSettings = adminSettingsObj.get<Schema::IServer::UserAdminSettings>();
        using RoleType = std::remove_reference_t<decltype(adminSettings.get_mutable_role())>;
        if (adminSettings.get_role() == RoleType::ADMIN)
        {
            hasAdmin = true;
        }
    }
    
    if (existingUserId.has_value())
    {
        std::cout << "User " << username << " already exists. This will reset the password." << std::endl;
        std::cout << "Are you sure to continue? (Y/[N]): " << std::flush;
        auto answer = co_await ReadLineAsync(tev);
        if (answer != "Y" && answer != "y")
        {
            throw std::runtime_error("Aborted by user");
        }
        co_await database->SetUserCredentialAsync(existingUserId.value(), static_cast<nlohmann::json>(credential).dump());
        if (!publicMetadata.empty())
        {
            co_await database->SetUserPublicMetadataAsync(existingUserId.value(), publicMetadata);
        }
        std::cout << "Password reset successfully for user: " << username << std::endl;
    }
    else if (!hasAdmin)
    {
        std::cout << "No admin user found. Registering the first admin user: " << username << std::endl;
        std::cout << "Are you sure to continue? (Y/[N]): " << std::flush;
        auto answer = co_await ReadLineAsync(tev);
        if (answer != "Y" && answer != "y")
        {
            throw std::runtime_error("Aborted by user");
        }
        /** The first user will always be admin */
        Schema::IServer::UserAdminSettings adminSettings;
        using RoleType = std::remove_reference_t<decltype(adminSettings.get_mutable_role())>;
        adminSettings.set_role(RoleType::ADMIN);
        auto userId = co_await database->CreateUserAsync(
            username,
            static_cast<nlohmann::json>(adminSettings).dump(),
            static_cast<nlohmann::json>(credential).dump());
        if (!publicMetadata.empty())
        {
            co_await database->SetUserPublicMetadataAsync(userId, publicMetadata);
        }
        std::cout << "First admin registered successfully: " << username << std::endl;
    }
    else
    {
        throw std::runtime_error("This is neither a password reset nor the first admin registration");
    }
}

static void SetToUnblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        throw std::runtime_error("Failed to get file descriptor flags");
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        throw std::runtime_error("Failed to set file descriptor to non-blocking");
    }
}

static JS::Promise<std::string> ReadLineAsync(Tev& tev)
{
    SetToUnblocking(STDIN_FILENO);
    std::string buffer;
    JS::Promise<void> promise;
    auto readHandler = tev.SetReadHandler(STDIN_FILENO, [&buffer, promise](){
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            promise.Reject("Failed to read from stdin");
        }
        else if (n == 0)
        {
            promise.Reject("EOF reached");
        }
        else
        {
            if (c == '\n')
            {
                promise.Resolve();
            }
            else
            {
                buffer.push_back(c);
            }
        }
    });
    co_await promise;
    co_return buffer;
}
