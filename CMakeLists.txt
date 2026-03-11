cmake_minimum_required(VERSION 3.16)
project(kv_module VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt6 ───────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Qml)

set(CMAKE_AUTOMOC ON)

# ── Logos SDK + liblogos ──────────────────────────────────────────────────────
set(LOGOS_CPP_SDK_ROOT "" CACHE PATH "Path to logos-cpp-sdk install")
set(LOGOS_LIBLOGOS_ROOT "" CACHE PATH "Path to logos-liblogos install")

if(NOT "${LOGOS_CPP_SDK_ROOT}" STREQUAL "" AND NOT "${LOGOS_LIBLOGOS_ROOT}" STREQUAL "")
    set(LOGOS_CORE_AVAILABLE ON)

    add_library(logos_cpp_sdk INTERFACE)
    target_include_directories(logos_cpp_sdk INTERFACE
        "${LOGOS_LIBLOGOS_ROOT}/include"
        "${LOGOS_CPP_SDK_ROOT}/include"
        "${LOGOS_CPP_SDK_ROOT}/include/cpp"
        "${LOGOS_CPP_SDK_ROOT}/include/core"
    )

    add_library(logos_core SHARED IMPORTED)
    set_target_properties(logos_core PROPERTIES
        IMPORTED_LOCATION "${LOGOS_LIBLOGOS_ROOT}/lib/liblogos_core.so"
    )

    find_library(LOGOS_SDK_LIB logos_sdk PATHS "${LOGOS_CPP_SDK_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
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

# ── OpenSSL (for AES-256-GCM encryption) ─────────────────────────────────────
find_package(OpenSSL REQUIRED)

# ── Plugin target ─────────────────────────────────────────────────────────────
qt_add_plugin(kv_module_plugin CLASS_NAME KvPlugin)
set_target_properties(kv_module_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "kv_module_plugin"
)

target_sources(kv_module_plugin PRIVATE
    src/kv_plugin.cpp
    src/plugin.cpp
    src/backends/MemoryBackend.cpp
    src/backends/FileBackend.cpp
)

if(WITH_ROCKSDB AND (RocksDB_FOUND OR ROCKSDB_FOUND))
    target_sources(kv_module_plugin PRIVATE src/backends/RocksDbBackend.cpp)
endif()

if(WITH_SQLITE)
    target_sources(kv_module_plugin PRIVATE src/backends/SqliteBackend.cpp)
endif()

set_property(TARGET kv_module_plugin PROPERTY PUBLIC_HEADER
    src/i_kv_module.h
)

target_include_directories(kv_module_plugin PRIVATE src)

target_link_libraries(kv_module_plugin PRIVATE
    Qt6::Core
    Qt6::Qml
    OpenSSL::Crypto
)

if(LOGOS_CORE_AVAILABLE)
    find_package(Qt6 REQUIRED COMPONENTS RemoteObjects)
    target_link_libraries(kv_module_plugin PRIVATE
        Qt6::RemoteObjects
        logos_cpp_sdk
        logos_core
        ${LOGOS_SDK_LIB}
    )
    target_compile_definitions(kv_module_plugin PRIVATE LOGOS_CORE_AVAILABLE)
endif()

if(WITH_ROCKSDB)
    if(RocksDB_FOUND)
        target_link_libraries(kv_module_plugin PRIVATE RocksDB::rocksdb)
    elseif(ROCKSDB_FOUND)
        target_link_libraries(kv_module_plugin PRIVATE PkgConfig::ROCKSDB)
    endif()
    target_compile_definitions(kv_module_plugin PRIVATE HAVE_ROCKSDB)
endif()

if(WITH_SQLITE)
    target_link_libraries(kv_module_plugin PRIVATE SQLite::SQLite3)
    target_compile_definitions(kv_module_plugin PRIVATE HAVE_SQLITE)
endif()

# ── Compile definitions ──────────────────────────────────────────────────────
target_compile_definitions(kv_module_plugin PRIVATE
    QT_PLUGIN
)

# ── RPATH ─────────────────────────────────────────────────────────────────────
if(APPLE)
    set_target_properties(kv_module_plugin PROPERTIES
        BUILD_RPATH   "@loader_path"
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    set_target_properties(kv_module_plugin PROPERTIES
        BUILD_RPATH   "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS kv_module_plugin
    LIBRARY       DESTINATION lib/kv_module
    PUBLIC_HEADER DESTINATION include
)

# Install manifest.json (metadata.json renamed for logoscore compatibility)
install(FILES ${CMAKE_SOURCE_DIR}/metadata.json
    DESTINATION lib/kv_module
    RENAME manifest.json
)

# ── Tests ─────────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build conformance tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
