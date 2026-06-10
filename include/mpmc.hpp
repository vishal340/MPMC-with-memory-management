#pragma once

#include <arch.hpp>
#include <data.hpp>
#include <os_memory.hpp>
#include <utils.hpp>

#include <atomic>
#include <cstddef>
#include <new>

namespace hft {

template <int Capacity>
  requires((Capacity & (Capacity - 1)) == 0)
class MPMC {
  static constexpr std::size_t kAlignment =
      (sizeof(Node) >= arch::cache_line_size) ? alignof(Node)
                                              : arch::cache_line_size;

public:
  MPMC() {
    const std::size_t bytes = static_cast<std::size_t>(Capacity) * sizeof(Node);
    region_ = os::map(bytes, kAlignment);
    ring_ = static_cast<Node*>(region_.data());

    for (int i = 0; i < Capacity; ++i) {
      (void)os::placement_construct<Node>(&ring_[i]);
      ring_[i].state.store(0, std::memory_order_relaxed);
    }
  }

  MPMC(const MPMC&) = delete;
  MPMC& operator=(const MPMC&) = delete;

  void push(int index, const proto::TaggedMessage& message) {
    int slot = index;
    std::uint8_t expected = 0;
    while (true) {
      if (ring_[slot].state.compare_exchange_weak(
              expected, 2, std::memory_order_acquire,
              std::memory_order_relaxed)) {
        ring_[slot].message = message;
        ring_[slot].state.store(1, std::memory_order_release);
        return;
      }
      if (expected == 2) {
        slot = (slot + 1) & (Capacity - 1);
        expected = 0;
      } else {
        cpu_pause();
        expected = 0;
      }
    }
  }

  [[nodiscard]] proto::TaggedMessage pop(int index) {
    int slot = index;
    std::uint8_t expected = 1;
    while (true) {
      if (ring_[slot].state.compare_exchange_weak(
              expected, 3, std::memory_order_acquire,
              std::memory_order_relaxed)) {
        proto::TaggedMessage ret = ring_[slot].message;
        ring_[slot].state.store(0, std::memory_order_release);
        return ret;
      }
      if (expected == 3) {
        slot = (slot + 1) & (Capacity - 1);
        expected = 1;
      } else {
        cpu_pause();
        expected = 1;
      }
    }
  }

  [[nodiscard]] const os::Region& region() const noexcept { return region_; }

private:
  os::Region region_{};
  Node* ring_{nullptr};
};

} // namespace hft
