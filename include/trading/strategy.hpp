#pragma once

#include <proto/protocols.hpp>

#include <cstring>

namespace hft::strategy {

inline constexpr std::int32_t kDefaultBandBps = 10;

inline std::int32_t update_ref_price(std::int32_t ref,
                                     std::int32_t price) noexcept {
  if (ref == 0) {
    return price;
  }
  return ref + (price - ref) / 8;
}

inline proto::Side evaluate(std::int32_t ref, std::int32_t price,
                            std::int32_t band_bps) noexcept {
  if (ref <= 0 || price <= 0) {
    return proto::Side::none;
  }

  const std::int32_t band = (ref * band_bps) / 10000;
  if (band <= 0) {
    return proto::Side::none;
  }

  if (price <= ref - band) {
    return proto::Side::buy;
  }
  if (price >= ref + band) {
    return proto::Side::sell;
  }
  return proto::Side::none;
}

inline void encode_signal(const proto::StrategySignal &signal,
                          proto::TaggedMessage &out) noexcept {
  out.kind = proto::Kind::signal;
  std::memcpy(out.bytes.data(), &signal, sizeof(signal));
}

inline proto::StrategySignal decode_signal(
    const proto::TaggedMessage &msg) noexcept {
  proto::StrategySignal signal{};
  if (msg.kind == proto::Kind::signal) {
    std::memcpy(&signal, msg.bytes.data(), sizeof(signal));
  }
  return signal;
}

} // namespace hft::strategy
