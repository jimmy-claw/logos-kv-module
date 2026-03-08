cmake_minimum_required(VERSION 3.16)
project(kv_module VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt6 ───────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Qml)

set(CMAKE_AUTOMOC ON)

# ── logos-cpp-sdk ─────────────────────────────────────────────────────────────
set(LOGOS_CORE_ROOT "" CACHE PATH "Path to logos-cpp-sdk install (contains include/ and lib/)")
if(NOT "${LOGOS_CORE_ROOT}" STREQUAL "")
    add_library(logos_cpp_sdk INTERFACE)
    target_include_directories(logos_cpp_sdk INTERFACE
        "${LOGOS_CORE_ROOT}/include"
        "${LOGOS_CORE_ROOT}/include/cpp"
    )

    add_library(logos_core SHARED IMPORTED)
    set_target_properties(logos_core PROPERTIES
        IMPORTED_LOCATION "${LOGOS_CORE_ROOT}/lib/liblogos_core.so"
    )
endif()

# ── RocksDB (optional) ────────────────────────────────────────────────────────
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

# ── SQLite (optional) ─────────────────────────────────────────────────────────
option(WITH_SQLITE "Build SQLite backend" OFF)
if(WITH_SQLITE)
    find_package(SQLite3 REQUIRED)
endif()

# ── Plugin target ─────────────────────────────────────────────────────────────
qt_add_plugin(kv_module CLASS_NAME KvPlugin)

target_sources(kv_module PRIVATE
    src/kv_plugin.cpp
    src/plugin.cpp
    src/backends/MemoryBackend.cpp
    src/backends/FileBackend.cpp
)

if(WITH_ROCKSDB AND (RocksDB_FOUND OR ROCKSDB_FOUND))
    target_sources(kv_module PRIVATE src/backends/RocksDbBackend.cpp)
endif()

if(WITH_SQLITE)
    target_sources(kv_module PRIVATE src/backends/SqliteBackend.cpp)
endif()

set_property(TARGET kv_module PROPERTY PUBLIC_HEADER
    src/i_kv_module.h
)

target_include_directories(kv_module PRIVATE src)

target_link_libraries(kv_module PRIVATE
    Qt6::Core
    Qt6::Qml
)

if(NOT "${LOGOS_CORE_ROOT}" STREQUAL "")
    target_link_libraries(kv_module PRIVATE logos_cpp_sdk logos_core)
    target_compile_definitions(kv_module PRIVATE LOGOS_CORE_AVAILABLE)
endif()

if(WITH_ROCKSDB)
    if(RocksDB_FOUND)
        target_link_libraries(kv_module PRIVATE RocksDB::rocksdb)
    elseif(ROCKSDB_FOUND)
        target_link_libraries(kv_module PRIVATE PkgConfig::ROCKSDB)
    endif()
    target_compile_definitions(kv_module PRIVATE HAVE_ROCKSDB)
endif()

if(WITH_SQLITE)
    target_link_libraries(kv_module PRIVATE SQLite::SQLite3)
    target_compile_definitions(kv_module PRIVATE HAVE_SQLITE)
endif()

# ── Compile definitions ──────────────────────────────────────────────────────
target_compile_definitions(kv_module PRIVATE
    QT_PLUGIN
)

# ── RPATH ─────────────────────────────────────────────────────────────────────
if(APPLE)
    set_target_properties(kv_module PROPERTIES
        BUILD_RPATH   "@loader_path"
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    set_target_properties(kv_module PROPERTIES
        BUILD_RPATH   "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS kv_module
    LIBRARY       DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# ── Tests ─────────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build conformance tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
