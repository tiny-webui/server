#pragma once

#include <map>
#include <unordered_set>
#include <list>
#include <string>
#include "schema/Rpc.h"

namespace TUI::Application
{
    template<typename ID>
    class ResourceVersionManager
    {
    public:
        ResourceVersionManager() = default;
        ~ResourceVersionManager() = default;

        void ReadBy(const std::vector<std::string>& resourcePath, const ID& id)
        {
            /** This creates the entry if it does not exist. */
            auto& ids = _states[resourcePath];
            if (ids.find(id) != ids.end())
            {
                return;
            }
            ids.insert(id);
        };

        void WrittenBy(const std::vector<std::string>& resourcePath, const ID& id)
        {
            /** This will make id the sole user that's up to date */
            auto& ids = _states[resourcePath];
            ids.clear();
            ids.insert(id);
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
            const auto& ids = item->second;
            if (ids.find(id) == ids.end())
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
            const auto& ids = item->second;
            if (ids.find(id) == ids.end())
            {
                throw TUI::Schema::Rpc::Exception(TUI::Schema::Rpc::ErrorCode::CONFLICT, "Resource outdated");
            }
        };
    private:
        /** Stores the IDs that are up to date on the resource path (the key). */
        std::map<std::vector<std::string>, std::unordered_set<ID>> _states{};
    };
}
