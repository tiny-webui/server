#include <cerrno>
#include <cstring>
#include <iostream>
#include <linux/perf_event.h>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>

/**
 * To run the perf test. Compile with Release config.
 * And run with root privilege for perf_event_open syscall.
 */

/** This is only to keep the linter happy. Enable this in the CmakeLists.txt */
#ifndef TUI_TEST_INTERFACES
#define TUI_TEST_INTERFACES
#endif

#include <../src/database/VectorSearch.h>
#include "Utility.h"

using namespace TUI::Database;

static void DotProductInt8_Baseline(
    size_t dimension,
    size_t nDataVectors,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores)
{
    for (size_t i = 0; i < nDataVectors; i++)
    {
        int32_t score = 0;
        for (size_t d = 0; d < dimension; d++)
        {
            score += static_cast<int32_t>(queryVector[d]) * static_cast<int32_t>(dataVectors[i * dimension + d]);
        }
        outScores[i] = score;
    }
}

class PerfEventCounter
{
public:
    explicit PerfEventCounter(uint32_t config)
    {
        struct perf_event_attr attr;
        std::memset(&attr, 0, sizeof(attr));
        attr.type = PERF_TYPE_HARDWARE;
        attr.size = sizeof(attr);
        attr.config = config;
        attr.disabled = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;

        _fd = static_cast<int>(syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0));
        if (_fd < 0)
        {
            std::cerr << "perf_event_open failed: " << std::strerror(errno) << std::endl;
        }
    }

    ~PerfEventCounter()
    {
        if (_fd >= 0)
        {
            close(_fd);
        }
    }

    PerfEventCounter(const PerfEventCounter&) = delete;
    PerfEventCounter& operator=(const PerfEventCounter&) = delete;

    void Reset()
    {
        if (_fd < 0)
        {
            return;
        }
        if (ioctl(_fd, PERF_EVENT_IOC_RESET, 0) == -1)
        {
            throw std::runtime_error(std::string("Failed to reset perf counter: ") + std::strerror(errno));
        }
    }

    void Start()
    {
        if (_fd < 0)
        {
            return;
        }
        if (ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0) == -1)
        {
            throw std::runtime_error(std::string("Failed to start perf counter: ") + std::strerror(errno));
        }
    }

    long long StopAndRead()
    {
        if (_fd < 0)
        {
            return 0;
        }
        if (ioctl(_fd, PERF_EVENT_IOC_DISABLE, 0) == -1)
        {
            throw std::runtime_error(std::string("Failed to stop perf counter: ") + std::strerror(errno));
        }
        long long value = 0;
        ssize_t bytes = ::read(_fd, &value, sizeof(value));
        if (bytes != static_cast<ssize_t>(sizeof(value)))
        {
            throw std::runtime_error(std::string("Failed to read perf counter: ") + std::strerror(errno));
        }
        return value;
    }

private:
    int _fd = -1;
};

#ifdef NDEBUG
static void TestDotProductInt8Performance()
{
    constexpr size_t dimension = 1024;
    constexpr size_t nDataVectors = 10'000;

    std::vector<int8_t> dataVectors(nDataVectors * dimension);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(-128, 127);
    for (auto& v : dataVectors)
    {
        v = static_cast<int8_t>(dist(rng));
    }

    std::vector<int32_t> scores(nDataVectors);

    PerfEventCounter cycleCounter(PERF_COUNT_HW_CPU_CYCLES);
    cycleCounter.Reset();
    cycleCounter.Start();

    size_t accumulation = 0;
    for (size_t q = 0; q < nDataVectors; ++q)
    {
        const int8_t* query = dataVectors.data() + q * dimension;
        auto metricFunc = VectorSearch::GetMetricInt8BatchFunction(VectorSearch::DistanceMetric::DOT_PRODUCT);
        metricFunc(
            dimension,
            nDataVectors,
            query,
            dataVectors.data(),
            scores.data());
        accumulation += static_cast<size_t>(scores[0]);
    }

    long long cycles = cycleCounter.StopAndRead();

    const double totalCalculations = static_cast<double>(nDataVectors) * static_cast<double>(nDataVectors);
    const double cyclesPerVector = static_cast<double>(cycles) / totalCalculations;

    std::cout << "Cycle count accumulation guard: " << accumulation << std::endl;
    std::cout << "Total cycles: " << cycles << std::endl;
    std::cout << "Cycles per vector calculation: " << cyclesPerVector << std::endl;

}

static void TestSearchTopKInt8Performance()
{
    constexpr size_t dimension = 1024;
    constexpr size_t nDataVectors = 10'000;
    constexpr size_t topK = 10;

    std::vector<int8_t> dataVectors(nDataVectors * dimension);
    std::mt19937 rng(43);
    std::uniform_int_distribution<int> dist(-128, 127);
    for (auto& v : dataVectors)
    {
        v = static_cast<int8_t>(dist(rng));
    }

    std::unordered_set<size_t> excludeIndices;

    PerfEventCounter cycleCounter(PERF_COUNT_HW_CPU_CYCLES);
    cycleCounter.Reset();
    cycleCounter.Start();

    size_t accumulation = 0;
    for (size_t q = 0; q < nDataVectors; ++q)
    {
        const int8_t* query = dataVectors.data() + q * dimension;
        auto scoreKeeper = VectorSearch::SearchTopKInt8(
            topK,
            dimension,
            VectorSearch::DistanceMetric::DOT_PRODUCT,
            query,
            nDataVectors,
            dataVectors.data(),
            excludeIndices);

        auto results = scoreKeeper.GetResultsAndClear();
        if (!results.empty())
        {
            accumulation += static_cast<size_t>(results.front().index);
        }
    }

    long long cycles = cycleCounter.StopAndRead();

    const double totalCalculations = static_cast<double>(nDataVectors) * static_cast<double>(nDataVectors);
    const double cyclesPerVector = static_cast<double>(cycles) / totalCalculations;

    std::cout << "Cycle count accumulation guard: " << accumulation << std::endl;
    std::cout << "Total cycles: " << cycles << std::endl;
    std::cout << "Cycles per vector calculation: " << cyclesPerVector << std::endl;
}

static void TestSearchTopKInt8MapPerformance()
{
    constexpr size_t dimension = 1024;
    constexpr size_t nDataVectors = 10'000;
    constexpr size_t topK = 10;

    std::unordered_map<size_t, std::vector<int8_t>> dataVectorMap;
    dataVectorMap.reserve(nDataVectors * 2);
    std::vector<size_t> keys;
    keys.reserve(nDataVectors);

    std::mt19937 rng(46);
    std::uniform_int_distribution<int> dist(-128, 127);
    for (size_t i = 0; i < nDataVectors; ++i)
    {
        std::vector<int8_t> vec(dimension);
        for (auto& v : vec)
        {
            v = static_cast<int8_t>(dist(rng));
        }
        keys.push_back(i);
        dataVectorMap.emplace(i, std::move(vec));
    }

    PerfEventCounter cycleCounter(PERF_COUNT_HW_CPU_CYCLES);
    cycleCounter.Reset();
    cycleCounter.Start();

    size_t accumulation = 0;
    for (size_t q = 0; q < nDataVectors; ++q)
    {
        size_t key = keys[q];
        const int8_t* query = dataVectorMap[key].data();

        auto scoreKeeper = VectorSearch::SearchTopKInt8(
            topK,
            dimension,
            VectorSearch::DistanceMetric::DOT_PRODUCT,
            query,
            dataVectorMap);

        auto results = scoreKeeper.GetResultsAndClear();
        if (!results.empty())
        {
            accumulation += static_cast<size_t>(results.front().index);
        }
    }

    long long cycles = cycleCounter.StopAndRead();

    const double totalCalculations = static_cast<double>(nDataVectors) * static_cast<double>(nDataVectors);
    const double cyclesPerVector = static_cast<double>(cycles) / totalCalculations;

    std::cout << "Cycle count accumulation guard: " << accumulation << std::endl;
    std::cout << "Total cycles: " << cycles << std::endl;
    std::cout << "Cycles per vector calculation: " << cyclesPerVector << std::endl;
}

#endif

static void TestDotProductInt8Correctness()
{
    struct TestCase
    {
        size_t dimension;
        size_t nDataVectors;
        uint32_t seed;
    };

    // Mix dimensions and dataset sizes to exercise remainder handling in both loops.
    std::vector<TestCase> cases = {
        {31, 9, 123},     // dimension not multiple of 32, dataset smaller than AVX2 batch.
        {1031, 10, 456},  // large dimension with remainder, dataset not multiple of 8/16.
        {64, 17, 789},    // dimension aligned, dataset forces final partial batch.
    };

    for (const auto& c : cases)
    {
        std::vector<int8_t> data(c.nDataVectors * c.dimension);
        std::mt19937 rng(c.seed);
        std::uniform_int_distribution<int> dist(-128, 127);
        for (auto& v : data)
        {
            v = static_cast<int8_t>(dist(rng));
        }

        std::vector<int32_t> baseline(c.nDataVectors);
        std::vector<int32_t> candidate(c.nDataVectors);

        for (size_t q = 0; q < c.nDataVectors; ++q)
        {
            const int8_t* query = data.data() + q * c.dimension;
            DotProductInt8_Baseline(
                c.dimension,
                c.nDataVectors,
                query,
                data.data(),
                baseline.data());

            auto metricFunc = VectorSearch::GetMetricInt8BatchFunction(VectorSearch::DistanceMetric::DOT_PRODUCT);
            metricFunc(
                c.dimension,
                c.nDataVectors,
                query,
                data.data(),
                candidate.data());

            for (size_t i = 0; i < c.nDataVectors; ++i)
            {
                if (baseline[i] != candidate[i])
                {
                    AssertWithMessage(false, "Mismatch case dimension=" + std::to_string(c.dimension) +
                        " nDataVectors=" + std::to_string(c.nDataVectors) +
                        " query=" + std::to_string(q) +
                        " index=" + std::to_string(i) +
                        " baseline=" + std::to_string(baseline[i]) +
                        " candidate=" + std::to_string(candidate[i]));
                }
            }
        }
    }
}

static void TestSearchTopKInt8Correctness()
{
    constexpr size_t dimension = 1024;
    constexpr size_t nDataVectors = 1000;
    constexpr size_t topK = 10;

    std::vector<int8_t> dataVectors(nDataVectors * dimension, 0);
    std::mt19937 rng(44);
    std::uniform_int_distribution<int> dist(-128, 127);
    for (auto& v : dataVectors)
    {
        v = static_cast<int8_t>(dist(rng));
    }

    std::unordered_set<size_t> excludeIndices;

    for (size_t q = 0; q < nDataVectors; ++q)
    {
        const int8_t* query = dataVectors.data() + q * dimension;
        auto scoreKeeper = VectorSearch::SearchTopKInt8(
            topK,
            dimension,
            VectorSearch::DistanceMetric::DOT_PRODUCT,
            query,
            nDataVectors,
            dataVectors.data(),
            excludeIndices);

        auto results = scoreKeeper.GetResultsAndClear();

        AssertWithMessage(results.size() == topK, "Unexpected number of results: " + std::to_string(results.size()));
        AssertWithMessage(!results.empty() && results.front().index == q,
            "Top-1 mismatch: query=" + std::to_string(q) +
            " got=" + std::to_string(results.front().index));
    }
}

static void TestSearchTopInt8ExcludeIndices()
{
    constexpr size_t dimension = 1024;
    constexpr size_t nDataVectors = 1000;
    constexpr size_t topK = 10;

    std::vector<int8_t> dataVectors(nDataVectors * dimension, 0);
    std::mt19937 rng(45);
    std::uniform_int_distribution<int> dist(-128, 127);
    for (auto& v : dataVectors)
    {
        v = static_cast<int8_t>(dist(rng));
    }

    for (size_t q = 0; q < nDataVectors; ++q)
    {
        std::unordered_set<size_t> excludeIndices = { q, (q + 1) % nDataVectors };
        const int8_t* query = dataVectors.data() + q * dimension;

        auto scoreKeeper = VectorSearch::SearchTopKInt8(
            topK,
            dimension,
            VectorSearch::DistanceMetric::DOT_PRODUCT,
            query,
            nDataVectors,
            dataVectors.data(),
            excludeIndices);

        auto results = scoreKeeper.GetResultsAndClear();
        AssertWithMessage(results.size() == topK, "Unexpected number of results: " + std::to_string(results.size()));

        for (const auto& scoredIndex : results)
        {
            size_t idx = static_cast<size_t>(scoredIndex.index);
            AssertWithMessage(excludeIndices.find(idx) == excludeIndices.end(),
                "Excluded index returned: query=" + std::to_string(q) +
                " index=" + std::to_string(idx));
        }
    }
}

static void TestSearchTopKInt8InitialScores()
{
    constexpr size_t dimension = 4;
    constexpr size_t nDataVectors = 3;
    constexpr size_t topK = 3;

    const int8_t query[dimension] = { 1, 1, 1, 1 };
    const int8_t dataVectors[nDataVectors * dimension] = {
        1, 1, 1, 1,
        2, 2, 2, 2,
        3, 3, 3, 3,
    };

    VectorSearch::ScoreKeeper<size_t> initialScores(
        topK, VectorSearch::GetScoreMode(VectorSearch::DistanceMetric::DOT_PRODUCT));
    initialScores.AddScore(10, 99);
    initialScores.AddScore(5, 98);

    std::unordered_set<size_t> excludeIndices;

    auto scoreKeeper = VectorSearch::SearchTopKInt8(
        topK,
        dimension,
        VectorSearch::DistanceMetric::DOT_PRODUCT,
        query,
        nDataVectors,
        dataVectors,
        excludeIndices,
        std::move(initialScores));

    auto resultsList = scoreKeeper.GetResultsAndClear();
    std::vector<VectorSearch::ScoredIndex<size_t>> results(resultsList.begin(), resultsList.end());

    const std::vector<size_t> expectedIndices = { 2, 99, 1 };
    const std::vector<int32_t> expectedScores = { 12, 10, 8 };

    AssertWithMessage(results.size() == expectedIndices.size(),
        "Unexpected number of results: " + std::to_string(results.size()));

    for (size_t i = 0; i < expectedIndices.size(); ++i)
    {
        AssertWithMessage(results[i].index == expectedIndices[i],
            "Index mismatch at position " + std::to_string(i) +
            " expected=" + std::to_string(expectedIndices[i]) +
            " got=" + std::to_string(results[i].index));
        AssertWithMessage(results[i].score == expectedScores[i],
            "Score mismatch at position " + std::to_string(i) +
            " expected=" + std::to_string(expectedScores[i]) +
            " got=" + std::to_string(results[i].score));
    }
}

static void TestSearchTopKInt8MapRandomCorrectness()
{
    constexpr size_t dimension = 1024;
    constexpr size_t nDataVectors = 1000;
    constexpr size_t topK = 10;

    std::unordered_map<size_t, std::vector<int8_t>> dataVectorMap;
    dataVectorMap.reserve(nDataVectors * 2);

    std::mt19937 rng(47);
    std::uniform_int_distribution<int> dist(-128, 127);
    for (size_t i = 0; i < nDataVectors; ++i)
    {
        std::vector<int8_t> vec(dimension);
        for (auto& v : vec)
        {
            v = static_cast<int8_t>(dist(rng));
        }
        dataVectorMap.emplace(i, std::move(vec));
    }

    for (size_t q = 0; q < nDataVectors; ++q)
    {
        const int8_t* query = dataVectorMap[q].data();
        auto scoreKeeper = VectorSearch::SearchTopKInt8(
            topK,
            dimension,
            VectorSearch::DistanceMetric::DOT_PRODUCT,
            query,
            dataVectorMap);

        auto results = scoreKeeper.GetResultsAndClear();

        AssertWithMessage(results.size() == topK, "Unexpected number of results: " + std::to_string(results.size()));
        AssertWithMessage(!results.empty() && results.front().index == q,
            "Top-1 mismatch: query=" + std::to_string(q) +
            " got=" + std::to_string(results.front().index));
    }
}

static void TestSearchTopKInt8MapInitialScores()
{
    constexpr size_t dimension = 4;
    constexpr size_t topK = 3;

    const int8_t query[dimension] = { 1, 1, 1, 1 };

    std::unordered_map<size_t, std::vector<int8_t>> dataVectorMap = {
        { 21, { 4, 4, 4, 4 } },
        { 13, { 3, 3, 3, 3 } },
        { 11, { 2, 2, 2, 2 } },
        { 7,  { 1, 1, 1, 1 } },
        { 5,  { -1, -1, -1, -1 } },
    };

    VectorSearch::ScoreKeeper<size_t> initialScores(
        topK, VectorSearch::GetScoreMode(VectorSearch::DistanceMetric::DOT_PRODUCT));
    initialScores.AddScore(10, 99);
    initialScores.AddScore(5, 98);
    auto initialOpt = std::optional<VectorSearch::ScoreKeeper<size_t>>(std::move(initialScores));

    auto scoreKeeper = VectorSearch::SearchTopKInt8(
        topK,
        dimension,
        VectorSearch::DistanceMetric::DOT_PRODUCT,
        query,
        dataVectorMap,
        std::move(initialOpt));

    auto resultsList = scoreKeeper.GetResultsAndClear();
    std::vector<VectorSearch::ScoredIndex<size_t>> results(resultsList.begin(), resultsList.end());

    const std::vector<size_t> expectedIndices = { 21, 13, 99 };
    const std::vector<int32_t> expectedScores = { 16, 12, 10 };

    AssertWithMessage(results.size() == expectedIndices.size(),
        "Unexpected number of results: " + std::to_string(results.size()));

    for (size_t i = 0; i < expectedIndices.size(); ++i)
    {
        AssertWithMessage(results[i].index == expectedIndices[i],
            "Index mismatch at position " + std::to_string(i) +
            " expected=" + std::to_string(expectedIndices[i]) +
            " got=" + std::to_string(results[i].index));
        AssertWithMessage(results[i].score == expectedScores[i],
            "Score mismatch at position " + std::to_string(i) +
            " expected=" + std::to_string(expectedScores[i]) +
            " got=" + std::to_string(results[i].score));
    }
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

#ifdef NDEBUG
    RunTest(TestDotProductInt8Performance());
    RunTest(TestSearchTopKInt8Performance());
    RunTest(TestSearchTopKInt8MapPerformance());
#endif // NDEBUG
    RunTest(TestDotProductInt8Correctness());
    RunTest(TestSearchTopKInt8Correctness());
    RunTest(TestSearchTopInt8ExcludeIndices());
    RunTest(TestSearchTopKInt8InitialScores());
    RunTest(TestSearchTopKInt8MapInitialScores());
    RunTest(TestSearchTopKInt8MapRandomCorrectness());

    return 0;
}

