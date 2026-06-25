#pragma once

#include <proto/sbe/common.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hft::proto::sbe {

#pragma pack(push, 1)

// FastOrderResp fixed root — templateId 21000, channel private-sbe.
struct FastOrderRespBody {
  static constexpr std::uint16_t kTemplateId = 21000;
  static constexpr std::size_t kFixedWireSize = 60;

  std::uint8_t category{}; // 1=spot, 2=linear, 3=inverse, 4=option
  std::uint8_t side{};     // 1=buy, 2=sell
  std::uint8_t order_status{};
  std::int8_t price_exponent{};
  std::int8_t size_exponent{};
  std::int8_t value_exponent{};
  std::uint16_t reject_reason{};
  std::int64_t price{};
  std::int64_t leaves_qty{};
  std::int64_t leaves_value{};
  std::int64_t creation_time{};
  std::int64_t updated_time{};
  std::int64_t seq{};
  std::int32_t symbol_id{};
};

#pragma pack(pop)

static_assert(sizeof(FastOrderRespBody) == FastOrderRespBody::kFixedWireSize);

inline constexpr std::size_t kMaxOrderIdLen = 32;
inline constexpr std::size_t kMaxOrderLinkIdLen = 32;

// Canonical in-memory fast ack (orderId/orderLinkId varString8 on wire).
struct FastOrderResp {
  static constexpr std::uint16_t kTemplateId = FastOrderRespBody::kTemplateId;
  static constexpr std::size_t kWireSize =
      FastOrderRespBody::kFixedWireSize + 2 + kMaxOrderIdLen + kMaxOrderLinkIdLen;

  FastOrderRespBody body{};
  std::uint8_t order_id_len{};
  char order_id[kMaxOrderIdLen]{};
  std::uint8_t order_link_id_len{};
  char order_link_id[kMaxOrderLinkIdLen]{};
};

static_assert(sizeof(FastOrderResp) == FastOrderResp::kWireSize);
static_assert(std::is_trivially_copyable_v<FastOrderResp>);

} // namespace hft::proto::sbe
