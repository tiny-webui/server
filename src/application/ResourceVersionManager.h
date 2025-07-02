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
                if ((std::uncaught_exceptions() <= _uncaughtExceptionsOnEnter) && (!_doNotConfirm))
                {
                    /** Only confirm the operation if the scope exited normally and do not confirm is not set */
                    if (_confirm)
                    {
                        _confirm();
                    }
                }
                if (_release)
                {
                    _release();
                }
            }

            /**
             * @brief Disable the auto confirmation of the lock when destructed.
             * 
             */
            void DoNotConfirm()
            {
                _doNotConfirm = true;
            }
        private:
            Lock(std::function<void()> confirm, std::function<void()> release)
                : _confirm(std::move(confirm)),
                  _release(std::move(release)),
                  _uncaughtExceptionsOnEnter(std::uncaught_exceptions())
            {
            }
            std::function<void()> _confirm;
            std::function<void()> _release;
            int _uncaughtExceptionsOnEnter;
            bool _doNotConfirm{false};
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
            /** Lock first, then check. */
            LockReadLock(resourcePath, id);
            std::weak_ptr<ResourceVersionManager<ID>> manager_ref = this->shared_from_this();
            Lock lock{
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
            CheckReaderVersion(resourcePath, id);
            return std::move(lock);
        }

        Lock GetWriteLock(const std::vector<std::string>& resourcePath, const ID& id)
        {
            LockWriteLock(resourcePath, id);
            std::weak_ptr<ResourceVersionManager<ID>> manager_ref = this->shared_from_this();
            Lock lock{
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
            CheckWriterVersion(resourcePath, id);
            return std::move(lock);
        }

        /** 
         * 
         * @The caller does not need to be up to date to delete something. 
         * As they may delete it from the result of a list read.
         */

        Lock GetDeleteLock(const std::vector<std::string>& resourcePath, const ID& id)
        {
            LockWriteLock(resourcePath, id);
            std::weak_ptr<ResourceVersionManager<ID>> manager_ref = this->shared_from_this();
            Lock lock{
                [=]()
                {
                    auto manager = manager_ref.lock();
                    if (manager)
                    {
                        manager->_states.erase(resourcePath);
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
            return std::move(lock);
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
