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
}

static CpuCapability DetectCpuCapability()
{
#if defined(__x86_64__)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw"))
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

typedef int32_t (*MetricInt8Func)(
    size_t dimension,
    const int8_t* queryVector,
    const int8_t* dataVectors);

static int32_t DotProductInt8_None(
    size_t dimension,
    const int8_t* queryVector,
    const int8_t* dataVectors)
{
    int32_t score = 0;
    for (size_t d = 0; d < dimension; d++)
    {
        score += static_cast<int32_t>(queryVector[d]) * static_cast<int32_t>(dataVectors[d]);
    }
    return score;
}

#if defined(__x86_64__)

__attribute__((target("avx2")))
static int32_t DotProductInt8_AVX2(
    size_t dimension,
    const int8_t* queryVector,
    const int8_t* dataVectors)
{
    size_t nBlocks = dimension / 16;
    size_t remainder = dimension % 16;

    __m256i accumulator = _mm256_setzero_si256();

    for (size_t i = 0; i < nBlocks; i++)
    {
        __m128i queryVec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(
            queryVector + i * 16));
        __m256i queryVecS16 = _mm256_cvtepi8_epi16(queryVec);

        __m128i dataVec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(
            dataVectors + i * 16));
        __m256i dataVecS16 = _mm256_cvtepi8_epi16(dataVec);

        __m256i prod = _mm256_madd_epi16(queryVecS16, dataVecS16);
        accumulator = _mm256_add_epi32(accumulator, prod);
    }

    /** Horizontal sum */
    int32_t score = 0;
    __m128i sum128 = _mm_add_epi32(
        _mm256_castsi256_si128(accumulator),
        _mm256_extracti128_si256(accumulator, 1));
    sum128 = _mm_hadd_epi32(sum128, sum128);
    sum128 = _mm_hadd_epi32(sum128, sum128);
    score = _mm_cvtsi128_si32(sum128);

    /** Try avoid this */
    if (remainder != 0)
    {
        size_t offset = nBlocks * 16;
        int32_t sum = 0;
        for (size_t d = 0; d < remainder; d++)
        {
            sum += static_cast<int32_t>(queryVector[offset + d]) *
                        static_cast<int32_t>(dataVectors[offset + d]);
        }
        score += sum;
    }

    return score;
}

__attribute__((target("avx512f,avx512bw")))
static int32_t DotProductInt8_AVX512(
    size_t dimension,
    const int8_t* queryVector,
    const int8_t* dataVectors)
{
    (void)dimension;
    (void)queryVector;
    (void)dataVectors;
    /** @todo */
    throw std::runtime_error("DotProductInt8_AVX512 not implemented");
}

#endif // x86_64 / i386

#if defined(__aarch64__)

__attribute__((target("neon")))
static int32_t DotProductInt8_NEON(
    size_t dimension,
    const int8_t* queryVector,
    const int8_t* dataVectors)
{
    (void)dimension;
    (void)queryVector;
    (void)dataVectors;
    /** @todo */
    throw std::runtime_error("DotProductInt8_NEON not implemented");
}

#endif

static MetricInt8Func GetMetricInt8Function(
    CpuCapability capability, DistanceMetric metric)
{
    switch (metric)
    {
    case DistanceMetric::DOT_PRODUCT:
        switch (capability)
        {
#if defined(__x86_64__) || defined(__i386__)
        case CpuCapability::AVX2:
            return &DotProductInt8_AVX2;
        case CpuCapability::AVX512:
            return &DotProductInt8_AVX512;
#endif // x86_64 / i386
#if defined(__aarch64__)
        case CpuCapability::NEON:
            return &DotProductInt8_NEON;
#endif // aarch64
        default:
            return &DotProductInt8_None;
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
    size_t* outIndices)
{
    MetricInt8Func metricFunc = GetMetricInt8Function(
        g_cpuCapability, distanceMetric);

    ScoreKeeper scoreKeeper(k, KeepMode::MAX_N);
    for(size_t i = 0; i < nDataVectors; i++)
    {
        if (excludeIndices.contains(i))
        {
            continue;
        }
        int32_t score = metricFunc(dimension, queryVector, dataVectors + i * dimension);
        scoreKeeper.AddScore(score, i);
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
    MetricInt8Func metricFunc = GetMetricInt8Function(
        g_cpuCapability, distanceMetric);
    for(size_t i = 0; i < nDataVectors; i++)
    {
        outScores[i] = metricFunc(dimension, queryVector, dataVectors + i * dimension);
    }
}
#endif // TUI_TEST_INTERFACES
