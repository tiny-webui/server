#pragma once

#include <memory>
#include <tev-cpp/Tev.h>
#include "rpc/RpcServer.h"
#include "common/Uuid.h"
#include "network/IServer.h"
#include "network/HttpClient.h"
#include "database/Database.h"
#include "apiProvider/IProvider.h"
#include "CallerId.h"
#include "ResourceVersionManager.h"

namespace TUI::Application
{
    class Service
    {
    public:
        /** @todo more dependencies */
        Service(
            Tev& tev,
            std::shared_ptr<Network::IServer<CallerId>> server,
            std::shared_ptr<Database::Database> database,
            std::function<void(const std::string&)> onCriticalError);
        
        Service(const Service&) = delete;
        Service& operator=(const Service&) = delete;
        Service(Service&&) = delete;
        Service& operator=(Service&&) = delete;   

        ~Service() = default;

        void Close();
    private:
        static constexpr uint64_t STREAM_BATCHING_INTERVAL_MS = 300;

        Tev& _tev;
        std::shared_ptr<Database::Database> _database;
        std::function<void(const std::string&)> _onCriticalError;
        std::shared_ptr<Network::Http::Client> _httpClient{nullptr};
        std::unique_ptr<Rpc::RpcServer<CallerId>> _rpcServer{nullptr};
        std::shared_ptr<ResourceVersionManager<CallerId>> _resourceVersionManager{ResourceVersionManager<CallerId>::Create()};
        std::unordered_map<Common::Uuid, Schema::IServer::UserAdminSettingsRole> _userRoleCache{};
        std::unordered_map<Common::Uuid, std::shared_ptr<ApiProvider::IProvider>> _providers{};

        /** Rpc handlers */
        JS::Promise<nlohmann::json> OnSetMetadataAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnGetMetadataAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnDeleteMetadataAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnGetChatListAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnNewChatAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnGetChatAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> DeleteChatAsync(CallerId callerId, nlohmann::json params);
        JS::AsyncGenerator<nlohmann::json, nlohmann::json> OnChatCompletionAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnExecuteGenerationTaskAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnGetModelListAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnNewModelAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnGetModelAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnDeleteModelAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnModifyModelAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnGetUserListAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnNewUserAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnDeleteUserAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnGetUserAdminSettingsAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnSetUserAdminSettingsAsync(CallerId callerId, nlohmann::json params);
        JS::Promise<nlohmann::json> OnSetUserCredentialAsync(CallerId callerId, nlohmann::json params);

        /** Connection handlers */
        void OnNewConnection(CallerId callerId);
        void OnConnectionClosed(CallerId callerId);
        /** Critical error handler */
        void OnCriticalError(const std::string& message);

        template<typename T>
        T ParseParams(const nlohmann::json& params) const
        {
            try
            {
                return params.get<T>();
            }
            catch (const nlohmann::json::exception& e)
            {
                throw Schema::Rpc::Exception(
                    Schema::Rpc::ErrorCode::BAD_REQUEST,
                    "Failed to parse parameters: " + std::string(e.what()));
            }
        }

        std::shared_ptr<ApiProvider::IProvider> GetProvider(const Common::Uuid& providerId);
        std::map<std::string, nlohmann::json> TryGetMetadata(const std::vector<std::string>& keys, const std::string& metadataString);
        std::string TryMergeMetadata(const std::string& base, std::map<std::string, nlohmann::json>& changes);
        std::string TryDeleteMetadata(const std::string& base, const std::vector<std::string>& keys);
        void CheckAdmin(const Common::Uuid& userId);
    };
}
