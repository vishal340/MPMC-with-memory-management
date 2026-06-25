#pragma once

#include <mpmc/mpmc.hpp>
#include <proto/protocols.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace hft::trading {

namespace detail {

inline void format_ouch_token(char (&out)[14], std::uint32_t token) noexcept {
  std::memset(out, '0', sizeof(out));
  char tmp[16]{};
  std::snprintf(tmp, sizeof(tmp), "%u", token);
  const std::size_t len = std::strlen(tmp);
  const std::size_t offset =
      len >= sizeof(out) ? 0 : sizeof(out) - len;
  std::memcpy(out + offset, tmp, std::min(len, sizeof(out)));
}

inline proto::OuchOrderAccepted
make_ouch_confirm(const proto::OuchEnterOrder &req,
                  std::uint64_t order_ref) noexcept {
  proto::OuchOrderAccepted ack{};
  ack.message_type = proto::OuchOrderAccepted::kType;
  format_ouch_token(ack.order_token, req.order_token);
  ack.buy_sell_indicator = req.buy_sell_indicator;
  ack.shares = req.shares;
  std::memcpy(ack.stock, req.stock, sizeof(ack.stock));
  ack.price = req.price;
  ack.time_in_force = req.time_in_force;
  ack.order_reference_number = order_ref;
  ack.order_state = 'A';
  return ack;
}

inline proto::NnfOrderConfirm
make_nnf_confirm(const proto::NnfOrderEntry &req,
                 double order_number) noexcept {
  proto::NnfOrderConfirm ack{};
  ack.transcode = proto::NnfOrderConfirm::kTranscodeOrderConfirmation;
  ack.reason_code = 0;
  ack.token = req.transaction_id;
  ack.order_number = order_number;
  ack.book_type = req.book_type;
  ack.buy_sell = req.buy_sell;
  ack.disclosed_vol = req.disclosed_vol;
  ack.disclosed_vol_remaining = req.disclosed_vol;
  ack.total_vol_remaining = req.volume;
  ack.volume = req.volume;
  ack.price = req.price;
  ack.order_flags = req.order_flags;
  ack.trader_id = req.trader_id;
  return ack;
}

inline proto::sbe::FastOrderResp
make_sbe_fast_confirm(const proto::sbe::CreateOrderReqV5 &req,
                      std::int64_t seq) noexcept {
  proto::sbe::FastOrderResp ack{};
  ack.body.category = static_cast<std::uint8_t>(req.category);
  ack.body.side = static_cast<std::uint8_t>(req.side);
  ack.body.order_status = 1;
  ack.body.price_exponent = req.price.exponent;
  ack.body.size_exponent = req.qty.exponent;
  ack.body.price = req.price.mantissa;
  ack.body.leaves_qty = req.qty.mantissa;
  ack.body.symbol_id = static_cast<std::int32_t>(req.symbol_id);
  ack.body.seq = seq;

  const std::size_t link_len =
      strnlen(req.order_link_id, sizeof(req.order_link_id));
  ack.order_link_id_len =
      static_cast<std::uint8_t>(std::min(link_len, sizeof(ack.order_link_id)));
  std::memcpy(ack.order_link_id, req.order_link_id, ack.order_link_id_len);

  const char *oid = "exch-oid";
  ack.order_id_len = 8;
  std::memcpy(ack.order_id, oid, ack.order_id_len);
  return ack;
}

} // namespace detail

template <int PendingCap, int ConfirmCap>
class ExchangeGateway {
public:
  ExchangeGateway(hft::MPMC<PendingCap> &pending_queue,
                  hft::MPMC<ConfirmCap> &confirm_queue) noexcept
      : pending_queue_(pending_queue), confirm_queue_(confirm_queue) {}

  [[nodiscard]] std::uint64_t confirms_emitted() const noexcept {
    return confirms_.load(std::memory_order_acquire);
  }

  void poll_once() noexcept {
    proto::TaggedMessage pending{};
    if (!pending_queue_.try_pop(pending)) {
      return;
    }

    proto::TaggedMessage confirm{};
    switch (pending.kind) {
    case proto::Kind::ouch: {
      proto::OuchEnterOrder req{};
      std::memcpy(&req, pending.bytes.data(), sizeof(req));
      const auto order_ref =
          next_ouch_ref_.fetch_add(1, std::memory_order_relaxed);
      const proto::OuchOrderAccepted ack =
          detail::make_ouch_confirm(req, order_ref);
      confirm.kind = proto::Kind::ouch_confirm;
      std::memcpy(confirm.bytes.data(), &ack, sizeof(ack));
      break;
    }
    case proto::Kind::nnf: {
      proto::NnfOrderEntry req{};
      std::memcpy(&req, pending.bytes.data(), sizeof(req));
      const double order_number =
          static_cast<double>(next_nnf_order_.fetch_add(
              1, std::memory_order_relaxed));
      const proto::NnfOrderConfirm ack =
          detail::make_nnf_confirm(req, order_number);
      confirm.kind = proto::Kind::nnf_confirm;
      std::memcpy(confirm.bytes.data(), &ack, sizeof(ack));
      break;
    }
    case proto::Kind::sbe_order_entry: {
      proto::sbe::CreateOrderReqV5 req{};
      std::memcpy(&req, pending.bytes.data(), sizeof(req));
      const auto seq = static_cast<std::int64_t>(
          next_sbe_seq_.fetch_add(1, std::memory_order_relaxed));
      const proto::sbe::FastOrderResp ack =
          detail::make_sbe_fast_confirm(req, seq);
      confirm.kind = proto::Kind::sbe_fast_order;
      std::memcpy(confirm.bytes.data(), &ack, sizeof(ack));
      break;
    }
    default:
      return;
    }

    confirm_queue_.push(confirm);
    confirms_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  hft::MPMC<PendingCap> &pending_queue_;
  hft::MPMC<ConfirmCap> &confirm_queue_;
  std::atomic<std::uint64_t> next_ouch_ref_{1};
  std::atomic<std::uint64_t> next_nnf_order_{1};
  std::atomic<std::uint64_t> next_sbe_seq_{1};
  std::atomic<std::uint64_t> confirms_{0};
};

} // namespace hft::trading
