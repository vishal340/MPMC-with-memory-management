#pragma once

#include <cstddef>
#include <cstdint>

namespace hft::arch {

#if defined(__x86_64__) || defined(_M_X64)
inline constexpr bool is_x86_64 = true;
inline constexpr bool is_arm64 = false;
inline constexpr const char* name = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
inline constexpr bool is_x86_64 = false;
inline constexpr bool is_arm64 = true;
inline constexpr const char* name = "arm64";
#else
#error "hft: unsupported target (expected x86_64 or aarch64/arm64)"
#endif

inline constexpr std::size_t cache_line_size = 64;

#if defined(__aarch64__) || defined(_M_ARM64)
#if defined(__APPLE__) || defined(__linux__)
inline constexpr std::size_t page_size = 16384;
#else
inline constexpr std::size_t page_size = 4096;
#endif
#else
inline constexpr std::size_t page_size = 4096;
#endif

inline constexpr std::size_t huge_page_size = is_arm64 ? 2097152 : 2097152;

inline constexpr std::size_t round_up(std::size_t value, std::size_t align) {
  return (value + align - 1) & ~(align - 1);
}

} // namespace hft::arch
