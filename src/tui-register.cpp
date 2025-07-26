#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <optional>

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
    auto database = co_await Database::Database::CreateAsync(tev, params.dbPath.value());
    /** Check if there are any existing user */
    if (!database->ListUser().empty())
    {
        throw std::runtime_error("There are existing users. You can only register the first user with this program.");
    }
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
    Schema::IServer::UserCredential credential;
    credential.set_salt(Common::Base64::Encode(saltOpt.value()));
    credential.set_w0(Common::Base64::Encode(w0Opt.value()));
    credential.set_l(Common::Base64::Encode(LOpt.value()));
    Schema::IServer::UserAdminSettings adminSettings;
    using RoleType = std::remove_reference_t<decltype(adminSettings.get_mutable_role())>;
    /** The first user should always be an admin */
    adminSettings.set_role(RoleType::ADMIN);
    co_await database->CreateUserAsync(
        username,
        static_cast<nlohmann::json>(adminSettings).dump(),
        static_cast<nlohmann::json>(credential).dump());
    std::cout << "First admin registered successfully: " << username << std::endl;
}
