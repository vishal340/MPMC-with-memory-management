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
  static constexpr int kMask = Capacity - 1;
  static constexpr std::size_t kAlignment =
      (sizeof(Node) >= arch::cache_line_size) ? alignof(Node)
                                              : arch::cache_line_size;

public:
  MPMC() {
    const std::size_t bytes = static_cast<std::size_t>(Capacity) * sizeof(Node);
    region_ = os::map(bytes, kAlignment);
    ring_ = static_cast<Node *>(region_.data());

    for (int i = 0; i < Capacity; ++i) {
      (void)os::placement_construct<Node>(&ring_[i]);
      ring_[i].state.store(0, std::memory_order_relaxed);
    }
  }

  MPMC(const MPMC &) = delete;
  MPMC &operator=(const MPMC &) = delete;

  void push(const proto::TaggedMessage &message) {
    int slot = post_cursor_.fetch_add(1, std::memory_order_relaxed) & kMask;
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
        slot = (slot + 1) & kMask;
        expected = 0;
      } else if (expected == 1) {
        slot = (slot + 1) & kMask;
        expected = 0;
      } else {
        cpu_pause();
        expected = 0;
      }
    }
  }

  [[nodiscard]] proto::TaggedMessage pop() {
    int slot = pop_cursor_.fetch_add(1, std::memory_order_relaxed) & kMask;
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
        slot = (slot + 1) & kMask;
        expected = 1;
      } else if (expected == 0) {
        slot = (slot + 1) & kMask;
        expected = 1;
      } else {
        cpu_pause();
        expected = 1;
      }
    }
  }

  [[nodiscard]] bool try_pop(proto::TaggedMessage &out) {
    int slot = pop_cursor_.load(std::memory_order_relaxed);
    for (int probe = 0; probe < Capacity; ++probe) {
      const int idx = (slot + probe) & kMask;
      std::uint8_t expected = 1;
      if (ring_[idx].state.compare_exchange_weak(
              expected, 3, std::memory_order_acquire,
              std::memory_order_relaxed)) {
        out = ring_[idx].message;
        ring_[idx].state.store(0, std::memory_order_release);
        pop_cursor_.store((idx + 1) & kMask, std::memory_order_relaxed);
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] const os::Region &region() const noexcept { return region_; }

private:
  os::Region region_{};
  Node *ring_{nullptr};
  alignas(arch::cache_line_size) std::atomic<int> post_cursor_{0};
  alignas(arch::cache_line_size) std::atomic<int> pop_cursor_{0};
};

} // namespace hft
