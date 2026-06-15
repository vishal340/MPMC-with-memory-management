#pragma once

#include <mpmc/mpmc.hpp>
#include <proto/protocols.hpp>
#include <trading/strategy.hpp>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace hft::trading {

template <int MarketCap, int SignalCap>
class ItchProducer {
public:
  ItchProducer(hft::MPMC<MarketCap> &market_queue,
               hft::MPMC<SignalCap> &signal_queue,
               std::int32_t band_bps = strategy::kDefaultBandBps) noexcept
      : market_queue_(market_queue), signal_queue_(signal_queue),
        band_bps_(band_bps) {}

  [[nodiscard]] std::uint64_t signals_emitted() const noexcept {
    return signals_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage msg{};
    if (!market_queue_.try_pop(msg) || msg.kind != hft::proto::Kind::itch) {
      return;
    }

    hft::proto::ItchAddOrder tick{};
    std::memcpy(&tick, msg.bytes.data(), sizeof(tick));
    on_tick(tick);
  }

  void on_tick(const hft::proto::ItchAddOrder &tick) noexcept {
    ref_price_ = strategy::update_ref_price(ref_price_,
                                            static_cast<std::int32_t>(tick.price));
    const hft::proto::Side side =
        strategy::evaluate(ref_price_, static_cast<std::int32_t>(tick.price), band_bps_);
    if (side == hft::proto::Side::none) {
      return;
    }

    hft::proto::StrategySignal signal{};
    signal.feed = hft::proto::Kind::itch;
    signal.side = side;
    signal.price = static_cast<std::int32_t>(tick.price);
    signal.quantity = static_cast<std::int32_t>(tick.shares);
    signal.order_token = next_token_.fetch_add(1, std::memory_order_relaxed);
    std::memcpy(signal.stock, tick.stock, sizeof(signal.stock));

    hft::proto::TaggedMessage out{};
    strategy::encode_signal(signal, out);
    signal_queue_.push(out);
    signals_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<MarketCap> &market_queue_;
  hft::MPMC<SignalCap> &signal_queue_;
  std::int32_t band_bps_{strategy::kDefaultBandBps};
  std::int32_t ref_price_{0};
  std::atomic<std::uint32_t> next_token_{1};
  std::atomic<std::uint64_t> signals_{0};
};

template <int MarketCap, int SignalCap>
class MtbtProducer {
public:
  MtbtProducer(hft::MPMC<MarketCap> &market_queue,
               hft::MPMC<SignalCap> &signal_queue,
               std::int32_t band_bps = strategy::kDefaultBandBps) noexcept
      : market_queue_(market_queue), signal_queue_(signal_queue),
        band_bps_(band_bps) {}

  [[nodiscard]] std::uint64_t signals_emitted() const noexcept {
    return signals_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage msg{};
    if (!market_queue_.try_pop(msg) || msg.kind != hft::proto::Kind::mtbt) {
      return;
    }

    hft::proto::MtbtNewOrder tick{};
    std::memcpy(&tick, msg.bytes.data(), sizeof(tick));
    on_tick(tick);
  }

  void on_tick(const hft::proto::MtbtNewOrder &tick) noexcept {
    ref_price_ = strategy::update_ref_price(ref_price_, tick.price);
    const hft::proto::Side side =
        strategy::evaluate(ref_price_, tick.price, band_bps_);
    if (side == hft::proto::Side::none) {
      return;
    }

    hft::proto::StrategySignal signal{};
    signal.feed = hft::proto::Kind::mtbt;
    signal.side = side;
    signal.price = tick.price;
    signal.quantity = tick.quantity;
    signal.order_token = next_token_.fetch_add(1, std::memory_order_relaxed);
    signal.token = tick.token;
    std::snprintf(signal.symbol, sizeof(signal.symbol), "TKN%d", tick.token);
    signal.series[0] = 'E';
    signal.series[1] = 'Q';

    hft::proto::TaggedMessage out{};
    strategy::encode_signal(signal, out);
    signal_queue_.push(out);
    signals_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<MarketCap> &market_queue_;
  hft::MPMC<SignalCap> &signal_queue_;
  std::int32_t band_bps_{strategy::kDefaultBandBps};
  std::int32_t ref_price_{0};
  std::atomic<std::uint32_t> next_token_{1};
  std::atomic<std::uint64_t> signals_{0};
};

} // namespace hft::trading
