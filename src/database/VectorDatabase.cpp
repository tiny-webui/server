#include "VectorDatabase.h"
#include "common/UniqueTypes.h"
#include "common/Uuid.h"
#include "VectorSearch.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>

using namespace TUI::Common;
using namespace TUI::Database;

namespace
{
    union Header
    {
        struct __attribute__((packed))
        {
            uint8_t magic[16];
            /** pad to 4 bytes aligned. */
            char id[(UUID_STR_LEN + 3) / 4 * 4];
            VectorDatabase::DataType dataType;
            uint32_t dimension;
        } fields;
        uint8_t raw[4096];
    };

    /** random magic */
    constexpr uint8_t EXPECTED_MAGIC[16] = {
        0xf0, 0x80, 0x35, 0x28, 0xe0, 0x31, 0xe3, 0x24,
        0x88, 0x1c, 0x7e, 0x76, 0x48, 0x1e, 0xf7, 0xac
    };

    enum class WalRecordType : uint32_t
    {
        DELETE = 1,
        INSERT = 2,
    };
}

/** UniqueMappedFile */

VectorDatabase::UniqueMappedFile::UniqueMappedFile(const std::filesystem::path& path, bool readOnly)
{
    int flags = O_CLOEXEC;
    flags |= readOnly ? O_RDONLY : O_RDWR;
    Unique::Fd fd(open(path.c_str(), flags));
    if (fd == -1)
    {
        throw std::runtime_error("Failed to open file for mapping");
    }
    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        throw std::runtime_error("Failed to stat file for mapping");
    }
    _size = static_cast<size_t>(st.st_size);
    int prot = PROT_READ;
    if (!readOnly)
    {
        prot |= PROT_WRITE;
    }
    flags = readOnly ? MAP_PRIVATE : MAP_SHARED;
    _data = mmap(nullptr, _size, prot, flags, fd, 0);
    if (_data == MAP_FAILED)
    {
        throw std::runtime_error("Failed to mmap file");
    }
}

VectorDatabase::UniqueMappedFile::~UniqueMappedFile()
{
    Unmap();
}

VectorDatabase::UniqueMappedFile::UniqueMappedFile(UniqueMappedFile&& other) noexcept
{
    _data = other._data;
    _size = other._size;
    other._data = nullptr;
    other._size = 0;
}

VectorDatabase::UniqueMappedFile& VectorDatabase::UniqueMappedFile::operator=(VectorDatabase::UniqueMappedFile&& other) noexcept
{
    if (this != &other)
    {
        Unmap();
        _data = other._data;
        _size = other._size;
        other._data = nullptr;
        other._size = 0;
    }
    return *this;
}

void VectorDatabase::UniqueMappedFile::Unmap() noexcept
{
    if (!_data)
    {
        return;
    }
    munmap(_data, _size);
    _data = nullptr;
    _size = 0;
}

size_t VectorDatabase::UniqueMappedFile::Size() const
{
    return _size;
}

void* VectorDatabase::UniqueMappedFile::Data()
{
    return _data;
}

const void* VectorDatabase::UniqueMappedFile::Data() const
{
    return _data;
}

bool VectorDatabase::UniqueMappedFile::operator==(std::nullptr_t) const noexcept
{
    return _data == nullptr;
}

/** VectorDatabase */

VectorDatabase::VectorDatabase(Tev& tev)
    : _workerThread(tev)
{
}

JS::Promise<std::shared_ptr<VectorDatabase>> VectorDatabase::CreateAsync(
    Tev& tev,
    const std::filesystem::path& dbPath,
    size_t dimension,
    VectorDatabase::DataType dataType,
    size_t walSizeSoftLimit)
{
    if (dataType != DataType::INT8)
    {
        throw std::runtime_error("Unsupported data type");
    }
    if (dimension == 0)
    {
        throw std::runtime_error("Dimension must be greater than 0");
    }
    auto db = std::shared_ptr<VectorDatabase>(new VectorDatabase(tev));
    db->_dimension = dimension;
    db->_dataType = dataType;
    db->_dbPath = std::filesystem::absolute(dbPath);
    db->_walSizeSoftLimit = walSizeSoftLimit;
    db->_newlyCreated = co_await db->LoadOrCreateAsync();
    co_return db;
}

JS::Promise<bool> VectorDatabase::LoadOrCreateAsync()
{
    return _workerThread.ExecTaskAsync([this]() -> bool {
        return LoadOrCreate();
    });
}

bool VectorDatabase::LoadOrCreate()
{
    auto root = _dbPath.parent_path();
    if (!root.empty())
    {
        std::filesystem::create_directories(root);
    }
    try
    {
        if (!std::filesystem::exists(_dbPath))
        {
            throw std::runtime_error("Database file does not exist");
        }
        UniqueMappedFile base(_dbPath, true);
        if (base.Size() < sizeof(Header))
        {
            throw std::runtime_error("Database file too small");
        }
        Header* baseHeader = reinterpret_cast<Header*>(base.Data());
        if (std::memcmp(baseHeader->fields.magic, EXPECTED_MAGIC, sizeof(EXPECTED_MAGIC)) != 0)
        {
            throw std::runtime_error("Invalid database file (bad magic)");
        }
        Uuid baseId(baseHeader->fields.id, sizeof(baseHeader->fields.id));
        if (baseHeader->fields.dataType != _dataType)
        {
            throw std::runtime_error("Data type mismatch");
        }
        if (baseHeader->fields.dimension != _dimension)
        {
            throw std::runtime_error("Dimension mismatch");
        }
        size_t baseVectorCount = (base.Size() - sizeof(Header)) / (_dimension * sizeof(int8_t));
        if (baseVectorCount * _dimension * sizeof(int8_t) + sizeof(Header) != base.Size())
        {
            throw std::runtime_error("Database file size invalid");
        }

        auto indexPath = GetIndexPath();
        if (!std::filesystem::exists(indexPath))
        {
            throw std::runtime_error("Index file does not exist");
        }
        UniqueMappedFile indices(indexPath, true);
        if (indices.Size() < sizeof(Header))
        {
            throw std::runtime_error("Index file too small");
        }
        Header* indexHeader = reinterpret_cast<Header*>(indices.Data());
        if (std::memcmp(indexHeader->fields.magic, EXPECTED_MAGIC, sizeof(EXPECTED_MAGIC)) != 0)
        {
            throw std::runtime_error("Invalid index file (bad magic)");
        }
        Uuid indexId(indexHeader->fields.id, sizeof(indexHeader->fields.id));
        if (indexId != baseId)
        {
            throw std::runtime_error("Index file does not match database file");
        }
        if (indexHeader->fields.dataType != _dataType)
        {
            throw std::runtime_error("Index data type mismatch");
        }
        if (indexHeader->fields.dimension != _dimension)
        {
            throw std::runtime_error("Index dimension mismatch");
        }
        size_t indexVectorCount = (indices.Size() - sizeof(Header)) / sizeof(IndexType);
        if (indexVectorCount * sizeof(IndexType) + sizeof(Header) != indices.Size())
        {
            throw std::runtime_error("Index file size invalid");
        }
        if (indexVectorCount != baseVectorCount)
        {
            throw std::runtime_error("Index vector count does not match database vector count");
        }

        IndexType nextIndex = 1;
        if (indexVectorCount > 0)
        {
            std::memcpy(&nextIndex, reinterpret_cast<uint8_t*>(indices.Data()) + sizeof(Header) + (indexVectorCount - 1) * sizeof(IndexType), sizeof(IndexType));
            nextIndex += 1;
        }

        auto walPath = GetWalPath();
        std::unordered_set<IndexType> baseDeletedIndices;
        std::unordered_map<IndexType, std::vector<int8_t>> walVectors;
        {
            UniqueMappedFile wal(walPath, true);
            if (wal.Size() < sizeof(Header))
            {
                throw std::runtime_error("WAL file too small");
            }
            Header* walHeader = reinterpret_cast<Header*>(wal.Data());
            if (std::memcmp(walHeader->fields.magic, EXPECTED_MAGIC, sizeof(EXPECTED_MAGIC)) != 0)
            {
                throw std::runtime_error("Invalid WAL file (bad magic)");
            }
            Uuid walId(walHeader->fields.id, sizeof(walHeader->fields.id));
            if (walId != baseId)
            {
                throw std::runtime_error("WAL file does not match database file");
            }
            if (walHeader->fields.dataType != _dataType)
            {
                throw std::runtime_error("WAL data type mismatch");
            }
            if (walHeader->fields.dimension != _dimension)
            {
                throw std::runtime_error("WAL dimension mismatch");
            }

            size_t offset = sizeof(Header);
            while (offset + sizeof(WalRecordType) + sizeof(IndexType) <= wal.Size())
            {
                WalRecordType recordType;
                std::memcpy(&recordType, reinterpret_cast<uint8_t*>(wal.Data()) + offset, sizeof(WalRecordType));
                offset += sizeof(WalRecordType);
                IndexType index;
                std::memcpy(&index, reinterpret_cast<uint8_t*>(wal.Data()) + offset, sizeof(IndexType));
                offset += sizeof(IndexType);
                nextIndex = index + 1;
                if (recordType == WalRecordType::DELETE)
                {
                    if (walVectors.contains(index))
                    {
                        walVectors.erase(index);
                    }
                    else
                    {
                        baseDeletedIndices.insert(index);
                    }
                }
                else if (recordType == WalRecordType::INSERT)
                {
                    std::vector<int8_t> vec(_dimension);
                    std::memcpy(vec.data(), reinterpret_cast<uint8_t*>(wal.Data()) + offset, _dimension * sizeof(int8_t));
                    offset += _dimension * sizeof(int8_t);
                    walVectors.emplace(index, std::move(vec));
                }
                else
                {
                    throw std::runtime_error("Invalid WAL file (unknown record type)" + std::to_string(static_cast<uint32_t>(recordType)));
                }
            }
            if (offset != wal.Size())
            {
                throw std::runtime_error("Invalid WAL file (trailing data)" + std::to_string(offset) + " != " + std::to_string(wal.Size()));
            }
        }

        std::unordered_set<size_t> baseDeletedSlots;
        {
            const IndexType* indexData = reinterpret_cast<const IndexType*>(
                reinterpret_cast<uint8_t*>(indices.Data()) + sizeof(Header));
            for (IndexType i = 0; i < indexVectorCount; i++)
            {
                IndexType index = indexData[i];
                if (baseDeletedIndices.contains(index))
                {
                    baseDeletedSlots.insert(i);
                }
            }
        }

        Unique::Fd wal(open(walPath.c_str(), O_CLOEXEC | O_APPEND | O_WRONLY));
        if (wal < 0)
        {
            throw std::runtime_error("Failed to open WAL file for appending");
        }
        struct stat st;
        if (fstat(wal, &st) == -1)
        {
            throw std::runtime_error("Failed to stat WAL file");
        }

        _nextIndex = nextIndex;
        _walSize.store(static_cast<size_t>(st.st_size), std::memory_order_relaxed);
        _wal = std::move(wal);
        _baseDeletedSlots = std::move(baseDeletedSlots);
        _walVectors = std::move(walVectors);
        _indices = std::move(indices);
        _base = std::move(base);
        return false;
    }
    catch(...)
    {
        /** @todo log */
    }
    /** Create the files */
    Header header{};
    memset(header.raw, 0, sizeof(header.raw));
    std::memcpy(header.fields.magic, EXPECTED_MAGIC, sizeof(EXPECTED_MAGIC));
    Uuid newId;
    std::string idStr = static_cast<std::string>(newId);
    std::memcpy(header.fields.id, idStr.c_str(), std::min(sizeof(header.fields.id), idStr.size()));
    header.fields.dataType = _dataType;
    header.fields.dimension = static_cast<uint32_t>(_dimension);
    {
        std::ofstream baseFile(_dbPath, std::ios::binary | std::ios::trunc);
        if (!baseFile.is_open())
        {
            throw std::runtime_error("Failed to create database file");
        }
        baseFile.write(reinterpret_cast<const char*>(header.raw), sizeof(header.raw));
        if (!baseFile)
        {
            throw std::runtime_error("Failed to write database file");
        }
    }
    {
        auto indexPath = GetIndexPath();
        std::ofstream indexFile(indexPath, std::ios::binary | std::ios::trunc);
        if (!indexFile.is_open())
        {
            throw std::runtime_error("Failed to create index file");
        }
        indexFile.write(reinterpret_cast<const char*>(header.raw), sizeof(header.raw));
        if (!indexFile)
        {
            throw std::runtime_error("Failed to write index file");
        }
    }
    {
        auto walPath = GetWalPath();
        std::ofstream walFile(walPath, std::ios::binary | std::ios::trunc);
        if (!walFile.is_open())
        {
            throw std::runtime_error("Failed to create WAL file");
        }
        walFile.write(reinterpret_cast<const char*>(header.raw), sizeof(header.raw));
        if (!walFile)
        {
            throw std::runtime_error("Failed to write WAL file");
        }
    }

    Unique::Fd wal(open(GetWalPath().c_str(), O_CLOEXEC | O_APPEND | O_WRONLY));
    if (wal < 0)
    {
        throw std::runtime_error("Failed to open WAL file for appending");
    }
    struct stat st;
    if (fstat(wal, &st) == -1)
    {
        throw std::runtime_error("Failed to stat WAL file");
    }
    UniqueMappedFile base(_dbPath, true);
    UniqueMappedFile indices(GetIndexPath(), true);

    _nextIndex = 1;
    _walSize.store(static_cast<size_t>(st.st_size), std::memory_order_relaxed);
    _wal = std::move(wal);
    _base = std::move(base);
    _indices = std::move(indices);
    return true;
}

JS::Promise<bool> VectorDatabase::DeleteVectorAsync(IndexType index)
{
    co_await _workerThread.ExecTaskAsync([this, index]() {
        DeleteVector(index);
    });
    size_t walSize = _walSize.load(std::memory_order_relaxed);
    co_return (walSize >= _walSizeSoftLimit) && !_compactInProgress;
}

void VectorDatabase::DeleteVector(IndexType index)
{
    bool needWrite = false;
    if (_walVectors.contains(index))
    {
        _walVectors.erase(index);
        needWrite = true;
    }
    else
    {
        auto slotOpt = LocateIndexInBase(index);
        if (slotOpt.has_value())
        {
            _baseDeletedSlots.insert(slotOpt.value());
            needWrite = true;
        }
    }

    if (needWrite)
    {
        auto recordType = WalRecordType::DELETE;
        ssize_t bytesWritten = 0;
        bytesWritten += write(_wal, reinterpret_cast<const void*>(&recordType), sizeof(recordType));
        bytesWritten += write(_wal, reinterpret_cast<const void*>(&index), sizeof(index));
        if (bytesWritten != static_cast<ssize_t>(sizeof(recordType) + sizeof(index)))
        {
            if (ftruncate(_wal, static_cast<off_t>(_walSize)) == -1)
            {
                throw std::runtime_error("Failed to write to WAL and failed to revert");
            }
            throw std::runtime_error("Failed to write to WAL");
        }
        fsync(_wal);
        _walSize.fetch_add(bytesWritten, std::memory_order_relaxed);
    }
}

JS::Promise<std::pair<VectorDatabase::IndexType, bool>> VectorDatabase::InsertVectorAsync(const std::vector<int8_t>& vec)
{
    IndexType index = co_await _workerThread.ExecTaskAsync([this, vec]() -> IndexType {
        return InsertVector(vec);
    });
    size_t walSize = _walSize.load(std::memory_order_relaxed);
    bool suggestsCompact = (walSize >= _walSizeSoftLimit) && !_compactInProgress;
    co_return std::make_pair(index, suggestsCompact);
}

VectorDatabase::IndexType VectorDatabase::InsertVector(const std::vector<int8_t>& vec)
{
    /** @todo */
    if (vec.size() != _dimension)
    {
        throw std::runtime_error("Vector dimension mismatch");
    }
    IndexType index = _nextIndex++;
    _walVectors[index] = vec;
    auto recordType = WalRecordType::INSERT;
    ssize_t bytesWritten = 0;
    bytesWritten += write(_wal, reinterpret_cast<const void*>(&recordType), sizeof(recordType));
    bytesWritten += write(_wal, reinterpret_cast<const void*>(&index), sizeof(index));
    bytesWritten += write(_wal, reinterpret_cast<const void*>(vec.data()), vec.size() * sizeof(int8_t));
    if (bytesWritten != static_cast<ssize_t>(sizeof(recordType) + sizeof(index) + vec.size() * sizeof(int8_t)))
    {
        if (ftruncate(_wal, static_cast<off_t>(_walSize)) == -1)
        {
            throw std::runtime_error("Failed to write to WAL and failed to revert");
        }
        throw std::runtime_error("Failed to write to WAL");
    }
    fsync(_wal);
    _walSize.fetch_add(bytesWritten, std::memory_order_relaxed);
    return index;
}

JS::Promise<std::vector<VectorDatabase::IndexType>> VectorDatabase::SearchTopKAsync(
    size_t k,
    const std::vector<int8_t>& query)
{
    co_return co_await _workerThread.ExecTaskAsync([this, k, query]() -> std::vector<IndexType> {
        return SearchTopK(k, query);
    });
}

std::vector<VectorDatabase::IndexType> VectorDatabase::SearchTopK(
    size_t k,
    const std::vector<int8_t>& query)
{
    if (_base == nullptr || _indices == nullptr)
    {
        throw std::runtime_error("Database not loaded");
    }
    /** Search base first */
    auto baseScoreKeeper = VectorSearch::SearchTopKInt8(
        k,
        _dimension,
        VectorSearch::DistanceMetric::DOT_PRODUCT,
        query.data(),
        (_base.Size() - sizeof(Header)) / (_dimension * sizeof(int8_t)),
        reinterpret_cast<const int8_t*>(reinterpret_cast<uint8_t*>(_base.Data()) + sizeof(Header)),
        _baseDeletedSlots);
    /** Translate base locations to indices */
    auto baseLocations = baseScoreKeeper.GetResultsAndClear();
    VectorSearch::ScoreKeeper<IndexType> baseScoreKeeperIndices(k, VectorSearch::GetScoreMode(VectorSearch::DistanceMetric::DOT_PRODUCT));
    const IndexType* indexData = reinterpret_cast<const IndexType*>(
        reinterpret_cast<const uint8_t*>(_indices.Data()) + sizeof(Header));
    for (const auto& item : baseLocations)
    {
        IndexType index = indexData[item.index];
        baseScoreKeeperIndices.AddScore(item.score, index);
    }
    /** Search WAL next */
    auto finalScoreKeeper = VectorSearch::SearchTopKInt8<IndexType>(
        k,
        _dimension,
        VectorSearch::DistanceMetric::DOT_PRODUCT,
        query.data(),
        _walVectors,
        std::make_optional(std::move(baseScoreKeeperIndices)));
    auto finalResults = finalScoreKeeper.GetResultsAndClear();
    std::vector<IndexType> resultIndices;
    resultIndices.reserve(finalResults.size());
    for (const auto& item : finalResults)
    {
        resultIndices.push_back(item.index);
    }
    return resultIndices;
}

JS::Promise<void> VectorDatabase::CompactAsync()
{
    if (_compactInProgress)
    {
        /** Ignore this request. No need to throw. */
        co_return;
    }
    _compactInProgress = true;
    co_await _workerThread.ExecTaskAsync([this]() -> void {
        Compact();
    });
    _compactInProgress = false;
}

void VectorDatabase::Compact()
{
    if (_base == nullptr || _indices == nullptr)
    {
        throw std::runtime_error("Database not loaded");
    }
    size_t baseVectorCount = (_base.Size() - sizeof(Header)) / (_dimension * sizeof(int8_t));
    size_t newVectorCount = baseVectorCount - _baseDeletedSlots.size() + _walVectors.size();
    /** Create new files */
    Unique::Fd newBase(open(GetNewBasePath().c_str(), O_CLOEXEC | O_CREAT | O_RDWR | O_TRUNC, 0644));
    if (newBase < 0)
    {
        throw std::runtime_error("Failed to create new database file");
    }
    size_t newBaseSize = sizeof(Header) + newVectorCount * _dimension * sizeof(int8_t);
    if (fallocate(newBase, 0, 0, static_cast<off_t>(newBaseSize)) != 0)
    {
        throw std::runtime_error("Failed to allocate space for new database file");
    }
    Unique::Fd newIndex(open(GetNewIndexPath().c_str(), O_CLOEXEC | O_CREAT | O_RDWR | O_TRUNC, 0644));
    if (newIndex < 0)
    {
        throw std::runtime_error("Failed to create new index file");
    }
    size_t newIndexSize = sizeof(Header) + newVectorCount * sizeof(IndexType);
    if (fallocate(newIndex, 0, 0, static_cast<off_t>(newIndexSize)) != 0)
    {
        throw std::runtime_error("Failed to allocate space for new index file");
    }
    Unique::Fd newWal(open(GetNewWalPath().c_str(), O_CLOEXEC | O_CREAT | O_RDWR | O_TRUNC, 0644));
    if (newWal < 0)
    {
        throw std::runtime_error("Failed to create new WAL file");
    }
    Unique::Fd folderFd(open(_dbPath.parent_path().lexically_normal().c_str(), O_CLOEXEC | O_DIRECTORY | O_RDONLY));
    if (folderFd < 0)
    {
        throw std::runtime_error("Failed to open database folder for fsync");
    }
    /** Write headers */
    Header header{};
    memset(header.raw, 0, sizeof(header.raw));
    std::memcpy(header.fields.magic, EXPECTED_MAGIC, sizeof(EXPECTED_MAGIC));
    Uuid newId;
    std::string idStr = static_cast<std::string>(newId);
    std::memcpy(header.fields.id, idStr.c_str(), std::min(sizeof(header.fields.id), idStr.size()));
    header.fields.dataType = _dataType;
    header.fields.dimension = static_cast<uint32_t>(_dimension);
    ssize_t bytesWritten = 0;
    bytesWritten = write(newBase, reinterpret_cast<const void*>(header.raw), sizeof(header.raw));
    if (bytesWritten != static_cast<ssize_t>(sizeof(header.raw)))
    {
        throw std::runtime_error("Failed to write new database file header");
    }
    bytesWritten = write(newIndex, reinterpret_cast<const void*>(header.raw), sizeof(header.raw));
    if (bytesWritten != static_cast<ssize_t>(sizeof(header.raw)))
    {
        throw std::runtime_error("Failed to write new index file header");
    }
    bytesWritten = write(newWal, reinterpret_cast<const void*>(header.raw), sizeof(header.raw));
    if (bytesWritten != static_cast<ssize_t>(sizeof(header.raw)))
    {
        throw std::runtime_error("Failed to write new WAL file header");
    }
    /** Write vectors and indices */
    uint8_t* baseData = reinterpret_cast<uint8_t*>(_base.Data()) + sizeof(Header);
    IndexType* indexData = reinterpret_cast<IndexType*>(
        reinterpret_cast<uint8_t*>(_indices.Data()) + sizeof(Header));
    for (size_t i = 0; i < baseVectorCount; i++)
    {
        if (_baseDeletedSlots.contains(i))
        {
            continue;
        }
        /** Copy vector */
        bytesWritten = write(
            newBase,
            reinterpret_cast<const void*>(baseData + i * _dimension * sizeof(int8_t)),
            _dimension * sizeof(int8_t));
        if (bytesWritten != static_cast<ssize_t>(_dimension * sizeof(int8_t)))
        {
            throw std::runtime_error("Failed to write vector to new database file");
        }
        /** Copy index */
        IndexType index = indexData[i];
        bytesWritten = write(
            newIndex,
            reinterpret_cast<const void*>(&index),
            sizeof(index));
        if (bytesWritten != static_cast<ssize_t>(sizeof(index)))
        {
            throw std::runtime_error("Failed to write index to new index file");
        }
    }
    std::vector<IndexType> walIndices;
    walIndices.reserve(_walVectors.size());
    for (const auto& pair : _walVectors)
    {
        walIndices.push_back(pair.first);
    }
    std::sort(walIndices.begin(), walIndices.end());
    for (const auto& index : walIndices)
    {
        const auto& vec = _walVectors[index];
        /** Copy vector */
        bytesWritten = write(
            newBase,
            reinterpret_cast<const void*>(vec.data()),
            _dimension * sizeof(int8_t));
        if (bytesWritten != static_cast<ssize_t>(_dimension * sizeof(int8_t)))
        {
            throw std::runtime_error("Failed to write vector to new database file");
        }
        /** Copy index */
        bytesWritten = write(
            newIndex,
            reinterpret_cast<const void*>(&index),
            sizeof(index));
        if (bytesWritten != static_cast<ssize_t>(sizeof(index)))
        {
            throw std::runtime_error("Failed to write index to new index file");
        }
    }
    fsync(newBase);
    fsync(newIndex);
    fsync(newWal);
    /** Replace old files */
    _base.Unmap();
    _indices.Unmap();
    _wal.Close();
    newBase.Close();
    newIndex.Close();
    newWal.Close();
    std::filesystem::rename(GetNewBasePath(), _dbPath);
    std::filesystem::rename(GetNewIndexPath(), GetIndexPath());
    std::filesystem::rename(GetNewWalPath(), GetWalPath());
    fsync(folderFd);
    /** Reload */
    _base = UniqueMappedFile(_dbPath, true);
    _indices = UniqueMappedFile(GetIndexPath(), true);
    _wal = Unique::Fd(open(GetWalPath().c_str(), O_CLOEXEC | O_APPEND | O_WRONLY));
    if (_wal < 0)
    {
        throw std::runtime_error("Failed to open WAL file for appending after compaction");
    }
    _baseDeletedSlots.clear();
    _walVectors.clear();
    _walSize.store(sizeof(Header), std::memory_order_relaxed);
}

bool VectorDatabase::IsNewlyCreated() const noexcept
{
    return _newlyCreated;
}

std::filesystem::path VectorDatabase::GetWalPath() const
{
    return _dbPath.string() + "-wal";
}

std::filesystem::path VectorDatabase::GetIndexPath() const
{
    return _dbPath.string() + "-index";
}

std::filesystem::path VectorDatabase::GetNewBasePath() const
{
    return _dbPath.string() + "-new";
}

std::filesystem::path VectorDatabase::GetNewWalPath() const
{
    return _dbPath.string() + "-wal-new";
}

std::filesystem::path VectorDatabase::GetNewIndexPath() const
{
    return _dbPath.string() + "-index-new";
}

std::optional<size_t> VectorDatabase::LocateIndexInBase(IndexType index) const
{
    if (_indices == nullptr)
    {
        throw std::runtime_error("Indices not loaded");
    }
    const IndexType* indexData = reinterpret_cast<const IndexType*>(
        reinterpret_cast<const uint8_t*>(_indices.Data()) + sizeof(Header));
    size_t indexCount = (_indices.Size() - sizeof(Header)) / sizeof(IndexType);
    auto it = std::lower_bound(
        indexData,
        indexData + indexCount,
        index);
    if (it == indexData + indexCount || *it != index)
    {
        return std::nullopt;
    }
    return static_cast<size_t>(it - indexData);
}
