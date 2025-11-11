#include <format>
#include <nlohmann/json.hpp>
#include "Database.h"
#include "common/Timestamp.h"

using namespace TUI::Common;
using namespace TUI::Database;
using namespace TUI::Schema;

JS::Promise<std::shared_ptr<Database>> Database::CreateAsync(Tev& tev, const std::filesystem::path& dbPath)
{
    auto db = std::shared_ptr<Database>(new Database());
    db->_db = co_await Sqlite::CreateAsync(tev, dbPath);
    /** Create tables */
    co_await db->_db->ExecAsync(
        "CREATE TABLE IF NOT EXISTS global ("
        "key TEXT PRIMARY KEY, "
        "value TEXT);");
    co_await db->_db->ExecAsync(
        "CREATE TABLE IF NOT EXISTS model ("
        "id TEXT PRIMARY KEY, "
        "metadata TEXT, "
        "settings TEXT);");
    co_await db->_db->ExecAsync(
        "CREATE TABLE IF NOT EXISTS user ("
        "id TEXT PRIMARY KEY, "
        "username TEXT UNIQUE, "
        "metadata TEXT, "
        "public_metadata TEXT, "
        "admin_settings TEXT, "
        "credential TEXT);");
    co_await db->_db->ExecAsync(
        "CREATE TABLE IF NOT EXISTS chat ("
        "timestamp INTEGER, "
        "user_id TEXT, "
        "id TEXT, "
        "metadata TEXT, "
        "PRIMARY KEY (user_id, id));");
    co_await db->_db->ExecAsync(
        "CREATE TABLE IF NOT EXISTS chat_content ("
        "user_id TEXT, "
        "chat_id TEXT, "
        "id TEXT, "
        "parent TEXT, "
        "children TEXT, "
        "message TEXT, "
        "timestamp INTEGER, "
        "PRIMARY KEY (user_id, chat_id, id));");
    co_return db;
}

JS::Promise<void> Database::SetGlobalValueAsync(const std::string& key, std::string value)
{
    co_await _db->ExecAsync(
        "INSERT OR REPLACE INTO global (key, value) VALUES (?, ?);",
        key, std::move(value));
}

std::optional<std::string> Database::GetGlobalValue(const std::string& key)
{
    auto result = _db->Exec(
        "SELECT value FROM global WHERE key = ?;",
        key);
    if (result.empty())
    {
        return std::nullopt;
    }
    auto& row = result.front();
    auto it = row.find("value");
    if (it == row.end() || !std::holds_alternative<std::string>(it->second))
    {
        return std::nullopt;
    }
    return std::get<std::string>(it->second);
}

JS::Promise<void> Database::DeleteGlobalValueAsync(const std::string& key)
{
    co_await _db->ExecAsync(
        "DELETE FROM global WHERE key = ?;",
        key);
}

JS::Promise<Uuid> Database::CreateModelAsync(const std::string& settings)
{
    Uuid id{};
    co_await _db->ExecAsync(
        "INSERT INTO model (id, settings) VALUES (?, ?);",
        static_cast<std::string>(id), settings);
    co_return id;
}

JS::Promise<void> Database::DeleteModelAsync(const Common::Uuid& id)
{
    co_await _db->ExecAsync(
        "DELETE FROM model WHERE id = ?;",
        static_cast<std::string>(id));
}

std::list<Database::IdMetadataPair> Database::ListModel()
{
    auto result = _db->Exec("SELECT id, metadata FROM model;");
    return ParseListTableIdWithMetadataResult(result);
}

JS::Promise<void> Database::SetModelMetadataAsync(const Uuid& id, std::string metadata)
{
    return SetStringToTableById("model", id, "metadata", std::move(metadata));
}

std::string Database::GetModelMetadata(const Uuid& id)
{
    return GetStringFromTableById("model", id, "metadata");
}

JS::Promise<void> Database::SetModelSettingsAsync(const Uuid& id, std::string settings)
{
    return SetStringToTableById("model", id, "settings", std::move(settings));
}

std::string Database::GetModelSettings(const Uuid& id)
{
    return GetStringFromTableById("model", id, "settings");
}

JS::Promise<Uuid> Database::CreateUserAsync(
    std::string username, std::string adminSettings, std::string credential)
{
    Uuid id{};
    co_await _db->ExecAsync(
        "INSERT INTO user (id, username, admin_settings, credential) VALUES (?, ?, ?, ?);",
        static_cast<std::string>(id),
        std::move(username),
        std::move(adminSettings),
        std::move(credential));
    co_return id;
}

JS::Promise<void> Database::DeleteUserAsync(const Uuid& id)
{
    co_await _db->ExecAsync(
        "DELETE FROM user WHERE id = ?;",
        static_cast<std::string>(id));
    co_await _db->ExecAsync(
        "DELETE FROM chat WHERE user_id = ?;",
        static_cast<std::string>(id));
    co_await _db->ExecAsync(
        "DELETE FROM chat_content WHERE user_id = ?;",
        static_cast<std::string>(id));
}

std::list<Database::UserListItem> Database::ListUser()
{
    auto result = _db->Exec("SELECT id, username, admin_settings, public_metadata FROM user;");
    std::list<UserListItem> list{};
    for (auto& row : result)
    {
        try
        {
            auto idItem = row.find("id");
            if (idItem == row.end())
            {
                continue;
            }
            Uuid id{std::get<std::string>(idItem->second)};

            auto usernameItem = row.find("username");
            if (usernameItem == row.end())
            {
                continue;
            }
            std::string username = std::move(std::get<std::string>(usernameItem->second));

            auto adminSettingsItem = row.find("admin_settings");
            if (adminSettingsItem == row.end())
            {
                continue;
            }
            std::string adminSettings = std::move(std::get<std::string>(adminSettingsItem->second));

            auto metadataItem = row.find("public_metadata");
            std::string metadata{};
            if (metadataItem == row.end())
            {
                continue;
            }
            if (std::holds_alternative<std::string>(metadataItem->second))
            {
                metadata = std::move(std::get<std::string>(metadataItem->second));
            }
            else if (!std::holds_alternative<std::nullptr_t>(metadataItem->second))
            {
                /** Invalid metadata */
                continue;
            }

            list.emplace_back(
                id, std::move(username), std::move(adminSettings), std::move(metadata));
        }
        catch(...)
        {
            /** @todo log */
            /** Ignored, avoid corrupted data from corrupting the whole application */
        }
    }
    return list;
}

JS::Promise<void> Database::SetUserPublicMetadataAsync(const Uuid& id, std::string metadata)
{
    return SetStringToTableById("user", id, "public_metadata", std::move(metadata));
}

std::string Database::GetUserPublicMetadata(const Uuid& id)
{
    return GetStringFromTableById("user", id, "public_metadata");
}

JS::Promise<void> Database::SetUserMetadataAsync(const Uuid& id, std::string metadata)
{
    return SetStringToTableById("user", id, "metadata", std::move(metadata));
}

std::string Database::GetUserMetadata(const Uuid& id)
{
    return GetStringFromTableById("user", id, "metadata");
}

JS::Promise<void> Database::SetUserAdminSettingsAsync(const Uuid& id, std::string settings)
{
    return SetStringToTableById("user", id, "admin_settings", std::move(settings));
}

std::string Database::GetUserAdminSettings(const Uuid& id)
{
    return GetStringFromTableById("user", id, "admin_settings");
}

JS::Promise<void> Database::SetUserCredentialAsync(const Uuid& id, std::string credential)
{
    return SetStringToTableById("user", id, "credential", std::move(credential));
}

std::string Database::GetUserCredential(const Uuid& id)
{
    return GetStringFromTableById("user", id, "credential");
}

Uuid Database::GetUserId(const std::string& username)
{
    auto result = _db->Exec(
        "SELECT id FROM user WHERE username = ?;",
        username);
    if (result.empty())
    {
        throw std::runtime_error("User not found");
    }
    auto row = result.front();
    auto item = row.find("id");
    if (item == row.end())
    {
        throw std::runtime_error("User ID not found");
    }
    return Uuid{std::get<std::string>(item->second)};
}

JS::Promise<Uuid> Database::CreateChatAsync(const Uuid& userId)
{
    Uuid id{};
    co_await _db->ExecAsync(
        "INSERT INTO chat (timestamp, user_id, id) VALUES(?, ?, ?);",
        Timestamp::GetWallClock(), 
        static_cast<std::string>(userId),
        static_cast<std::string>(id));
    co_return id;
}

JS::Promise<void> Database::DeleteChatAsync(const Uuid& userId, const Uuid& id)
{
    co_await _db->ExecAsync(
        "DELETE FROM chat WHERE user_id = ? AND id = ?;",
        static_cast<std::string>(userId), static_cast<std::string>(id));
    co_await _db->ExecAsync(
        "DELETE FROM chat_content WHERE user_id = ? AND chat_id = ?;",
        static_cast<std::string>(userId), static_cast<std::string>(id));
}

size_t Database::GetChatCount(const Uuid& userId)
{
    auto result = _db->Exec(
        "SELECT COUNT(*) AS count FROM chat WHERE user_id = ?;",
        static_cast<std::string>(userId));
    if (result.empty())
    {
        throw std::runtime_error("Empty result");
    }
    auto row = result.front();
    auto item = row.find("count");
    if (item == row.end())
    {
        throw std::runtime_error("Count not found");
    }
    return static_cast<size_t>(std::get<int64_t>(item->second));
}

std::list<Database::IdMetadataPair> Database::ListChat(
    const Uuid& userId, size_t from , size_t limit)
{
    auto sql = std::format(
        "SELECT id, metadata FROM chat WHERE user_id = ? ORDER BY timestamp DESC LIMIT {} OFFSET {}",
        limit, from);
    auto result = _db->Exec(sql, static_cast<std::string>(userId));
    return ParseListTableIdWithMetadataResult(result);
}

JS::Promise<void> Database::SetChatMetadataAsync(const Uuid& userId, const Uuid& id, std::string metadata)
{
    return SetStringToChatAsync(userId, id, "metadata", metadata);
}

std::string Database::GetChatMetadata(const Uuid& userId, const Uuid& id)
{
    return GetStringFromChat(userId, id, "metadata");
}

JS::Promise<void> Database::AppendChatHistoryAsync(
    const Uuid& userId,
    const Uuid& chatId,
    IServer::MessageNode node,
    bool updateParent)
{
    if (node.get_parent().has_value() && updateParent)
    {
        auto result = _db->Exec(
            "SELECT children FROM chat_content WHERE user_id = ? AND chat_id = ? AND id = ?;",
            static_cast<std::string>(userId),
            static_cast<std::string>(chatId),
            node.get_parent().value());
        if (result.empty())
        {
            throw std::runtime_error("Parent message not found");
        }
        auto& row = result.front();
        auto it = row.find("children");
        if (it == row.end())
        {
            throw std::runtime_error("Children field not found");
        }
        std::string childrenStr;
        if (std::holds_alternative<std::string>(it->second))
        {
            childrenStr = std::get<std::string>(it->second);
        }
        else if (std::holds_alternative<std::nullptr_t>(it->second))
        {
            childrenStr = "[]";
        }
        else
        {
            throw std::runtime_error("Invalid children field type");
        }
        nlohmann::json children = nlohmann::json::parse(childrenStr);
        children.push_back(node.get_id());
        co_await _db->ExecAsync(
            "UPDATE chat_content SET children = ? WHERE user_id = ? AND chat_id = ? AND id = ?;",
            children.dump(),
            static_cast<std::string>(userId),
            static_cast<std::string>(chatId),
            node.get_parent().value());
    }

    co_await _db->ExecAsync(
        "INSERT INTO chat_content (user_id, chat_id, id, parent, children, message, timestamp) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);",
        static_cast<std::string>(userId),
        static_cast<std::string>(chatId),
        node.get_id(),
        node.get_parent().value_or(""),
        (static_cast<nlohmann::json>(node.get_children())).dump(),
        (static_cast<nlohmann::json>(node.get_message())).dump(),
        static_cast<int64_t>(node.get_timestamp()));
}

IServer::TreeHistory Database::GetChatHistory(const Uuid& userId, const Uuid& id)
{
    auto result = _db->Exec(
        "SELECT id, parent, children, message, timestamp FROM chat_content "
        "WHERE user_id = ? AND chat_id = ?;",
        static_cast<std::string>(userId),
        static_cast<std::string>(id));
    IServer::TreeHistory history{};
    auto& nodes = history.get_mutable_nodes();
    for (auto& row : result)
    {
        try
        {
            IServer::MessageNode node{};

            auto messageIdItem = row.find("id");
            if (messageIdItem == row.end())
            {
                continue;
            }
            node.set_id(std::get<std::string>(messageIdItem->second));

            auto parentItem = row.find("parent");
            if (parentItem != row.end() && !std::holds_alternative<std::nullptr_t>(parentItem->second))
            {
                auto parentStr = std::get<std::string>(parentItem->second);
                if (!parentStr.empty())
                {
                    node.set_parent(parentStr);
                }
            }

            auto childrenItem = row.find("children");
            if (childrenItem != row.end() && std::holds_alternative<std::string>(childrenItem->second))
            {
                auto childrenStr = std::get<std::string>(childrenItem->second);
                nlohmann::json childrenJson = nlohmann::json::parse(childrenStr);
                std::vector<std::string> children{};
                for (const auto& child : childrenJson)
                {
                    children.emplace_back(child.get<std::string>());
                }
                node.set_children(children);
            }

            auto messageItem = row.find("message");
            if (messageItem == row.end())
            {
                continue;
            }
            std::string messageStr = std::get<std::string>(messageItem->second);
            auto message = nlohmann::json::parse(messageStr).get<IServer::Message>();
            node.set_message(std::move(message));

            auto timestampItem = row.find("timestamp");
            if (timestampItem == row.end())
            {
                continue;
            }
            int64_t timestamp = std::get<int64_t>(timestampItem->second);
            node.set_timestamp(static_cast<double>(timestamp));

            nodes.emplace(node.get_id(), std::move(node));
        }
        catch(...)
        {
            /** @todo log */
            /** Ignored */
        }
    }
    return history;
}

std::list<Database::IdMetadataPair> Database::ParseListTableIdWithMetadataResult(
    Sqlite::ExecResult& result)
{
    std::list<IdMetadataPair> list{};
    for (auto& row : result)
    {
        try
        {
            auto idItem = row.find("id");
            if (idItem == row.end())
            {
                continue;
            }
            Uuid id{std::get<std::string>(idItem->second)};
            auto metadataItem = row.find("metadata");
            if (metadataItem == row.end())
            {
                continue;
            }
            if (std::holds_alternative<std::string>(metadataItem->second))
            {
                list.emplace_back(id, std::move(std::get<std::string>(metadataItem->second)));
            }
            else if (std::holds_alternative<std::nullptr_t>(metadataItem->second))
            {
                /** If metadata is not set, treat it as an empty string */
                list.emplace_back(id, std::string{});
            }
            else
            {
                continue;
            }
        }
        catch(...)
        {
            /** @todo log */
            /** Ignored, avoid corrupted data from corrupting the whole application */
        }
    }
    return list;
}

JS::Promise<void> Database::SetStringToTableById(
    const std::string& table, const Uuid& id, const std::string& name, std::string value)
{
    auto sql = std::format("UPDATE {} SET {} = ? WHERE id = ?;", table, name);
    co_await _db->ExecAsync(sql, std::move(value), static_cast<std::string>(id));
}

std::string Database::GetStringFromTableById(
    const std::string& table, const Uuid& id, const std::string& name)
{
    auto sql = std::format("SELECT {} FROM {} WHERE id = ?;", name, table);
    auto result = _db->Exec(sql, static_cast<std::string>(id));
    if (result.empty())
    {
        throw std::runtime_error(std::format("Item not found in {}", table));
    }
    auto row = result.front();
    auto item = row.find(name);
    if (item == row.end())
    {
        throw std::runtime_error(std::format("{} not found in row", name));
    }
    if (std::holds_alternative<std::string>(item->second))
    {
        return std::move(std::get<std::string>(item->second));
    }
    else if (std::holds_alternative<std::nullptr_t>(item->second))
    {
        /** If the string is not set, return an empty string. */
        return std::string{};
    }
    else
    {
        throw std::runtime_error("Invalid type");
    }
}

JS::Promise<void> Database::SetStringToChatAsync(
    const Uuid& userId, const Uuid& id, const std::string& name, std::string value)
{
    auto sql = std::format(
        "UPDATE chat SET {} = ?, timestamp = ? WHERE user_id = ? AND id = ?;",
        name);
    co_await _db->ExecAsync(
        sql,
        std::move(value),
        Timestamp::GetWallClock(),
        static_cast<std::string>(userId),
        static_cast<std::string>(id));
}

std::string Database::GetStringFromChat(
    const Uuid& userId, const Uuid& id, const std::string& name)
{
    auto sql = std::format(
        "SELECT {} FROM chat WHERE user_id = ? AND id = ?;",
        name);
    auto result = _db->Exec(
        sql,
        static_cast<std::string>(userId), static_cast<std::string>(id));
    if (result.empty())
    {
        throw std::runtime_error("Chat not found");
    }
    auto row = result.front();
    auto item = row.find(name);
    if (item == row.end())
    {
        throw std::runtime_error(std::format("{} not found in row", name));
    }
    if (std::holds_alternative<std::string>(item->second))
    {
        return std::move(std::get<std::string>(item->second));
    }
    else if (std::holds_alternative<std::nullptr_t>(item->second))
    {
        /** If the string is not set, return an empty string. */
        return std::string{};
    }
    else
    {
        throw std::runtime_error("Invalid type");
    }
}

