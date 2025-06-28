#include <iostream>
#include <filesystem>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include "database/Database.h"
#include "Utility.h"

using namespace TUI::Database;
using namespace TUI::Common;

Tev tev{};
std::string dbPath{};
std::shared_ptr<Database> db{nullptr};

JS::Promise<void> TestCreateAsync()
{
    db = co_await Database::CreateAsync(tev, dbPath);
}

void TestClose()
{
    db = nullptr;
}

JS::Promise<void> TestGlobalAsync()
{
    std::string key = "test-key";
    std::string value = "test-value";
    co_await db->SetGlobalValueAsync(key, value);
    auto retrievedValue = db->GetGlobalValue(key);
    AssertWithMessage(retrievedValue.has_value(), "Global value should be set");
    AssertWithMessage(retrievedValue.value() == value, "Global value should match");
    std::string newValue = "new-test-value";
    co_await db->SetGlobalValueAsync(key, newValue);
    retrievedValue = db->GetGlobalValue(key);
    AssertWithMessage(retrievedValue.has_value(), "Global value should be set");
    AssertWithMessage(retrievedValue.value() == newValue, "Global value should match");
    co_await db->DeleteGlobalValueAsync(key);
    retrievedValue = db->GetGlobalValue(key);
    AssertWithMessage(!retrievedValue.has_value(), "Global value should be deleted");
}

JS::Promise<void> TestModelAsync()
{
    auto modelId = co_await db->CreateModelAsync("test-settings");
    auto models = db->ListModel();
    bool modelFound = false;
    for (const auto& model : models)
    {
        if (model.id == modelId)
        {
            modelFound = true;
            break;
        }
    }
    AssertWithMessage(modelFound, "Newly created model not found");
    co_await db->SetModelMetadataAsync(modelId, "test-metadata");
    auto metadata = db->GetModelMetadata(modelId);
    AssertWithMessage(metadata == "test-metadata", "Model metadata should match");
    auto oldSettings = db->GetModelSettings(modelId);
    AssertWithMessage(oldSettings == "test-settings", "Model settings should match");
    co_await db->SetModelSettingsAsync(modelId, "test-settings-new");
    auto newSettings = db->GetModelSettings(modelId);
    AssertWithMessage(newSettings == "test-settings-new", "Model settings should match");
    co_await db->DeleteModelAsync(modelId);
    models = db->ListModel();
    modelFound = false;
    for (const auto& model : models)
    {
        if (model.id == modelId)
        {
            modelFound = true;
            break;
        }
    }
    AssertWithMessage(!modelFound, "Deleted model found");
}

JS::Promise<void> TestUserAsync()
{
    std::string usernameInput = "test-user";
    std::string adminSettingsInput = "test-admin-settings";
    std::string adminSettingsInput2 = "test-admin-settings2";
    std::string credentialInput = "test-credential";
    std::string credentialInput2 = "test-credential2";
    std::string publicMetadataInput = "test-public-metadata";
    std::string publicMetadataInput2 = "test-public-metadata2";
    std::string metadataInput = "test-metadata";
    std::string metadataInput2 = "test-metadata2";

    auto userId = co_await db->CreateUserAsync(
        usernameInput, adminSettingsInput, credentialInput);
    auto users = db->ListUser();
    bool userFound = false;
    for (const auto& user : users)
    {
        if (user.id == userId)
        {
            userFound = true;
            AssertWithMessage(user.userName == usernameInput, "Username should match");
            AssertWithMessage(user.adminSettings == adminSettingsInput, "Admin settings should match");
            AssertWithMessage(user.publicMetadata.empty(), "Public metadata should be empty");
            break;
        }
    }
    AssertWithMessage(userFound, "Newly created user not found");
    auto credential = db->GetUserCredential(userId);
    AssertWithMessage(credential == credentialInput, "Credential should match");

    co_await db->SetUserMetadataAsync(userId, metadataInput2);
    auto metadata = db->GetUserMetadata(userId);
    AssertWithMessage(metadata == metadataInput2, "User metadata should match");
    co_await db->SetUserPublicMetadataAsync(userId, publicMetadataInput2);
    auto publicMetadata = db->GetUserPublicMetadata(userId);
    AssertWithMessage(publicMetadata == publicMetadataInput2, "User public metadata should match");
    co_await db->SetUserAdminSettingsAsync(userId, adminSettingsInput2);
    auto adminSettings = db->GetUserAdminSettings(userId);
    AssertWithMessage(adminSettings == adminSettingsInput2, "User admin settings should match");
    co_await db->SetUserCredentialAsync(userId, credentialInput2);
    credential = db->GetUserCredential(userId);
    AssertWithMessage(credential == credentialInput2, "User credential should match");
    auto foundUserId = db->GetUserId(usernameInput);
    AssertWithMessage(foundUserId == userId, "GetUserId should return the correct user ID");
    try
    {
        co_await db->CreateUserAsync(usernameInput, adminSettingsInput, credentialInput);
        AssertWithMessage(false, "Creating user with existing username should throw an exception");
    }
    catch(const std::exception& e)
    {
        /** ignore */
    }
    co_await db->DeleteUserAsync(userId);
    users = db->ListUser();
    userFound = false;
    for (const auto& user : users)
    {
        if (user.id == userId)
        {
            userFound = true;
            break;
        }
    }
    AssertWithMessage(!userFound, "Deleted user found");
}

JS::Promise<void> TestChatAsync()
{
    std::string username = "test-user2";
    auto userId = co_await db->CreateUserAsync(username, "", "");
    auto chatId = co_await db->CreateChatAsync(userId);
    auto chats = db->ListChat(userId);
    bool chatFound = false;
    for (const auto& chat : chats)
    {
        if (chat.id == chatId)
        {
            chatFound = true;
            break;
        }
    }
    AssertWithMessage(chatFound, "Newly created chat not found");
    co_await db->SetChatMetadataAsync(userId, chatId, "test-chat-metadata");
    auto metadata = db->GetChatMetadata(userId, chatId);
    AssertWithMessage(metadata == "test-chat-metadata", "Chat metadata should match");
    co_await db->SetChatContentAsync(userId, chatId, "test-chat-content");
    auto content = db->GetChatContent(userId, chatId);
    AssertWithMessage(content == "test-chat-content", "Chat content should match");
    size_t count = db->GetChatCount(userId);
    AssertWithMessage(count == 1, "Chat count should be 1");
    co_await db->DeleteChatAsync(userId, chatId);
    chats = db->ListChat(userId);
    chatFound = false;
    for (const auto& chat : chats)
    {
        if (chat.id == chatId)
        {
            chatFound = true;
            break;
        }
    }
    AssertWithMessage(!chatFound, "Deleted chat found");
    chatId = co_await db->CreateChatAsync(userId);
    chats = db->ListChat(userId);
    chatFound = false;
    for (const auto& chat : chats)
    {
        if (chat.id == chatId)
        {
            chatFound = true;
            break;
        }
    }
    AssertWithMessage(chatFound, "Newly created chat not found");
    /** Chats should be deleted with the user */
    co_await db->DeleteUserAsync(userId);
    chats = db->ListChat(userId);
    chatFound = false;
    for (const auto& chat : chats)
    {
        if (chat.id == chatId)
        {
            chatFound = true;
            break;
        }
    }
    AssertWithMessage(!chatFound, "Deleted chat found");
}

JS::Promise<void> TestAsync()
{
    /** Always run this first */
    RunAsyncTest(TestCreateAsync());
    RunAsyncTest(TestGlobalAsync());
    RunAsyncTest(TestModelAsync());
    RunAsyncTest(TestUserAsync());
    RunAsyncTest(TestChatAsync());
    RunTest(TestClose());
}

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <database_path>" << std::endl;
        return 1;
    }
    dbPath = argv[1];
    /** Delete the old database */
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

    TestAsync();

    tev.MainLoop();

    return 0;
}

