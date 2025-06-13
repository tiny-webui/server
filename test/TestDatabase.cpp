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

JS::Promise<void> TestModelAsync()
{
    Uuid modelId{};
    co_await db->CreateModelAsync(modelId, "test-provider");
    auto models = db->ListModel();
    bool modelFound = false;
    for (const auto& model : models)
    {
        if (model == modelId)
        {
            modelFound = true;
            break;
        }
    }
    AssertWithMessage(modelFound, "Newly created model not found");
    auto provider = db->GetModelProvider(modelId);
    AssertWithMessage(provider == "test-provider", "Model provider should match");
    co_await db->SetModelMetadataAsync(modelId, "test-metadata");
    auto metadata = db->GetModelMetadata(modelId);
    AssertWithMessage(metadata == "test-metadata", "Model metadata should match");
    co_await db->SetModelParamsAsync(modelId, "test-params");
    auto params = db->GetModelParams(modelId);
    AssertWithMessage(params == "test-params", "Model params should match");
    co_await db->DeleteModelAsync(modelId);
    models = db->ListModel();
    modelFound = false;
    for (const auto& model : models)
    {
        if (model == modelId)
        {
            modelFound = true;
            break;
        }
    }
    AssertWithMessage(!modelFound, "Deleted model found");
}

JS::Promise<void> TestUserAsync()
{
    Uuid userId{};
    co_await db->CreateUserAsync(userId);
    auto users = db->ListUser();
    bool userFound = false;
    for (const auto& user : users)
    {
        if (user == userId)
        {
            userFound = true;
            break;
        }
    }
    AssertWithMessage(userFound, "Newly created user not found");
    co_await db->SetUserMetadataAsync(userId, "test-user-metadata");
    auto metadata = db->GetUserMetadata(userId);
    AssertWithMessage(metadata == "test-user-metadata", "User metadata should match");
    co_await db->SetUserCredentialAsync(userId, "test-credential");
    auto credential = db->GetUserCredential(userId);
    AssertWithMessage(credential == "test-credential", "User credential should match");
    co_await db->DeleteUserAsync(userId);
    users = db->ListUser();
    userFound = false;
    for (const auto& user : users)
    {
        if (user == userId)
        {
            userFound = true;
            break;
        }
    }
    AssertWithMessage(!userFound, "Deleted user found");
}

JS::Promise<void> TestChatAsync()
{
    Uuid userId{};
    Uuid chatId{};
    co_await db->CreateUserAsync(userId);
    co_await db->CreateChatAsync(userId, chatId);
    auto chats = db->ListChat(userId);
    bool chatFound = false;
    for (const auto& chat : chats)
    {
        if (chat == chatId)
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
        if (chat == chatId)
        {
            chatFound = true;
            break;
        }
    }
    AssertWithMessage(!chatFound, "Deleted chat found");
    co_await db->CreateChatAsync(userId, chatId);
    chats = db->ListChat(userId);
    chatFound = false;
    for (const auto& chat : chats)
    {
        if (chat == chatId)
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
        if (chat == chatId)
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
    RunAsyncTest(TestModelAsync());
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

