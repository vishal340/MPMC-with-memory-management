#pragma once

#include <mem/memory_pool.hpp>
#include <mpmc/mpmc.hpp>
#include <proto/protocols.hpp>
#include <trading/strategy.hpp>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace hft::trading {

template <int MarketCap, int SignalCap, std::size_t PoolCap>
class ItchProducer {
public:
  ItchProducer(hft::MPMC<MarketCap> &market_queue,
               hft::MPMC<SignalCap> &signal_queue,
               hft::mem::TaggedPool<PoolCap> &pool,
               std::int32_t band_bps = strategy::kDefaultBandBps) noexcept
      : market_queue_(market_queue), signal_queue_(signal_queue), pool_(pool),
        band_bps_(band_bps) {}

  [[nodiscard]] std::uint64_t signals_emitted() const noexcept {
    return signals_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage *msg = nullptr;
    if (!market_queue_.try_pop(msg) || msg == nullptr ||
        msg->kind != hft::proto::Kind::itch) {
      if (msg != nullptr) {
        pool_.release(msg);
      }
      return;
    }

    hft::proto::ItchAddOrder tick{};
    std::memcpy(&tick, msg->bytes.data(), sizeof(tick));
    on_tick(tick, msg);
  }

  void on_tick(const hft::proto::ItchAddOrder &tick,
               hft::proto::TaggedMessage *owned = nullptr) noexcept {
    ref_price_ = strategy::update_ref_price(ref_price_,
                                            static_cast<std::int32_t>(tick.price));
    const hft::proto::Side side =
        strategy::evaluate(ref_price_, static_cast<std::int32_t>(tick.price), band_bps_);
    if (side == hft::proto::Side::none) {
      if (owned != nullptr) {
        pool_.release(owned);
      }
      return;
    }

    hft::proto::StrategySignal signal{};
    signal.feed = hft::proto::Kind::itch;
    signal.side = side;
    signal.price = static_cast<std::int32_t>(tick.price);
    signal.quantity = static_cast<std::int32_t>(tick.shares);
    signal.order_token = next_token_.fetch_add(1, std::memory_order_relaxed);
    std::memcpy(signal.stock, tick.stock, sizeof(signal.stock));

    hft::proto::TaggedMessage *out = owned;
    if (out == nullptr) {
      out = pool_.acquire();
      if (out == nullptr) {
        return;
      }
    }
    strategy::encode_signal(signal, *out);
    signal_queue_.push(out);
    signals_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<MarketCap> &market_queue_;
  hft::MPMC<SignalCap> &signal_queue_;
  hft::mem::TaggedPool<PoolCap> &pool_;
  std::int32_t band_bps_{strategy::kDefaultBandBps};
  std::int32_t ref_price_{0};
  std::atomic<std::uint32_t> next_token_{1};
  std::atomic<std::uint64_t> signals_{0};
};

template <int MarketCap, int SignalCap, std::size_t PoolCap>
class MtbtProducer {
public:
  MtbtProducer(hft::MPMC<MarketCap> &market_queue,
               hft::MPMC<SignalCap> &signal_queue,
               hft::mem::TaggedPool<PoolCap> &pool,
               std::int32_t band_bps = strategy::kDefaultBandBps) noexcept
      : market_queue_(market_queue), signal_queue_(signal_queue), pool_(pool),
        band_bps_(band_bps) {}

  [[nodiscard]] std::uint64_t signals_emitted() const noexcept {
    return signals_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage *msg = nullptr;
    if (!market_queue_.try_pop(msg) || msg == nullptr ||
        msg->kind != hft::proto::Kind::mtbt) {
      if (msg != nullptr) {
        pool_.release(msg);
      }
      return;
    }

    hft::proto::MtbtNewOrder tick{};
    std::memcpy(&tick, msg->bytes.data(), sizeof(tick));
    on_tick(tick, msg);
  }

  void on_tick(const hft::proto::MtbtNewOrder &tick,
               hft::proto::TaggedMessage *owned = nullptr) noexcept {
    ref_price_ = strategy::update_ref_price(ref_price_, tick.price);
    const hft::proto::Side side =
        strategy::evaluate(ref_price_, tick.price, band_bps_);
    if (side == hft::proto::Side::none) {
      if (owned != nullptr) {
        pool_.release(owned);
      }
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

    hft::proto::TaggedMessage *out = owned;
    if (out == nullptr) {
      out = pool_.acquire();
      if (out == nullptr) {
        return;
      }
    }
    strategy::encode_signal(signal, *out);
    signal_queue_.push(out);
    signals_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<MarketCap> &market_queue_;
  hft::MPMC<SignalCap> &signal_queue_;
  hft::mem::TaggedPool<PoolCap> &pool_;
  std::int32_t band_bps_{strategy::kDefaultBandBps};
  std::int32_t ref_price_{0};
  std::atomic<std::uint32_t> next_token_{1};
  std::atomic<std::uint64_t> signals_{0};
};

template <int MarketCap, int SignalCap, std::size_t PoolCap>
class SbeBboProducer {
public:
  SbeBboProducer(hft::MPMC<MarketCap> &market_queue,
                 hft::MPMC<SignalCap> &signal_queue,
                 hft::mem::TaggedPool<PoolCap> &pool,
                 std::int32_t band_bps = strategy::kDefaultBandBps) noexcept
      : market_queue_(market_queue), signal_queue_(signal_queue), pool_(pool),
        band_bps_(band_bps) {}

  [[nodiscard]] std::uint64_t signals_emitted() const noexcept {
    return signals_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage *msg = nullptr;
    if (!market_queue_.try_pop(msg) || msg == nullptr ||
        msg->kind != hft::proto::Kind::sbe_market) {
      if (msg != nullptr) {
        pool_.release(msg);
      }
      return;
    }

    hft::proto::sbe::BestObRpi tick{};
    std::memcpy(&tick, msg->bytes.data(), sizeof(tick));
    on_tick(tick, msg);
  }

  void on_tick(const hft::proto::sbe::BestObRpi &tick,
               hft::proto::TaggedMessage *owned = nullptr) noexcept {
    const std::int32_t mid = static_cast<std::int32_t>(hft::proto::sbe::apply_exponent(
        (tick.body.bid_normal_price + tick.body.ask_normal_price) / 2,
        tick.body.price_exponent));
    ref_price_ = strategy::update_ref_price(ref_price_, mid);
    const hft::proto::Side side =
        strategy::evaluate(ref_price_, mid, band_bps_);
    if (side == hft::proto::Side::none) {
      if (owned != nullptr) {
        pool_.release(owned);
      }
      return;
    }

    hft::proto::StrategySignal signal{};
    signal.feed = hft::proto::Kind::sbe_market;
    signal.side = side;
    signal.price = mid;
    signal.quantity = static_cast<std::int32_t>(hft::proto::sbe::apply_exponent(
        tick.body.bid_normal_size, tick.body.size_exponent));
    signal.order_token = next_token_.fetch_add(1, std::memory_order_relaxed);
    signal.token = static_cast<std::int32_t>(tick.body.u);
    const std::size_t sym_len =
        std::min<std::size_t>(tick.symbol_len, sizeof(signal.symbol) - 1);
    std::memcpy(signal.symbol, tick.symbol, sym_len);
    signal.symbol[sym_len] = '\0';

    hft::proto::TaggedMessage *out = owned;
    if (out == nullptr) {
      out = pool_.acquire();
      if (out == nullptr) {
        return;
      }
    }
    strategy::encode_signal(signal, *out);
    signal_queue_.push(out);
    signals_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<MarketCap> &market_queue_;
  hft::MPMC<SignalCap> &signal_queue_;
  hft::mem::TaggedPool<PoolCap> &pool_;
  std::int32_t band_bps_{strategy::kDefaultBandBps};
  std::int32_t ref_price_{0};
  std::atomic<std::uint32_t> next_token_{1};
  std::atomic<std::uint64_t> signals_{0};
};

template <int MarketCap, int SignalCap, std::size_t PoolCap>
class McxTobProducer {
public:
  McxTobProducer(hft::MPMC<MarketCap> &market_queue,
                 hft::MPMC<SignalCap> &signal_queue,
                 hft::mem::TaggedPool<PoolCap> &pool,
                 std::int32_t band_bps = strategy::kDefaultBandBps) noexcept
      : market_queue_(market_queue), signal_queue_(signal_queue), pool_(pool),
        band_bps_(band_bps) {}

  [[nodiscard]] std::uint64_t signals_emitted() const noexcept {
    return signals_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage *msg = nullptr;
    if (!market_queue_.try_pop(msg) || msg == nullptr ||
        msg->kind != hft::proto::Kind::mcx_market) {
      if (msg != nullptr) {
        pool_.release(msg);
      }
      return;
    }

    hft::proto::mcx::TopOfBook tick{};
    std::memcpy(&tick, msg->bytes.data(), sizeof(tick));
    on_tick(tick, msg);
  }

  void on_tick(const hft::proto::mcx::TopOfBook &tick,
               hft::proto::TaggedMessage *owned = nullptr) noexcept {
    const std::int32_t mid =
        static_cast<std::int32_t>((tick.bid_price + tick.ask_price) / 2);
    ref_price_ = strategy::update_ref_price(ref_price_, mid);
    const hft::proto::Side side =
        strategy::evaluate(ref_price_, mid, band_bps_);
    if (side == hft::proto::Side::none) {
      if (owned != nullptr) {
        pool_.release(owned);
      }
      return;
    }

    hft::proto::StrategySignal signal{};
    signal.feed = hft::proto::Kind::mcx_market;
    signal.side = side;
    signal.price = mid;
    signal.quantity = static_cast<std::int32_t>(tick.bid_qty);
    signal.order_token = next_token_.fetch_add(1, std::memory_order_relaxed);
    signal.token = static_cast<std::int32_t>(tick.simple_security_id);
    const std::size_t sym_len =
        std::min<std::size_t>(sizeof(signal.symbol) - 1,
                              strnlen(tick.symbol, sizeof(tick.symbol)));
    std::memcpy(signal.symbol, tick.symbol, sym_len);
    signal.symbol[sym_len] = '\0';

    hft::proto::TaggedMessage *out = owned;
    if (out == nullptr) {
      out = pool_.acquire();
      if (out == nullptr) {
        return;
      }
    }
    strategy::encode_signal(signal, *out);
    signal_queue_.push(out);
    signals_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<MarketCap> &market_queue_;
  hft::MPMC<SignalCap> &signal_queue_;
  hft::mem::TaggedPool<PoolCap> &pool_;
  std::int32_t band_bps_{strategy::kDefaultBandBps};
  std::int32_t ref_price_{0};
  std::atomic<std::uint32_t> next_token_{1};
  std::atomic<std::uint64_t> signals_{0};
};

} // namespace hft::trading
