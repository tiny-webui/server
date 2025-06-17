#include <iostream>
#include <memory>
#include <js-style-co-routine/AsyncGenerator.h>
#include <js-style-co-routine/Promise.h>
#include "rpc/RpcServer.h"
#include "schema/IServer.h"
#include "Utility.h"

using namespace TUI;

class Connection : public Network::IConnection<int>
{
public:
    Connection(int id) : _id(id) {}
    ~Connection() override
    {
        Close();
    };

    void Close() override
    {
        if (_closed)
        {
            return;
        }
        _closed = true;
        _txGenerator.Finish();
        _rxGenerator.Finish();
    }

    /**
     * @brief Send from peer. So this is RX.
     * 
     * @param message 
     */
    void Send(std::vector<std::uint8_t> message) override
    {
        if (_closed)
        {
            throw std::runtime_error("Connection is closed");
        }
        _rxGenerator.Feed(std::move(message));
    }

    /**
     * @brief Receive by peer. So this is TX.
     * 
     * @return JS::Promise<std::optional<std::vector<std::uint8_t>>> 
     */
    JS::Promise<std::optional<std::vector<std::uint8_t>>> ReceiveAsync() override
    {
        if (_closed)
        {
            throw std::runtime_error("Connection is closed");
        }
        return _txGenerator.NextAsync();
    }

    int GetId() const override
    { 
        return _id;
    }

    bool IsClosed() const noexcept override
    {
        return _closed;
    }

    /**
     * @brief Make a normal request.
     * 
     * For simplicity, concurrent requests are not supported.
     * 
     * @param method 
     * @param params 
     * @return JS::Promise<nlohmann::json> 
     */
    JS::Promise<nlohmann::json> MakeRequestAsync(const std::string& method, const nlohmann::json& params)
    {
        SendRequest(method, params);
        auto message = co_await _rxGenerator.NextAsync();
        if (!message.has_value())
        {
            throw std::runtime_error("Connection closed while waiting for response");
        }
        auto response = ParseResponse(message.value());
        if (!response.has_value())
        {
            throw std::runtime_error("Invalid end message");
        }
        co_return response.value();
    }

    JS::AsyncGenerator<nlohmann::json> MakeStreamRequest(const std::string& method, const nlohmann::json& params)
    {
        SendRequest(method, params);
        while (true)
        {
            auto message = co_await _rxGenerator.NextAsync();
            if (!message.has_value())
            {
                throw std::runtime_error("Connection closed while waiting for response");
            }
            auto response = ParseResponse(message.value());
            if (!response.has_value())
            {
                break;
            }
            co_yield response.value();
        }
    }

    void SendNotification(const std::string& method, const nlohmann::json& params)
    {
        SendRequest(method, params);
    }
private:
    int _id;
    JS::AsyncGenerator<std::vector<std::uint8_t>> _txGenerator{};
    JS::AsyncGenerator<std::vector<std::uint8_t>> _rxGenerator{};
    int _messageIdSeed{0};
    bool _closed{false};

    void SendRequest(const std::string& method, const nlohmann::json& params)
    {
        Schema::Rpc::Request request{};
        request.set_id(++_messageIdSeed);
        request.set_method(method);
        request.set_params(params);
        nlohmann::json requestJson{};
        Schema::Rpc::to_json(requestJson, request);
        auto requestStr = requestJson.dump();
        std::vector<std::uint8_t> requestData(requestStr.begin(), requestStr.end());
        _txGenerator.Feed(std::move(requestData));
    }

    std::optional<nlohmann::json> ParseResponse(const std::vector<std::uint8_t>& rawData)
    {
        auto responseJson = nlohmann::json::parse(rawData.begin(), rawData.end());
        if (responseJson.contains("error"))
        {
            auto errorResponse = responseJson.get<Schema::Rpc::ErrorResponse>();
            throw Schema::Rpc::Exception(
                errorResponse.get_error().get_code(),
                errorResponse.get_error().get_message());
        }
        else if (responseJson.contains("end"))
        {
            return std::nullopt;
        }
        else
        {
            auto response = responseJson.get<Schema::Rpc::Response>();
            return std::make_optional<nlohmann::json>(response.get_result());
        }
    }
};

class Server : public Network::IServer<int>
{
public:
    Server() = default;
    ~Server() override
    {
        Close();
    };

    void Close() override
    {
        if (_closed)
        {
            return;
        }
        _closed = true;
        _connectionGenerator.Finish();
    }

    bool IsClosed() const noexcept override
    {
        return _closed;
    }

    JS::Promise<std::optional<std::shared_ptr<Network::IConnection<int>>>> AcceptAsync() override
    {
        if (_closed)
        {
            throw std::runtime_error("Server is closed, cannot accept new connections.");
        }
        return _connectionGenerator.NextAsync();
    }

    std::shared_ptr<Connection> CreateConnection()
    {
        auto connection = std::make_shared<Connection>(++_connectionIdSeed);
        _connectionGenerator.Feed(connection);
        return connection;
    }
private:
    JS::AsyncGenerator<std::shared_ptr<Network::IConnection<int>>> _connectionGenerator{};
    bool _closed{false};
    int _connectionIdSeed{0};
};

class TestServer
{
public:
    std::unique_ptr<Rpc::RpcServer<int>> rpcServer{nullptr};
    int lastClosedConnectionId{-1};
    nlohmann::json lastNotificationParams{};

    TestServer(std::shared_ptr<Network::IServer<int>> server)
    {
        rpcServer = std::make_unique<Rpc::RpcServer<int>>(
            std::move(server),
            std::unordered_map<std::string, Rpc::RpcServer<int>::RequestHandler>{
                {"request", std::bind(&TestServer::TestRequestHandler, this, std::placeholders::_1, std::placeholders::_2)}
            },
            std::unordered_map<std::string, Rpc::RpcServer<int>::StreamRequestHandler>{
                {"stream", std::bind(&TestServer::TestStreamRequestHandler, this, std::placeholders::_1, std::placeholders::_2)}
            },
            std::unordered_map<std::string, Rpc::RpcServer<int>::NotificationHandler>{
                {"notification", std::bind(&TestServer::TestNotificationHandler, this, std::placeholders::_1, std::placeholders::_2)}
            },
            [](int id) { std::cout << "New connection: " << id << std::endl; },
            [this](int id) {
                std::cout << "Connection closed: " << id << std::endl;
                lastClosedConnectionId = id;
            },
            [](const std::string& error) { std::cerr << "Critical error: " << error << std::endl; });
    }

    JS::Promise<nlohmann::json> TestRequestHandler(int id, nlohmann::json params)
    {
        std::cout << "Received request from connection " << id << ": " << params.dump() << std::endl;
        co_return "Hello";
    }

    JS::AsyncGenerator<nlohmann::json> TestStreamRequestHandler(int id, nlohmann::json params)
    {
        std::cout << "Received stream request from connection " << id << ": " << params.dump() << std::endl;
        for (int i = 0; i < 5; ++i)
        {
            nlohmann::json item = i;
            co_yield item;
        }
    }

    void TestNotificationHandler(int id, nlohmann::json params)
    {
        std::cout << "Received notification from connection " << id << ": " << params.dump() << std::endl;
        lastNotificationParams = params;
    }
};

std::shared_ptr<Server> g_server{nullptr};
std::unique_ptr<TestServer> g_testServer{nullptr};

void TestCreate()
{
    g_server = std::make_shared<Server>();
    g_testServer = std::make_unique<TestServer>(g_server);
}

void TestCleanup()
{
    g_testServer.reset();
    g_server.reset();
}

void TestConnection()
{
    auto connection1 = g_server->CreateConnection();
    auto connection2 = g_server->CreateConnection();
    auto count = g_testServer->rpcServer->CountConnection([](const int&) { return true; });
    AssertWithMessage(count == 2, "Expected 2 connections, got " + std::to_string(count));
    g_testServer->rpcServer->CloseConnection([=](const int& id) { return id == connection1->GetId(); });
    count = g_testServer->rpcServer->CountConnection([](const int&) { return true; });
    AssertWithMessage(count == 1, "Expected 1 connection, got " + std::to_string(count));
    connection2->Close();
    count = g_testServer->rpcServer->CountConnection([](const int&) { return true; });
    AssertWithMessage(count == 0, "Expected 0 connections, got " + std::to_string(count));
    AssertWithMessage(g_testServer->lastClosedConnectionId == connection2->GetId(),
                      "Expected last closed connection id to be " + std::to_string(connection2->GetId()) +
                      ", got " + std::to_string(g_testServer->lastClosedConnectionId));
}

JS::Promise<void> TestRequestAsync()
{
    auto connection = g_server->CreateConnection();
    auto params =  nlohmann::json{{"key", "value"}};
    auto response = co_await connection->MakeRequestAsync("request", params);
    AssertWithMessage(response.get<std::string>() == "Hello", "Expected response 'Hello', got '" + response.dump() + "'");
    co_return;
}

JS::Promise<void> TestStreamRequestAsync()
{
    auto connection = g_server->CreateConnection();
    auto params = nlohmann::json{{"key", "value"}};
    auto stream = connection->MakeStreamRequest("stream", params);
    int count = 0;
    while (true)
    {
        auto item = co_await stream.NextAsync();
        if (!item.has_value())
        {
            break;
        }
        AssertWithMessage(item->is_number_integer(), "Expected item to be an integer, got " + item->dump());
        AssertWithMessage(item->get<int>() == count, "Expected item " + std::to_string(count) + ", got " + item->dump());
        ++count;
    }
    AssertWithMessage(count == 5, "Expected 5 items, got " + std::to_string(count));
    co_return;
}

void TestNotification()
{
    auto connection = g_server->CreateConnection();
    nlohmann::json params = "hello";
    connection->SendNotification("notification", params);
    AssertWithMessage(g_testServer->lastNotificationParams.get<std::string>() == params.get<std::string>(),
                      "Expected last notification params to be " + params.dump() +
                      ", got " + g_testServer->lastNotificationParams.dump());
}

JS::Promise<void> TestAsync()
{
    RunTest(TestCreate());
    RunTest(TestConnection());
    RunAsyncTest(TestRequestAsync());
    RunAsyncTest(TestStreamRequestAsync());
    RunTest(TestNotification());
    RunTest(TestCleanup());
    co_return;
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    TestAsync();

    return 0;
}
