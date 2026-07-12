#pragma once

#include <feed/frame.hpp>
#include <proto/itch_decode.hpp>
#include <mem/memory_pool.hpp>
#include <mpmc/mpmc.hpp>
#include <proto/mtbt_decode.hpp>
#include <proto/mcx/order.hpp>
#include <proto/protocols.hpp>

#include <cstring>

namespace hft::ingress {

template <std::size_t ItchCap, std::size_t OuchCap, std::size_t MtbtCap,
          std::size_t NnfCap, int QueueCap>
[[nodiscard]] inline frame::Stats ingest_itch_frame(
    const std::byte *frame, std::size_t len,
    hft::mem::ProtocolArena<ItchCap, OuchCap, MtbtCap, NnfCap> &arena,
    hft::MPMC<QueueCap> &queue) noexcept {
  return hft::proto::split_itch_frame(
      frame, len,
      [&](const std::byte *body, std::size_t body_len) noexcept -> bool {
        hft::proto::ItchAddOrder decoded{};
        if (!hft::proto::decode_itch_add_order(body, body_len, decoded)) {
          return false;
        }

        auto *slot = arena.acquire_itch();
        if (slot == nullptr) {
          return false;
        }
        *slot = decoded;

        hft::proto::TaggedMessage tagged{};
        arena.copy_to_tagged(*slot, tagged);
        arena.release(slot);

        queue.push(tagged);
        return true;
      });
}

template <std::size_t ItchCap, std::size_t OuchCap, std::size_t MtbtCap,
          std::size_t NnfCap, int QueueCap>
[[nodiscard]] inline frame::Stats ingest_mtbt_frame(
    const std::byte *frame, std::size_t len,
    hft::mem::ProtocolArena<ItchCap, OuchCap, MtbtCap, NnfCap> &arena,
    hft::MPMC<QueueCap> &queue) noexcept {
  return hft::proto::split_mtbt_frame(
      frame, len,
      [&](const std::byte *packet, std::size_t packet_len,
          const hft::proto::MtbtStreamHeader &header) noexcept -> bool {
        (void)header;

        hft::proto::MtbtDecodedOrder decoded{};
        if (!hft::proto::decode_mtbt_order(packet, packet_len, decoded)) {
          return false;
        }

        auto *slot = arena.acquire_mtbt();
        if (slot == nullptr) {
          return false;
        }
        *slot = decoded.order;

        hft::proto::TaggedMessage tagged{};
        arena.copy_to_tagged(*slot, tagged);
        arena.release(slot);

        queue.push(tagged);
        return true;
      });
}

template <int QueueCap>
[[nodiscard]] inline frame::Stats ingest_mcx_tob_frame(
    const std::byte *frame, std::size_t len,
    hft::MPMC<QueueCap> &queue) noexcept {
  frame::Stats stats{};
  if (frame == nullptr || len != hft::proto::mcx::TopOfBook::kWireSize) {
    return stats;
  }

  hft::proto::mcx::TopOfBook tick{};
  std::memcpy(&tick, frame, sizeof(tick));

  hft::proto::TaggedMessage tagged{};
  tagged.kind = hft::proto::Kind::mcx_market;
  std::memcpy(tagged.bytes.data(), &tick, sizeof(tick));
  queue.push(tagged);

  stats.saved = 1;
  stats.consumed = len;
  return stats;
}

} // namespace hft::ingress
