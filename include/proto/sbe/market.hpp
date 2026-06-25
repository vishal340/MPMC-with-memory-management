#pragma once

#include <proto/sbe/common.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hft::proto::sbe {

#pragma pack(push, 1)

// BestOBRpiEvent — templateId 20000, channel ob.rpi.1.sbe.<symbol>.
struct BestObRpiBody {
  static constexpr std::uint16_t kTemplateId = 20000;
  static constexpr std::size_t kFixedWireSize = 98;

  std::int64_t ts{};
  std::int64_t seq{};
  std::int64_t cts{};
  std::int64_t u{};
  std::int64_t ask_normal_price{};
  std::int64_t ask_normal_size{};
  std::int64_t ask_rpi_price{};
  std::int64_t ask_rpi_size{};
  std::int64_t bid_normal_price{};
  std::int64_t bid_normal_size{};
  std::int64_t bid_rpi_price{};
  std::int64_t bid_rpi_size{};
  std::int8_t price_exponent{};
  std::int8_t size_exponent{};
};

// OBL50Event level — templateId 20001, one price level in asks/bids group.
struct ObL50Level {
  static constexpr std::size_t kWireSize = 16;

  std::int64_t price{};
  std::int64_t size{};
};

// OBL50Event fixed root — templateId 20001, channel ob.50.sbe.<symbol>.
struct ObL50Body {
  static constexpr std::uint16_t kTemplateId = 20001;
  static constexpr std::size_t kFixedWireSize = 35;

  std::int64_t ts{};
  std::int64_t seq{};
  std::int64_t cts{};
  std::int64_t u{};
  std::int8_t price_exponent{};
  std::int8_t size_exponent{};
  PkgType pkg_type{PkgType::snapshot};
};

// PublicTradeEvent item — one fill inside the tradeItems group.
struct PublicTradeItem {
  static constexpr std::size_t kWireSize = 35;

  std::int64_t fill_time{};
  std::int64_t price{};
  std::int64_t size{};
  std::int64_t seq{};
  SideType side{SideType::unknown};
  BoolEnum is_block_trade{BoolEnum::false_};
  BoolEnum is_rpi{BoolEnum::false_};
  // execId varString8 follows each item on the wire.
};

// PublicTradeEvent fixed root — templateId 20002.
struct PublicTradeBody {
  static constexpr std::uint16_t kTemplateId = 20002;
  static constexpr std::size_t kFixedWireSize = 10;

  std::int64_t ts{};
  std::int8_t price_exponent{};
  std::int8_t size_exponent{};
};

#pragma pack(pop)

static_assert(sizeof(BestObRpiBody) == BestObRpiBody::kFixedWireSize);
static_assert(sizeof(ObL50Level) == ObL50Level::kWireSize);
static_assert(sizeof(ObL50Body) == ObL50Body::kFixedWireSize);
static_assert(sizeof(PublicTradeItem) == PublicTradeItem::kWireSize);
static_assert(sizeof(PublicTradeBody) == PublicTradeBody::kFixedWireSize);

inline constexpr std::size_t kMaxSymbolLen = 16;
inline constexpr std::size_t kMaxExecIdLen = 32;

// Canonical in-memory BBO tick (fixed layout for MPMC slots).
struct BestObRpi {
  static constexpr std::uint16_t kTemplateId = BestObRpiBody::kTemplateId;
  static constexpr std::size_t kWireSize =
      BestObRpiBody::kFixedWireSize + 1 + kMaxSymbolLen;

  BestObRpiBody body{};
  std::uint8_t symbol_len{};
  char symbol[kMaxSymbolLen]{};
};

static_assert(sizeof(BestObRpi) == BestObRpi::kWireSize);
static_assert(std::is_trivially_copyable_v<BestObRpi>);

} // namespace hft::proto::sbe
