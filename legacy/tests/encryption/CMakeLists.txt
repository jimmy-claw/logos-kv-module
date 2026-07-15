cmake_minimum_required(VERSION 3.16)
project(kv_encryption_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(OpenSSL REQUIRED)

add_executable(encryption_test
    EncryptionTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kv_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/backends/MemoryBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/backends/FileBackend.cpp
)

target_include_directories(encryption_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

target_link_libraries(encryption_test
    GTest::GTest GTest::Main Threads::Threads
    Qt6::Core
    OpenSSL::Crypto
)

include(GoogleTest)
gtest_discover_tests(encryption_test)
