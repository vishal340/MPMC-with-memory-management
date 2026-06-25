#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hft::proto::sbe {

// Bybit SBE — FIX/SBE 1.0, little-endian. Shared across market, order entry,
// and fast-order schemas (see Bybit v5 SBE docs).

#pragma pack(push, 1)

struct MessageHeader {
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t block_length{};
  std::uint16_t template_id{};
  std::uint16_t schema_id{};
  std::uint16_t version{};
};

struct Decimal64 {
  static constexpr std::size_t kWireSize = 9;

  std::int8_t exponent{};
  std::int64_t mantissa{};
};

struct GroupSize16 {
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t block_length{};
  std::uint16_t num_in_group{};
};

#pragma pack(pop)

static_assert(sizeof(MessageHeader) == MessageHeader::kWireSize);
static_assert(sizeof(Decimal64) == Decimal64::kWireSize);
static_assert(sizeof(GroupSize16) == GroupSize16::kWireSize);
static_assert(std::is_trivially_copyable_v<MessageHeader>);
static_assert(std::is_trivially_copyable_v<Decimal64>);

// Market schema (quote.sbe): schemaId=1, version=0.
inline constexpr std::uint16_t kMarketSchemaId = 1;
inline constexpr std::uint16_t kMarketSchemaVersion = 0;

// WS order entry schema (order.trading.api.sbe): schemaId=2, version=2.
inline constexpr std::uint16_t kOrderEntrySchemaId = 2;
inline constexpr std::uint16_t kOrderEntrySchemaVersion = 2;

// Fast order response schema (order.fast.sbe): schemaId=1, version=0.
inline constexpr std::uint16_t kFastOrderSchemaId = 1;
inline constexpr std::uint16_t kFastOrderSchemaVersion = 0;

enum class PkgType : std::uint8_t { snapshot = 0, delta = 1 };

enum class SideType : std::uint8_t {
  unknown = 0,
  buy = 1,
  sell = 2,
  non_representable = 254,
};

enum class BoolEnum : std::uint8_t {
  false_ = 0,
  true_ = 1,
  non_representable = 254,
};

enum class CategoryType : std::uint8_t {
  unknown = 0,
  spot = 1,
  linear = 2,
  inverse = 3,
  option = 4,
  non_representable = 254,
};

enum class OrderType : std::uint8_t {
  unknown = 0,
  market = 1,
  limit = 2,
  non_representable = 254,
};

enum class TimeInForceType : std::uint8_t {
  unknown = 0,
  good_till_cancel = 1,
  post_only = 2,
  immediate_or_cancel = 3,
  fill_or_kill = 4,
  rpi = 5,
  non_representable = 254,
};

enum class PositionIdxType : std::uint8_t {
  one_way = 0,
  hedge_buy = 1,
  hedge_sell = 2,
  unknown = 253,
  non_representable = 254,
};

enum class MarketUnitType : std::uint8_t {
  unknown = 0,
  base_coin = 1,
  quote_coin = 2,
  non_representable = 254,
};

enum class SmpType : std::uint8_t {
  unknown = 0,
  cancel_taker = 1,
  cancel_maker = 2,
  cancel_both = 3,
  non_representable = 254,
};

[[nodiscard]] inline std::int64_t
apply_exponent(std::int64_t mantissa, std::int8_t exponent) noexcept {
  if (exponent == 0) {
    return mantissa;
  }
  if (exponent > 0) {
    std::int64_t scaled = mantissa;
    for (std::int8_t i = 0; i < exponent; ++i) {
      scaled *= 10;
    }
    return scaled;
  }
  std::int64_t scaled = mantissa;
  for (std::int8_t i = 0; i > exponent; --i) {
    scaled /= 10;
  }
  return scaled;
}

} // namespace hft::proto::sbe
