#pragma once

#include <proto/sbe/common.hpp>
#include <proto/sbe/fast_order.hpp>
#include <proto/sbe/market.hpp>
#include <proto/sbe/order_entry.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hft::proto::sbe {

[[nodiscard]] inline bool read_header(const std::byte *data, std::size_t len,
                                      MessageHeader &out) noexcept {
  if (data == nullptr || len < MessageHeader::kWireSize) {
    return false;
  }
  std::memcpy(&out, data, MessageHeader::kWireSize);
  return true;
}

[[nodiscard]] inline bool read_var_string8(const std::byte *data,
                                             std::size_t len, std::size_t &offset,
                                             char *out, std::size_t out_cap,
                                             std::uint8_t &out_len) noexcept {
  if (offset >= len) {
    return false;
  }
  const std::uint8_t str_len =
      static_cast<std::uint8_t>(data[offset]);
  ++offset;
  if (offset + str_len > len || str_len >= out_cap) {
    return false;
  }
  out_len = str_len;
  if (str_len > 0) {
    std::memcpy(out, data + offset, str_len);
  }
  out[str_len] = '\0';
  offset += str_len;
  return true;
}

[[nodiscard]] inline bool
decode_best_ob_rpi(const void *payload, std::size_t len,
                   BestObRpi &out) noexcept {
  if (payload == nullptr ||
      len < MessageHeader::kWireSize + BestObRpiBody::kFixedWireSize + 1) {
    return false;
  }

  const auto *bytes = static_cast<const std::byte *>(payload);
  MessageHeader header{};
  if (!read_header(bytes, len, header)) {
    return false;
  }
  if (header.template_id != BestObRpi::kTemplateId ||
      header.schema_id != kMarketSchemaId ||
      header.block_length < BestObRpiBody::kFixedWireSize) {
    return false;
  }

  std::size_t offset = MessageHeader::kWireSize;
  std::memcpy(&out.body, bytes + offset, BestObRpiBody::kFixedWireSize);
  offset += BestObRpiBody::kFixedWireSize;

  return read_var_string8(bytes, len, offset, out.symbol, sizeof(out.symbol),
                          out.symbol_len);
}

[[nodiscard]] inline bool
decode_create_order_req(const void *payload, std::size_t len,
                        CreateOrderReqV5 &out) noexcept {
  if (payload == nullptr ||
      len < MessageHeader::kWireSize + CreateOrderReqV5::kWireSize) {
    return false;
  }

  const auto *bytes = static_cast<const std::byte *>(payload);
  MessageHeader header{};
  if (!read_header(bytes, len, header)) {
    return false;
  }
  if (header.template_id != CreateOrderReqV5::kTemplateId ||
      header.schema_id != kOrderEntrySchemaId ||
      header.block_length < CreateOrderReqV5::kWireSize) {
    return false;
  }

  std::memcpy(&out, bytes + MessageHeader::kWireSize,
              CreateOrderReqV5::kWireSize);
  return true;
}

[[nodiscard]] inline bool
decode_fast_order_resp(const void *payload, std::size_t len,
                       FastOrderResp &out) noexcept {
  if (payload == nullptr ||
      len < MessageHeader::kWireSize + FastOrderRespBody::kFixedWireSize + 1) {
    return false;
  }

  const auto *bytes = static_cast<const std::byte *>(payload);
  MessageHeader header{};
  if (!read_header(bytes, len, header)) {
    return false;
  }
  if (header.template_id != FastOrderResp::kTemplateId ||
      header.schema_id != kFastOrderSchemaId ||
      header.block_length < FastOrderRespBody::kFixedWireSize) {
    return false;
  }

  std::size_t offset = MessageHeader::kWireSize;
  std::memcpy(&out.body, bytes + offset, FastOrderRespBody::kFixedWireSize);
  offset += FastOrderRespBody::kFixedWireSize;

  if (!read_var_string8(bytes, len, offset, out.order_id, sizeof(out.order_id),
                        out.order_id_len)) {
    return false;
  }
  return read_var_string8(bytes, len, offset, out.order_link_id,
                          sizeof(out.order_link_id), out.order_link_id_len);
}

inline void encode_header(std::byte *dst, std::uint16_t block_length,
                          std::uint16_t template_id, std::uint16_t schema_id,
                          std::uint16_t version) noexcept {
  MessageHeader header{};
  header.block_length = block_length;
  header.template_id = template_id;
  header.schema_id = schema_id;
  header.version = version;
  std::memcpy(dst, &header, sizeof(header));
}

inline std::size_t
append_var_string8(std::byte *dst, std::size_t cap, std::size_t offset,
                   const char *str, std::uint8_t str_len) noexcept {
  if (offset + 1 + str_len > cap) {
    return offset;
  }
  dst[offset++] = static_cast<std::byte>(str_len);
  if (str_len > 0) {
    std::memcpy(dst + offset, str, str_len);
    offset += str_len;
  }
  return offset;
}

[[nodiscard]] inline std::size_t
encode_best_ob_rpi(const BestObRpi &msg, std::byte *dst,
                   std::size_t cap) noexcept {
  const std::size_t total =
      MessageHeader::kWireSize + BestObRpiBody::kFixedWireSize + 1 +
      msg.symbol_len;
  if (cap < total) {
    return 0;
  }

  encode_header(dst, static_cast<std::uint16_t>(BestObRpiBody::kFixedWireSize),
                BestObRpi::kTemplateId, kMarketSchemaId, kMarketSchemaVersion);
  std::size_t offset = MessageHeader::kWireSize;
  std::memcpy(dst + offset, &msg.body, BestObRpiBody::kFixedWireSize);
  offset += BestObRpiBody::kFixedWireSize;
  return append_var_string8(dst, cap, offset, msg.symbol, msg.symbol_len);
}

[[nodiscard]] inline std::size_t
encode_create_order_req(const CreateOrderReqV5 &msg, std::byte *dst,
                        std::size_t cap) noexcept {
  const std::size_t total =
      MessageHeader::kWireSize + CreateOrderReqV5::kWireSize;
  if (cap < total) {
    return 0;
  }

  encode_header(dst, static_cast<std::uint16_t>(CreateOrderReqV5::kWireSize),
                CreateOrderReqV5::kTemplateId, kOrderEntrySchemaId,
                kOrderEntrySchemaVersion);
  std::memcpy(dst + MessageHeader::kWireSize, &msg, CreateOrderReqV5::kWireSize);
  return total;
}

[[nodiscard]] inline std::size_t
encode_fast_order_resp(const FastOrderResp &msg, std::byte *dst,
                       std::size_t cap) noexcept {
  const std::size_t total = MessageHeader::kWireSize +
                            FastOrderRespBody::kFixedWireSize + 2 +
                            msg.order_id_len + msg.order_link_id_len;
  if (cap < total) {
    return 0;
  }

  encode_header(dst,
                static_cast<std::uint16_t>(FastOrderRespBody::kFixedWireSize),
                FastOrderResp::kTemplateId, kFastOrderSchemaId,
                kFastOrderSchemaVersion);
  std::size_t offset = MessageHeader::kWireSize;
  std::memcpy(dst + offset, &msg.body, FastOrderRespBody::kFixedWireSize);
  offset += FastOrderRespBody::kFixedWireSize;
  offset = append_var_string8(dst, cap, offset, msg.order_id, msg.order_id_len);
  return append_var_string8(dst, cap, offset, msg.order_link_id,
                            msg.order_link_id_len);
}

// SBE market frame: [messageHeader][body][var tail] per WebSocket binary frame.
template <typename Handler>
[[nodiscard]] inline bool split_sbe_frame(const std::byte *frame,
                                            std::size_t len,
                                            Handler &&on_message) noexcept {
  if (frame == nullptr || len < MessageHeader::kWireSize) {
    return false;
  }

  MessageHeader header{};
  if (!read_header(frame, len, header)) {
    return false;
  }

  const std::size_t body_len =
      MessageHeader::kWireSize + header.block_length;
  if (body_len > len) {
    return false;
  }

  return on_message(frame, len, header);
}

} // namespace hft::proto::sbe
