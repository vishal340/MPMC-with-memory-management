#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <thread>

#include <frame.hpp>
#include <frame_ingress.hpp>
#include <lz4_codec.hpp>
#include <memory_pool.hpp>
#include <mpmc.hpp>
#include <shared_inlet.hpp>
#include <utils.hpp>

namespace hft::feed {

template <std::size_t ItchCap, std::size_t OuchCap, std::size_t MtbtCap,
          std::size_t NnfCap, int QueueCap>
class Reader {
public:
  Reader() = default;

  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  ~Reader() { stop(); }

  void start(SharedInlet* inlet,
             hft::mem::ProtocolArena<ItchCap, OuchCap, MtbtCap, NnfCap>& arena,
             hft::MPMC<QueueCap>& queue) {
    stop();
    inlet_ = inlet;
    arena_ = &arena;
    queue_ = &queue;
    last_sequence_ = inlet_->header.sequence.load(std::memory_order_acquire);
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
          inlet_->header.sequence.load(std::memory_order_acquire);
      if (sequence == last_sequence_) {
        cpu_pause();
        continue;
      }

      if (inlet_->header.magic != SharedInletHeader::kMagic) {
        last_sequence_ = sequence;
        continue;
      }

      const auto kind = inlet_->header.kind;
      const auto flags = inlet_->header.flags;
      const std::size_t payload_len = inlet_->header.payload_len;
      if (payload_len == 0 || payload_len > inlet_->payload.size()) {
        last_sequence_ = sequence;
        continue;
      }

      const std::byte* frame_data = inlet_->payload.data();
      std::size_t frame_len = payload_len;

      if (kind == Kind::mtbt && has_flag(flags, InletFlags::lz4_compressed)) {
        std::size_t decoded_len = 0;
        const std::size_t expected = inlet_->header.uncompressed_len;
        if (expected == 0 || expected > scratch.size()) {
          last_sequence_ = sequence;
          continue;
        }
        if (!hft::lz4::decompress(frame_data, frame_len, scratch.data(),
                                  scratch.size(), decoded_len) ||
            decoded_len != expected) {
          last_sequence_ = sequence;
          continue;
        }
        frame_data = scratch.data();
        frame_len = decoded_len;
      }

      if (kind == Kind::itch) {
        (void)hft::ingress::ingest_itch_frame(frame_data, frame_len, *arena_,
                                              *queue_);
      } else if (kind == Kind::mtbt) {
        (void)hft::ingress::ingest_mtbt_frame(frame_data, frame_len, *arena_,
                                              *queue_);
      }

      last_sequence_ = sequence;
      inlet_->header.processed.store(sequence, std::memory_order_release);
    }
  }

  SharedInlet* inlet_{nullptr};
  hft::mem::ProtocolArena<ItchCap, OuchCap, MtbtCap, NnfCap>* arena_{nullptr};
  hft::MPMC<QueueCap>* queue_{nullptr};
  std::uint64_t last_sequence_{0};
  std::atomic<bool> running_{false};
  std::thread thread_;
};

} // namespace hft::feed
