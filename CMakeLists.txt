cmake_minimum_required(VERSION 3.10)

project(tui-test LANGUAGES CXX)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Enable verbose build
set(CMAKE_VERBOSE_MAKEFILE ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict warnings and errors
add_compile_options(-Wall -Wextra -Werror -pedantic)

# Find all .cpp files under ./src
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Remove files from src/schema/src
file(GLOB_RECURSE SCHEMA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/schema/src/*.cpp")
list(REMOVE_ITEM SOURCES ${SCHEMA_SOURCES})

add_executable(tui-server ${SOURCES})

target_include_directories(tui-server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(tui-server
    PRIVATE
        tev-cpp
        curl
        websockets
        sqlite3
        uuid
        ssl
        crypto)
