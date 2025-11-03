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

# Add extra include and library paths for local libraries.
if(DEFINED ENV{TUI_EXTRA_INCLUDE_PATH})
    include_directories($ENV{TUI_EXTRA_INCLUDE_PATH})
endif()
if(DEFINED ENV{TUI_EXTRA_LIB_PATH})
    link_directories($ENV{TUI_EXTRA_LIB_PATH})
endif()

# Find all .cpp files under ./src
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Remove files from src/schema/src
file(GLOB_RECURSE SCHEMA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/schema/src/*.cpp")
list(REMOVE_ITEM SOURCES ${SCHEMA_SOURCES})

# Remove the tui-register.cpp entry
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/tui-register.cpp")

add_executable(tui-server ${SOURCES})

target_include_directories(tui-server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(tui-server
    PRIVATE
        curl
        websockets
        sqlite3
        uuid
        sodium)

add_executable(tui-register
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tui-register.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/Base64.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/Timestamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/Utf8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/database/Database.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/database/Sqlite.cpp)

target_include_directories(tui-register
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(tui-register
    PRIVATE
        sqlite3
        uuid)

install(TARGETS tui-server RUNTIME DESTINATION bin)
install(TARGETS tui-register RUNTIME DESTINATION bin)
