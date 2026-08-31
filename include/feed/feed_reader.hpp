#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <thread>

#include <feed/frame.hpp>
#include <feed/frame_ingress.hpp>
#include <codec/lz4_codec.hpp>
#include <mem/memory_pool.hpp>
#include <mpmc/mpmc.hpp>
#include <feed/shared_inlet.hpp>
#include <core/utils.hpp>

namespace hft::feed {

template <std::size_t PoolCap, int QueueCap>
class Reader {
public:
  Reader() = default;

  Reader(const Reader &) = delete;
  Reader &operator=(const Reader &) = delete;

  ~Reader() { stop(); }

  void start(SharedInlet *inlet, hft::mem::TaggedPool<PoolCap> &pool,
             hft::MPMC<QueueCap> &queue) {
    stop();
    inlet_ = inlet;
    pool_ = &pool;
    queue_ = &queue;
    last_sequence_ = inlet_->sequence.load(std::memory_order_acquire);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { run(); });
  }

  void stop() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  [[nodiscard]] std::uint64_t last_sequence() const noexcept {
    return last_sequence_;
  }

private:
  void run() {
    std::array<std::byte, hft::frame::kCapacity> scratch{};

    while (running_.load(std::memory_order_acquire)) {
      const std::uint64_t sequence =
          inlet_->sequence.load(std::memory_order_acquire);
      if (sequence == last_sequence_) {
        cpu_pause();
        continue;
      }

      const FrameSlot *slot = published_slot(*inlet_);
      if (slot->magic != FrameSlot::kMagic) {
        last_sequence_ = sequence;
        inlet_->processed.store(sequence, std::memory_order_release);
        continue;
      }

      const auto kind = slot->kind;
      const auto flags = slot->flags;
      const std::size_t payload_len = slot->payload_len;
      if (payload_len == 0 || payload_len > slot->payload.size()) {
        last_sequence_ = sequence;
        inlet_->processed.store(sequence, std::memory_order_release);
        continue;
      }

      const std::byte *frame_data = slot->payload.data();
      std::size_t frame_len = payload_len;

      if (kind == Kind::mtbt && has_flag(flags, InletFlags::lz4_compressed)) {
        std::size_t decoded_len = 0;
        const std::size_t expected = slot->uncompressed_len;
        if (expected == 0 || expected > scratch.size()) {
          last_sequence_ = sequence;
          inlet_->processed.store(sequence, std::memory_order_release);
          continue;
        }
        if (!hft::lz4::decompress(frame_data, frame_len, scratch.data(),
                                  scratch.size(), decoded_len) ||
            decoded_len != expected) {
          last_sequence_ = sequence;
          inlet_->processed.store(sequence, std::memory_order_release);
          continue;
        }
        frame_data = scratch.data();
        frame_len = decoded_len;
      }

      if (kind == Kind::itch) {
        (void)hft::ingress::ingest_itch_frame(frame_data, frame_len, *pool_,
                                              *queue_);
      } else if (kind == Kind::mtbt) {
        (void)hft::ingress::ingest_mtbt_frame(frame_data, frame_len, *pool_,
                                              *queue_);
      } else if (kind == Kind::mcx) {
        (void)hft::ingress::ingest_mcx_tob_frame(frame_data, frame_len, *pool_,
                                                 *queue_);
      }

      last_sequence_ = sequence;
      inlet_->processed.store(sequence, std::memory_order_release);
    }
  }

  SharedInlet *inlet_{nullptr};
  hft::mem::TaggedPool<PoolCap> *pool_{nullptr};
  hft::MPMC<QueueCap> *queue_{nullptr};
  std::uint64_t last_sequence_{0};
  std::atomic<bool> running_{false};
  std::thread thread_;
};

} // namespace hft::feed
