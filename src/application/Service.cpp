#include "Service.h"
#include "apiProvider/Factory.h"
#include "network/HttpStreamResponseParser.h"
#include "common/Utilities.h"

using namespace TUI;
using namespace TUI::Application;

Service::Service(
    Tev& tev,
    std::shared_ptr<Network::IServer<CallerId>> server,
    std::shared_ptr<Database::Database> database,
    std::function<void(const std::string&)> onCriticalError)
    : _database(std::move(database)),
      _onCriticalError(onCriticalError),
      _httpClient(Network::Http::Client::Create(tev))
{
    _rpcServer = std::make_unique<Rpc::RpcServer<CallerId>>(
        std::move(server),
        std::unordered_map<std::string, Rpc::RpcServer<CallerId>::RequestHandler>{
            {"setMetadata", std::bind(&Service::OnSetMetadataAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"getMetadata", std::bind(&Service::OnGetMetadataAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"deleteMetadata", std::bind(&Service::OnDeleteMetadataAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"getChatList", std::bind(&Service::OnGetChatListAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"newChat", std::bind(&Service::OnNewChatAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"getChat", std::bind(&Service::OnGetChatAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"deleteChat", std::bind(&Service::DeleteChatAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"executeGenerationTask", std::bind(&Service::OnExecuteGenerationTaskAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"getModelList", std::bind(&Service::OnGetModelListAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"newModel", std::bind(&Service::OnNewModelAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"getModel", std::bind(&Service::OnGetModelAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"deleteModel", std::bind(&Service::OnDeleteModelAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"modifyModel", std::bind(&Service::OnModifyModelAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"getUserList", std::bind(&Service::OnGetUserListAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"newUser", std::bind(&Service::OnNewUserAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"deleteUser", std::bind(&Service::OnDeleteUserAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"getUserAdminSettings", std::bind(&Service::OnGetUserAdminSettingsAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"setUserAdminSettings", std::bind(&Service::OnSetUserAdminSettingsAsync, this, std::placeholders::_1, std::placeholders::_2)},
            {"setUserCredential", std::bind(&Service::OnSetUserCredentialAsync, this, std::placeholders::_1, std::placeholders::_2)}
        },
        std::unordered_map<std::string, Rpc::RpcServer<CallerId>::StreamRequestHandler>{
            {"chatCompletion", std::bind(&Service::OnChatCompletionAsync, this, std::placeholders::_1, std::placeholders::_2)},
        },
        std::unordered_map<std::string, Rpc::RpcServer<CallerId>::NotificationHandler>{
            /** Empty */
        },
        std::bind(&Service::OnNewConnection, this, std::placeholders::_1),
        std::bind(&Service::OnConnectionClosed, this, std::placeholders::_1),
        std::bind(&Service::OnCriticalError, this, std::placeholders::_1)
    );
    /** @todo more dependencies */
}

void Service::Close()
{
    _rpcServer.reset();
    _database.reset();
    _httpClient.reset();
    /** @todo more */
}

/**
 * @brief 
 * 
 * @attention Metadata has no version locking.
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnSetMetadataAsync(CallerId callerId, nlohmann::json paramsJson)
{
    auto params = ParseParams<Schema::IServer::SetMetadataParams>(paramsJson);
    auto& path = params.get_path();
    if (path.empty())
    {
        throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid path");
    }
    if (path[0] == "global")
    {
        /** Admin */
        CheckAdmin(callerId.userId);
        if (path.size() != 1)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid global path");
        }
        auto metadataStringOptional = _database->GetGlobalValue("metadata");
        auto metadataString = metadataStringOptional.has_value() ?
            metadataStringOptional.value() : "{}";
        auto newMetadataString = TryMergeMetadata(metadataString, params.get_mutable_entries());
        co_await _database->SetGlobalValueAsync(
            "metadata",
            std::move(newMetadataString));
    }
    else if (path[0] == "model")
    {
        /** Admin */
        CheckAdmin(callerId.userId);
        if (path.size() != 2)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid model path");
        }
        Common::Uuid modelId{path[1]};
        auto metadataString = _database->GetModelMetadata(modelId);
        auto newMetadataString = TryMergeMetadata(metadataString, params.get_mutable_entries());
        co_await _database->SetModelMetadataAsync(
            modelId,
            std::move(newMetadataString));
    }
    else if (path[0] == "user")
    {
        /** Current user */
        if (path.size() != 1)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid user path");
        }
        auto metadataString = _database->GetUserMetadata(callerId.userId);
        auto newMetadataString = TryMergeMetadata(metadataString, params.get_mutable_entries());
        co_await _database->SetUserMetadataAsync(
            callerId.userId,
            std::move(newMetadataString));
    }
    else if (path[0] == "userPublic")
    {
        /** Current user */
        if (path.size() != 1)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid user public path");
        }
        auto metadataString = _database->GetUserPublicMetadata(callerId.userId);
        auto newMetadataString = TryMergeMetadata(metadataString, params.get_mutable_entries());
        co_await _database->SetUserPublicMetadataAsync(
            callerId.userId,
            std::move(newMetadataString));
    }
    else if (path[0] == "chat")
    {
        /** Current user */
        if (path.size() != 2)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid chat path");
        }
        Common::Uuid chatId{path[1]};
        auto metadataString = _database->GetChatMetadata(callerId.userId, chatId);
        auto newMetadataString = TryMergeMetadata(metadataString, params.get_mutable_entries());
        co_await _database->SetChatMetadataAsync(
            callerId.userId, chatId,
            std::move(newMetadataString));
    }
    else
    {
        throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid path");
    }
    /** Return null */
    co_return nlohmann::json{};
}

JS::Promise<nlohmann::json> Service::OnGetMetadataAsync(CallerId callerId, nlohmann::json paramsJson)
{
    auto params = ParseParams<Schema::IServer::GetMetadataParams>(paramsJson);
    auto& path = params.get_path();
    std::string metadataString{};
    if (path.empty())
    {
        throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid path");
    }
    if (path[0] == "global")
    {
        /** Any */
        if (path.size() != 1)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid global path");
        }
        auto metadataStringOptional = _database->GetGlobalValue("metadata");
        metadataString = metadataStringOptional.has_value() ?
            metadataStringOptional.value() : "{}";
    }
    else if (path[0] == "model")
    {
        /** Any */
        CheckAdmin(callerId.userId);
        if (path.size() != 2)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid model path");
        }
        Common::Uuid modelId{path[1]};
        metadataString = _database->GetModelMetadata(modelId);
    }
    else if (path[0] == "user")
    {
        /** Current user */
        if (path.size() != 1)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid user path");
        }
        metadataString = _database->GetUserMetadata(callerId.userId);
    }
    else if (path[0] == "userPublic")
    {
        /** Current user or admin */
        Common::Uuid targetUserId = callerId.userId;
        if (path.size() == 2)
        {
            /** Only admin can read other user's public metadata */
            CheckAdmin(callerId.userId);
            targetUserId = Common::Uuid{path[1]};
        }
        else if (path.size() != 1)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid user public path");
        }
        metadataString = _database->GetUserPublicMetadata(targetUserId);
    }
    else if (path[0] == "chat")
    {
        /** Current user */
        if (path.size() != 2)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid chat path");
        }
        Common::Uuid chatId{path[1]};
        metadataString = _database->GetChatMetadata(callerId.userId, chatId);
    }
    else
    {
        throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid path");
    }
    auto metadata = TryGetMetadata(params.get_keys(), metadataString);
    co_return nlohmann::json(metadata);
}

JS::Promise<nlohmann::json> Service::OnDeleteMetadataAsync(CallerId callerId, nlohmann::json paramsJson)
{
    auto params = ParseParams<Schema::IServer::DeleteMetadataParams>(paramsJson);
    auto& path = params.get_path();
    if (path.empty())
    {
        throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid path");
    }
    if (path[0] == "global")
    {
        /** Admin */
        CheckAdmin(callerId.userId);
        if (path.size() != 1)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid global path");
        }
        auto metadataStringOptional = _database->GetGlobalValue("metadata");
        auto metadataString = metadataStringOptional.has_value() ?
            metadataStringOptional.value() : "{}";
        auto newMetadataString = TryDeleteMetadata(metadataString, params.get_keys());
        co_await _database->SetGlobalValueAsync(
            "metadata",
            std::move(newMetadataString));
    }
    else if (path[0] == "model")
    {
        /** Admin */
        CheckAdmin(callerId.userId);
        if (path.size() != 2)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid model path");
        }
        Common::Uuid modelId{path[1]};
        auto metadataString = _database->GetModelMetadata(modelId);
        auto newMetadataString = TryDeleteMetadata(metadataString, params.get_keys());
        co_await _database->SetModelMetadataAsync(
            modelId,
            std::move(newMetadataString));
    }
    else if (path[0] == "user")
    {
        /** Current user */
        if (path.size() != 1)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid user path");
        }
        auto metadataString = _database->GetUserMetadata(callerId.userId);
       auto newMetadataString = TryDeleteMetadata(metadataString, params.get_keys());
        co_await _database->SetUserMetadataAsync(
            callerId.userId,
            std::move(newMetadataString));
    }
    else if (path[0] == "userPublic")
    {
        /** Current user */
        if (path.size() != 1)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid user public path");
        }
        auto metadataString = _database->GetUserPublicMetadata(callerId.userId);
        auto newMetadataString = TryDeleteMetadata(metadataString, params.get_keys());
        co_await _database->SetUserPublicMetadataAsync(
            callerId.userId,
            std::move(newMetadataString));
    }
    else if (path[0] == "chat")
    {
        /** Current user */
        if (path.size() != 2)
        {
            throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid chat path");
        }
        Common::Uuid chatId{path[1]};
        auto metadataString = _database->GetChatMetadata(callerId.userId, chatId);
        auto newMetadataString = TryDeleteMetadata(metadataString, params.get_keys());
        co_await _database->SetChatMetadataAsync(
            callerId.userId, chatId,
            std::move(newMetadataString));
    }
    else
    {
        throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Invalid path");
    }
    /** Return null */
    co_return nlohmann::json{};
}

/**
 * @brief 
 * 
 * @attention Access: current user
 * 
 * @param callerId 
 * @param paramsJson GetChatListParams 
 * @return JS::Promise<nlohmann::json> GetChatListResult
 */
JS::Promise<nlohmann::json> Service::OnGetChatListAsync(CallerId callerId, nlohmann::json paramsJson)
{
    auto params = ParseParams<Schema::IServer::GetChatListParams>(paramsJson);
    auto lock = _resourceVersionManager->GetReadLock(
        {"chatList", static_cast<std::string>(callerId.userId)}, callerId);
    auto start = params.get_start();
    auto quantity = params.get_quantity();
    if (start < 0 || quantity < 0)
    {
        throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::BAD_REQUEST, "Start and quantity must be non-negative");
    }

    auto list = _database->ListChat(callerId.userId, static_cast<size_t>(start), static_cast<size_t>(quantity));
    Schema::IServer::GetChatListResult result{};
    auto& resultList = result.get_mutable_list();
    resultList.reserve(list.size());
    auto metadataKeys = params.get_meta_data_keys();
    for (const auto& item : list)
    {
        using EntryType = std::remove_reference<decltype(resultList)>::type::value_type;
        EntryType entry;
        entry.set_id(static_cast<std::string>(item.id));
        if (metadataKeys.has_value())
        {
            using MetadataType = std::invoke_result_t<decltype(&EntryType::get_metadata), EntryType&>;
            MetadataType metadata{TryGetMetadata(metadataKeys.value(), item.metadata)};
            entry.set_metadata(std::move(metadata));
        }
        resultList.push_back(std::move(entry));
    }
    resultList.shrink_to_fit();
    co_return static_cast<nlohmann::json>(result);
}

/**
 * @brief 
 * 
 * @attention Access: current user
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnNewChatAsync(CallerId callerId, nlohmann::json paramsJson)
{
    (void)paramsJson;
    auto lock = _resourceVersionManager->GetWriteLock(
        {"chatList", static_cast<std::string>(callerId.userId)}, callerId);
    auto chatId = co_await _database->CreateChatAsync(callerId.userId);
    /** Creation equals to the first read. */
    auto readLock = _resourceVersionManager->GetReadLock(
        {"chat", static_cast<std::string>(callerId.userId), static_cast<std::string>(chatId)}, callerId);
    co_return static_cast<nlohmann::json>(static_cast<std::string>(chatId));
}

/**
 * @brief 
 * 
 * @attention Access: current user
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnGetChatAsync(CallerId callerId, nlohmann::json paramsJson)
{
    auto params = ParseParams<std::string>(paramsJson);
    Common::Uuid chatId{params};
    auto lock = _resourceVersionManager->GetReadLock(
        {"chat", static_cast<std::string>(callerId.userId), static_cast<std::string>(chatId)}, callerId);
    auto contentStr = _database->GetChatContent(callerId.userId, chatId);
    auto content = nlohmann::json::parse(contentStr).get<Schema::IServer::TreeHistory>();
    co_return static_cast<nlohmann::json>(content);
}

/**
 * @brief 
 * 
 * @attention Access: current user
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::DeleteChatAsync(CallerId callerId, nlohmann::json paramsJson)
{
    auto params = ParseParams<std::string>(paramsJson);
    Common::Uuid chatId{params};
    auto lock0 = _resourceVersionManager->GetWriteLock(
        {"chatList", static_cast<std::string>(callerId.userId)}, callerId);
    auto lock1 = _resourceVersionManager->GetDeleteLock(
        {"chat", static_cast<std::string>(callerId.userId), static_cast<std::string>(chatId)}, callerId);

    co_await _database->DeleteChatAsync(callerId.userId, chatId);
    /** Return null */
    co_return nlohmann::json{};
}

/**
 * @brief 
 * 
 * @attention Access: current user
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::AsyncGenerator<nlohmann::json, nlohmann::json> 
 */
JS::AsyncGenerator<nlohmann::json, nlohmann::json> Service::OnChatCompletionAsync(CallerId callerId, nlohmann::json paramsJson)
{
    using MessageRoleType = std::remove_reference<std::invoke_result_t<decltype(&Schema::IServer::Message::get_role), Schema::IServer::Message&>>::type;

    auto params = ParseParams<Schema::IServer::ChatCompletionParams>(paramsJson);
    if (params.get_user_message().get_role() != MessageRoleType::USER)
    {
        throw Schema::Rpc::Exception(
            Schema::Rpc::ErrorCode::BAD_REQUEST,
            "The user message must have the role user");
    }
    Common::Uuid chatId{params.get_id()};
    auto lock = _resourceVersionManager->GetWriteLock(
        {"chat", static_cast<std::string>(callerId.userId), static_cast<std::string>(chatId)}, callerId);

    int64_t userMessageTimestamp = Common::Utilities::GetTimestamp();
    /** Construct the linear history from the tree history and the new user message */
    Schema::IServer::TreeHistory treeHistory{};
    {
        auto contentStr = _database->GetChatContent(callerId.userId, chatId);
        if (!contentStr.empty())
        {
            treeHistory = nlohmann::json::parse(contentStr).get<Schema::IServer::TreeHistory>();
        }
    }
    auto& nodes = treeHistory.get_mutable_nodes();
    Schema::IServer::LinearHistory history{};
    {
        std::list<Schema::IServer::Message> historyList{};
        auto parentIdStr = params.get_parent();
        while (parentIdStr.has_value())
        {
            auto item = nodes.find(parentIdStr.value());
            if (item == nodes.end())
            {
                throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::NOT_FOUND, "Parent message not found");
            }
            auto& node = item->second;
            /** 
             * We are searching in reverse. So push to the front
             * We need to save the tree history back. So don't move the message.
             */
            historyList.push_front(node.get_message());
            parentIdStr = node.get_parent();
        }
        /** The previous message should be a assistant message if it exists */
        if (!historyList.empty() && historyList.back().get_role() != MessageRoleType::ASSISTANT)
        {
            throw Schema::Rpc::Exception(
                Schema::Rpc::ErrorCode::BAD_REQUEST,
                "The parent message must be an assistant message");
        }

        /** We need to use the user message later. So don't move it. */
        historyList.push_back(params.get_user_message());
        history.reserve(historyList.size());
        for (auto&& item : historyList)
        {
            history.push_back(std::move(item));
        }
    }
    
    /** Send the request */
    std::string wholeResponse{};
    {
        Common::Uuid modelId{params.get_model_id()};
        auto provider = GetProvider(modelId);
        auto requestData = provider->FormatRequest(history, true);
        auto request = _httpClient->MakeStreamRequest(
            Network::Http::Method::POST,
            requestData);
        auto stream = request.GetResponseStream();
        Network::Http::StreamResponse::AsyncParser parser{stream};
        auto eventStream = parser.Parse();
        
        while (true)
        {
            auto event = co_await eventStream.NextAsync();
            if (!event.has_value())
            {
                break;
            }
            auto content = provider->ParseStreamResponse(event.value());
            if (!content.has_value())
            {
                continue;
            }
            using ContentTypeType = std::remove_reference<decltype(content.value().get_type())>::type;
            if (content.value().get_type() != ContentTypeType::TEXT)
            {
                throw Schema::Rpc::Exception(
                    Schema::Rpc::ErrorCode::BAD_GATEWAY,
                    content.value().get_data());
            }
            auto& data = content.value().get_data();
            wholeResponse += data;
            co_yield static_cast<nlohmann::json>(data);
        }
    }

    /** Save changes to the tree history */
    Common::Uuid userMessageId{};
    Common::Uuid responseMessageId{};
    {
        Schema::IServer::MessageNode responseNode{};   
        responseNode.set_id(static_cast<std::string>(responseMessageId));
        Schema::IServer::Message responseMessage{};
        responseMessage.set_role(MessageRoleType::ASSISTANT);
        using MessageContentType = std::remove_reference<decltype(responseMessage.get_mutable_content())>::type;
        MessageContentType responseContents{};
        decltype(responseContents)::value_type responseContent{};
        using ContentTypeType = std::remove_reference<decltype(responseContent.get_mutable_type())>::type;
        responseContent.set_type(ContentTypeType::TEXT);
        responseContent.set_data(wholeResponse);
        responseContents.push_back(std::move(responseContent));
        responseMessage.set_content(std::move(responseContents));
        responseNode.set_message(std::move(responseMessage));
        responseNode.set_parent(static_cast<std::string>(userMessageId));
        responseNode.set_timestamp(static_cast<double>(Common::Utilities::GetTimestamp()));
        nodes.emplace(static_cast<std::string>(responseMessageId), std::move(responseNode));

        Schema::IServer::MessageNode userNode{};
        userNode.set_id(static_cast<std::string>(userMessageId));
        userNode.set_message(std::move(params.get_mutable_user_message()));
        userNode.set_children({static_cast<std::string>(responseMessageId)});
        if (params.get_parent().has_value())
        {
            userNode.set_parent(params.get_parent());
            auto item = nodes.find(params.get_parent().value());
            if (item == nodes.end())
            {
                throw Schema::Rpc::Exception(Schema::Rpc::ErrorCode::NOT_FOUND, "Parent message not found");
            } 
            auto& parentNode = item->second;
            parentNode.get_mutable_children().push_back(static_cast<std::string>(userMessageId));
        }
        userNode.set_timestamp(static_cast<double>(userMessageTimestamp));
        nodes.emplace(static_cast<std::string>(userMessageId), std::move(userNode));
    }
    /** Save the tree history back */
    {
        auto contentStr = static_cast<nlohmann::json>(treeHistory).dump();
        co_await _database->SetChatContentAsync(callerId.userId, chatId, std::move(contentStr));
    }

    /** Return completion info */
    Schema::IServer::ChatCompletionInfo completionInfo{};
    completionInfo.set_user_message_id(static_cast<std::string>(userMessageId));
    completionInfo.set_assistant_message_id(static_cast<std::string>(responseMessageId));
    co_return static_cast<nlohmann::json>(completionInfo);
}

/**
 * @brief 
 * 
 * @todo If model access control is added, this function should check if the caller has access to the model.
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnExecuteGenerationTaskAsync(CallerId callerId, nlohmann::json paramsJson)
{
    (void)callerId;
    auto params = ParseParams<Schema::IServer::ExecuteGenerationTaskParams>(paramsJson);
    Common::Uuid modelId{params.get_model_id()};
    auto provider = GetProvider(modelId);
    Schema::IServer::LinearHistory history{};
    history.push_back(std::move(params.get_message()));
    auto requestData = provider->FormatRequest(history, false);
    auto request = _httpClient->MakeRequest(
        Network::Http::Method::POST,
        requestData);
    auto response = co_await request.GetResponseAsync();
    auto content = provider->ParseResponse(response);
    using ContentTypeType = std::remove_reference<decltype(content.get_type())>::type;
    if (content.get_type() != ContentTypeType::TEXT)
    {
        throw Schema::Rpc::Exception(
            Schema::Rpc::ErrorCode::BAD_GATEWAY,
            content.get_data());
    }
    co_return static_cast<nlohmann::json>(content.get_data());
}

/**
 * @brief Get model list with metadata.
 * 
 * @attention Access: any user
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnGetModelListAsync(CallerId callerId, nlohmann::json paramsJson)
{
    auto params = ParseParams<Schema::IServer::GetModelListParams>(paramsJson);
    auto lock = _resourceVersionManager->GetReadLock({"modelList"}, callerId);

    auto list = _database->ListModel();
    auto metadataKeys = params.get_metadata_keys();
    Schema::IServer::GetModelListResult result{};
    result.reserve(list.size());
    for (const auto& item : list)
    {
        using EntryType = decltype(result)::value_type;
        EntryType entry{};
        entry.set_id(static_cast<std::string>(item.id));
        if (metadataKeys.has_value())
        {
            using MetadataType = std::invoke_result_t<decltype(&EntryType::get_metadata), EntryType&>;
            MetadataType metadata{TryGetMetadata(metadataKeys.value(), item.metadata)};
            entry.set_metadata(metadata);
        }
        result.push_back(std::move(entry));
    }

    co_return static_cast<nlohmann::json>(result);
}

/**
 * @brief Create a new model.
 * 
 * @attention Access: admin
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnNewModelAsync(CallerId callerId, nlohmann::json paramsJson)
{
    CheckAdmin(callerId.userId);
    auto params = ParseParams<Schema::IServer::ModelSettings>(paramsJson);
    auto lock = _resourceVersionManager->GetWriteLock({"modelList"}, callerId);

    {
        /** Check params by creating a instance */
        ApiProvider::Factory::CreateProvider(
            params.get_provider_name(), params.get_provider_params());
    }
    auto settingsString = (static_cast<nlohmann::json>(params)).dump();
    auto id = co_await _database->CreateModelAsync(settingsString);
    auto readLock = _resourceVersionManager->GetReadLock(
        {"model", static_cast<std::string>(id)}, callerId);

    co_return static_cast<nlohmann::json>(static_cast<std::string>(id));
}

/**
 * @brief Get the model's admin settings 
 * 
 * @attention Access: admin
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnGetModelAsync(CallerId callerId, nlohmann::json paramsJson)
{
    CheckAdmin(callerId.userId);
    auto params = ParseParams<std::string>(paramsJson);
    Common::Uuid modelId{params};
    auto lock = _resourceVersionManager->GetReadLock(
        {"model", static_cast<std::string>(modelId)}, callerId);

    auto settingsString = _database->GetModelSettings(modelId);
    auto settings = nlohmann::json::parse(settingsString).get<Schema::IServer::ModelSettings>();

    co_return static_cast<nlohmann::json>(settings);
}

/**
 * @brief Delete a model
 * 
 * @attention Access: admin
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnDeleteModelAsync(CallerId callerId, nlohmann::json paramsJson)
{
    CheckAdmin(callerId.userId);
    auto params = ParseParams<std::string>(paramsJson);
    Common::Uuid modelId{params};
    auto lock0 = _resourceVersionManager->GetWriteLock({"modelList"}, callerId);
    auto lock1 = _resourceVersionManager->GetDeleteLock(
        {"model", static_cast<std::string>(modelId)}, callerId);

    co_await _database->DeleteModelAsync(modelId);
    _providers.erase(modelId);

    /** Return null */
    co_return nlohmann::json{};
}

/**
 * @brief Modify a model's admin settings
 * 
 * @attention Access: admin
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnModifyModelAsync(CallerId callerId, nlohmann::json paramsJson)
{
    CheckAdmin(callerId.userId);
    auto params = ParseParams<Schema::IServer::ModifyModelSettingsParams>(paramsJson);
    Common::Uuid modelId{params.get_id()};
    auto lock = _resourceVersionManager->GetWriteLock(
        {"model", static_cast<std::string>(modelId)}, callerId);

    auto settings = params.get_settings();
    co_await _database->SetModelSettingsAsync(modelId, (static_cast<nlohmann::json>(settings)).dump());
    /** Model parameter changed, delete the existing provider. */
    _providers.erase(modelId);

    /** return null */
    co_return nlohmann::json{};
}

/**
 * @brief 
 * 
 * @attention Access: admin
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnGetUserListAsync(CallerId callerId, nlohmann::json paramsJson)
{
    CheckAdmin(callerId.userId);
    auto params = ParseParams<Schema::IServer::GetUserListParams>(paramsJson);
    auto lock = _resourceVersionManager->GetReadLock({"userList"}, callerId);
    auto list = _database->ListUser();
    Schema::IServer::GetUserListResult result{};
    result.reserve(list.size());
    for (const auto& item: list)
    {
        using EntryType = decltype(result)::value_type;
        EntryType entry{};
        entry.set_id(static_cast<std::string>(item.id));
        entry.set_user_name(item.userName);
        auto userAdminSettings = nlohmann::json::parse(item.adminSettings).get<Schema::IServer::UserAdminSettings>();
        /** @todo force up to date the user admin settings */
        entry.set_admin_settings(std::move(userAdminSettings));
        if (params.get_metadata_keys().has_value())
        {
            using MetadataType = std::remove_reference<decltype(entry.get_public_metadata())>::type;
            MetadataType metadata{TryGetMetadata(params.get_metadata_keys().value(), item.publicMetadata)};
            entry.set_public_metadata(std::move(metadata));
        }
        result.push_back(std::move(entry));
    }
    result.shrink_to_fit();
    co_return static_cast<nlohmann::json>(result);
}

/**
 * @brief 
 * 
 * @attention Access: admin
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnNewUserAsync(CallerId callerId, nlohmann::json paramsJson)
{
    CheckAdmin(callerId.userId);
    auto params = ParseParams<Schema::IServer::NewUserParams>(paramsJson);
    auto lock = _resourceVersionManager->GetWriteLock({"userList"}, callerId);
    auto userId = co_await _database->CreateUserAsync(
        params.get_user_name(),
        (static_cast<nlohmann::json>(params.get_admin_settings())).dump(),
        (static_cast<nlohmann::json>(params.get_credential()).dump()));
    auto adminSettingsLock = _resourceVersionManager->GetReadLock(
        {"user", static_cast<std::string>(userId), "adminSettings"}, callerId);
    co_return static_cast<nlohmann::json>(static_cast<std::string>(userId));
}

/**
 * @brief 
 * 
 * @attention Access: admin
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnDeleteUserAsync(CallerId callerId, nlohmann::json paramsJson)
{
    CheckAdmin(callerId.userId);
    auto params = ParseParams<std::string>(paramsJson);
    Common::Uuid userId{params};
    auto lock0 = _resourceVersionManager->GetWriteLock({"userList"}, callerId);
    auto lock1 = _resourceVersionManager->GetDeleteLock(
        {"user", static_cast<std::string>(userId), "adminSettings"}, callerId);

    co_await _database->DeleteUserAsync(userId);
    _userRoleCache.erase(userId);
    /** Return null */
    co_return nlohmann::json{};
}

/**
 * @brief 
 * 
 * @attention Access: admin or current user
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnGetUserAdminSettingsAsync(CallerId callerId, nlohmann::json paramsJson)
{
    auto params = ParseParams<std::string>(paramsJson);
    Common::Uuid userId{params};
    if (callerId.userId != userId)
    {
        CheckAdmin(callerId.userId);
    }
    auto lock = _resourceVersionManager->GetReadLock(
        {"user", static_cast<std::string>(userId), "adminSettings"}, callerId);
    
    auto settingsString = _database->GetUserAdminSettings(userId);
    auto settings = nlohmann::json::parse(settingsString).get<Schema::IServer::UserAdminSettings>();

    co_return static_cast<nlohmann::json>(settings);
}

/**
 * @brief 
 * 
 * @attention Access: admin
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnSetUserAdminSettingsAsync(CallerId callerId, nlohmann::json paramsJson)
{
    CheckAdmin(callerId.userId);
    auto params = ParseParams<Schema::IServer::SetUserAdminSettingsParams>(paramsJson);
    Common::Uuid userId{params.get_id()};
    auto lock = _resourceVersionManager->GetWriteLock(
        {"user", static_cast<std::string>(userId), "adminSettings"}, callerId);
    
    auto& settings = params.get_admin_settings();
    auto settingsString = (static_cast<nlohmann::json>(settings)).dump();
    co_await _database->SetUserAdminSettingsAsync(userId, std::move(settingsString));
    _userRoleCache[userId] = settings.get_role();

    /** Return null */
    co_return nlohmann::json{};
}

/**
 * @brief 
 * 
 * @attention Access: curent user
 * 
 * @param callerId 
 * @param paramsJson 
 * @return JS::Promise<nlohmann::json> 
 */
JS::Promise<nlohmann::json> Service::OnSetUserCredentialAsync(CallerId callerId, nlohmann::json paramsJson)
{
    auto params = ParseParams<Schema::IServer::UserCredential>(paramsJson);
    /** DO NOT acquire lock. The credential is write only */
    co_await _database->SetUserCredentialAsync(
        callerId.userId,
        (static_cast<nlohmann::json>(params)).dump());
    /** Return null */
    co_return nlohmann::json{};
}

void Service::OnNewConnection(CallerId callerId)
{
    /** 
     * set up the user role cache if not already
     * @todo update the cache when user admin settings change 
     */
    if (_userRoleCache.find(callerId.userId) == _userRoleCache.end())
    {
        auto userAdminSettingsString = _database->GetUserAdminSettings(callerId.userId);
        auto userAdminSettingsJson = nlohmann::json::parse(userAdminSettingsString);
        auto userAdminSettings = userAdminSettingsJson.get<Schema::IServer::UserAdminSettings>();
        _userRoleCache[callerId.userId] = userAdminSettings.get_role();
    }
    /** @todo more logic */
}

void Service::OnConnectionClosed(CallerId callerId)
{
    (void)callerId;

    /** @todo more logic */
}

void Service::OnCriticalError(const std::string& message)
{
    /** @todo close */
    if (_onCriticalError)
    {
        _onCriticalError(message);
    }
}

std::shared_ptr<ApiProvider::IProvider> Service::GetProvider(const Common::Uuid& id)
{
    auto item = _providers.find(id);
    if (item != _providers.end())
    {
        return item->second;
    }
    auto settingsString = _database->GetModelSettings(id);
    auto settings = nlohmann::json::parse(settingsString).get<Schema::IServer::ModelSettings>();
    auto provider = ApiProvider::Factory::CreateProvider(settings.get_provider_name(), settings.get_provider_params());
    _providers[id] = provider;
    return provider;
}

std::map<std::string, nlohmann::json> Service::TryGetMetadata(const std::vector<std::string>& keys, const std::string& metadataString)
{
    std::map<std::string, nlohmann::json> metadata{};
    if (keys.empty())
    {
        return metadata;
    }
    try
    {
        auto metadataJson = nlohmann::json::parse(metadataString);
        for (const auto& key : keys)
        {
            if (metadataJson.contains(key))
            {
                metadata[key] = std::move(metadataJson.at(key));
            }
        }
    }
    catch(...)
    {
        /** Ignored */
    }
    return metadata;
}

std::string Service::TryMergeMetadata(const std::string& base, std::map<std::string, nlohmann::json>& changes)
{
    try
    {
        auto metadata = nlohmann::json::parse(base);
        for (auto& [key, value] : changes)
        {
            metadata[key] = std::move(value);
        }
        return metadata.dump();
    }
    catch(...)
    {
        /** @todo logging */
        /** If anything happens, use the new value as the metadata */
        return nlohmann::json(changes).dump();
    }
}

std::string Service::TryDeleteMetadata(const std::string& base, const std::vector<std::string>& keys)
{
    try
    {
        auto metadata = nlohmann::json::parse(base);
        for (const auto& key : keys)
        {
            metadata.erase(key);
        }
        return metadata.dump();
    }
    catch(...)
    {
        /** @todo logging */
        /** If anything happens, just delete the whole thing */
        return "{}";
    }
}


void Service::CheckAdmin(const Common::Uuid& userId)
{
    Schema::IServer::UserAdminSettingsRole role = Schema::IServer::UserAdminSettingsRole::USER;
    auto item = _userRoleCache.find(userId);
    if (item == _userRoleCache.end())
    {
        auto settingsString = _database->GetUserAdminSettings(userId);
        auto settings = nlohmann::json::parse(settingsString).get<Schema::IServer::UserAdminSettings>();
        role = settings.get_role();
        _userRoleCache[userId] = role;
    }
    else
    {
        role = item->second;
    }
    if (role != Schema::IServer::UserAdminSettingsRole::ADMIN)
    {
        throw Schema::Rpc::Exception(
            Schema::Rpc::ErrorCode::UNAUTHORIZED,
            "Current user is not an admin");
    }
}

