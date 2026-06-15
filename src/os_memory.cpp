#include <mem/os_memory.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace hft::os {

namespace detail {

[[noreturn]] inline void map_abort() noexcept { std::abort(); }

Region map_impl(std::size_t bytes, std::size_t alignment, MapFlags flags,
                bool shared) noexcept;

} // namespace detail

void Region::release() noexcept {
  if (base_ == nullptr) {
    return;
  }

#if defined(_WIN32)
  VirtualFree(base_, 0, MEM_RELEASE);
#else
  munmap(base_, mapped_bytes_);
#endif

  base_ = nullptr;
  mapped_bytes_ = 0;
}

Region map(std::size_t bytes, std::size_t alignment, MapFlags flags) noexcept {
  return detail::map_impl(bytes, alignment, flags, false);
}

Region map_shared(std::size_t bytes, std::size_t alignment,
                    MapFlags flags) noexcept {
  return detail::map_impl(bytes, alignment, flags, true);
}

namespace detail {

Region map_impl(const std::size_t bytes, const std::size_t alignment,
                const MapFlags flags, const bool shared) noexcept {
  if (bytes == 0) {
    map_abort();
  }
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    map_abort();
  }

  const std::size_t aligned_bytes = arch::round_up(bytes, alignment);
  const std::size_t mapped_bytes =
      arch::round_up(aligned_bytes, arch::page_size);

#if defined(_WIN32)
  void* base = VirtualAlloc(nullptr, mapped_bytes, MEM_COMMIT | MEM_RESERVE,
                            PAGE_READWRITE);
  if (base == nullptr) {
    map_abort();
  }
#else
  int map_flags = (shared ? MAP_SHARED : MAP_PRIVATE) | MAP_ANONYMOUS;
#if defined(MAP_HUGETLB)
  if (has_flag(flags, MapFlags::huge_page_hint)) {
    map_flags |= MAP_HUGETLB;
  }
#endif

  void* base = mmap(nullptr, mapped_bytes, PROT_READ | PROT_WRITE, map_flags, -1, 0);
  if (base == MAP_FAILED) {
    if (has_flag(flags, MapFlags::huge_page_hint)) {
      base = mmap(nullptr, mapped_bytes, PROT_READ | PROT_WRITE,
                  (shared ? MAP_SHARED : MAP_PRIVATE) | MAP_ANONYMOUS, -1, 0);
    }
    if (base == MAP_FAILED) {
      map_abort();
    }
  }

#if defined(MADV_HUGEPAGE)
  if (has_flag(flags, MapFlags::huge_page_hint)) {
    madvise(base, mapped_bytes, MADV_HUGEPAGE);
  }
#endif
#if defined(MADV_DONTDUMP)
  madvise(base, mapped_bytes, MADV_DONTDUMP);
#endif

  if (has_flag(flags, MapFlags::locked)) {
    if (mlock(base, mapped_bytes) != 0) {
      munmap(base, mapped_bytes);
      map_abort();
    }
  }
#endif

  return Region(base, mapped_bytes);
}

} // namespace detail

} // namespace hft::os
