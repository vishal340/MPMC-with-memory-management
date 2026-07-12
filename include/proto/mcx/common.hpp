#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hft::proto::mcx {

// MCX Enhanced Trading Interface (ETI) — flat binary, little-endian, FIX 5.0 SP2
// semantics. BodyLen is always a multiple of 8 (MCX ETI API v1.4.x).

#pragma pack(push, 1)

struct MessageHeaderIn {
  static constexpr std::size_t kWireSize = 16;

  std::uint32_t body_len{};
  std::uint16_t template_id{};
  char network_msg_id[8]{};
  char pad2[2]{};
};

struct MessageHeaderOut {
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t body_len{};
  std::uint16_t template_id{};
  char pad2[2]{};
};

struct RequestHeader {
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t msg_seq_num{};
  std::uint32_t sender_sub_id{};
};

struct ResponseHeaderMe {
  static constexpr std::size_t kWireSize = 72;

  std::uint64_t request_time{};
  std::uint64_t reserve0{};
  std::uint64_t reserve1{};
  std::uint64_t reserve2{};
  std::uint64_t reserve3{};
  std::uint64_t sending_time{};
  std::uint32_t msg_seq_num{};
  std::uint16_t partition_id{};
  std::uint8_t appl_id{};
  char appl_msg_id[16]{};
  std::uint8_t last_fragment{};
};

#pragma pack(pop)

static_assert(sizeof(MessageHeaderIn) == MessageHeaderIn::kWireSize);
static_assert(sizeof(MessageHeaderOut) == MessageHeaderOut::kWireSize);
static_assert(sizeof(RequestHeader) == RequestHeader::kWireSize);
static_assert(sizeof(ResponseHeaderMe) == ResponseHeaderMe::kWireSize);

inline constexpr std::uint16_t kTemplateNewOrderSingle = 10100;
inline constexpr std::uint16_t kTemplateNewOrderSingleShort = 10125;
inline constexpr std::uint16_t kTemplateExecutionReportNew = 10101;
inline constexpr std::uint16_t kTemplateSessionLogon = 10000;

enum class Side : std::uint8_t { buy = 1, sell = 2 };

enum class OrdType : std::uint8_t {
  limit = 2,
  stop_market = 3,
  stop_limit = 4,
  market_to_limit = 5,
};

enum class TimeInForce : std::uint8_t {
  day = 0,
  gtc = 1,
  ioc = 3,
  eos = 7,
  gtd = 6,
};

enum class AccountType : std::uint8_t { own = 1, client = 3, institution = 5 };

enum class OrdStatus : char {
  new_ = '0',
  cancelled = '4',
  suspended = '9',
};

enum class ExecType : char {
  new_ = '0',
  cancelled = '4',
  triggered = 'L',
  replaced = '5',
};

inline constexpr std::uint16_t kExecRestatementOrderAdded = 101;

inline std::uint32_t align_body_len(std::uint32_t len) noexcept {
  return (len + 7U) & ~7U;
}

} // namespace hft::proto::mcx
