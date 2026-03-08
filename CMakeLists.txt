cmake_minimum_required(VERSION 3.16)
project(kv_module VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── LogosModule.cmake ────────────────────────────────────────────────────────
if(DEFINED ENV{LOGOS_MODULE_CMAKE_PATH})
    include($ENV{LOGOS_MODULE_CMAKE_PATH})
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LogosModule.cmake")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR
        "LogosModule.cmake not found. Set LOGOS_MODULE_CMAKE_PATH or use 'nix build'.")
endif()

# ── SQLite (optional) ────────────────────────────────────────────────────────
option(WITH_SQLITE "Build SQLite backend" OFF)
if(WITH_SQLITE)
    find_package(SQLite3 REQUIRED)
endif()

# ── RocksDB (optional) ──────────────────────────────────────────────────────
option(WITH_ROCKSDB "Build RocksDB backend" OFF)
if(WITH_ROCKSDB)
    find_package(RocksDB QUIET)
    if(NOT RocksDB_FOUND)
        find_package(PkgConfig)
        if(PkgConfig_FOUND)
            pkg_check_modules(ROCKSDB IMPORTED_TARGET rocksdb)
        endif()
    endif()
endif()

# ── Source files ─────────────────────────────────────────────────────────────
set(KV_SOURCES
    src/i_kv_module.h
    src/kv_interface.h
    src/kv_plugin.h
    src/kv_plugin.cpp
    src/plugin.cpp
    src/backends/MemoryBackend.cpp
    src/backends/FileBackend.cpp
)

if(WITH_ROCKSDB AND (RocksDB_FOUND OR ROCKSDB_FOUND))
    list(APPEND KV_SOURCES src/backends/RocksDbBackend.cpp)

if(WITH_SQLITE)
    list(APPEND KV_SOURCES src/backends/SqliteBackend.cpp)
endif()
endif()

# ── Module target ────────────────────────────────────────────────────────────
# logos_module() creates target "kv_module_module_plugin" with output
# name "kv_module_plugin", links Qt6::Core and Qt6::RemoteObjects,
# sets up AUTOMOC, RPATH, and install rules.
logos_module(
    NAME kv_module
    SOURCES ${KV_SOURCES}
)

# SDK is always available when built through logos_module()
target_compile_definitions(kv_module_module_plugin PRIVATE LOGOS_CORE_AVAILABLE)

# ── RocksDB linking ─────────────────────────────────────────────────────────
if(WITH_ROCKSDB)
    if(RocksDB_FOUND)
        target_link_libraries(kv_module_module_plugin PRIVATE RocksDB::rocksdb)
    elseif(ROCKSDB_FOUND)
        target_link_libraries(kv_module_module_plugin PRIVATE PkgConfig::ROCKSDB)
    endif()
    target_compile_definitions(kv_module_module_plugin PRIVATE HAVE_ROCKSDB)

if(WITH_SQLITE)
    target_link_libraries(kv_module_module_plugin PRIVATE SQLite::SQLite3)
    target_compile_definitions(kv_module_module_plugin PRIVATE HAVE_SQLITE)
endif()
endif()

# ── Tests ────────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build conformance tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
