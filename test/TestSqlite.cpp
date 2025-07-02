#include <iostream>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include "database/Sqlite.h"
#include "Utility.h"

using namespace TUI::Database;

Tev tev{};
std::string dbPath{};

JS::Promise<void> TestCreateAsync()
{
    auto db = co_await Sqlite::CreateAsync(tev, dbPath);
}

JS::Promise<void> TestOperationsAsync()
{
    /** reopen the database */
    auto db = co_await Sqlite::CreateAsync(tev, dbPath);
    /** Create a table */
    co_await db->ExecAsync("CREATE TABLE IF NOT EXISTS test (text TEXT PRIMARY KEY, double FLOAT, int64 INTEGER, blob BLOB);");
    /** Insert some data */
    co_await db->ExecAsync("INSERT INTO test (text, double, int64, blob) VALUES (?, ?, ?, ?);",
                           "test1", 3.14, static_cast<int64_t>(42), std::vector<uint8_t>{0x01, 0x02, 0x03});
    /** Insert data with NULL values */
    co_await db->ExecAsync("INSERT INTO test (text, double, int64, blob) VALUES (?, ?, ?, ?);",
                           "test2", nullptr, nullptr, nullptr);
    /** Select data */
    auto result = co_await db->ExecAsync("SELECT * FROM test;");
    for (const auto& row : result)
    {
        std::cout << "Row:" << std::endl;
        for (const auto& [columnName, value] : row)
        {
            std::cout << "    " << columnName << ": ";
            if (std::holds_alternative<std::nullptr_t>(value))
            {
                std::cout << "NULL";
            }
            else if (std::holds_alternative<std::string>(value))
            {
                std::cout << std::get<std::string>(value);
            }
            else if (std::holds_alternative<int64_t>(value))
            {
                std::cout << std::get<int64_t>(value);
            }
            else if (std::holds_alternative<double>(value))
            {
                std::cout << std::get<double>(value);
            }
            else if (std::holds_alternative<std::vector<uint8_t>>(value))
            {
                const auto& blob = std::get<std::vector<uint8_t>>(value);
                std::cout << "BLOB of size " << blob.size();
            }
            else
            {
                std::cout << "Unknown type";
            }
            std::cout << std::endl;
        }
    }
    /** Concurrent operation */
    auto writePromise = db->ExecAsync("INSERT INTO test (text, double, int64, blob) VALUES (?, ?, ?, ?);",
                                   "test3", 2.71, static_cast<int64_t>(84), std::vector<uint8_t>{0x04, 0x05, 0x06});
    auto result2 = co_await db->ExecAsync("SELECT * FROM test WHERE text = ?;", "test3");
    co_await writePromise;
}

JS::Promise<void> TestAsync()
{
    RunAsyncTest(TestCreateAsync());
    RunAsyncTest(TestOperationsAsync());
}

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <db_path>" << std::endl;
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


