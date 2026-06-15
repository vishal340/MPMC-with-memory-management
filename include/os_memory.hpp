#pragma once

#include <arch.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

namespace hft::os {

enum class MapFlags : std::uint32_t {
  none = 0,
  locked = 1 << 0,
  huge_page_hint = 1 << 1,
};

constexpr MapFlags operator|(MapFlags lhs, MapFlags rhs) noexcept {
  return static_cast<MapFlags>(static_cast<std::uint32_t>(lhs) |
                               static_cast<std::uint32_t>(rhs));
}

constexpr MapFlags operator&(MapFlags lhs, MapFlags rhs) noexcept {
  return static_cast<MapFlags>(static_cast<std::uint32_t>(lhs) &
                               static_cast<std::uint32_t>(rhs));
}

constexpr bool has_flag(MapFlags flags, MapFlags flag) noexcept {
  return (flags & flag) == flag;
}

class Region {
public:
  Region() noexcept = default;
  Region(void *base, std::size_t mapped_bytes) noexcept
      : base_(base), mapped_bytes_(mapped_bytes) {}

  Region(const Region &) = delete;
  Region &operator=(const Region &) = delete;

  Region(Region &&other) noexcept { *this = std::move(other); }
  Region &operator=(Region &&other) noexcept {
    if (this != &other) [[likely]] {
      release();
      base_ = other.base_;
      mapped_bytes_ = other.mapped_bytes_;
      other.base_ = nullptr;
      other.mapped_bytes_ = 0;
    }
    return *this;
  }

  ~Region() { release(); }

  [[nodiscard]] void *data() const noexcept { return base_; }
  [[nodiscard]] std::size_t size() const noexcept { return mapped_bytes_; }
  [[nodiscard]] explicit operator bool() const noexcept {
    return base_ != nullptr;
  }

  void release() noexcept;

private:
  void *base_{nullptr};
  std::size_t mapped_bytes_{0};
};

[[nodiscard]] Region map(std::size_t bytes, std::size_t alignment,
                         MapFlags flags = MapFlags::none) noexcept;

[[nodiscard]] Region map_shared(std::size_t bytes, std::size_t alignment,
                                MapFlags flags = MapFlags::none) noexcept;

template <typename T> [[nodiscard]] T *placement_construct(void *storage) {
  return new (storage) T{};
}

template <typename T> void placement_destroy(T *object) noexcept {
  object->~T();
}

} // namespace hft::os
