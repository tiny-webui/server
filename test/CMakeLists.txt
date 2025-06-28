cmake_minimum_required(VERSION 3.10)

project(tui-test LANGUAGES CXX)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Enable verbose build
set(CMAKE_VERBOSE_MAKEFILE ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict warnings and errors
add_compile_options(-Wall -Wextra -Werror -pedantic)

add_executable(TestHttpClient
    TestHttpClient.cpp
    ../src/network/CurlTypes.cpp
    ../src/network/HttpClient.cpp)

target_include_directories(TestHttpClient
    PRIVATE
        ../src)

target_link_libraries(TestHttpClient
    PRIVATE
        curl
        tev-cpp)

add_executable(TestWebSocketServer
    TestWebSocketServer.cpp
    ../src/network/LwsTypes.cpp
    ../src/network/WebSocketServer.cpp)

target_include_directories(TestWebSocketServer
    PRIVATE
        ../src)

target_link_libraries(TestWebSocketServer
    PRIVATE
        tev-cpp
        websockets)

add_executable(TestHttpStreamResponseParser
    TestHttpStreamResponseParser.cpp
    ../src/network/HttpStreamResponseParser.cpp)

target_include_directories(TestHttpStreamResponseParser
    PRIVATE
        ../src)

add_executable(TestApiProviderOption
    TestApiProviderOption.cpp)

target_include_directories(TestApiProviderOption
    PRIVATE
        ../src)

add_executable(TestApiProvider
    TestApiProvider.cpp
    ../src/network/HttpClient.cpp
    ../src/network/CurlTypes.cpp
    ../src/network/HttpStreamResponseParser.cpp
    ../src/apiProvider/Factory.cpp
    ../src/apiProvider/AzureOpenAI.cpp)

target_include_directories(TestApiProvider
    PRIVATE
        ../src)

target_link_libraries(TestApiProvider
    PRIVATE
        tev-cpp
        curl)

add_executable(TestTevInjectionQueue
    TestTevInjectionQueue.cpp)

target_include_directories(TestTevInjectionQueue
    PRIVATE
        ../src)

target_link_libraries(TestTevInjectionQueue
    PRIVATE
        tev-cpp)

add_executable(TestWorkerThread
    TestWorkerThread.cpp)

target_include_directories(TestWorkerThread
    PRIVATE
        ../src)

target_link_libraries(TestWorkerThread
    PRIVATE
        tev-cpp)

add_executable(TestSqlite
    TestSqlite.cpp
    ../src/database/Sqlite.cpp)

target_include_directories(TestSqlite
    PRIVATE
        ../src)

target_link_libraries(TestSqlite
    PRIVATE
        tev-cpp
        sqlite3)

add_executable(TestUuid
    TestUuid.cpp)

target_include_directories(TestUuid
    PRIVATE
        ../src)

target_link_libraries(TestUuid
    PRIVATE
        uuid)

add_executable(TestDatabase
    TestDatabase.cpp
    ../src/database/Database.cpp
    ../src/database/Sqlite.cpp
    ../src/common/Utilities.cpp)

target_include_directories(TestDatabase
    PRIVATE
        ../src)

target_link_libraries(TestDatabase
    PRIVATE
        tev-cpp
        sqlite3
        uuid)

add_executable(TestRpcServer
    TestRpcServer.cpp)

target_include_directories(TestRpcServer
    PRIVATE
        ../src)

add_executable(TestResourceVersionManager
    TestResourceVersionManager.cpp)

target_include_directories(TestResourceVersionManager
    PRIVATE
        ../src)

# Get all .cpp files from ../src except main.cpp
file(GLOB_RECURSE TEST_SERVICE_SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp)
list(REMOVE_ITEM TEST_SERVICE_SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cpp)

add_executable(TestService
    TestService.cpp
    ${TEST_SERVICE_SRC_FILES})

target_include_directories(TestService
    PRIVATE
        ../src)

target_link_libraries(TestService
    PRIVATE
        tev-cpp
        curl
        websockets
        sqlite3
        uuid)
