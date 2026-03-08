cmake_minimum_required(VERSION 3.16)

# Support standalone build (for CI) or as a subdirectory
if(NOT CMAKE_PROJECT_NAME OR CMAKE_PROJECT_NAME STREQUAL "kv_module_tests")
    project(kv_module_tests LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

enable_testing()

add_executable(backend_conformance_test
    BackendConformanceTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/backends/MemoryBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/backends/FileBackend.cpp
)

target_include_directories(backend_conformance_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(backend_conformance_test
    GTest::GTest
    GTest::Main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(backend_conformance_test)
