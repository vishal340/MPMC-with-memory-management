#pragma once

#include <mpmc/mpmc.hpp>
#include <proto/protocols.hpp>
#include <trading/strategy.hpp>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace hft::trading {

template <int SignalCap, int PendingCap>
class OuchConsumer {
public:
  OuchConsumer(hft::MPMC<SignalCap> &signal_queue,
               hft::MPMC<PendingCap> &pending_queue) noexcept
      : signal_queue_(signal_queue), pending_queue_(pending_queue) {}

  [[nodiscard]] std::uint64_t orders_submitted() const noexcept {
    return submitted_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage msg{};
    if (!signal_queue_.try_pop(msg) || msg.kind != hft::proto::Kind::signal) {
      return;
    }

    const hft::proto::StrategySignal signal = strategy::decode_signal(msg);
    if (signal.feed != hft::proto::Kind::itch ||
        signal.side == hft::proto::Side::none) {
      return;
    }

    hft::proto::OuchEnterOrder order{};
    order.message_type = hft::proto::OuchEnterOrder::kType;
    order.order_token = signal.order_token;
    order.buy_sell_indicator = signal.side == hft::proto::Side::buy ? 'B' : 'S';
    order.shares = static_cast<std::uint32_t>(signal.quantity);
    order.price = static_cast<std::uint32_t>(signal.price);
    order.time_in_force = 0;
    std::memcpy(order.stock, signal.stock, sizeof(order.stock));

    hft::proto::TaggedMessage out{};
    out.kind = hft::proto::Kind::ouch;
    std::memcpy(out.bytes.data(), &order, sizeof(order));
    pending_queue_.push(out);
    submitted_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<SignalCap> &signal_queue_;
  hft::MPMC<PendingCap> &pending_queue_;
  std::atomic<std::uint64_t> submitted_{0};
};

template <int SignalCap, int PendingCap>
class NnfConsumer {
public:
  NnfConsumer(hft::MPMC<SignalCap> &signal_queue,
              hft::MPMC<PendingCap> &pending_queue,
              std::int32_t trader_id = 1) noexcept
      : signal_queue_(signal_queue), pending_queue_(pending_queue),
        trader_id_(trader_id) {}

  [[nodiscard]] std::uint64_t orders_submitted() const noexcept {
    return submitted_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage msg{};
    if (!signal_queue_.try_pop(msg) || msg.kind != hft::proto::Kind::signal) {
      return;
    }

    const hft::proto::StrategySignal signal = strategy::decode_signal(msg);
    if (signal.feed != hft::proto::Kind::mtbt ||
        signal.side == hft::proto::Side::none) {
      return;
    }

    hft::proto::NnfOrderEntry order{};
    order.transcode = hft::proto::NnfOrderEntry::kTranscodeBoardLotInTr;
    order.trader_id = trader_id_;
    order.buy_sell = signal.side == hft::proto::Side::buy ? 1 : 2;
    order.volume = signal.quantity;
    order.price = signal.price;
    order.book_type = 1;
    std::memcpy(order.sec_info.symbol, signal.symbol, sizeof(signal.symbol));
    std::memcpy(order.sec_info.series, signal.series, sizeof(signal.series));
    if (signal.token != 0) {
      order.transaction_id = signal.token;
    }

    hft::proto::TaggedMessage out{};
    out.kind = hft::proto::Kind::nnf;
    std::memcpy(out.bytes.data(), &order, sizeof(order));
    pending_queue_.push(out);
    submitted_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<SignalCap> &signal_queue_;
  hft::MPMC<PendingCap> &pending_queue_;
  std::int32_t trader_id_{1};
  std::atomic<std::uint64_t> submitted_{0};
};

template <int SignalCap, int PendingCap>
class SbeOrderEntryConsumer {
public:
  SbeOrderEntryConsumer(hft::MPMC<SignalCap> &signal_queue,
                        hft::MPMC<PendingCap> &pending_queue,
                        std::int64_t symbol_id = 1) noexcept
      : signal_queue_(signal_queue), pending_queue_(pending_queue),
        symbol_id_(symbol_id) {}

  [[nodiscard]] std::uint64_t orders_submitted() const noexcept {
    return submitted_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage msg{};
    if (!signal_queue_.try_pop(msg) || msg.kind != hft::proto::Kind::signal) {
      return;
    }

    const hft::proto::StrategySignal signal = strategy::decode_signal(msg);
    if (signal.feed != hft::proto::Kind::sbe_market ||
        signal.side == hft::proto::Side::none) {
      return;
    }

    hft::proto::sbe::CreateOrderReqV5 order{};
    order.category = hft::proto::sbe::CategoryType::spot;
    order.symbol_id = symbol_id_;
    order.side = signal.side == hft::proto::Side::buy
                     ? hft::proto::sbe::SideType::buy
                     : hft::proto::sbe::SideType::sell;
    order.order_type = hft::proto::sbe::OrderType::limit;
    order.qty.mantissa = signal.quantity;
    order.qty.exponent = 0;
    order.price.mantissa = signal.price;
    order.price.exponent = 0;
    order.time_in_force = hft::proto::sbe::TimeInForceType::good_till_cancel;
    std::snprintf(order.order_link_id, sizeof(order.order_link_id), "lnk%u",
                  signal.order_token);

    hft::proto::TaggedMessage out{};
    out.kind = hft::proto::Kind::sbe_order_entry;
    std::memcpy(out.bytes.data(), &order, sizeof(order));
    pending_queue_.push(out);
    submitted_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<SignalCap> &signal_queue_;
  hft::MPMC<PendingCap> &pending_queue_;
  std::int64_t symbol_id_{1};
  std::atomic<std::uint64_t> submitted_{0};
};

} // namespace hft::trading
