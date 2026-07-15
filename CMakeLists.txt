cmake_minimum_required(VERSION 3.16)
project(kv_module VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Logos Module Builder ──────────────────────────────────────────────────────
find_package(LogosModule REQUIRED)

logos_module(kv_module
    IMPL_HEADER src/kv_module_impl.h
    IMPL_SOURCE src/kv_module_impl.cpp
    SOURCES
        src/backends/KvBackend.h
        src/backends/MemoryBackend.h
        src/backends/MemoryBackend.cpp
        src/backends/FileBackend.h
        src/backends/FileBackend.cpp
)
