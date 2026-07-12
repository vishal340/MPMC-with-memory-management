#pragma once

#include <proto/mcx/common.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hft::proto::mcx {

#pragma pack(push, 1)

// New Order Response (standard order) — TemplateID 10101, ExecutionReport MsgType 8.
struct ExecutionReportNew {
  static constexpr std::uint16_t kTemplateId = kTemplateExecutionReportNew;
  static constexpr std::size_t kWireSize = 188;

  std::uint32_t body_len{kWireSize};
  std::uint16_t template_id{kTemplateId};
  char pad2_header[2]{};
  std::uint64_t request_time{};
  std::uint64_t reserve0{};
  std::uint64_t reserve1{};
  std::uint64_t reserve2{};
  std::uint64_t reserve3{};
  std::uint64_t sending_time{};
  std::uint32_t msg_seq_num{};
  std::uint16_t partition_id{};
  std::uint8_t appl_id{4};
  char appl_msg_id[16]{};
  std::uint8_t last_fragment{1};
  std::uint64_t order_id{};
  std::uint64_t cl_ord_id{};
  std::int64_t simple_security_id{};
  char pad4_body[4]{};
  std::int64_t price_mk_to_limit_px{};
  std::int64_t reserved1{};
  std::int64_t reserved2{};
  std::uint64_t exec_id{};
  std::uint64_t trd_reg_ts_entry_time{};
  std::uint64_t reserve14{};
  std::uint64_t lst_updt_time{};
  std::uint64_t filler1{};
  std::uint32_t filler2{};
  std::uint16_t filler4{};
  char ord_status{static_cast<char>(OrdStatus::new_)};
  char exec_type{static_cast<char>(ExecType::new_)};
  std::uint16_t exec_restatement_reason{kExecRestatementOrderAdded};
  std::uint8_t product_complex{1};
  std::uint8_t filler5{};
  char pad4_trailer[4]{};
};

#pragma pack(pop)

static_assert(sizeof(ExecutionReportNew) == ExecutionReportNew::kWireSize);
static_assert(std::is_trivially_copyable_v<ExecutionReportNew>);

} // namespace hft::proto::mcx
