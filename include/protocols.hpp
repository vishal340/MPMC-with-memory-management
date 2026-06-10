#pragma once

#include <arch.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hft::proto {

#pragma pack(push, 1)

// NASDAQ ITCH 5.0 — Add Order (no MPID), message type 'A' (36 bytes).
struct ItchAddOrder {
  static constexpr char kType = 'A';
  static constexpr std::size_t kWireSize = 36;

  char message_type{kType};
  std::uint16_t stock_locate{};
  std::uint16_t tracking_number{};
  std::uint8_t timestamp[6]{};
  std::uint64_t order_reference_number{};
  char side{}; // 'B' or 'S'
  std::uint32_t shares{};
  char stock[8]{};
  std::uint32_t price{};
};

// NASDAQ OUCH 4.2 — Enter Order, message type 'O' (45 bytes).
struct OuchEnterOrder {
  static constexpr char kType = 'O';
  static constexpr std::size_t kWireSize = 45;

  char message_type{kType};
  std::uint32_t order_token{};
  char buy_sell_indicator{};
  std::uint32_t shares{};
  char stock[8]{};
  std::uint32_t price{};
  std::uint32_t time_in_force{};
  char firm[4]{};
  char display{};
  std::uint32_t customer_information{};
  char attribs{};
  char order_state{};
  char locates[6]{};
  char reserved[2]{};
};

// NSE MTBT — New Order tick, message type 'N' (48 bytes).
struct MtbtNewOrder {
  static constexpr char kType = 'N';
  static constexpr std::size_t kWireSize = 48;

  char message_type{kType};
  std::uint64_t timestamp{};
  std::uint64_t order_id{};
  std::uint32_t token{};
  char order_type{};
  std::uint32_t price{};
  std::uint32_t quantity{};
  std::uint16_t stream_id{};
  std::uint32_t sequence_number{};
  char side{};
  char reserved[11]{};
};

// NSE NNF — Order Entry Request (CM segment layout, 256 bytes).
struct NnfOrderEntry {
  static constexpr char kType = 'O';
  static constexpr std::size_t kWireSize = 256;

  char message_type{kType};
  char trader_id[10]{};
  char account[10]{};
  char pan[10]{};
  std::uint32_t token{};
  char buy_sell_indicator{};
  std::uint32_t disclosed_volume{};
  std::uint32_t volume{};
  std::uint32_t price{};
  std::uint32_t good_till_date{};
  char order_flags[2]{};
  char broker_id[5]{};
  char open_close{};
  char settlor[12]{};
  std::uint16_t pro_client_indicator{};
  std::uint16_t settlement_period{};
  char reserved[180]{};
};

#pragma pack(pop)

static_assert(sizeof(ItchAddOrder) == ItchAddOrder::kWireSize);
static_assert(sizeof(OuchEnterOrder) == OuchEnterOrder::kWireSize);
static_assert(sizeof(MtbtNewOrder) == MtbtNewOrder::kWireSize);
static_assert(sizeof(NnfOrderEntry) == NnfOrderEntry::kWireSize);
static_assert(std::is_trivially_copyable_v<ItchAddOrder>);
static_assert(std::is_trivially_copyable_v<OuchEnterOrder>);
static_assert(std::is_trivially_copyable_v<MtbtNewOrder>);
static_assert(std::is_trivially_copyable_v<NnfOrderEntry>);

enum class Kind : std::uint8_t {
  itch = 1,
  ouch = 2,
  mtbt = 3,
  nnf = 4,
};

template <typename T>
inline constexpr Kind kind_of = Kind::itch;

template <>
inline constexpr Kind kind_of<ItchAddOrder> = Kind::itch;
template <>
inline constexpr Kind kind_of<OuchEnterOrder> = Kind::ouch;
template <>
inline constexpr Kind kind_of<MtbtNewOrder> = Kind::mtbt;
template <>
inline constexpr Kind kind_of<NnfOrderEntry> = Kind::nnf;

inline constexpr std::size_t slot_size(Kind kind) noexcept {
  switch (kind) {
  case Kind::itch:
    return sizeof(ItchAddOrder);
  case Kind::ouch:
    return sizeof(OuchEnterOrder);
  case Kind::mtbt:
    return sizeof(MtbtNewOrder);
  case Kind::nnf:
    return sizeof(NnfOrderEntry);
  }
  return 0;
}

inline constexpr std::size_t max_slot_size() noexcept {
  return std::max({sizeof(ItchAddOrder), sizeof(OuchEnterOrder),
                   sizeof(MtbtNewOrder), sizeof(NnfOrderEntry)});
}

template <typename T>
concept WireMessage =
    std::same_as<T, ItchAddOrder> || std::same_as<T, OuchEnterOrder> ||
    std::same_as<T, MtbtNewOrder> || std::same_as<T, NnfOrderEntry>;

struct TaggedMessage {
  Kind kind{Kind::itch};
  alignas(arch::cache_line_size) std::array<std::byte, max_slot_size()> bytes{};
};

} // namespace hft::proto
