#include "Utility.h"
#include <algorithm>
#include <filesystem>
#include <list>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include "database/VectorDatabase.h"

using namespace TUI::Database;

constexpr size_t VECTOR_DIMENSION = 1024;
constexpr size_t WAL_SIZE_SOFT_LIMIT = 256 * 1024; /** 256KiB */

JS::Promise<void> TestCreateNewAsync(Tev& tev, std::string dbPath)
{
    auto db = co_await VectorDatabase::CreateAsync(
        tev,
        dbPath,
        VECTOR_DIMENSION,
        VectorDatabase::DataType::INT8,
        WAL_SIZE_SOFT_LIMIT);
    AssertWithMessage(db->IsNewlyCreated(), "Database should be newly created");
}

JS::Promise<void> TestReopenAsync(Tev& tev, std::string dbPath)
{
    auto db = co_await VectorDatabase::CreateAsync(
        tev,
        dbPath,
        VECTOR_DIMENSION,
        VectorDatabase::DataType::INT8,
        WAL_SIZE_SOFT_LIMIT);
    AssertWithMessage(!db->IsNewlyCreated(), "Database should not be newly created");
}

JS::Promise<void> TestBasicOperationsAsync(Tev& tev, std::string dbPath)
{
    auto db = co_await VectorDatabase::CreateAsync(
        tev,
        dbPath,
        VECTOR_DIMENSION,
        VectorDatabase::DataType::INT8,
        WAL_SIZE_SOFT_LIMIT);
    AssertWithMessage(!db->IsNewlyCreated(), "Database should not be newly created");

    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> dist(-128, 127);

    std::unordered_map<VectorDatabase::IndexType, std::vector<int8_t>> indexToVector;
    indexToVector.reserve(10);

    for (int i = 0; i < 10; ++i)
    {
        std::vector<int8_t> vec(VECTOR_DIMENSION);
        for (auto& v : vec)
        {
            v = static_cast<int8_t>(dist(rng));
        }
        auto [index, suggestsCompact] = co_await db->InsertVectorAsync(vec);
        (void)suggestsCompact;
        indexToVector.emplace(index, vec);
    }

    AssertWithMessage(!indexToVector.empty(), "At least one vector must be inserted");

    const auto queryIt = indexToVector.begin();
    const auto queryIndex = queryIt->first;
    const auto& queryVec = queryIt->second;

    auto searchResults = co_await db->SearchTopKAsync(3, queryVec);
    AssertWithMessage(searchResults.size() == 3, "Search should return top 3 results");
    AssertWithMessage(searchResults.front() == queryIndex, "First search result should be the inserted vector itself");

    bool suggestsCompact = co_await db->DeleteVectorAsync(queryIndex);
    (void)suggestsCompact;

    auto searchResultsAfterDelete = co_await db->SearchTopKAsync(3, queryVec);
    AssertWithMessage(searchResultsAfterDelete.size() == 3, "Search should return top 3 results after deletion");
    auto it = std::find(searchResultsAfterDelete.begin(), searchResultsAfterDelete.end(), queryIndex);
    AssertWithMessage(it == searchResultsAfterDelete.end(), "Deleted vector should not appear in search results");
}

JS::Promise<void> TestCompactAsync(Tev& tev, std::string dbPath)
{
    auto db = co_await VectorDatabase::CreateAsync(
        tev,
        dbPath,
        VECTOR_DIMENSION,
        VectorDatabase::DataType::INT8,
        WAL_SIZE_SOFT_LIMIT);
    AssertWithMessage(!db->IsNewlyCreated(), "Database should not be newly created");

    std::mt19937 rng(4321);
    std::uniform_int_distribution<int> valueDist(-128, 127);
    std::uniform_int_distribution<int> opDist(0, 99);

    auto makeVector = [&]() {
        std::vector<int8_t> vec(VECTOR_DIMENSION);
        for (auto& v : vec)
        {
            v = static_cast<int8_t>(valueDist(rng));
        }
        return vec;
    };

    std::unordered_map<VectorDatabase::IndexType, std::vector<int8_t>> activeVectors;
    std::list<std::pair<VectorDatabase::IndexType, std::vector<int8_t>>> deletedVectors;
    std::unordered_set<VectorDatabase::IndexType> deletedIndices;

    bool suggested = false;
    for (int op = 0; op < 4000 && !suggested; ++op)
    {
        bool doInsert = activeVectors.empty() || opDist(rng) < 80;
        if (doInsert)
        {
            auto vec = makeVector();
            auto [idx, suggests] = co_await db->InsertVectorAsync(vec);
            activeVectors.emplace(idx, std::move(vec));
            suggested = suggests;
        }
        else
        {
            auto it = activeVectors.begin();
            if (activeVectors.size() > 1)
            {
                std::uniform_int_distribution<size_t> idxDist(0, activeVectors.size() - 1);
                size_t steps = idxDist(rng);
                std::advance(it, steps);
            }
            auto idx = it->first;
            auto vec = it->second;
            bool suggests = co_await db->DeleteVectorAsync(idx);
            deletedVectors.emplace_back(idx, std::move(vec));
            deletedIndices.insert(idx);
            activeVectors.erase(it);
            suggested = suggests;
        }
    }

    AssertWithMessage(suggested, "First phase should suggest compact");
    co_await db->CompactAsync();

    if (activeVectors.empty())
    {
        auto vec = makeVector();
        auto [idx, suggests] = co_await db->InsertVectorAsync(vec);
        (void)suggests;
        activeVectors.emplace(idx, std::move(vec));
    }

    for (const auto& [idx, vec] : activeVectors)
    {
        auto results = co_await db->SearchTopKAsync(3, vec);
        AssertWithMessage(!results.empty(), "Search should return results for active vectors");
        AssertWithMessage(results.front() == idx, "Active vector should be top result");
    }

    for (const auto& [idx, vec] : deletedVectors)
    {
        AssertWithMessage(deletedIndices.contains(idx), "Deleted index should be tracked");
        auto results = co_await db->SearchTopKAsync(3, vec);
        auto it = std::find(results.begin(), results.end(), idx);
        AssertWithMessage(it == results.end(), "Deleted vector should not be returned");
    }

    suggested = false;
    for (int op = 0; op < 4000 && !suggested; ++op)
    {
        bool doInsert = activeVectors.empty() || opDist(rng) < 80;
        if (doInsert)
        {
            auto vec = makeVector();
            auto [idx, suggests] = co_await db->InsertVectorAsync(vec);
            activeVectors.emplace(idx, std::move(vec));
            suggested = suggests;
        }
        else
        {
            auto it = activeVectors.begin();
            if (activeVectors.size() > 1)
            {
                std::uniform_int_distribution<size_t> idxDist(0, activeVectors.size() - 1);
                size_t steps = idxDist(rng);
                std::advance(it, steps);
            }
            auto idx = it->first;
            auto vec = it->second;
            bool suggests = co_await db->DeleteVectorAsync(idx);
            deletedVectors.emplace_back(idx, std::move(vec));
            deletedIndices.insert(idx);
            activeVectors.erase(it);
            suggested = suggests;
        }
    }

    AssertWithMessage(suggested, "Second phase should suggest compact");
    co_await db->CompactAsync();

    if (activeVectors.empty())
    {
        auto vec = makeVector();
        auto [idx, suggests] = co_await db->InsertVectorAsync(vec);
        (void)suggests;
        activeVectors.emplace(idx, std::move(vec));
    }

    for (const auto& [idx, vec] : activeVectors)
    {
        auto results = co_await db->SearchTopKAsync(3, vec);
        AssertWithMessage(!results.empty(), "Search should return results for active vectors");
        AssertWithMessage(results.front() == idx, "Active vector should be top result");
    }

    for (const auto& [idx, vec] : deletedVectors)
    {
        AssertWithMessage(deletedIndices.contains(idx), "Deleted index should be tracked");
        auto results = co_await db->SearchTopKAsync(3, vec);
        auto it = std::find(results.begin(), results.end(), idx);
        AssertWithMessage(it == results.end(), "Deleted vector should not be returned");
    }

    suggested = false;
    for (int op = 0; op < 4000 && !suggested; ++op)
    {
        bool doInsert = activeVectors.empty() || opDist(rng) < 80;
        if (doInsert)
        {
            auto vec = makeVector();
            auto [idx, suggests] = co_await db->InsertVectorAsync(vec);
            activeVectors.emplace(idx, std::move(vec));
            suggested = suggests;
        }
        else
        {
            auto it = activeVectors.begin();
            if (activeVectors.size() > 1)
            {
                std::uniform_int_distribution<size_t> idxDist(0, activeVectors.size() - 1);
                size_t steps = idxDist(rng);
                std::advance(it, steps);
            }
            auto idx = it->first;
            auto vec = it->second;
            bool suggests = co_await db->DeleteVectorAsync(idx);
            deletedVectors.emplace_back(idx, std::move(vec));
            deletedIndices.insert(idx);
            activeVectors.erase(it);
            suggested = suggests;
        }
    }

    AssertWithMessage(suggested, "Thrid phase should suggest compact");

    if (activeVectors.empty())
    {
        auto vec = makeVector();
        auto [idx, suggests] = co_await db->InsertVectorAsync(vec);
        (void)suggests;
        activeVectors.emplace(idx, std::move(vec));
    }

    for (const auto& [idx, vec] : activeVectors)
    {
        auto results = co_await db->SearchTopKAsync(3, vec);
        AssertWithMessage(!results.empty(), "Search should return results for active vectors");
        AssertWithMessage(results.front() == idx, "Active vector should be top result");
    }

    for (const auto& [idx, vec] : deletedVectors)
    {
        AssertWithMessage(deletedIndices.contains(idx), "Deleted index should be tracked");
        auto results = co_await db->SearchTopKAsync(3, vec);
        auto it = std::find(results.begin(), results.end(), idx);
        AssertWithMessage(it == results.end(), "Deleted vector should not be returned");
    }

    db.reset();

    db = co_await VectorDatabase::CreateAsync(
        tev,
        dbPath,
        VECTOR_DIMENSION,
        VectorDatabase::DataType::INT8,
        WAL_SIZE_SOFT_LIMIT);
    AssertWithMessage(!db->IsNewlyCreated(), "Database should reopen existing data");

    for (const auto& [idx, vec] : activeVectors)
    {
        auto results = co_await db->SearchTopKAsync(3, vec);
        AssertWithMessage(!results.empty(), "Search should return results for active vectors after reopen");
        AssertWithMessage(results.front() == idx, "Active vector should be top result after reopen");
    }

    for (const auto& [idx, vec] : deletedVectors)
    {
        AssertWithMessage(deletedIndices.contains(idx), "Deleted index should be tracked after reopen");
        auto results = co_await db->SearchTopKAsync(3, vec);
        auto it = std::find(results.begin(), results.end(), idx);
        AssertWithMessage(it == results.end(), "Deleted vector should not be returned after reopen");
    }

}

JS::Promise<void> TestAsync(Tev& tev, std::string dbPath)
{
    RunAsyncTest(TestCreateNewAsync(tev, dbPath));
    RunAsyncTest(TestReopenAsync(tev, dbPath));
    RunAsyncTest(TestBasicOperationsAsync(tev, dbPath));
    RunAsyncTest(TestCompactAsync(tev, dbPath));
}

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <database_path>" << std::endl;
        return 1;
    }

    std::string dbPath(argv[1]);
    /** Delete the old database */
    if (std::filesystem::exists(dbPath))
    {
        std::filesystem::remove(dbPath);
    }
    if (std::filesystem::exists(dbPath + "-wal"))
    {
        std::filesystem::remove(dbPath + "-wal");
    }
    if (std::filesystem::exists(dbPath + "-index"))
    {
        std::filesystem::remove(dbPath + "-index");
    }

    Tev tev{};

    TestAsync(tev, std::move(dbPath));

    tev.MainLoop();

    return 0;
}

