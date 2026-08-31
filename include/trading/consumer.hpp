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

template <int SignalCap, int PendingCap, std::size_t PoolCap>
class OuchConsumer {
public:
  OuchConsumer(hft::MPMC<SignalCap> &signal_queue,
               hft::MPMC<PendingCap> &pending_queue,
               hft::mem::TaggedPool<PoolCap> &pool) noexcept
      : signal_queue_(signal_queue), pending_queue_(pending_queue), pool_(pool) {
  }

  [[nodiscard]] std::uint64_t orders_submitted() const noexcept {
    return submitted_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage *msg = nullptr;
    if (!signal_queue_.try_pop(msg) || msg == nullptr ||
        msg->kind != hft::proto::Kind::signal) {
      if (msg != nullptr) {
        pool_.release(msg);
      }
      return;
    }

    const hft::proto::StrategySignal signal = strategy::decode_signal(*msg);
    if (signal.feed != hft::proto::Kind::itch) {
      signal_queue_.push(msg);
      return;
    }
    if (signal.side == hft::proto::Side::none) {
      pool_.release(msg);
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

    msg->kind = hft::proto::Kind::ouch;
    std::memcpy(msg->bytes.data(), &order, sizeof(order));
    pending_queue_.push(msg);
    submitted_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<SignalCap> &signal_queue_;
  hft::MPMC<PendingCap> &pending_queue_;
  hft::mem::TaggedPool<PoolCap> &pool_;
  std::atomic<std::uint64_t> submitted_{0};
};

template <int SignalCap, int PendingCap, std::size_t PoolCap>
class NnfConsumer {
public:
  NnfConsumer(hft::MPMC<SignalCap> &signal_queue,
              hft::MPMC<PendingCap> &pending_queue,
              hft::mem::TaggedPool<PoolCap> &pool,
              std::int32_t trader_id = 1) noexcept
      : signal_queue_(signal_queue), pending_queue_(pending_queue), pool_(pool),
        trader_id_(trader_id) {}

  [[nodiscard]] std::uint64_t orders_submitted() const noexcept {
    return submitted_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage *msg = nullptr;
    if (!signal_queue_.try_pop(msg) || msg == nullptr ||
        msg->kind != hft::proto::Kind::signal) {
      if (msg != nullptr) {
        pool_.release(msg);
      }
      return;
    }

    const hft::proto::StrategySignal signal = strategy::decode_signal(*msg);
    if (signal.feed != hft::proto::Kind::mtbt) {
      signal_queue_.push(msg);
      return;
    }
    if (signal.side == hft::proto::Side::none) {
      pool_.release(msg);
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

    msg->kind = hft::proto::Kind::nnf;
    std::memcpy(msg->bytes.data(), &order, sizeof(order));
    pending_queue_.push(msg);
    submitted_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<SignalCap> &signal_queue_;
  hft::MPMC<PendingCap> &pending_queue_;
  hft::mem::TaggedPool<PoolCap> &pool_;
  std::int32_t trader_id_{1};
  std::atomic<std::uint64_t> submitted_{0};
};

template <int SignalCap, int PendingCap, std::size_t PoolCap>
class SbeOrderEntryConsumer {
public:
  SbeOrderEntryConsumer(hft::MPMC<SignalCap> &signal_queue,
                        hft::MPMC<PendingCap> &pending_queue,
                        hft::mem::TaggedPool<PoolCap> &pool,
                        std::int64_t symbol_id = 1) noexcept
      : signal_queue_(signal_queue), pending_queue_(pending_queue), pool_(pool),
        symbol_id_(symbol_id) {}

  [[nodiscard]] std::uint64_t orders_submitted() const noexcept {
    return submitted_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage *msg = nullptr;
    if (!signal_queue_.try_pop(msg) || msg == nullptr ||
        msg->kind != hft::proto::Kind::signal) {
      if (msg != nullptr) {
        pool_.release(msg);
      }
      return;
    }

    const hft::proto::StrategySignal signal = strategy::decode_signal(*msg);
    if (signal.feed != hft::proto::Kind::sbe_market) {
      signal_queue_.push(msg);
      return;
    }
    if (signal.side == hft::proto::Side::none) {
      pool_.release(msg);
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

    msg->kind = hft::proto::Kind::sbe_order_entry;
    std::memcpy(msg->bytes.data(), &order, sizeof(order));
    pending_queue_.push(msg);
    submitted_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<SignalCap> &signal_queue_;
  hft::MPMC<PendingCap> &pending_queue_;
  hft::mem::TaggedPool<PoolCap> &pool_;
  std::int64_t symbol_id_{1};
  std::atomic<std::uint64_t> submitted_{0};
};

template <int SignalCap, int PendingCap, std::size_t PoolCap>
class McxOrderConsumer {
public:
  McxOrderConsumer(hft::MPMC<SignalCap> &signal_queue,
                   hft::MPMC<PendingCap> &pending_queue,
                   hft::mem::TaggedPool<PoolCap> &pool,
                   std::uint32_t sender_sub_id = 1,
                   std::uint32_t simple_security_id = 5001) noexcept
      : signal_queue_(signal_queue), pending_queue_(pending_queue), pool_(pool),
        sender_sub_id_(sender_sub_id), simple_security_id_(simple_security_id) {
  }

  [[nodiscard]] std::uint64_t orders_submitted() const noexcept {
    return submitted_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    hft::proto::TaggedMessage *msg = nullptr;
    if (!signal_queue_.try_pop(msg) || msg == nullptr ||
        msg->kind != hft::proto::Kind::signal) {
      if (msg != nullptr) {
        pool_.release(msg);
      }
      return;
    }

    const hft::proto::StrategySignal signal = strategy::decode_signal(*msg);
    if (signal.feed != hft::proto::Kind::mcx_market) {
      signal_queue_.push(msg);
      return;
    }
    if (signal.side == hft::proto::Side::none) {
      pool_.release(msg);
      return;
    }

    hft::proto::mcx::NewOrderSingleShort order{};
    order.msg_seq_num = next_seq_.fetch_add(1, std::memory_order_relaxed);
    order.sender_sub_id = sender_sub_id_;
    order.price = signal.price;
    order.cl_ord_id = signal.order_token;
    order.order_qty = signal.quantity;
    order.disclosed_qty = signal.quantity;
    order.simple_security_id = simple_security_id_;
    order.side = signal.side == hft::proto::Side::buy
                     ? static_cast<std::uint8_t>(hft::proto::mcx::Side::buy)
                     : static_cast<std::uint8_t>(hft::proto::mcx::Side::sell);
    order.time_in_force =
        static_cast<std::uint8_t>(hft::proto::mcx::TimeInForce::day);
    order.strategy_trigger_seq_no =
        static_cast<std::uint64_t>(signal.order_token);
    const std::size_t ucc_len =
        std::min<std::size_t>(sizeof(order.free_text1) - 1,
                              strnlen(signal.symbol, sizeof(signal.symbol)));
    std::memcpy(order.free_text1, signal.symbol, ucc_len);

    msg->kind = hft::proto::Kind::mcx_order;
    std::memcpy(msg->bytes.data(), &order, sizeof(order));
    pending_queue_.push(msg);
    submitted_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<SignalCap> &signal_queue_;
  hft::MPMC<PendingCap> &pending_queue_;
  hft::mem::TaggedPool<PoolCap> &pool_;
  std::uint32_t sender_sub_id_{1};
  std::uint32_t simple_security_id_{5001};
  std::atomic<std::uint32_t> next_seq_{1};
  std::atomic<std::uint64_t> submitted_{0};
};

} // namespace hft::trading
