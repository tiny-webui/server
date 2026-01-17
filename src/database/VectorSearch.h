#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <list>
#include <optional>
#include <vector>
#include <queue>
#include <functional>


namespace TUI::Database
{

class VectorSearch
{

public:
    enum class DistanceMetric
    {
        DOT_PRODUCT,
    };

    enum class ScoreMode
    {
        MAX_N,
        MIN_N,
    };

    template<typename T>
    struct ScoredIndex
    {
        int32_t score;
        T index;
    };

    template<typename T>
    class ScoreKeeper
    {
    public:

        explicit ScoreKeeper(size_t maxSize, ScoreMode mode)
            : _maxSize(maxSize), _comparator(MakeComparator(mode)), _heap(_comparator)
        {
        }

        void AddScore(int32_t score, uint64_t index)
        {
            if (_heap.size() < _maxSize)
            {
                _heap.emplace(ScoredIndex{ score, index });
            }
            else if (_comparator({ score, index }, _heap.top()))
            {
                _heap.pop();
                _heap.emplace(ScoredIndex{ score, index });
            }
        }

        std::list<ScoredIndex<T>> GetResultsAndClear()
        {
            std::list<ScoredIndex<T>> results;
            while (!_heap.empty())
            {
                results.push_back(_heap.top());
                _heap.pop();
            }
            std::reverse(results.begin(), results.end());
            return results;
        }

    private:
        static std::function<bool(const ScoredIndex<T>&, const ScoredIndex<T>&)> MakeComparator(ScoreMode mode)
        {
            if (mode == ScoreMode::MAX_N)
            {
                return [](const ScoredIndex<T>& a, const ScoredIndex<T>& b) {
                    return a.score > b.score;
                };
            }

            return [](const ScoredIndex<T>& a, const ScoredIndex<T>& b) {
                return a.score < b.score;
            };
        }

        size_t _maxSize;
        std::function<bool(const ScoredIndex<T>&, const ScoredIndex<T>&)> _comparator;
        std::priority_queue<
            ScoredIndex<T>,
            std::vector<ScoredIndex<T>>,
            std::function<bool(const ScoredIndex<T>&, const ScoredIndex<T>&)>>
            _heap;
    };

#ifdef TUI_TEST_INTERFACES
public:
#else
private:
#endif // TUI_TEST_INTERFACES

    using MetricInt8BatchFunc = void (*)(
        size_t dimension,
        size_t nDataVectors,
        const int8_t* queryVector,
        const int8_t* dataVectors,
        int32_t* outScores);

    static MetricInt8BatchFunc GetMetricInt8BatchFunction(
        DistanceMetric distanceMetric);

    static ScoreMode GetScoreMode(DistanceMetric distanceMetric);

public:

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
     * @return ScoreKeeper<size_t> The ScoreKeeper containing (max) top k results.
     */
    static ScoreKeeper<size_t> SearchTopKInt8(
        size_t k,
        size_t dimension,
        DistanceMetric distanceMetric,
        const int8_t* queryVector,
        size_t nDataVectors,
        const int8_t* dataVectors,
        const std::unordered_set<size_t>& excludeIndices,
        std::optional<ScoreKeeper<size_t>> initialScores = std::nullopt);

    template<typename T>
    static ScoreKeeper<T> SearchTopKInt8(
        size_t k,
        size_t dimension,
        DistanceMetric distanceMetric,
        const int8_t* queryVector,
        std::unordered_map<T, std::vector<int8_t>>& dataVectorMap,
        std::optional<ScoreKeeper<T>> initialScores = std::nullopt)
    {
        auto metricFunc = GetMetricInt8BatchFunction(distanceMetric);
        ScoreKeeper<T> scoreKeeper(k, GetScoreMode(distanceMetric));
        if (initialScores.has_value())
        {
            scoreKeeper = std::move(initialScores.value());
        }
        int32_t score = 0;
        for (const auto& [index, dataVector] : dataVectorMap)
        {
            metricFunc(
                dimension,
                1,
                queryVector,
                dataVector.data(),
                &score);
            scoreKeeper.AddScore(score, index);
        }
        return scoreKeeper;
    }

};

}
