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

set(BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/backends/MemoryBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/backends/FileBackend.cpp
)

set(BACKEND_LIBS
    GTest::GTest
    GTest::Main
    Threads::Threads
)

option(WITH_ROCKSDB "Build with RocksDB backend" OFF)
if(WITH_ROCKSDB)
    find_package(RocksDB QUIET)
    if(NOT RocksDB_FOUND)
        find_package(PkgConfig)
        if(PkgConfig_FOUND)
            pkg_check_modules(ROCKSDB IMPORTED_TARGET rocksdb)
        endif()
    endif()

    if(RocksDB_FOUND OR ROCKSDB_FOUND)
        list(APPEND BACKEND_SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/backends/RocksDbBackend.cpp
        )
    endif()
endif()

option(WITH_SQLITE "Build with SQLite backend" OFF)
if(WITH_SQLITE)
    find_package(SQLite3 REQUIRED)
    list(APPEND BACKEND_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/backends/SqliteBackend.cpp
    )
endif()

add_executable(backend_conformance_test
    BackendConformanceTest.cpp
    ${BACKEND_SOURCES}
)

target_include_directories(backend_conformance_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(backend_conformance_test ${BACKEND_LIBS})

if(WITH_ROCKSDB)
    if(RocksDB_FOUND)
        target_link_libraries(backend_conformance_test RocksDB::rocksdb)
    elseif(ROCKSDB_FOUND)
        target_link_libraries(backend_conformance_test PkgConfig::ROCKSDB)
    endif()
    target_compile_definitions(backend_conformance_test PRIVATE HAVE_ROCKSDB)
endif()

if(WITH_SQLITE)
    target_link_libraries(backend_conformance_test SQLite::SQLite3)
    target_compile_definitions(backend_conformance_test PRIVATE HAVE_SQLITE)
endif()

include(GoogleTest)
gtest_discover_tests(backend_conformance_test)
