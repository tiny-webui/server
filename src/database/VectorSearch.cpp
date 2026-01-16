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
            : _maxSize(maxSize), _mode(mode),
              heap_([this](const ScoredIndex& a, const ScoredIndex& b) {
                  return Compare(a, b);
              })
        {
        }

        void AddScore(int32_t score, uint64_t index)
        {
            if (heap_.size() < _maxSize)
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

        size_t _maxSize;
        KeepMode _mode;

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
            return (_mode == KeepMode::MIN_N) ? CompareMin(a, b) : CompareMax(a, b);
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

typedef void (*MetricInt8BatchFunc)(
    size_t dimension,
    size_t nDataVectors,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores);

static void DotProductInt8Batch_None(
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
            score += static_cast<int32_t>(queryVector[d]) * 
                static_cast<int32_t>(dataVectors[i * dimension + d]);
        }
        outScores[i] = score;
    }
}

#if defined(__x86_64__)

__attribute__((target("avx2")))
static void DotProductInt8Batch_AVX2(
    size_t dimension,
    size_t nDataVectors,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores)
{
    size_t nBlocks = dimension / 16;
    size_t remainder = dimension % 16;

    const int8_t* dataOffset = dataVectors;
    for (size_t i = 0; i < nDataVectors; i++)
    {
        __m256i accumulator = _mm256_setzero_si256();

        for (size_t j = 0; j < nBlocks; j++)
        {
            __m128i queryVec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(
                queryVector + j * 16));
            __m256i queryVecS16 = _mm256_cvtepi8_epi16(queryVec);

            __m128i dataVec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(
                dataOffset + j * 16));
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
        if (__builtin_expect(remainder != 0, 0))
        {
            size_t offset = nBlocks * 16;
            for (size_t d = 0; d < remainder; d++)
            {
                score += static_cast<int32_t>(queryVector[offset + d]) *
                            static_cast<int32_t>(dataOffset[offset + d]);
            }
        }

        outScores[i] = score;
        dataOffset += dimension;
    }
}

__attribute__((target("avx512f,avx512bw")))
static void DotProductInt8Batch_AVX512(
    size_t dimension,
    size_t nDataVectors,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores)
{
    size_t nBlocks = dimension / 32;
    size_t remainder = dimension % 32;

    const int8_t* dataOffset = dataVectors;
    for (size_t i = 0; i < nDataVectors; i++)
    {
        __m512i accumulator = _mm512_setzero_si512();
        
        for (size_t j = 0; j < nBlocks; j++)
        {
            __m256i queryVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(
                queryVector + j * 32));
            __m512i queryVecS16 = _mm512_cvtepi8_epi16(queryVec);

            __m256i dataVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(
                dataOffset + j * 32));
            __m512i dataVecS16 = _mm512_cvtepi8_epi16(dataVec);

            __m512i prod = _mm512_madd_epi16(queryVecS16, dataVecS16);
            accumulator = _mm512_add_epi32(accumulator, prod);
        }

        int32_t score = 0;
        __m256i sum256 = _mm256_add_epi32(
            _mm512_castsi512_si256(accumulator),
            _mm512_extracti64x4_epi64(accumulator, 1));
        __m128i sum128 = _mm_add_epi32(
            _mm256_castsi256_si128(sum256),
            _mm256_extracti128_si256(sum256, 1));
        sum128 = _mm_hadd_epi32(sum128, sum128);
        sum128 = _mm_hadd_epi32(sum128, sum128);
        score = _mm_cvtsi128_si32(sum128);

        if (__builtin_expect(remainder != 0, 0))
        {
            size_t offset = nBlocks * 32;
            for (size_t d = 0; d < remainder; d++)
            {
                score += static_cast<int32_t>(queryVector[offset + d]) *
                            static_cast<int32_t>(dataOffset[offset + d]);
            }
        }

        outScores[i] = score;
        dataOffset += dimension;
    }
}

#endif // x86_64 / i386

#if defined(__aarch64__)

__attribute__((target("neon")))
static void DotProductInt8Batch_NEON(
    size_t dimension,
    size_t nDataVectors,
    const int8_t* queryVector,
    const int8_t* dataVectors,
    int32_t* outScores)
{
    (void)dimension;
    (void)nDataVectors;
    (void)queryVector;
    (void)dataVectors;
    (void)outScores;
    /** @todo */
    throw std::runtime_error("DotProductInt8Batch_NEON not implemented");
}

#endif

static MetricInt8BatchFunc GetMetricInt8BatchFunction(
    CpuCapability capability, DistanceMetric metric)
{
    switch (metric)
    {
    case DistanceMetric::DOT_PRODUCT:
        switch (capability)
        {
#if defined(__x86_64__) || defined(__i386__)
        case CpuCapability::AVX2:
            return &DotProductInt8Batch_AVX2;
        case CpuCapability::AVX512:
            return &DotProductInt8Batch_AVX512;
#endif // x86_64 / i386
#if defined(__aarch64__)
        case CpuCapability::NEON:
            return &DotProductInt8Batch_NEON;
#endif // aarch64
        default:
            return &DotProductInt8Batch_None;
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
    MetricInt8BatchFunc metricFunc = GetMetricInt8BatchFunction(
        g_cpuCapability, distanceMetric);

    ScoreKeeper scoreKeeper(k, KeepMode::MAX_N);

    constexpr size_t batchSize = 1024;
    std::vector<int32_t> scores(batchSize);
    for(size_t offset = 0; offset < nDataVectors; offset += batchSize)
    {
        size_t actualBatchSize = std::min(static_cast<size_t>(batchSize), nDataVectors - offset);
        metricFunc(
            dimension,
            actualBatchSize,
            queryVector,
            dataVectors + offset * dimension,
            scores.data());
        for(size_t i = 0; i < actualBatchSize; i++)
        {
            size_t dataIndex = offset + i;
            if (__builtin_expect(excludeIndices.contains(dataIndex), 0))
            {
                continue;
            }
            scoreKeeper.AddScore(scores[i], dataIndex);
        }
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
    MetricInt8BatchFunc metricFunc = GetMetricInt8BatchFunction(
        g_cpuCapability, distanceMetric);
    metricFunc(
        dimension,
        nDataVectors,
        queryVector,
        dataVectors,
        outScores);
}
#endif // TUI_TEST_INTERFACES
