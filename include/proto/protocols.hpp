#pragma once

#include <core/arch.hpp>
#include <proto/sbe/fast_order.hpp>
#include <proto/sbe/market.hpp>
#include <proto/sbe/order_entry.hpp>
#include <proto/mcx/confirm.hpp>
#include <proto/mcx/order.hpp>

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

// NASDAQ OUCH 4.2 — Accepted (inbound), message type 'A' (66 bytes).
struct OuchOrderAccepted {
  static constexpr char kType = 'A';
  static constexpr std::size_t kWireSize = 66;

  char message_type{kType};
  std::uint64_t timestamp{};
  char order_token[14]{};
  char buy_sell_indicator{};
  std::uint32_t shares{};
  char stock[8]{};
  std::uint32_t price{};
  std::uint32_t time_in_force{};
  char firm[4]{};
  char display{};
  std::uint64_t order_reference_number{};
  char capacity{};
  char intermarket_sweep_eligibility{};
  std::uint32_t minimum_quantity{};
  char cross_type{};
  char order_state{}; // 'A' live, 'D' dead
  char bbo_weight_indicator{};
};
struct MtbtStreamHeader {
  static constexpr std::size_t kWireSize = 8;

  std::int16_t msg_len{};
  std::int16_t stream_id{};
  std::int32_t seq_no{};
};
struct MtbtNewOrder {
  static constexpr char kTypeNew = 'N';
  static constexpr char kTypeMod = 'M';
  static constexpr char kTypeCancel = 'X';
  static constexpr std::size_t kWireSize = 26;

  char message_type{kTypeNew};
  std::int32_t timestamp{};  // nanoseconds since 01-Jan-1980 00:00:00
  double order_id{};
  std::int32_t token{};
  char order_type{};         // 'B' buy, 'S' sell
  std::int32_t price{};      // paise (segment-specific scale)
  std::int32_t quantity{};
};

#pragma pack(pop)

#pragma pack(push, 2)

struct NnfSecInfo {
  static constexpr std::size_t kWireSize = 12;

  char symbol[10]{};
  char series[2]{};
};

struct NnfStOrderFlags {
  static constexpr std::size_t kWireSize = 2;

  std::uint16_t bits{};
};
struct NnfOrderEntry {
  static constexpr std::uint16_t kTranscodeBoardLotInTr = 20000;
  static constexpr std::size_t kWireSize = 136;

  std::int16_t transcode{kTranscodeBoardLotInTr};
  std::int32_t trader_id{};
  NnfSecInfo sec_info{};
  char account_number[10]{};
  std::int16_t book_type{};
  std::int16_t buy_sell{};
  std::int32_t disclosed_vol{};
  std::int32_t volume{};
  std::int32_t price{};
  std::int32_t good_till_date{};
  NnfStOrderFlags order_flags{};
  std::int16_t branch_id{};
  std::int32_t user_id{};
  char broker_id[5]{};
  char suspended{};
  char settlor[12]{};
  std::int16_t pro_client{};
  double nnf_field{};
  std::int32_t transaction_id{};
  char pan[10]{};
  std::int32_t algo_id{};
  std::int16_t reserved_filler{};
  char reserved[32]{};
};

// NSE NNF ORDER_RESPONSE — ORDER_CONFIRMATION (transcode 2073), 112-byte body.
struct NnfOrderConfirm {
  static constexpr std::int16_t kTranscodeOrderConfirmation = 2073;
  static constexpr std::size_t kWireSize = 100;

  std::int16_t transcode{kTranscodeOrderConfirmation};
  std::int16_t reason_code{}; // 0 = accepted
  std::int32_t token{};
  double order_number{};
  std::int16_t book_type{};
  std::int16_t buy_sell{};
  std::int32_t disclosed_vol{};
  std::int32_t disclosed_vol_remaining{};
  std::int32_t total_vol_remaining{};
  std::int32_t volume{};
  std::int32_t price{};
  std::int32_t trigger_price{};
  NnfStOrderFlags order_flags{};
  std::int32_t trader_id{};
  double nnf_field{};
  std::int32_t algo_id{};
  std::int64_t activity_time_nanos{};
  std::int16_t competitor_period{};
  std::int16_t solicitor_period{};
  std::int16_t auction_number{};
  char suspended{};
  char filler{};
  char pan[10]{};
  char segment{};
  char reserved[11]{};
};

#pragma pack(pop)

#pragma pack(push, 1)

// NSE NNF MESSAGE_HEADER — 40 bytes (prefixed on full interactive packets).
struct NnfMessageHeader {
  static constexpr std::size_t kWireSize = 40;

  std::int16_t transaction_code{};
  std::int32_t log_time{};
  char alpha_char[2]{};
  std::int32_t trader_id{};
  std::int16_t error_code{};
  char timestamp[8]{};
  char timestamp1[8]{};
  char timestamp2[8]{};
  std::int16_t message_length{};
};

#pragma pack(pop)

enum class Kind : std::uint8_t {
  itch = 1,
  ouch = 2,
  mtbt = 3,
  nnf = 4,
  signal = 5,
  sbe_market = 6,
  sbe_order_entry = 7,
  sbe_fast_order = 8,
  ouch_confirm = 9,
  nnf_confirm = 10,
  mcx_market = 11,
  mcx_order = 12,
  mcx_confirm = 13,
};

enum class Side : std::int16_t { none = 0, buy = 1, sell = -1 };

#pragma pack(push, 1)

struct StrategySignal {
  static constexpr std::size_t kWireSize = 39;

  Kind feed{Kind::itch};
  Side side{Side::none};
  std::int32_t price{};
  std::int32_t quantity{};
  std::uint32_t order_token{};
  char stock[8]{};
  std::int32_t token{};
  char symbol[10]{};
  char series[2]{};
};

#pragma pack(pop)

static_assert(sizeof(ItchAddOrder) == ItchAddOrder::kWireSize);
static_assert(sizeof(OuchEnterOrder) == OuchEnterOrder::kWireSize);
static_assert(sizeof(OuchOrderAccepted) == OuchOrderAccepted::kWireSize);
static_assert(sizeof(MtbtStreamHeader) == MtbtStreamHeader::kWireSize);
static_assert(sizeof(MtbtNewOrder) == MtbtNewOrder::kWireSize);
static_assert(sizeof(NnfSecInfo) == NnfSecInfo::kWireSize);
static_assert(sizeof(NnfStOrderFlags) == NnfStOrderFlags::kWireSize);
static_assert(sizeof(NnfOrderEntry) == NnfOrderEntry::kWireSize);
static_assert(sizeof(NnfOrderConfirm) == NnfOrderConfirm::kWireSize);
static_assert(sizeof(NnfMessageHeader) == NnfMessageHeader::kWireSize);
static_assert(std::is_trivially_copyable_v<ItchAddOrder>);
static_assert(std::is_trivially_copyable_v<OuchEnterOrder>);
static_assert(std::is_trivially_copyable_v<OuchOrderAccepted>);
static_assert(std::is_trivially_copyable_v<MtbtNewOrder>);
static_assert(sizeof(StrategySignal) == StrategySignal::kWireSize);
static_assert(std::is_trivially_copyable_v<NnfOrderEntry>);
static_assert(std::is_trivially_copyable_v<NnfOrderConfirm>);
static_assert(std::is_trivially_copyable_v<StrategySignal>);
static_assert(sizeof(sbe::BestObRpi) == sbe::BestObRpi::kWireSize);
static_assert(sizeof(sbe::CreateOrderReqV5) == sbe::CreateOrderReqV5::kWireSize);
static_assert(sizeof(sbe::FastOrderResp) == sbe::FastOrderResp::kWireSize);
static_assert(sizeof(mcx::NewOrderSingleShort) == mcx::NewOrderSingleShort::kWireSize);
static_assert(sizeof(mcx::ExecutionReportNew) == mcx::ExecutionReportNew::kWireSize);
static_assert(sizeof(mcx::TopOfBook) == mcx::TopOfBook::kWireSize);

template <typename T>
inline constexpr Kind kind_of = Kind::itch;

template <>
inline constexpr Kind kind_of<ItchAddOrder> = Kind::itch;
template <>
inline constexpr Kind kind_of<OuchEnterOrder> = Kind::ouch;
template <>
inline constexpr Kind kind_of<OuchOrderAccepted> = Kind::ouch_confirm;
template <>
inline constexpr Kind kind_of<MtbtNewOrder> = Kind::mtbt;
template <>
inline constexpr Kind kind_of<NnfOrderEntry> = Kind::nnf;
template <>
inline constexpr Kind kind_of<NnfOrderConfirm> = Kind::nnf_confirm;
template <>
inline constexpr Kind kind_of<sbe::BestObRpi> = Kind::sbe_market;
template <>
inline constexpr Kind kind_of<sbe::CreateOrderReqV5> = Kind::sbe_order_entry;
template <>
inline constexpr Kind kind_of<sbe::FastOrderResp> = Kind::sbe_fast_order;
template <>
inline constexpr Kind kind_of<mcx::TopOfBook> = Kind::mcx_market;
template <>
inline constexpr Kind kind_of<mcx::NewOrderSingleShort> = Kind::mcx_order;
template <>
inline constexpr Kind kind_of<mcx::ExecutionReportNew> = Kind::mcx_confirm;

inline constexpr bool is_order_confirm(Kind kind) noexcept {
  return kind == Kind::ouch_confirm || kind == Kind::nnf_confirm ||
         kind == Kind::sbe_fast_order || kind == Kind::mcx_confirm;
}

inline constexpr bool is_order_request(Kind kind) noexcept {
  return kind == Kind::ouch || kind == Kind::nnf ||
         kind == Kind::sbe_order_entry || kind == Kind::mcx_order;
}

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
  case Kind::signal:
    return sizeof(StrategySignal);
  case Kind::sbe_market:
    return sizeof(sbe::BestObRpi);
  case Kind::sbe_order_entry:
    return sizeof(sbe::CreateOrderReqV5);
  case Kind::sbe_fast_order:
    return sizeof(sbe::FastOrderResp);
  case Kind::ouch_confirm:
    return sizeof(OuchOrderAccepted);
  case Kind::nnf_confirm:
    return sizeof(NnfOrderConfirm);
  case Kind::mcx_market:
    return sizeof(mcx::TopOfBook);
  case Kind::mcx_order:
    return sizeof(mcx::NewOrderSingleShort);
  case Kind::mcx_confirm:
    return sizeof(mcx::ExecutionReportNew);
  }
  return 0;
}

inline constexpr std::size_t max_slot_size() noexcept {
  return std::max({sizeof(ItchAddOrder), sizeof(OuchEnterOrder),
                   sizeof(OuchOrderAccepted), sizeof(MtbtNewOrder),
                   sizeof(NnfOrderEntry), sizeof(NnfOrderConfirm),
                   sizeof(StrategySignal), sizeof(sbe::BestObRpi),
                   sizeof(sbe::CreateOrderReqV5), sizeof(sbe::FastOrderResp),
                   sizeof(mcx::NewOrderSingleShort),
                   sizeof(mcx::ExecutionReportNew), sizeof(mcx::TopOfBook)});
}

template <typename T>
concept WireMessage =
    std::same_as<T, ItchAddOrder> || std::same_as<T, OuchEnterOrder> ||
    std::same_as<T, OuchOrderAccepted> || std::same_as<T, MtbtNewOrder> ||
    std::same_as<T, NnfOrderEntry> || std::same_as<T, NnfOrderConfirm> ||
    std::same_as<T, sbe::BestObRpi> ||
    std::same_as<T, sbe::CreateOrderReqV5> ||
    std::same_as<T, sbe::FastOrderResp> ||
    std::same_as<T, mcx::TopOfBook> ||
    std::same_as<T, mcx::NewOrderSingleShort> ||
    std::same_as<T, mcx::ExecutionReportNew>;

struct TaggedMessage {
  Kind kind{Kind::itch};
  alignas(arch::cache_line_size) std::array<std::byte, max_slot_size()> bytes{};
};

} // namespace hft::proto
