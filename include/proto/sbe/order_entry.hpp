#pragma once

#include <proto/sbe/common.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hft::proto::sbe {

#pragma pack(push, 1)

struct ApiRequestHeader {
  static constexpr std::size_t kWireSize = 140;

  char req_id[64]{};
  std::uint64_t timestamp{};
  std::uint32_t recv_window{};
  char referer[64]{};
};

struct ApiRespHeader {
  static constexpr std::size_t kWireSize = 232;

  char req_id[64]{};
  char conn_id[64]{};
  char trace_id[64]{};
  std::int64_t time_now{};
  std::int64_t in_time{};
  std::int64_t bapi_limit{};
  std::int64_t bapi_limit_status{};
  std::int64_t bapi_limit_reset_timestamp{};
};

struct CommonOrderRespData {
  static constexpr std::size_t kWireSize = 128;

  char order_id[64]{};
  char order_link_id[64]{};
};

// AuthReq — templateId 1.
struct AuthReq {
  static constexpr std::uint16_t kTemplateId = 1;
  static constexpr std::size_t kWireSize = 200;

  char req_id[64]{};
  char api_key[64]{};
  std::uint64_t expires{};
  char signature[64]{};
};

// CreateOrderReqV5 — templateId 5 (fixed-width on the wire).
struct CreateOrderReqV5 {
  static constexpr std::uint16_t kTemplateId = 5;
  static constexpr std::size_t kWireSize = 242;

  ApiRequestHeader header{};
  CategoryType category{CategoryType::spot};
  std::int64_t symbol_id{};
  SideType side{SideType::buy};
  OrderType order_type{OrderType::limit};
  Decimal64 qty{};
  Decimal64 price{};
  char order_link_id[64]{};
  TimeInForceType time_in_force{TimeInForceType::good_till_cancel};
  PositionIdxType position_idx{PositionIdxType::one_way};
  MarketUnitType market_unit{MarketUnitType::base_coin};
  BoolEnum is_leverage{BoolEnum::false_};
  BoolEnum reduce_only{BoolEnum::false_};
  BoolEnum close_on_trigger{BoolEnum::false_};
  BoolEnum mmp{BoolEnum::false_};
  SmpType smp_type{SmpType::unknown};
  BoolEnum rpi_taker_access{BoolEnum::false_};
};

// CreateOrderRespV5 fixed root — templateId 6 (retMsg varString16 on wire).
struct CreateOrderRespV5 {
  static constexpr std::uint16_t kTemplateId = 6;
  static constexpr std::size_t kWireSize = 364;

  ApiRespHeader resp_header{};
  std::int32_t ret_code{};
  CommonOrderRespData result{};
};

// CancelOrderReqV5 — templateId 9.
struct CancelOrderReqV5 {
  static constexpr std::uint16_t kTemplateId = 9;
  static constexpr std::size_t kWireSize = 277;

  ApiRequestHeader header{};
  CategoryType category{CategoryType::spot};
  std::int64_t symbol_id{};
  char order_id[64]{};
  char order_link_id[64]{};
};

#pragma pack(pop)

static_assert(sizeof(ApiRequestHeader) == ApiRequestHeader::kWireSize);
static_assert(sizeof(ApiRespHeader) == ApiRespHeader::kWireSize);
static_assert(sizeof(CommonOrderRespData) == CommonOrderRespData::kWireSize);
static_assert(sizeof(AuthReq) == AuthReq::kWireSize);
static_assert(sizeof(CreateOrderReqV5) == CreateOrderReqV5::kWireSize);
static_assert(sizeof(CreateOrderRespV5) == CreateOrderRespV5::kWireSize);
static_assert(sizeof(CancelOrderReqV5) == CancelOrderReqV5::kWireSize);
static_assert(std::is_trivially_copyable_v<CreateOrderReqV5>);

} // namespace hft::proto::sbe
