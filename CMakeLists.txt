cmake_minimum_required(VERSION 3.16)
project(kv_module VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

include(LogosModule)

logos_module(
    NAME kv_module
    SOURCES
        src/kv_plugin.cpp
        src/backends/MemoryBackend.cpp
        src/backends/FileBackend.cpp
    HEADERS
        src/kv_interface.h
        src/kv_plugin.h
        src/backends/KvBackend.h
        src/backends/MemoryBackend.h
        src/backends/FileBackend.h
)

option(BUILD_TESTS "Build conformance tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
