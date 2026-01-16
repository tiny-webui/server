#include <cerrno>
#include <cstring>
#include <iostream>
#include <linux/perf_event.h>
#include <random>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_set>
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

using namespace TUI::Database::VectorSearch;

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
            throw std::runtime_error(std::string("perf_event_open failed: ") + std::strerror(errno));
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
        if (ioctl(_fd, PERF_EVENT_IOC_RESET, 0) == -1)
        {
            throw std::runtime_error(std::string("Failed to reset perf counter: ") + std::strerror(errno));
        }
    }

    void Start()
    {
        if (ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0) == -1)
        {
            throw std::runtime_error(std::string("Failed to start perf counter: ") + std::strerror(errno));
        }
    }

    long long StopAndRead()
    {
        if (ioctl(_fd, PERF_EVENT_IOC_DISABLE, 0) == -1)
        {
            throw std::runtime_error(std::string("Failed to stop perf counter: ") + std::strerror(errno));
        }
        return Read();
    }

private:
    long long Read() const
    {
        long long value = 0;
        ssize_t bytes = ::read(_fd, &value, sizeof(value));
        if (bytes != static_cast<ssize_t>(sizeof(value)))
        {
            throw std::runtime_error(std::string("Failed to read perf counter: ") + std::strerror(errno));
        }
        return value;
    }

    int _fd = -1;
};

#ifdef NDEBUG
static void TestDotProductInt8Performance()
{
    constexpr size_t kDimension = 1024;
    constexpr size_t kDataVectors = 10'000;

    std::vector<int8_t> dataVectors(kDataVectors * kDimension);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(-128, 127);
    for (auto& v : dataVectors)
    {
        v = static_cast<int8_t>(dist(rng));
    }

    std::vector<int32_t> scores(kDataVectors);

    PerfEventCounter cycleCounter(PERF_COUNT_HW_CPU_CYCLES);
    cycleCounter.Reset();
    cycleCounter.Start();

    size_t accumulation = 0;
    for (size_t q = 0; q < kDataVectors; ++q)
    {
        const int8_t* query = dataVectors.data() + q * kDimension;
        TestMetricCalculation(
            kDimension,
            kDataVectors,
            DistanceMetric::DOT_PRODUCT,
            query,
            dataVectors.data(),
            scores.data());
        accumulation += static_cast<size_t>(scores[0]);
    }

    long long cycles = cycleCounter.StopAndRead();

    const double totalCalculations = static_cast<double>(kDataVectors) * static_cast<double>(kDataVectors);
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

            TestMetricCalculation(
                c.dimension,
                c.nDataVectors,
                DistanceMetric::DOT_PRODUCT,
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
    constexpr size_t kDimension = 1024;
    constexpr size_t kDataVectors = 1000;
    constexpr size_t kTopK = 10;

    std::vector<int8_t> dataVectors(kDataVectors * kDimension, 0);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> smallDist(-1, 1);

    // Create mostly-random vectors, but with a unique strong component to ensure
    // the self dot-product is strictly the maximum.
    for (size_t i = 0; i < kDataVectors; ++i)
    {
        int8_t* vec = dataVectors.data() + i * kDimension;
        for (size_t d = 0; d < kDimension; ++d)
        {
            vec[d] = static_cast<int8_t>(smallDist(rng));
        }
        // Ensure uniqueness and dominance.
        vec[i] = static_cast<int8_t>(127);
    }

    std::vector<uint64_t> outIndices(kTopK);
    std::unordered_set<size_t> excludeIndices;

    for (size_t q = 0; q < kDataVectors; ++q)
    {
        const int8_t* query = dataVectors.data() + q * kDimension;
        size_t results = SearchTopKInt8(
            kTopK,
            kDimension,
            kDataVectors,
            excludeIndices,
            DistanceMetric::DOT_PRODUCT,
            query,
            dataVectors.data(),
            outIndices.data());

        AssertWithMessage(results == kTopK, "Unexpected number of results: " + std::to_string(results));
        AssertWithMessage(outIndices[0] == q,
            "Top-1 mismatch: query=" + std::to_string(q) +
            " got=" + std::to_string(outIndices[0]));
    }
}

static void TestSearchTopInt8ExcludeIndices()
{
    constexpr size_t kDimension = 1024;
    constexpr size_t kDataVectors = 1000;
    constexpr size_t kTopK = 10;

    std::vector<int8_t> dataVectors(kDataVectors * kDimension, 0);
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> smallDist(-1, 1);

    // Create mostly-random vectors with a unique strong component.
    for (size_t i = 0; i < kDataVectors; ++i)
    {
        int8_t* vec = dataVectors.data() + i * kDimension;
        for (size_t d = 0; d < kDimension; ++d)
        {
            vec[d] = static_cast<int8_t>(smallDist(rng));
        }
        vec[i] = static_cast<int8_t>(127);
    }

    std::vector<uint64_t> outIndices(kTopK);

    for (size_t q = 0; q < kDataVectors; ++q)
    {
        std::unordered_set<size_t> excludeIndices = { q, (q + 1) % kDataVectors };
        const int8_t* query = dataVectors.data() + q * kDimension;

        size_t results = SearchTopKInt8(
            kTopK,
            kDimension,
            kDataVectors,
            excludeIndices,
            DistanceMetric::DOT_PRODUCT,
            query,
            dataVectors.data(),
            outIndices.data());

        AssertWithMessage(results == kTopK, "Unexpected number of results: " + std::to_string(results));

        for (size_t i = 0; i < results; ++i)
        {
            size_t idx = static_cast<size_t>(outIndices[i]);
            AssertWithMessage(excludeIndices.find(idx) == excludeIndices.end(),
                "Excluded index returned: query=" + std::to_string(q) +
                " index=" + std::to_string(idx));
        }
    }
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

#ifdef NDEBUG
    RunTest(TestDotProductInt8Performance());
#endif // NDEBUG
    RunTest(TestDotProductInt8Correctness());
    RunTest(TestSearchTopKInt8Correctness());
    RunTest(TestSearchTopInt8ExcludeIndices());

    return 0;
}

