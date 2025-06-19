#pragma once

#include <map>
#include <unordered_set>
#include <list>
#include <string>
#include "schema/Rpc.h"

namespace TUI::Application
{
    template<typename ID>
    class ResourceVersionManager : public std::enable_shared_from_this<ResourceVersionManager<ID>>
    {
    public:
        class Lock
        {
        public:
            friend class ResourceVersionManager<ID>;
            
            Lock(const Lock&) = delete;
            Lock& operator=(const Lock&) = delete;
            Lock(Lock&&) = default;
            Lock& operator=(Lock&&) = default;

            ~Lock()
            {
                _release();
            }

            /**
             * @brief Confirm the operation is successful. This will update the resource version.
             * If this is not called. The lock will release without modifying the resource version.
             */
            void Confirm()
            {
                _confirm();
            }
        private:
            Lock(std::function<void()> confirm, std::function<void()> release)
                : _confirm(std::move(confirm)), _release(std::move(release))
            {
            }
            std::function<void()> _confirm;
            std::function<void()> _release;
        };

        static std::shared_ptr<ResourceVersionManager<ID>> Create()
        {
            return std::shared_ptr<ResourceVersionManager<ID>>(new ResourceVersionManager<ID>());
        }

        ResourceVersionManager(const ResourceVersionManager&) = delete;
        ResourceVersionManager& operator=(const ResourceVersionManager&) = delete;
        ResourceVersionManager(ResourceVersionManager&&) = delete;
        ResourceVersionManager& operator=(ResourceVersionManager&&) = delete;
        ~ResourceVersionManager() = default;
        
        Lock GetReadLock(const std::vector<std::string>& resourcePath, const ID& id)
        {
            CheckReaderVersion(resourcePath, id);
            LockReadLock(resourcePath, id);
            std::weak_ptr<ResourceVersionManager<ID>> manager_ref = this->shared_from_this();
            return Lock{
                [=]()
                {
                    auto manager = manager_ref.lock();
                    if (manager)
                    {
                        manager->ConfirmRead(resourcePath, id);
                    }
                },
                [=]()
                {
                    auto manager = manager_ref.lock();
                    if (manager)
                    {
                        manager->ReleaseReadLock(resourcePath, id);
                    }
                }
            };
        }

        Lock GetWriteLock(const std::vector<std::string>& resourcePath, const ID& id)
        {
            CheckWriterVersion(resourcePath, id);
            LockWriteLock(resourcePath, id);
            std::weak_ptr<ResourceVersionManager<ID>> manager_ref = this->shared_from_this();
            return Lock{
                [=]()
                {
                    auto manager = manager_ref.lock();
                    if (manager)
                    {
                        manager->ConfirmWrite(resourcePath, id);
                    }
                },
                [=]()
                {
                    auto manager = manager_ref.lock();
                    if (manager)
                    {
                        manager->ReleaseWriteLock(resourcePath, id);
                    }
                }
            };
        }

    private:
        struct ResourceState
        {
            std::unordered_set<ID> upToDateSet{};
            std::unordered_set<ID> readLockHolders{};
            std::optional<ID> writeLockHolder{};
        };
        /** Stores the IDs that are up to date on the resource path (the key). */
        std::map<std::vector<std::string>, ResourceState> _states{};

        ResourceVersionManager() = default;

        void ConfirmRead(const std::vector<std::string>& resourcePath, const ID& id)
        {
            /** This creates the entry if it does not exist. */
            auto& state = _states[resourcePath];
            if (state.upToDateSet.find(id) != state.upToDateSet.end())
            {
                return;
            }
            state.upToDateSet.insert(id);
        };

        void ConfirmWrite(const std::vector<std::string>& resourcePath, const ID& id)
        {
            /** This will make id the sole user that's up to date */
            auto& state = _states[resourcePath];
            state.upToDateSet.clear();
            state.upToDateSet.insert(id);
        };

        /**
         * @brief Checks if the reader is up to date with the resource.
         * 
         * @param resourcePath 
         * @param id
         * @throws Rpc::Exception with code NOT_MODIFIED if the resource is already up to date
         */
        void CheckReaderVersion(const std::vector<std::string>& resourcePath, const ID& id) const
        {
            auto item = _states.find(resourcePath);
            if (item == _states.end())
            {
                /** This resource is not recorded. Thus, the reader cannot be up to date. */
                return;
            }
            const auto& state = item->second;
            if (state.upToDateSet.find(id) == state.upToDateSet.end())
            {
                return;
            }
            throw TUI::Schema::Rpc::Exception(TUI::Schema::Rpc::ErrorCode::NOT_MODIFIED, "Resource up to date");
        };

        /**
         * @brief Checks if the writer is not up to date before modifying the resource.
         * 
         * @param resourcePath 
         * @param id 
         * @throws Rpc::Exception with code CONFLICT if the resource is not up to date.
         */
        void CheckWriterVersion(const std::vector<std::string>& resourcePath, const ID& id) const
        {
            auto item = _states.find(resourcePath);
            if (item == _states.end())
            {
                /** The resource is not recorded. Thus, the writer cannot be up to date */
                throw TUI::Schema::Rpc::Exception(TUI::Schema::Rpc::ErrorCode::CONFLICT, "Resource outdated");
            }
            const auto& state = item->second;
            if (state.upToDateSet.find(id) == state.upToDateSet.end())
            {
                throw TUI::Schema::Rpc::Exception(TUI::Schema::Rpc::ErrorCode::CONFLICT, "Resource outdated");
            }
        };

        void LockReadLock(const std::vector<std::string>& resourcePath, const ID& id)
        {
            auto& state = _states[resourcePath];
            if (state.writeLockHolder.has_value())
            {
                throw TUI::Schema::Rpc::Exception(TUI::Schema::Rpc::ErrorCode::LOCKED, "Resource is locked for writing");
            }
            state.readLockHolders.insert(id);
        }

        void ReleaseReadLock(const std::vector<std::string>& resourcePath, const ID& id)
        {
            auto item = _states.find(resourcePath);
            if (item == _states.end())
            {
                /** Resource is not recorded, not able to release */
                return; 
            }
            auto& state = item->second;
            auto it = state.readLockHolders.find(id);
            if (it == state.readLockHolders.end())
            {
                /** The ID is not holding a read lock, nothing to release */
                return;
            }
            state.readLockHolders.erase(it);
        }

        void LockWriteLock(const std::vector<std::string>& resourcePath, const ID& id)
        {
            auto& state = _states[resourcePath];
            if (state.writeLockHolder.has_value())
            {
                throw TUI::Schema::Rpc::Exception(TUI::Schema::Rpc::ErrorCode::LOCKED, "Resource is locked for writing");
            }
            if (!state.readLockHolders.empty())
            {
                throw TUI::Schema::Rpc::Exception(TUI::Schema::Rpc::ErrorCode::LOCKED, "Resource is locked for reading");
            }
            state.writeLockHolder = id;
        }

        void ReleaseWriteLock(const std::vector<std::string>& resourcePath, const ID& id)
        {
            auto item = _states.find(resourcePath);
            if (item == _states.end())
            {
                /** Resource is not recorded, not able to release */
                return; 
            }
            auto& state = item->second;
            if (!state.writeLockHolder.has_value() || state.writeLockHolder.value() != id)
            {
                /** The ID is not holding a write lock, nothing to release */
                return;
            }
            state.writeLockHolder.reset();
        }
    };
}
