cmake_minimum_required(VERSION 3.14)
project(kv_module LANGUAGES CXX)

# Include the Logos Module CMake helper (provided by logos-module-builder)
if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LogosModule.cmake")
    include(cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found. Set LOGOS_MODULE_BUILDER_ROOT or ensure logos-module-builder is in your flake inputs.")
endif()

logos_module(
    NAME kv_module
    SOURCES
        src/kv_module_impl.h
        src/kv_module_impl.cpp
        src/backends/KvBackend.h
        src/backends/MemoryBackend.h
        src/backends/MemoryBackend.cpp
        src/backends/FileBackend.h
        src/backends/FileBackend.cpp
)
