#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <list>
#include <variant>
#include <unordered_map>
#include <js-style-co-routine/Promise.h>
#include <tev-cpp/Tev.h>
#include <sqlite3.h>
#include "common/UniqueTypes.h"
#include "common/WorkerThread.h"

namespace TUI::Database
{
    /**
     * Design decisions:
     * 
     * Provide a async interface and a sync interface.
     * Async operations will take place in a separate thread in queue.
     * The async interface is intended for writes, while the sync interface is intended for reads.
     * This avoids writes from blocking reads in WAL mode.
     * 
     * More function wrappings should happen in the upper layers.
     */
    class Sqlite
    {
    public:
        using Value = std::variant<std::nullptr_t, int64_t, double, std::string, std::vector<uint8_t>>;

        static JS::Promise<std::shared_ptr<Sqlite>> CreateAsync(Tev& tev, const std::filesystem::path& dbPath);
        ~Sqlite() = default;

        Sqlite(const Sqlite&) = delete;
        Sqlite& operator=(const Sqlite&) = delete;
        Sqlite(Sqlite&&) noexcept = delete;
        Sqlite& operator=(Sqlite&&) noexcept = delete;

        using ExecResult = std::list<std::unordered_map<std::string, Value>>;
        template<typename... Args>
        JS::Promise<ExecResult> ExecAsync(const std::string& query, Args&&... args)
        {
            /** Use tuple so values are moved/copied efficiently */
            auto tup = std::make_tuple(std::forward<Args>(args)...);
            return _workerThread.ExecTaskAsync([this, query, tup = std::move(tup)]() -> ExecResult {
                return std::apply([&](auto&&... unpackedArgs) {
                    UniqueStmt stmt(_dbAsync, query, std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
                    return ExecInternal(stmt);
                }, std::move(tup));
            });
        }
        template<typename... Args>
        ExecResult Exec(const std::string& query, Args&&... args)
        {
            UniqueStmt stmt(_db, query, std::forward<Args>(args)...);
            return ExecInternal(stmt);
        }

    private:
        class UniqueSqlite3
        {
        public:
            explicit UniqueSqlite3(sqlite3* db);
            ~UniqueSqlite3();

            UniqueSqlite3(const UniqueSqlite3&) = delete;
            UniqueSqlite3& operator=(const UniqueSqlite3&) = delete;
            UniqueSqlite3(UniqueSqlite3&&) noexcept;
            UniqueSqlite3& operator=(UniqueSqlite3&&) noexcept;

            operator sqlite3*() const;
        private:
            sqlite3* _db{nullptr};
        };

        class UniqueStmt
        {
        public:
            template<typename... Args>
            UniqueStmt(sqlite3* db, const std::string& query, Args&&... args)
            {
                sqlite3_stmt* stmt = nullptr;
                if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
                {
                    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
                }
                _stmt = UniqueStmtInternal(stmt);
                int i = 1;
                (BindValue(i++, std::forward<Args>(args)), ...);
            }
            ~UniqueStmt() = default;

            UniqueStmt(const UniqueStmt&) = delete;
            UniqueStmt& operator=(const UniqueStmt&) = delete;
            UniqueStmt(UniqueStmt&&) noexcept = default;
            UniqueStmt& operator=(UniqueStmt&&) noexcept = default;

            operator sqlite3_stmt*() const;
        private:
            class UniqueStmtInternal
            {
            public:
                explicit UniqueStmtInternal(sqlite3_stmt* stmt);
                ~UniqueStmtInternal();
                UniqueStmtInternal(const UniqueStmtInternal&) = delete;
                UniqueStmtInternal& operator=(const UniqueStmtInternal&) = delete;
                UniqueStmtInternal(UniqueStmtInternal&& other) noexcept;
                UniqueStmtInternal& operator=(UniqueStmtInternal&& other) noexcept;

                operator sqlite3_stmt*() const;
            private:
                sqlite3_stmt* _stmt{nullptr};
            };

            void BindValue(int i, std::string&& value);
            void BindValue(int i, const std::string& value);
            void BindValue(int i, std::vector<uint8_t>&& value);
            void BindValue(int i, const std::vector<uint8_t>& value);
            void BindValue(int i, int64_t value);
            void BindValue(int i, double value);
            void BindValue(int i, std::nullptr_t);

            std::list<std::string> _bondStrings{};
            std::list<std::vector<uint8_t>> _bondBlobs{};
            /** Put this last so it is destructed first */
            UniqueStmtInternal _stmt{nullptr};
        };

        static std::string SqliteErrorToMessage(int rc);

        Sqlite(Tev& tev);

        ExecResult ExecInternal(const UniqueStmt& stmt);

        Tev& _tev;
        UniqueSqlite3 _db{nullptr};
        UniqueSqlite3 _dbAsync{nullptr};
        Common::WorkerThread _workerThread;
    };
}
