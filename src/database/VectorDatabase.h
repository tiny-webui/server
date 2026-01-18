#pragma once

#include <memory>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <atomic>

#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>

#include "common/WorkerThread.h"

namespace TUI::Database
{

    class VectorDatabase
    {
    public:
        enum class DataType : std::uint32_t
        {
            UNKNOWN = 0,
            INT8 = 1,
        };

        using IndexType = uint64_t;

        static JS::Promise<std::shared_ptr<VectorDatabase>> CreateAsync(
            Tev& tev,
            const std::filesystem::path& dbPath,
            size_t dimension,
            DataType dataType,
            size_t walSizeSoftLimit = 1024 * 1024 /** 1MiB */);

        VectorDatabase(const VectorDatabase&) = delete;
        VectorDatabase& operator=(const VectorDatabase&) = delete;
        VectorDatabase(VectorDatabase&&) noexcept = delete;
        VectorDatabase& operator=(VectorDatabase&&) noexcept = delete;
        ~VectorDatabase() = default;

        bool IsNewlyCreated() const noexcept;

        JS::Promise<void> CompactAsync();

        /**
         * @brief 
         * 
         * @param index 
         * @return JS::Promise<bool> If suggests a compact.
         */
        JS::Promise<bool> DeleteVectorAsync(IndexType index);
        /** Int8 interface. */
        /**
         * @brief
         * 
         * @param vec Make sure vec is alive during the async operation. (Don't fire and forget.)
         * @return JS::Promise<IndexType> The inserted index and if suggests a compact.
         */
        JS::Promise<std::pair<IndexType, bool>> InsertVectorAsync(const std::vector<int8_t>& vec);
        /**
         * @brief 
         * 
         * @param k 
         * @param query Make sure query is alive during the async operation. (Don't fire and forget.)
         * @return JS::Promise<std::vector<IndexType>> 
         */
        JS::Promise<std::vector<IndexType>> SearchTopKAsync(
            size_t k,
            const std::vector<int8_t>& query);

    private:
        class UniqueMappedFile
        {
        public:
            UniqueMappedFile() = default;
            UniqueMappedFile(const std::filesystem::path& path, bool readOnly);
            ~UniqueMappedFile();
            UniqueMappedFile(const UniqueMappedFile&) = delete;
            UniqueMappedFile& operator=(const UniqueMappedFile&) = delete;
            UniqueMappedFile(UniqueMappedFile&& other) noexcept;
            UniqueMappedFile& operator=(UniqueMappedFile&& other) noexcept;

            void Unmap() noexcept;
            size_t Size() const;
            void* Data();
            const void* Data() const;
            bool operator==(std::nullptr_t) const noexcept;
        private:
            void* _data{nullptr};
            size_t _size{0};
        };

        explicit VectorDatabase(Tev& tev);

        JS::Promise<bool> LoadOrCreateAsync();
        bool LoadOrCreate();
        void DeleteVector(IndexType index);
        IndexType InsertVector(const std::vector<int8_t>& vec);
        std::vector<IndexType> SearchTopK(
            size_t k,
            const std::vector<int8_t>& query);
        void Compact();

        std::filesystem::path GetWalPath() const;
        std::filesystem::path GetIndexPath() const;
        std::filesystem::path GetNewBasePath() const;
        std::filesystem::path GetNewWalPath() const;
        std::filesystem::path GetNewIndexPath() const;
        std::optional<size_t> LocateIndexInBase(IndexType index) const;

        Common::WorkerThread _workerThread;
        bool _closed{false};
        bool _newlyCreated{false};
        size_t _walSizeSoftLimit{1024 * 1024}; /** 1MiB */
        bool _compactInProgress{false};

        /** Resources both threads read / write */
        std::atomic<size_t> _walSize{0};

        /** Resources for workerThread or readonly for both threads */
        std::filesystem::path _dbPath{};
        size_t _dimension{0};
        DataType _dataType{DataType::UNKNOWN};
        UniqueMappedFile _base{};
        UniqueMappedFile _indices{};
        Common::Unique::Fd _wal{-1};
        std::unordered_set<size_t> _baseDeletedSlots{};
        std::unordered_map<IndexType, std::vector<int8_t>> _walVectors{};
        IndexType _nextIndex{1};
    };

}
