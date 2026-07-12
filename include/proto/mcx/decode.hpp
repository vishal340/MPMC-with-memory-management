#pragma once

#include <proto/mcx/confirm.hpp>
#include <proto/mcx/order.hpp>

#include <cstring>

namespace hft::proto::mcx {

[[nodiscard]] inline bool read_template_id(const void *data, std::size_t len,
                                             std::uint16_t &out) noexcept {
  if (data == nullptr || len < 6) {
    return false;
  }
  const auto *bytes = static_cast<const std::byte *>(data);
  std::uint16_t template_id{};
  std::memcpy(&template_id, bytes + 4, sizeof(template_id));
  out = template_id;
  return true;
}

[[nodiscard]] inline bool
decode_new_order_short(const void *payload, std::size_t len,
                       NewOrderSingleShort &out) noexcept {
  if (payload == nullptr || len < NewOrderSingleShort::kWireSize) {
    return false;
  }
  std::memcpy(&out, payload, NewOrderSingleShort::kWireSize);
  return out.template_id == NewOrderSingleShort::kTemplateId;
}

[[nodiscard]] inline bool
decode_execution_report(const void *payload, std::size_t len,
                        ExecutionReportNew &out) noexcept {
  if (payload == nullptr || len < ExecutionReportNew::kWireSize) {
    return false;
  }
  std::memcpy(&out, payload, ExecutionReportNew::kWireSize);
  return out.template_id == ExecutionReportNew::kTemplateId;
}

[[nodiscard]] inline std::size_t
encode_new_order_short(const NewOrderSingleShort &msg, std::byte *dst,
                       std::size_t cap) noexcept {
  if (cap < NewOrderSingleShort::kWireSize) {
    return 0;
  }
  NewOrderSingleShort wire = msg;
  wire.body_len = static_cast<std::uint32_t>(NewOrderSingleShort::kWireSize);
  wire.template_id = NewOrderSingleShort::kTemplateId;
  std::memcpy(dst, &wire, NewOrderSingleShort::kWireSize);
  return NewOrderSingleShort::kWireSize;
}

[[nodiscard]] inline std::size_t
encode_execution_report(const ExecutionReportNew &msg, std::byte *dst,
                        std::size_t cap) noexcept {
  if (cap < ExecutionReportNew::kWireSize) {
    return 0;
  }
  ExecutionReportNew wire = msg;
  wire.body_len = static_cast<std::uint32_t>(ExecutionReportNew::kWireSize);
  wire.template_id = ExecutionReportNew::kTemplateId;
  std::memcpy(dst, &wire, ExecutionReportNew::kWireSize);
  return ExecutionReportNew::kWireSize;
}

template <typename Handler>
[[nodiscard]] inline bool split_eti_frame(const std::byte *frame, std::size_t len,
                                          Handler &&on_message) noexcept {
  if (frame == nullptr || len < 8) {
    return false;
  }

  std::uint32_t body_len{};
  std::memcpy(&body_len, frame, sizeof(body_len));
  if (body_len == 0 || body_len > len) {
    return false;
  }

  return on_message(frame, body_len);
}

} // namespace hft::proto::mcx
