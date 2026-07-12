#pragma once

#include <proto/mcx/common.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hft::proto::mcx {

#pragma pack(push, 1)

// New Order Single (short layout) — TemplateID 10125, MsgType D (224-byte full layout
// is TemplateID 10100). Short layout: limit orders only, no GTD.
struct NewOrderSingleShort {
  static constexpr std::uint16_t kTemplateId = kTemplateNewOrderSingleShort;
  static constexpr std::size_t kWireSize = 120;

  std::uint32_t body_len{kWireSize};
  std::uint16_t template_id{kTemplateId};
  char network_msg_id[8]{};
  char pad2_header[2]{};
  std::uint32_t msg_seq_num{};
  std::uint32_t sender_sub_id{};
  std::int64_t price{};
  std::uint64_t terminal_info{};
  std::uint64_t cl_ord_id{};
  std::int64_t order_qty{};
  std::int64_t disclosed_qty{};
  std::uint32_t simple_security_id{};
  std::uint32_t filler2{};
  std::uint16_t filler4{};
  std::uint8_t account_type{static_cast<std::uint8_t>(AccountType::own)};
  std::uint8_t side{static_cast<std::uint8_t>(Side::buy)};
  std::uint8_t price_validity_check_type{0};
  std::uint8_t time_in_force{static_cast<std::uint8_t>(TimeInForce::day)};
  std::uint8_t smpf_order_identifier{0};
  std::uint8_t exec_inst{0};
  std::uint64_t strategy_id{};
  std::uint64_t strategy_trigger_seq_no{};
  char free_text1[12]{};
  char cp_code[12]{};
};

#pragma pack(pop)

static_assert(sizeof(NewOrderSingleShort) == NewOrderSingleShort::kWireSize);
static_assert(std::is_trivially_copyable_v<NewOrderSingleShort>);

inline constexpr std::size_t kMaxUccLen = 12;

// Canonical top-of-book tick for commodity strategy input.
struct TopOfBook {
  static constexpr std::size_t kWireSize = 56;

  std::uint32_t simple_security_id{};
  std::int64_t bid_price{};
  std::int64_t ask_price{};
  std::int64_t bid_qty{};
  std::int64_t ask_qty{};
  char symbol[kMaxUccLen]{};
};

static_assert(sizeof(TopOfBook) == TopOfBook::kWireSize);
static_assert(std::is_trivially_copyable_v<TopOfBook>);

} // namespace hft::proto::mcx
