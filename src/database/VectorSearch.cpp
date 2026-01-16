#include "VectorSearch.h"

#include <algorithm>
#include <queue>
#include <sys/auxv.h>
#include <stdexcept>
#include <functional>

/**
 * Please don't target 32-bit architectures.
 * No acceleration will be provided for them.
 */

#if defined(__x86_64__)
#include <immintrin.h>
#elif defined(__aarch64__)
#include <arm_neon.h>
#endif  // architecture selection

using namespace TUI::Database::VectorSearch;

namespace
{
    enum class KeepMode
    {
        MAX_N,
        MIN_N,
    };

    class ScoreKeeper
    {
    public:
        explicit ScoreKeeper(size_t maxSize, KeepMode mode)
            : maxSize_(maxSize), mode_(mode),
              heap_([this](const ScoredIndex& a, const ScoredIndex& b) {
                  return Compare(a, b);
              })
        {
        }

        void AddScore(int32_t score, uint64_t index)
        {
            if (heap_.size() < maxSize_)
            {
                heap_.emplace(ScoredIndex{ score, index });
            }
            else if (Compare({ score, index }, heap_.top()))
            {
                heap_.pop();
                heap_.emplace(ScoredIndex{ score, index });
            }
        }

        size_t DumpResults(uint64_t* outIndices)
        {
            size_t count = heap_.size();
            for (size_t i = 0; i < count; ++i)
            {
                outIndices[count - i - 1] = heap_.top().index;
                heap_.pop();
            }
            return count;
        }
    
    private:
        struct ScoredIndex
        {
            int32_t score;
            uint64_t index;
        };

        size_t maxSize_;
        KeepMode mode_;

        static bool CompareMax(const ScoredIndex& a, const ScoredIndex& b)
        {
            return a.score > b.score;
        }

        static bool CompareMin(const ScoredIndex& a, const ScoredIndex& b)
        {
            return a.score < b.score;
        }

        bool Compare(const ScoredIndex& a, const ScoredIndex& b) const
        {
            return (mode_ == KeepMode::MIN_N) ? CompareMin(a, b) : CompareMax(a, b);
        }

        std::priority_queue<
            ScoredIndex,
            std::vector<ScoredIndex>,
            std::function<bool(const ScoredIndex&, const ScoredIndex&)>>
            heap_;
    };

    enum class CpuCapability
    {
        NONE,
        AVX2,
        AVX512,
        NEON,
    };

    constexpr size_t MAX_BATCH_SIZE_NONE = 1;
    constexpr size_t MAX_BATCH_SIZE_AVX2 = 11;
    constexpr size_t MAX_BATCH_SIZE_AVX512 = 16;
    constexpr size_t MAX_BATCH_SIZE_NEON = 8;
}

static CpuCapability DetectCpuCapability()
{
#if defined(__x86_64__)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx512f"))
    {
        return CpuCapability::AVX512;
    }
    if (__builtin_cpu_supports("avx2"))
    {
        return CpuCapability::AVX2;
    }
#elif defined(__aarch64__)
    // AArch64 mandates NEON support in the architecture.
    return CpuCapability::NEON;
#endif  // architecture selection
    return CpuCapability::NONE;
}

static size_t GetMaxBatchSize(CpuCapability capability)
{
    switch (capability)
    {
    case CpuCapability::AVX2:
        return MAX_BATCH_SIZE_AVX2;
    case CpuCapability::AVX512:
        return MAX_BATCH_SIZE_AVX512;
    case CpuCapability::NEON:
        return MAX_BATCH_SIZE_NEON;
    default:
        return MAX_BATCH_SIZE_NONE;
    }
}

typedef void (*BatchMetricInt8Func)(
    size_t dimension,
    size_t batchSize,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores);

static void BatchDotProductInt8_None(
    size_t dimension,
    size_t batchSize,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores)
{
    for (size_t i = 0; i < batchSize; i++)
    {
        int32_t score = 0;
        for (size_t d = 0; d < dimension; d++)
        {
            score += static_cast<int32_t>(queryVector[d]) * static_cast<int32_t>(dataVectors[i * dimension + d]);
        }
        outScores[i] = score;
    }
}

#if defined(__x86_64__)

__attribute__((target("avx2")))
static void BatchDotProductInt8_AVX2(
    size_t dimension,
    size_t batchSize,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores)
{
    if (batchSize > MAX_BATCH_SIZE_AVX2)
    {
        throw std::invalid_argument("Batch size exceeds MAX_BATCH_SIZE_AVX2");
    }

    size_t nBlocks = dimension / 32;
    size_t remainder = dimension % 32;

    /** Batch size registers */
    __m256i accumulators[MAX_BATCH_SIZE_AVX2];
    for (size_t j = 0; j < batchSize; j++)
    {
        accumulators[j] = _mm256_setzero_si256();
    }

    for (size_t i = 0; i < nBlocks; i++)
    {
        __m256i queryVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(
            queryVector + i * 32));
        __m128i queryLo = _mm256_castsi256_si128(queryVec);
        __m128i queryHi = _mm256_extracti128_si256(queryVec, 1);
        /** 2 registers */
        __m256i queryLoS16 = _mm256_cvtepi8_epi16(queryLo);
        __m256i queryHiS16 = _mm256_cvtepi8_epi16(queryHi);

        for (size_t j = 0; j < batchSize; j++)
        {
            __m256i dataVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(
                dataVectors + j * dimension + i * 32));
            __m128i dataLo = _mm256_castsi256_si128(dataVec);
            __m128i dataHi = _mm256_extracti128_si256(dataVec, 1);
            /** 2 registers */
            __m256i dataLoS16 = _mm256_cvtepi8_epi16(dataLo);
            __m256i dataHiS16 = _mm256_cvtepi8_epi16(dataHi);

            /** 1 register (can reuse dataLoS16) */
            __m256i prod = _mm256_madd_epi16(queryLoS16, dataLoS16);
            accumulators[j] = _mm256_add_epi32(accumulators[j], prod);
            prod = _mm256_madd_epi16(queryHiS16, dataHiS16);
            accumulators[j] = _mm256_add_epi32(accumulators[j], prod);
        }
    }

    /** Horizontal sum */
    for (size_t j = 0; j < batchSize; j++)
    {
        __m128i sum128 = _mm_add_epi32(
            _mm256_castsi256_si128(accumulators[j]),
            _mm256_extracti128_si256(accumulators[j], 1));
        sum128 = _mm_hadd_epi32(sum128, sum128);
        sum128 = _mm_hadd_epi32(sum128, sum128);
        outScores[j] = _mm_cvtsi128_si32(sum128);
    }

    /** Try avoid this */
    if (remainder != 0)
    {
        size_t offset = nBlocks * 32;
        for (size_t j = 0; j < batchSize; j++)
        {
            int32_t score = 0;
            for (size_t d = 0; d < remainder; d++)
            {
                score += static_cast<int32_t>(queryVector[offset + d]) *
                         static_cast<int32_t>(dataVectors[j * dimension + offset + d]);
            }
            outScores[j] += score;
        }
    }
}

__attribute__((target("avx512f")))
static void BatchDotProductInt8_AVX512(
    size_t dimension,
    size_t batchSize,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores)
{
    (void)dimension;
    (void)batchSize;
    (void)queryVector;
    (void)dataVectors;
    (void)outScores;
    /** @todo */
}

#endif // x86_64 / i386

#if defined(__aarch64__)

__attribute__((target("neon")))
static void BatchDotProductInt8_NEON(
    size_t dimension,
    size_t batchSize,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores)
{
    /** @todo */
}

#endif

static BatchMetricInt8Func GetBatchMetricInt8Function(
    CpuCapability capability, DistanceMetric metric)
{
    switch (metric)
    {
    case DistanceMetric::DOT_PRODUCT:
        switch (capability)
        {
#if defined(__x86_64__) || defined(__i386__)
        case CpuCapability::AVX2:
            return &BatchDotProductInt8_AVX2;
        case CpuCapability::AVX512:
            return &BatchDotProductInt8_AVX512;
#endif // x86_64 / i386
#if defined(__aarch64__)
        case CpuCapability::NEON:
            return &BatchDotProductInt8_NEON;
#endif // aarch64
        default:
            return &BatchDotProductInt8_None;
        }
    default:
        throw std::invalid_argument("Unsupported DistanceMetric");
    }
}

static CpuCapability g_cpuCapability = DetectCpuCapability();

size_t TUI::Database::VectorSearch::SearchTopKInt8(
    size_t k,
    size_t dimension,
    size_t nDataVectors,
    const std::unordered_set<size_t>& excludeIndices,
    DistanceMetric distanceMetric,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    uint64_t* outIndices)
{
    size_t maxBatchSize = GetMaxBatchSize(g_cpuCapability);
    BatchMetricInt8Func batchMetricFunc = GetBatchMetricInt8Function(
        g_cpuCapability, distanceMetric);

    ScoreKeeper scoreKeeper(k, KeepMode::MAX_N);
    size_t i = 0;
    std::vector<int32_t> batchScores(maxBatchSize);
    while (i < nDataVectors)
    {
        size_t batchSize = std::min(maxBatchSize, nDataVectors - i);
        batchMetricFunc(
            dimension, batchSize, queryVector, dataVectors + i * dimension, batchScores.data());
        for (size_t j = 0; j < batchSize; ++j)
        {
            if (excludeIndices.find(i + j) != excludeIndices.end())
            {
                continue;
            }
            scoreKeeper.AddScore(batchScores[j], static_cast<uint64_t>(i + j));
        }
        i += batchSize;
    }

    return scoreKeeper.DumpResults(outIndices);
}

#ifdef TUI_TEST_INTERFACES
void TUI::Database::VectorSearch::TestMetricCalculation(
    size_t dimension,
    size_t nDataVectors,
    DistanceMetric distanceMetric,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores)
{
    size_t maxBatchSize = GetMaxBatchSize(g_cpuCapability);
    BatchMetricInt8Func batchMetricFunc = GetBatchMetricInt8Function(
        g_cpuCapability, distanceMetric);

    size_t i = 0;
    while (i < nDataVectors)
    {
        size_t batchSize = std::min(maxBatchSize, nDataVectors - i);
        batchMetricFunc(
            dimension, batchSize, queryVector, dataVectors + i * dimension, outScores + i);
        i += batchSize;
    }
}
#endif // TUI_TEST_INTERFACES
