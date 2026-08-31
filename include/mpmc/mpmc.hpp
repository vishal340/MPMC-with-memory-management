#pragma once

#include <core/arch.hpp>
#include <mpmc/data.hpp>
#include <mem/os_memory.hpp>
#include <core/utils.hpp>

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
      ring_[i].message.store(nullptr, std::memory_order_relaxed);
    }
  }

  MPMC(const MPMC &) = delete;
  MPMC &operator=(const MPMC &) = delete;

  // Transfers ownership of message into the ring. message must be non-null.
  void push(proto::TaggedMessage *message) {
    if (message == nullptr) {
      return;
    }

    int slot = post_cursor_.fetch_add(1, std::memory_order_relaxed) & kMask;
    proto::TaggedMessage *expected = nullptr;
    while (true) {
      if (ring_[slot].message.compare_exchange_weak(
              expected, message, std::memory_order_release,
              std::memory_order_relaxed)) {
        return;
      }
      // Slot occupied — probe next.
      slot = (slot + 1) & kMask;
      expected = nullptr;
      cpu_pause();
    }
  }

  // Blocks until a message is available. Caller owns the returned pointer.
  [[nodiscard]] proto::TaggedMessage *pop() {
    int slot = pop_cursor_.fetch_add(1, std::memory_order_relaxed) & kMask;
    while (true) {
      proto::TaggedMessage *message =
          ring_[slot].message.exchange(nullptr, std::memory_order_acquire);
      if (message != nullptr) {
        return message;
      }
      slot = (slot + 1) & kMask;
      cpu_pause();
    }
  }

  // Non-blocking take. On success, caller owns *out.
  [[nodiscard]] bool try_pop(proto::TaggedMessage *&out) {
    int slot = pop_cursor_.load(std::memory_order_relaxed);
    for (int probe = 0; probe < Capacity; ++probe) {
      const int idx = (slot + probe) & kMask;
      proto::TaggedMessage *message =
          ring_[idx].message.exchange(nullptr, std::memory_order_acquire);
      if (message != nullptr) {
        out = message;
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
