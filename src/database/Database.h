#pragma once

#include <memory>
#include <string>
#include <list>
#include <format>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include "common/Uuid.h"
#include "schema/IServer.h"
#include "Sqlite.h"

namespace TUI::Database
{
    /**
     * @brief Application specific database wrapper.
     * 
     */
    class Database
    {
    public:
        static JS::Promise<std::shared_ptr<Database>> CreateAsync(Tev& tev, const std::filesystem::path& dbPath);
        
        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;
        Database(Database&&) noexcept = delete;
        Database& operator=(Database&&) noexcept = delete;

        ~Database() = default;

        /** Global KV */
        JS::Promise<void> SetGlobalValueAsync(const std::string& key, std::string value);
        std::optional<std::string> GetGlobalValue(const std::string& key);
        JS::Promise<void> DeleteGlobalValueAsync(const std::string& key);

        /** Model */
        JS::Promise<Common::Uuid> CreateModelAsync(const std::string& settings);
        JS::Promise<void> DeleteModelAsync(const Common::Uuid& id);
        struct IdMetadataPair
        {
            Common::Uuid id;
            std::string metadata;
        };
        std::list<IdMetadataPair> ListModel();
        JS::Promise<void> SetModelMetadataAsync(const Common::Uuid& id, std::string metadata);
        std::string GetModelMetadata(const Common::Uuid& id);
        JS::Promise<void> SetModelSettingsAsync(const Common::Uuid& id, std::string settings);
        std::string GetModelSettings(const Common::Uuid& id);

        /** User */
        JS::Promise<Common::Uuid> CreateUserAsync(
            std::string username, std::string adminSettings, std::string credential);
        JS::Promise<void> DeleteUserAsync(const Common::Uuid& id);
        struct UserListItem
        {
            Common::Uuid id;
            std::string userName;
            std::string adminSettings;
            std::string publicMetadata;
            std::string adminMetadata;
        };
        std::list<UserListItem> ListUser();
        JS::Promise<void> SetUserPublicMetadataAsync(const Common::Uuid& id, std::string metadata);
        std::string GetUserPublicMetadata(const Common::Uuid& id);
        JS::Promise<void> SetUserAdminMetadataAsync(const Common::Uuid& id, std::string metadata);
        std::string GetUserAdminMetadata(const Common::Uuid& id);
        JS::Promise<void> SetUserMetadataAsync(const Common::Uuid& id, std::string metadata);
        std::string GetUserMetadata(const Common::Uuid& id);
        JS::Promise<void> SetUserAdminSettingsAsync(const Common::Uuid& id, std::string settings);
        std::string GetUserAdminSettings(const Common::Uuid& id);
        JS::Promise<void> SetUserCredentialAsync(const Common::Uuid& id, std::string credential);
        std::string GetUserCredential(const Common::Uuid& id);
        Common::Uuid GetUserId(const std::string& username);

        /** Chat */
        JS::Promise<Common::Uuid> CreateChatAsync(const Common::Uuid& userId);
        JS::Promise<void> DeleteChatAsync(const Common::Uuid& userId, const Common::Uuid& chatId);
        size_t GetChatCount(const Common::Uuid& userId);
        std::list<IdMetadataPair> ListChat(
            const Common::Uuid& userId, size_t from = 0, size_t limit = 50);
        JS::Promise<void> SetChatMetadataAsync(
            const Common::Uuid& userId, const Common::Uuid& chatId, std::string metadata);
        std::string GetChatMetadata(const Common::Uuid& userId, const Common::Uuid& chatId);
        JS::Promise<void> AppendChatHistoryAsync(
            const Common::Uuid& userId,
            const Common::Uuid& chatId,
            Schema::IServer::MessageNode node,
            bool updateParent = true);
        Schema::IServer::TreeHistory GetChatHistory(const Common::Uuid& userId, const Common::Uuid& chatId);

    private:
        Database() = default;

        std::list<IdMetadataPair> ParseListTableIdWithMetadataResult(
            Sqlite::ExecResult& result);
        JS::Promise<void> SetStringToTableById(
            const std::string& table, const Common::Uuid& id, const std::string& name, std::string value);
        std::string GetStringFromTableById(
            const std::string& table, const Common::Uuid& id, const std::string& name);
        JS::Promise<void> SetStringToChatAsync(
            const Common::Uuid& userId, const Common::Uuid& id, const std::string& name, std::string value);
        std::string GetStringFromChat(
            const Common::Uuid& userId, const Common::Uuid& id, const std::string& name);

        std::shared_ptr<Sqlite> _db;
    };
}
