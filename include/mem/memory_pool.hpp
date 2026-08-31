#pragma once

#include <core/arch.hpp>
#include <mem/os_memory.hpp>
#include <proto/protocols.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hft::mem {

namespace detail {

inline std::size_t index_of(const void *slot, const void *base,
                            std::size_t slot_stride) noexcept {
  const auto base_addr = reinterpret_cast<std::uintptr_t>(base);
  const auto slot_addr = reinterpret_cast<std::uintptr_t>(slot);
  return (slot_addr - base_addr) / slot_stride;
}

} // namespace detail

// Pool of TaggedMessage bodies. Acquire/release synchronizes only the slot
// state byte; message payloads are plain memory owned by the holder of the
// pointer.
template <std::size_t Capacity>
  requires(Capacity > 0)
class TaggedPool {
  struct alignas(arch::cache_line_size) Slot {
    proto::TaggedMessage message{};
    std::atomic<std::uint8_t> state{0}; // 0 = free, 1 = acquired
  };

public:
  TaggedPool() {
    constexpr std::size_t bytes = Capacity * sizeof(Slot);
    region_ = os::map(bytes, alignof(Slot));
    slots_ = static_cast<Slot *>(region_.data());

    for (std::size_t i = 0; i < Capacity; ++i) {
      slots_[i].state.store(0, std::memory_order_relaxed);
    }
  }

  TaggedPool(const TaggedPool &) = delete;
  TaggedPool &operator=(const TaggedPool &) = delete;

  [[nodiscard]] proto::TaggedMessage *acquire() noexcept {
    for (std::size_t i = 0; i < Capacity; ++i) {
      std::uint8_t expected = 0;
      if (slots_[i].state.compare_exchange_weak(expected, 1,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
        slots_[i].message.kind = proto::Kind::itch;
        std::memset(slots_[i].message.bytes.data(), 0,
                    slots_[i].message.bytes.size());
        return &slots_[i].message;
      }
    }
    return nullptr;
  }

  void release(proto::TaggedMessage *message) noexcept {
    if (message == nullptr) {
      return;
    }

    const std::size_t index =
        detail::index_of(message, &slots_[0].message, sizeof(Slot));
    if (index >= Capacity) {
      return;
    }

    slots_[index].state.store(0, std::memory_order_release);
  }

  template <proto::WireMessage T>
  [[nodiscard]] proto::TaggedMessage *acquire_filled(const T &src) noexcept {
    proto::TaggedMessage *slot = acquire();
    if (slot == nullptr) {
      return nullptr;
    }
    slot->kind = proto::kind_of<T>;
    std::memcpy(slot->bytes.data(), &src, sizeof(src));
    return slot;
  }

  [[nodiscard]] std::size_t capacity() const noexcept { return Capacity; }
  [[nodiscard]] const os::Region &region() const noexcept { return region_; }

private:
  os::Region region_{};
  Slot *slots_{nullptr};
};

} // namespace hft::mem
