#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace TUI::Database::VectorSearch
{
    enum class DistanceMetric
    {
        DOT_PRODUCT,
    };

    /**
     * @brief 
     * @warning The caller must ensure all buffers' sizes are appropriate.
     *      Otherwise, invalid memory access will occur.
     * 
     * @param k 
     * @param dimension 
     * @param nDataVectors 
     * @param queryVector Must be of size dimension * sizeof(int8_t)
     * @param dataVectors Must be of size nDataVectors * dimension * sizeof(int8_t)
     * @param outIndices Must be of size at least k * sizeof(size_t)
     * 
     * @return size_t The actual number of results written to outIndices.
     */
    size_t SearchTopKInt8(
        size_t k,
        size_t dimension,
        size_t nDataVectors,
        const std::unordered_set<size_t>& excludeIndices,
        DistanceMetric distanceMetric,
        const int8_t* queryVector,
        const int8_t* dataVectors,
        size_t* outIndices);

#ifdef TUI_TEST_INTERFACES
    void TestMetricCalculation(
        size_t dimension,
        size_t nDataVectors,
        DistanceMetric distanceMetric,
        const int8_t* queryVector,
        const int8_t* dataVectors,
        int32_t* outScores);
#endif // TUI_TEST_INTERFACES
}
