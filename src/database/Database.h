#pragma once

#include <memory>
#include <string>
#include <vector>
#include <format>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include "common/Uuid.h"
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

        /** Model */
        JS::Promise<void> CreateModelAsync(const Common::Uuid& id, const std::string& provider);
        JS::Promise<void> DeleteModelAsync(const Common::Uuid& id);
        std::vector<Common::Uuid> ListModel();
        std::vector<std::pair<Common::Uuid, std::string>> ListModelWithMetadata();
        JS::Promise<void> SetModelMetadataAsync(const Common::Uuid& id, std::string metadata);
        std::string GetModelMetadata(const Common::Uuid& id);
        JS::Promise<void> SetModelParamsAsync(const Common::Uuid& id, std::string params);
        std::string GetModelParams(const Common::Uuid& id);
        std::string GetModelProvider(const Common::Uuid& id);

        /** User */
        JS::Promise<void> CreateUserAsync(const Common::Uuid& id);
        JS::Promise<void> DeleteUserAsync(const Common::Uuid& id);
        std::vector<Common::Uuid> ListUser();
        std::vector<std::pair<Common::Uuid, std::string>> ListUserWithMetadata();
        JS::Promise<void> SetUserMetadataAsync(const Common::Uuid& id, std::string metadata);
        std::string GetUserMetadata(const Common::Uuid& id);
        JS::Promise<void> SetUserCredentialAsync(const Common::Uuid& id, std::string credential);
        std::string GetUserCredential(const Common::Uuid& id);

        /** Chat */
        JS::Promise<void> CreateChatAsync(const Common::Uuid& userId, const Common::Uuid& chatId);
        JS::Promise<void> DeleteChatAsync(const Common::Uuid& userId, const Common::Uuid& chatId);
        size_t GetChatCount(const Common::Uuid& userId);
        std::vector<Common::Uuid> ListChat(
            const Common::Uuid& userId, size_t from = 0, size_t limit = 50);
        std::vector<std::pair<Common::Uuid, std::string>> ListChatWithMetadata(
            const Common::Uuid& userId, size_t from = 0, size_t limit = 50);
        JS::Promise<void> SetChatMetadataAsync(
            const Common::Uuid& userId, const Common::Uuid& chatId, std::string metadata);
        std::string GetChatMetadata(const Common::Uuid& userId, const Common::Uuid& chatId);
        JS::Promise<void> SetChatContentAsync(
            const Common::Uuid& userId, const Common::Uuid& chatId, std::string content);
        std::string GetChatContent(const Common::Uuid& userId, const Common::Uuid& chatId);

    private:
        Database() = default;

        std::vector<Common::Uuid> ListTableId(const std::string& table);
        std::vector<Common::Uuid> ParseListTableIdResult(Sqlite::ExecResult& result);
        std::vector<std::pair<Common::Uuid, std::string>> ListTableIdWithMetadata(
            const std::string& table);
        std::vector<std::pair<Common::Uuid, std::string>> ParseListTableIdWithMetadataResult(
            Sqlite::ExecResult& result);
        JS::Promise<void> SetStringToTableById(
            const std::string& table, const Common::Uuid& id, const std::string& name, std::string value);
        std::string GetStringFromTableById(
            const std::string& table, const Common::Uuid& id, const std::string& name);
        JS::Promise<void> SetStringToChatAsync(
            const Common::Uuid& userId, const Common::Uuid& id, const std::string& name, std::string value);
        std::string GetStringFromChat(
            const Common::Uuid& userId, const Common::Uuid& id, const std::string& name);
        /**
         * @brief Get the current timestamp in milliseconds.
         * 
         * @return int64_t 
         */
        int64_t GetTimestamp();

        std::shared_ptr<Sqlite> _db;
    };
}
