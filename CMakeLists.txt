cmake_minimum_required(VERSION 3.20)
project(miniredis VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MINIREDIS_BUILD_TESTS "Build the unit tests" ON)

add_library(miniredis_warnings INTERFACE)
target_compile_options(miniredis_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
)

find_package(Threads REQUIRED)

add_library(miniredis_core STATIC
    src/app/server_options.cpp
    src/command/builtin_commands.cpp
    src/command/command_dispatcher.cpp
    src/concurrency/thread_pool.cpp
    src/net/client_session.cpp
    src/net/socket.cpp
    src/net/tcp_server.cpp
    src/resp/resp_encoder.cpp
    src/resp/resp_parser.cpp
    src/resp/resp_value.cpp
    src/store/expiry_reaper.cpp
    src/store/key_value_store.cpp
)
target_include_directories(miniredis_core PUBLIC include)
target_link_libraries(miniredis_core PUBLIC Threads::Threads PRIVATE miniredis_warnings)

add_executable(miniredis src/main.cpp)
target_link_libraries(miniredis PRIVATE miniredis_core miniredis_warnings)

if(MINIREDIS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
