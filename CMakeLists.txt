cmake_minimum_required(VERSION 3.15)
project(MPMC_Memory_Management VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compile_commands.json generation for Clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MPMC_TARGET mpmc_memory_management)

add_executable(${MPMC_TARGET}
    src/main.cpp
    src/lz4_codec.cpp
    src/os_memory.cpp
    src/udp_multicast.cpp
)

target_include_directories(${MPMC_TARGET} PRIVATE include)

target_compile_options(${MPMC_TARGET} PRIVATE -fno-exceptions)

find_path(LZ4_INCLUDE_DIR NAMES lz4.h)
find_library(LZ4_LIBRARY NAMES lz4)
if(NOT LZ4_INCLUDE_DIR OR NOT LZ4_LIBRARY)
    message(FATAL_ERROR "LZ4 not found; install lz4 (e.g. brew install lz4)")
endif()
target_include_directories(${MPMC_TARGET} PRIVATE ${LZ4_INCLUDE_DIR})
target_link_libraries(${MPMC_TARGET} PRIVATE ${LZ4_LIBRARY})

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(${MPMC_TARGET} PRIVATE HFT_TARGET_ARM64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(${MPMC_TARGET} PRIVATE HFT_TARGET_X86_64=1)
endif()

# Automatically symlink compile_commands.json to the project root for Clangd
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/compile_commands.json")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
    )
endif()
