#pragma once

#include <protocols.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hft::proto {

inline constexpr std::size_t kMtbtOrderWireSize =
    MtbtStreamHeader::kWireSize + MtbtNewOrder::kWireSize;

struct MtbtDecodedOrder {
  MtbtStreamHeader header{};
  MtbtNewOrder order{};
};

[[nodiscard]] inline bool decode_mtbt_stream_header(const void* wire,
                                                  std::size_t len,
                                                  MtbtStreamHeader& out) noexcept {
  if (wire == nullptr || len < MtbtStreamHeader::kWireSize) {
    return false;
  }
  std::memcpy(&out, wire, MtbtStreamHeader::kWireSize);
  return true;
}

[[nodiscard]] inline bool decode_mtbt_order_body(const void* body,
                                                 std::size_t len,
                                                 MtbtNewOrder& out) noexcept {
  if (body == nullptr || len < MtbtNewOrder::kWireSize) {
    return false;
  }
  std::memcpy(&out, body, MtbtNewOrder::kWireSize);
  return out.message_type == MtbtNewOrder::kTypeNew ||
         out.message_type == MtbtNewOrder::kTypeMod ||
         out.message_type == MtbtNewOrder::kTypeCancel;
}

// Wire layout: [STREAM_HEADER 8 B][order body 26 B]
[[nodiscard]] inline bool decode_mtbt_order(const void* wire, std::size_t len,
                                            MtbtDecodedOrder& out) noexcept {
  if (wire == nullptr || len < kMtbtOrderWireSize) {
    return false;
  }

  const auto* bytes = static_cast<const std::byte*>(wire);
  if (!decode_mtbt_stream_header(bytes, len, out.header)) {
    return false;
  }

  if (out.header.msg_len != static_cast<std::int16_t>(kMtbtOrderWireSize)) {
    return false;
  }

  return decode_mtbt_order_body(bytes + MtbtStreamHeader::kWireSize,
                                len - MtbtStreamHeader::kWireSize, out.order);
}

} // namespace hft::proto
