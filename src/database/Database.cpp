#include <format>
#include "Database.h"

using namespace TUI::Common;
using namespace TUI::Database;

JS::Promise<std::shared_ptr<Database>> Database::CreateAsync(Tev& tev, const std::filesystem::path& dbPath)
{
    auto db = std::shared_ptr<Database>(new Database());
    db->_db = co_await Sqlite::CreateAsync(tev, dbPath);
    /** Create tables */
    co_await db->_db->ExecAsync(
        "CREATE TABLE IF NOT EXISTS model ("
        "id TEXT PRIMARY KEY, "
        "provider TEXT, "
        "metadata TEXT, "
        "params TEXT);");
    co_await db->_db->ExecAsync(
        "CREATE TABLE IF NOT EXISTS user ("
        "id TEXT PRIMARY KEY, "
        "username TEXT UNIQUE, "
        "metadata TEXT, "
        "credential TEXT);");
    co_await db->_db->ExecAsync(
        "CREATE TABLE IF NOT EXISTS chat ("
        "timestamp INTEGER, "
        "user_id TEXT, "
        "id TEXT, "
        "metadata TEXT, "
        "content TEXT, "
        "PRIMARY KEY (user_id, id));");
    co_return db;
}

JS::Promise<Uuid> Database::CreateModelAsync(const std::string& provider)
{
    Uuid id{};
    co_await _db->ExecAsync(
        "INSERT INTO model (id, provider) VALUES (?, ?);",
        static_cast<std::string>(id), provider);
    co_return id;
}

JS::Promise<void> Database::DeleteModelAsync(const Common::Uuid& id)
{
    co_await _db->ExecAsync(
        "DELETE FROM model WHERE id = ?;",
        static_cast<std::string>(id));
}

std::list<Uuid> Database::ListModel()
{
    return ListTableId("model");
}

std::list<std::pair<Uuid, std::string>> Database::ListModelWithMetadata()
{
    return ListTableIdWithMetadata("model");
}

JS::Promise<void> Database::SetModelMetadataAsync(const Uuid& id, std::string metadata)
{
    return SetStringToTableById("model", id, "metadata", std::move(metadata));
}

std::string Database::GetModelMetadata(const Uuid& id)
{
    return GetStringFromTableById("model", id, "metadata");
}

JS::Promise<void> Database::SetModelParamsAsync(const Uuid& id, std::string params)
{
    return SetStringToTableById("model", id, "params", std::move(params));
}

std::string Database::GetModelParams(const Uuid& id)
{
    return GetStringFromTableById("model", id, "params");
}

std::string Database::GetModelProvider(const Uuid& id)
{
    return GetStringFromTableById("model", id, "provider");
}

JS::Promise<Uuid> Database::CreateUserAsync(const std::string& username)
{
    Uuid id{};
    co_await _db->ExecAsync(
        "INSERT INTO user (id, username) VALUES (?, ?);",
        static_cast<std::string>(id), username);
    co_return id;
}

JS::Promise<void> Database::DeleteUserAsync(const Uuid& id)
{
    co_await _db->ExecAsync(
        "DELETE FROM user WHERE id = ?",
        static_cast<std::string>(id));
    co_await _db->ExecAsync(
        "DELETE FROM chat WHERE user_id = ?",
        static_cast<std::string>(id));
}

std::list<Uuid> Database::ListUser()
{
    return ListTableId("user");
}

std::list<std::pair<Uuid, std::string>> Database::ListUserWithMetadata()
{
    return ListTableIdWithMetadata("user");
}

JS::Promise<void> Database::SetUserMetadataAsync(const Uuid& id, std::string metadata)
{
    return SetStringToTableById("user", id, "metadata", std::move(metadata));
}

std::string Database::GetUserMetadata(const Uuid& id)
{
    return GetStringFromTableById("user", id, "metadata");
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
    return Uuid(item->second.Get<std::string>());
}

JS::Promise<Uuid> Database::CreateChatAsync(const Uuid& userId)
{
    Uuid id{};
    co_await _db->ExecAsync(
        "INSERT INTO chat (timestamp, user_id, id) VALUES(?, ?, ?);",
        GetTimestamp(), static_cast<std::string>(userId), static_cast<std::string>(id));
    co_return id;
}

JS::Promise<void> Database::DeleteChatAsync(const Uuid& userId, const Uuid& id)
{
    co_await _db->ExecAsync(
        "DELETE FROM chat WHERE user_id = ? AND id = ?;",
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
    return static_cast<size_t>(item->second.Get<int64_t>());
}

std::list<Uuid> Database::ListChat(const Uuid& userId, size_t from, size_t limit)
{
    auto sql = std::format(
        "SELECT id FROM chat WHERE user_id = ? ORDER BY timestamp DESC LIMIT {} OFFSET {}",
        limit, from);
    auto result = _db->Exec(sql, static_cast<std::string>(userId));
    return ParseListTableIdResult(result);
}

std::list<std::pair<Uuid, std::string>> Database::ListChatWithMetadata(
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

JS::Promise<void> Database::SetChatContentAsync(const Uuid& userId, const Uuid& id, std::string content)
{
    return SetStringToChatAsync(userId, id, "content", content);
}

std::string Database::GetChatContent(const Uuid& userId, const Uuid& id)
{
    return GetStringFromChat(userId, id, "content");
}

std::list<Uuid> Database::ListTableId(const std::string& table)
{
    auto sql = std::format("SELECT id from {};", table);
    auto result = _db->Exec(sql);
    return ParseListTableIdResult(result);
}

std::list<Uuid> Database::ParseListTableIdResult(Sqlite::ExecResult& result)
{
    std::list<Uuid> list{};
    for (const auto& row : result)
    {
        try
        {
            auto item = row.find("id");
            if (item == row.end())
            {
                continue;
            }
            list.emplace_back(Uuid(item->second.template Get<std::string>()));
        }
        catch(...)
        {
            /** @todo log */
            /** Ignored. Avoid corrupted data from corrupting the whole application. */
        }
    }
    return list;
}

std::list<std::pair<Uuid, std::string>> Database::ListTableIdWithMetadata(
    const std::string& table)
{
    auto sql = std::format("SELECT id, metadata FROM {};", table);
    auto result = _db->Exec(sql);
    return ParseListTableIdWithMetadataResult(result);
}

std::list<std::pair<Uuid, std::string>> Database::ParseListTableIdWithMetadataResult(
    Sqlite::ExecResult& result)
{
    std::list<std::pair<Uuid, std::string>> list{};
    for (const auto& row : result)
    {
        try
        {
            auto idItem = row.find("id");
            if (idItem == row.end())
            {
                continue;
            }
            Uuid id{idItem->second.template Get<std::string>()};
            auto metadataItem = row.find("metadata");
            if (metadataItem == row.end())
            {
                continue;
            }
            if (metadataItem->second.GetType() == Sqlite::Value::Type::text)
            {
                list.emplace_back(id, std::move(metadataItem->second));
            }
            else if (metadataItem->second.GetType() == Sqlite::Value::Type::null)
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
    if (item->second.GetType() == Sqlite::Value::Type::text)
    {
        return static_cast<std::string>(item->second);
    }
    else if (item->second.GetType() == Sqlite::Value::Type::null)
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
        std::move(value), GetTimestamp(), static_cast<std::string>(userId), static_cast<std::string>(id));
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
    if (item->second.GetType() == Sqlite::Value::Type::text)
    {
        return static_cast<std::string>(item->second);
    }
    else if (item->second.GetType() == Sqlite::Value::Type::null)
    {
        /** If the string is not set, return an empty string. */
        return std::string{};
    }
    else
    {
        throw std::runtime_error("Invalid type");
    }
}

int64_t Database::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}
