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
