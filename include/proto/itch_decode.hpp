#pragma once

#include <feed/frame.hpp>
#include <proto/protocols.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hft::proto {

inline constexpr std::size_t kItchLengthPrefix = 2;

[[nodiscard]] inline std::uint16_t itch_read_be16(const std::byte* data) noexcept {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(data[0]) << 8) |
      static_cast<std::uint16_t>(data[1]));
}

[[nodiscard]] inline bool decode_itch_add_order(const void* body, std::size_t len,
                                                ItchAddOrder& out) noexcept {
  if (body == nullptr || len != ItchAddOrder::kWireSize) {
    return false;
  }
  std::memcpy(&out, body, ItchAddOrder::kWireSize);
  return out.message_type == ItchAddOrder::kType;
}

// ITCH frame: [BE16 length][body] repeated. Length is body size only.
template <typename Handler>
[[nodiscard]] inline frame::Stats split_itch_frame(const std::byte* frame,
                                                   std::size_t len,
                                                   Handler&& on_message) noexcept {
  frame::Stats stats{};
  if (frame == nullptr) {
    return stats;
  }

  std::size_t offset = 0;
  while (offset + kItchLengthPrefix <= len) {
    const std::uint16_t body_len =
        itch_read_be16(frame + offset);
    offset += kItchLengthPrefix;

    if (body_len == 0 || offset + body_len > len) {
      break;
    }

    const std::byte* body = frame + offset;
    if (!on_message(body, body_len)) {
      ++stats.skipped;
    } else {
      ++stats.saved;
    }

    offset += body_len;
  }

  stats.consumed = offset;
  return stats;
}

} // namespace hft::proto
