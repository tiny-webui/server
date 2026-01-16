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

# Add extra include and library paths for local libraries.
if(DEFINED ENV{TUI_EXTRA_INCLUDE_PATH})
    include_directories($ENV{TUI_EXTRA_INCLUDE_PATH})
endif()
if(DEFINED ENV{TUI_EXTRA_LIB_PATH})
    link_directories($ENV{TUI_EXTRA_LIB_PATH})
endif()

add_executable(TestHttpClient
    TestHttpClient.cpp
    ../src/network/CurlTypes.cpp
    ../src/network/HttpClient.cpp)

target_include_directories(TestHttpClient
    PRIVATE
        ../src)

target_link_libraries(TestHttpClient
    PRIVATE
        curl)

add_executable(TestWebSocketServer
    TestWebSocketServer.cpp
    ../src/network/LwsTypes.cpp
    ../src/network/WebSocketServer.cpp)

target_include_directories(TestWebSocketServer
    PRIVATE
        ../src)

target_link_libraries(TestWebSocketServer
    PRIVATE
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
    ../src/apiProvider/AzureOpenAI.cpp
    ../src/apiProvider/OpenAI.cpp)

target_include_directories(TestApiProvider
    PRIVATE
        ../src)

target_link_libraries(TestApiProvider
    PRIVATE
        curl)

add_executable(TestTevInjectionQueue
    TestTevInjectionQueue.cpp)

target_include_directories(TestTevInjectionQueue
    PRIVATE
        ../src)

add_executable(TestWorkerThread
    TestWorkerThread.cpp)

target_include_directories(TestWorkerThread
    PRIVATE
        ../src)

add_executable(TestSqlite
    TestSqlite.cpp
    ../src/database/Sqlite.cpp)

target_include_directories(TestSqlite
    PRIVATE
        ../src)

target_link_libraries(TestSqlite
    PRIVATE
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
    ../src/common/Timestamp.cpp)

target_include_directories(TestDatabase
    PRIVATE
        ../src)

target_link_libraries(TestDatabase
    PRIVATE
        sqlite3
        uuid)

add_executable(TestRpcServer
    TestRpcServer.cpp)

target_include_directories(TestRpcServer
    PRIVATE
        ../src)

add_executable(TestCounter
    TestCounter.cpp)

target_include_directories(TestCounter
    PRIVATE
        ../src)

add_executable(TestChaCha20Poly1305
    TestChaCha20Poly1305.cpp
    ../src/cipher/ChaCha20Poly1305.cpp)

target_include_directories(TestChaCha20Poly1305
    PRIVATE
        ../src)

target_link_libraries(TestChaCha20Poly1305
    PRIVATE
        sodium)

add_executable(TestChaCha20Poly1305Interactive
    TestChaCha20Poly1305Interactive.cpp
    ../src/cipher/ChaCha20Poly1305.cpp
    ../src/common/Base64.cpp)

target_include_directories(TestChaCha20Poly1305Interactive
    PRIVATE
        ../src)

target_link_libraries(TestChaCha20Poly1305Interactive
    PRIVATE
        sodium)

add_executable(TestCryptoKdfHkdfSha256
    TestCryptoKdfHkdfSha256.cpp)

target_link_libraries(TestCryptoKdfHkdfSha256
    PRIVATE
        sodium)

add_executable(TestEcdhePsk
    TestEcdhePsk.cpp
    ../src/cipher/EcdhePsk.cpp
    ../src/cipher/ChaCha20Poly1305.cpp)

target_include_directories(TestEcdhePsk
    PRIVATE
        ../src)

target_link_libraries(TestEcdhePsk
    PRIVATE
        sodium)

add_executable(TestEcdhePskInteractive
    TestEcdhePskInteractive.cpp
    ../src/cipher/EcdhePsk.cpp
    ../src/cipher/ChaCha20Poly1305.cpp
    ../src/common/Base64.cpp)

target_include_directories(TestEcdhePskInteractive
    PRIVATE
        ../src)

target_link_libraries(TestEcdhePskInteractive
    PRIVATE
        sodium)

add_executable(TestEd25519
    TestEd25519.cpp
    ../src/cipher/Ed25519.cpp)

target_include_directories(TestEd25519
    PRIVATE
        ../src)

target_link_libraries(TestEd25519
    PRIVATE
        sodium)

add_executable(TestSpake2p
    TestSpake2p.cpp
    ../src/cipher/Spake2p.cpp
    ../src/cipher/Ed25519.cpp
    ../src/cipher/ChaCha20Poly1305.cpp)

target_include_directories(TestSpake2p
    PRIVATE
        ../src)

target_link_libraries(TestSpake2p
    PRIVATE
        sodium)

add_executable(TestSpake2pInteractive
    TestSpake2pInteractive.cpp
    ../src/cipher/Spake2p.cpp
    ../src/cipher/Ed25519.cpp
    ../src/cipher/ChaCha20Poly1305.cpp
    ../src/common/Base64.cpp)

target_include_directories(TestSpake2pInteractive
    PRIVATE
        ../src)

target_link_libraries(TestSpake2pInteractive
    PRIVATE
        sodium)

add_executable(TestResourceVersionManager
    TestResourceVersionManager.cpp)

target_include_directories(TestResourceVersionManager
    PRIVATE
        ../src)

add_executable(TestStreamBatcher
    TestStreamBatcher.cpp)

target_include_directories(TestStreamBatcher
    PRIVATE
        ../src)

add_executable(TestCache
    TestCache.cpp)

target_include_directories(TestCache
    PRIVATE
        ../src)

add_executable(TestFakeCredentialGenerator
    TestFakeCredentialGenerator.cpp
    ../src/cipher/FakeCredentialGenerator.cpp
    ../src/cipher/Ed25519.cpp)

target_include_directories(TestFakeCredentialGenerator
    PRIVATE
        ../src)

target_link_libraries(TestFakeCredentialGenerator
    PRIVATE
        sodium)

add_executable(TestBruteForceLimiter
    TestBruteForceLimiter.cpp
    ../src/cipher/BruteForceLimiter.cpp
    ../src/common/Timestamp.cpp)

target_include_directories(TestBruteForceLimiter
    PRIVATE
        ../src)

add_executable(TestSecureSession
    TestSecureSession.cpp
    ../src/application/SecureSession.cpp
    ../src/cipher/ChaCha20Poly1305.cpp
    ../src/cipher/EcdhePsk.cpp
    ../src/cipher/Ed25519.cpp
    ../src/cipher/Spake2p.cpp
    ../src/cipher/FakeCredentialGenerator.cpp
    ../src/cipher/BruteForceLimiter.cpp
    ../src/common/Base64.cpp
    ../src/common/Timestamp.cpp)

target_include_directories(TestSecureSession
    PRIVATE
        ../src)

target_link_libraries(TestSecureSession
    PRIVATE
        uuid
        sodium)

add_executable(TestBase64
    TestBase64.cpp
    ../src/common/Base64.cpp)

target_include_directories(TestBase64
    PRIVATE
        ../src)

add_executable(TestTlv
    TestTlv.cpp)

target_include_directories(TestTlv
    PRIVATE
        ../src)

add_executable(TestUtf8
    TestUtf8.cpp
    ../src/common/Utf8.cpp)

target_include_directories(TestUtf8
    PRIVATE
        ../src)

add_executable(TestRegister
    TestRegister.cpp
    ../src/cipher/ChaCha20Poly1305.cpp
    ../src/cipher/Ed25519.cpp
    ../src/cipher/Spake2p.cpp
    ../src/common/Base64.cpp)

target_include_directories(TestRegister
    PRIVATE
        ../src)

target_link_libraries(TestRegister
    PRIVATE
        sodium)

# Get all .cpp files from ../src except main.cpp
file(GLOB_RECURSE TEST_SERVICE_SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp)
list(REMOVE_ITEM TEST_SERVICE_SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/tui-server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/tui-register.cpp)

add_executable(TestService
    TestService.cpp
    ${TEST_SERVICE_SRC_FILES})

target_include_directories(TestService
    PRIVATE
        ../src)

target_link_libraries(TestService
    PRIVATE
        curl
        websockets
        sqlite3
        uuid
        sodium)

add_executable(TestTuiServer
    TestTuiServer.cpp
    ${TEST_SERVICE_SRC_FILES})

target_include_directories(TestTuiServer
    PRIVATE
        ../src)

target_link_libraries(TestTuiServer
    PRIVATE
        curl
        websockets
        sqlite3
        uuid
        sodium)

add_executable(TestVectorSearch
    TestVectorSearch.cpp
    ../src/database/VectorSearch.cpp)

target_include_directories(TestVectorSearch
    PRIVATE
        ../src)

target_compile_definitions(TestVectorSearch
    PRIVATE
        TUI_TEST_INTERFACES)
