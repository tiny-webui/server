#include <iostream>
#include <filesystem>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include "database/Database.h"
#include "schema/IServer.h"
#include "Utility.h"

using namespace TUI::Database;
using namespace TUI::Common;
using namespace TUI::Schema;

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
    std::string adminMetadataInput = "test-admin-metadata";
    std::string metadataInput = "test-metadata";

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
            AssertWithMessage(user.adminMetadata.empty(), "Admin metadata should be empty");
            break;
        }
    }
    AssertWithMessage(userFound, "Newly created user not found");
    auto credential = db->GetUserCredential(userId);
    AssertWithMessage(credential == credentialInput, "Credential should match");

    co_await db->SetUserMetadataAsync(userId, metadataInput);
    auto metadata = db->GetUserMetadata(userId);
    AssertWithMessage(metadata == metadataInput, "User metadata should match");
    co_await db->SetUserPublicMetadataAsync(userId, publicMetadataInput);
    auto publicMetadata = db->GetUserPublicMetadata(userId);
    AssertWithMessage(publicMetadata == publicMetadataInput, "User public metadata should match");
    co_await db->SetUserAdminMetadataAsync(userId, adminMetadataInput);
    auto adminMetadata = db->GetUserAdminMetadata(userId);
    AssertWithMessage(adminMetadataInput == adminMetadata, "User admin metadata should match");
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
    {
        IServer::Message message0{};
        using MessageRoleType = std::remove_reference<decltype(message0.get_mutable_role())>::type;
        message0.set_role(MessageRoleType::USER);
        using MessageContentType = std::remove_reference<decltype(message0.get_mutable_content())>::type::value_type;
        MessageContentType content0{};
        using MessageContentTypeType = std::remove_reference<decltype(content0.get_mutable_type())>::type;
        content0.set_type(MessageContentTypeType::TEXT);
        content0.set_data("Hello, this is a test message.");
        message0.get_mutable_content().push_back(std::move(content0));
        IServer::MessageNode node0{};
        node0.set_id("node0");
        node0.set_timestamp(1.0);
        node0.set_message(std::move(message0));
        co_await db->AppendChatHistoryAsync(userId, chatId, node0);

        IServer::MessageNode node1{};
        node1.set_id("node1");
        node1.set_parent("node0");
        node1.set_timestamp(2.0);
        IServer::Message message1{};
        message1.set_role(MessageRoleType::ASSISTANT);
        MessageContentType content1{};
        content1.set_type(MessageContentTypeType::TEXT);
        content1.set_data("This is a response message.");
        message1.get_mutable_content().push_back(std::move(content1));
        node1.set_message(std::move(message1));
        co_await db->AppendChatHistoryAsync(userId, chatId, node1);
        
        auto history = db->GetChatHistory(userId, chatId);
        auto& nodes = history.get_nodes();
        AssertWithMessage(nodes.size() == 2, "Chat history should have 2 messages");
        auto retrievedNode0It = nodes.find("node0");
        AssertWithMessage(retrievedNode0It != nodes.end(), "Node0 should be found");
        auto& retrievedNode0 = retrievedNode0It->second;
        AssertWithMessage(retrievedNode0.get_timestamp() == 1.0, "Node0 timestamp should match");
        AssertWithMessage(retrievedNode0.get_message().get_role() == MessageRoleType::USER, "Node0 role should match");
        AssertWithMessage(retrievedNode0.get_message().get_content().size() == 1, "Node0 content size should match");
        AssertWithMessage(retrievedNode0.get_message().get_content().front().get_data() == "Hello, this is a test message.", "Node0 content data should match");
        AssertWithMessage(retrievedNode0.get_parent().has_value() == false, "Node0 parent should be null");
        AssertWithMessage(retrievedNode0.get_children().size() == 1, "Node0 should have 1 child");
        AssertWithMessage(retrievedNode0.get_children().front() == "node1", "Node0 child ID should match");
        auto retrievedNode1It = nodes.find("node1");
        AssertWithMessage(retrievedNode1It != nodes.end(), "Node1 should be found");
        auto& retrievedNode1 = retrievedNode1It->second;
        AssertWithMessage(retrievedNode1.get_timestamp() == 2.0, "Node1 timestamp should match");
        AssertWithMessage(retrievedNode1.get_message().get_role() == MessageRoleType::ASSISTANT, "Node1 role should match");
        AssertWithMessage(retrievedNode1.get_message().get_content().size() == 1, "Node1 content size should match");
        AssertWithMessage(retrievedNode1.get_message().get_content().front().get_data() == "This is a response message.", "Node1 content data should match");
        AssertWithMessage(retrievedNode1.get_parent().has_value() == true, "Node1 parent should not be null");
        AssertWithMessage(retrievedNode1.get_parent().value() == "node0", "Node1 parent ID should match");
        AssertWithMessage(retrievedNode1.get_children().empty(), "Node1 should have no children");
    }
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

