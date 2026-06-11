cmake_minimum_required(VERSION 3.15)
project(MPMC_Memory_Management VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compile_commands.json generation for Clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MPMC_TARGET mpmc_memory_management)

add_executable(${MPMC_TARGET}
    src/main.cpp
    src/lzo.cpp
    src/os_memory.cpp
)

target_include_directories(${MPMC_TARGET} PRIVATE include)

find_path(LZO2_INCLUDE_DIR NAMES lzo/lzo1x.h)
find_library(LZO2_LIBRARY NAMES lzo2)
if(NOT LZO2_INCLUDE_DIR OR NOT LZO2_LIBRARY)
    message(FATAL_ERROR "LZO2 not found; install lzo (e.g. brew install lzo)")
endif()
target_include_directories(${MPMC_TARGET} PRIVATE ${LZO2_INCLUDE_DIR})
target_link_libraries(${MPMC_TARGET} PRIVATE ${LZO2_LIBRARY})

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
