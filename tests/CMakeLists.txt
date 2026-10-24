include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(miniredis_tests
    command_dispatcher_test.cpp
    expiry_reaper_test.cpp
    key_value_store_test.cpp
    lru_cache_test.cpp
    resp_encoder_test.cpp
    resp_parser_test.cpp
    server_options_test.cpp
    tcp_server_test.cpp
    thread_pool_test.cpp
)
target_link_libraries(miniredis_tests PRIVATE miniredis_core miniredis_warnings GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(miniredis_tests)
