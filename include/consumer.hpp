#pragma once

#include <mpmc.hpp>
#include <protocols.hpp>
#include <strategy.hpp>

#include <atomic>
#include <cstring>

namespace hft::trading {

template <int SignalCap, int OrderCap>
class OuchConsumer {
public:
  OuchConsumer(hft::MPMC<SignalCap> &signal_queue,
               hft::MPMC<OrderCap> &order_queue) noexcept
      : signal_queue_(signal_queue), order_queue_(order_queue) {}

  [[nodiscard]] std::uint64_t orders_sent() const noexcept {
    return orders_.load(std::memory_order_acquire);
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
    order_queue_.push(out);
    orders_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<SignalCap> &signal_queue_;
  hft::MPMC<OrderCap> &order_queue_;
  std::atomic<std::uint64_t> orders_{0};
};

template <int SignalCap, int OrderCap>
class NnfConsumer {
public:
  NnfConsumer(hft::MPMC<SignalCap> &signal_queue,
              hft::MPMC<OrderCap> &order_queue,
              std::int32_t trader_id = 1) noexcept
      : signal_queue_(signal_queue), order_queue_(order_queue),
        trader_id_(trader_id) {}

  [[nodiscard]] std::uint64_t orders_sent() const noexcept {
    return orders_.load(std::memory_order_acquire);
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
    order_queue_.push(out);
    orders_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<SignalCap> &signal_queue_;
  hft::MPMC<OrderCap> &order_queue_;
  std::int32_t trader_id_{1};
  std::atomic<std::uint64_t> orders_{0};
};

} // namespace hft::trading
