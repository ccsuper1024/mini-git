cmake_minimum_required(VERSION 3.10)

project(mini_git VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MINIGIT_BUILD_TESTS "Build tests" ON)

find_package(ZLIB REQUIRED)
find_package(spdlog REQUIRED)

if(MINIGIT_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
endif()

add_library(minigit
    src/hash.cpp
    src/zlib_utils.cpp
    src/filesystem.cpp
    src/blob.cpp
    src/object_store.cpp
    src/tree.cpp
    src/commit.cpp
    src/refs.cpp
    src/checkout.cpp
    src/index.cpp
)

target_include_directories(minigit
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(minigit
    PUBLIC
        spdlog::spdlog
        ZLIB::ZLIB
)

add_executable(minigit_cli
    src/main.cpp
)

target_link_libraries(minigit_cli
    PRIVATE
        minigit
)

if(MINIGIT_BUILD_TESTS)
    add_executable(minigit_tests
        tests/test_hash.cpp
        tests/test_zlib_utils.cpp
        tests/test_object_store.cpp
        tests/test_tree.cpp
        tests/test_commit.cpp
        tests/test_refs.cpp
        tests/test_checkout.cpp
        tests/test_index.cpp
    )

    target_link_libraries(minigit_tests
        PRIVATE
            minigit
            GTest::gtest
            GTest::gtest_main
    )

    add_test(NAME minigit_tests COMMAND minigit_tests)
endif()
