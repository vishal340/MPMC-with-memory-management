#include <core/arch.hpp>
#include <trading/consumer.hpp>
#include <trading/gateway.hpp>
#include <feed/feed_reader.hpp>
#include <feed/multicast_receiver.hpp>
#include <feed/multicast_simulator.hpp>
#include <feed/frame.hpp>
#include <codec/lz4_codec.hpp>
#include <mem/memory_pool.hpp>
#include <mpmc/mpmc_runtime.hpp>
#include <proto/mtbt_decode.hpp>
#include <trading/producer.hpp>
#include <proto/protocols.hpp>
#include <proto/sbe/decode.hpp>
#include <proto/mcx/decode.hpp>
#include <feed/shared_inlet_map.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <new>
#include <thread>

namespace {

constexpr int kQueueCap = 4096;
constexpr std::size_t kPoolCap = 16384;

using ItchProducer = hft::trading::ItchProducer<kQueueCap, kQueueCap, kPoolCap>;
using MtbtProducer = hft::trading::MtbtProducer<kQueueCap, kQueueCap, kPoolCap>;
using McxProducer = hft::trading::McxTobProducer<kQueueCap, kQueueCap, kPoolCap>;
using OuchConsumer = hft::trading::OuchConsumer<kQueueCap, kQueueCap, kPoolCap>;
using NnfConsumer = hft::trading::NnfConsumer<kQueueCap, kQueueCap, kPoolCap>;
using McxConsumer = hft::trading::McxOrderConsumer<kQueueCap, kQueueCap, kPoolCap>;
using Gateway = hft::trading::ExchangeGateway<kQueueCap, kQueueCap, kPoolCap>;
using SbeProducer = hft::trading::SbeBboProducer<kQueueCap, kQueueCap, kPoolCap>;
using SbeConsumer =
    hft::trading::SbeOrderEntryConsumer<kQueueCap, kQueueCap, kPoolCap>;

void append_itch_add_order(std::array<std::byte, hft::frame::kCapacity> &frame,
                           std::size_t &offset, std::uint16_t stock_locate,
                           std::uint32_t shares, std::uint32_t price) {
  hft::proto::ItchAddOrder body{};
  body.stock_locate = stock_locate;
  body.shares = shares;
  body.price = price;
  body.side = 'B';
  std::memcpy(body.stock, "AAPL    ", 8);

  const auto body_len =
      static_cast<std::uint16_t>(hft::proto::ItchAddOrder::kWireSize);
  frame[offset++] = static_cast<std::byte>(body_len >> 8);
  frame[offset++] = static_cast<std::byte>(body_len & 0xFF);
  std::memcpy(frame.data() + offset, &body, sizeof(body));
  offset += sizeof(body);
}

void append_mtbt_order(std::array<std::byte, hft::frame::kCapacity> &frame,
                       std::size_t &offset, std::int16_t stream_id,
                       std::int32_t token, std::int32_t quantity,
                       std::int32_t price) {
  hft::proto::MtbtStreamHeader header{};
  header.msg_len = static_cast<std::int16_t>(hft::proto::kMtbtOrderWireSize);
  header.stream_id = stream_id;
  header.seq_no = static_cast<std::int32_t>(offset);

  hft::proto::MtbtNewOrder body{};
  body.token = token;
  body.quantity = quantity;
  body.price = price;
  body.order_type = 'B';

  std::memcpy(frame.data() + offset, &header, sizeof(header));
  offset += sizeof(header);
  std::memcpy(frame.data() + offset, &body, sizeof(body));
  offset += sizeof(body);
}

void route_market_tick(hft::proto::TaggedMessage *msg, ItchProducer &itch,
                       MtbtProducer &mtbt, McxProducer &mcx,
                       hft::mem::TaggedPool<kPoolCap> &pool) {
  if (msg == nullptr) {
    return;
  }
  if (msg->kind == hft::proto::Kind::itch) {
    hft::proto::ItchAddOrder tick{};
    std::memcpy(&tick, msg->bytes.data(), sizeof(tick));
    itch.on_tick(tick, msg);
    return;
  }
  if (msg->kind == hft::proto::Kind::mtbt) {
    hft::proto::MtbtNewOrder tick{};
    std::memcpy(&tick, msg->bytes.data(), sizeof(tick));
    mtbt.on_tick(tick, msg);
    return;
  }
  if (msg->kind == hft::proto::Kind::mcx_market) {
    hft::proto::mcx::TopOfBook tick{};
    std::memcpy(&tick, msg->bytes.data(), sizeof(tick));
    mcx.on_tick(tick, msg);
    return;
  }
  pool.release(msg);
}

void drain_signals(OuchConsumer &ouch, NnfConsumer &nnf, McxConsumer &mcx,
                   std::size_t rounds) {
  for (std::size_t i = 0; i < rounds; ++i) {
    ouch.poll_once();
    nnf.poll_once();
    mcx.poll_once();
  }
}

hft::proto::mcx::TopOfBook make_mcx_tob(std::int64_t bid, std::int64_t ask,
                                        std::uint32_t security_id);

void publish_feed_frames(hft::feed::SharedInlet &inlet,
                         hft::feed::MulticastSimulator *simulator) {
  std::array<std::byte, hft::frame::kCapacity> itch_frame{};
  std::size_t itch_len = 0;
  append_itch_add_order(itch_frame, itch_len, 10, 100, 10000);
  append_itch_add_order(itch_frame, itch_len, 20, 100, 9980);

  std::array<std::byte, hft::frame::kCapacity> mtbt_frame{};
  std::size_t mtbt_len = 0;
  append_mtbt_order(mtbt_frame, mtbt_len, 1, 111, 10, 250000);
  append_mtbt_order(mtbt_frame, mtbt_len, 2, 222, 10, 251000);

  const auto mcx_ref = make_mcx_tob(7200000, 7200100, 5001);
  const auto mcx_buy = make_mcx_tob(7100000, 7100100, 5001);

  if (simulator != nullptr) {
    simulator->publish_itch(itch_frame.data(), itch_len);
    simulator->publish_mtbt(mtbt_frame.data(), mtbt_len);
    simulator->publish_mcx(reinterpret_cast<const std::byte *>(&mcx_ref),
                           sizeof(mcx_ref));
    simulator->publish_mcx(reinterpret_cast<const std::byte *>(&mcx_buy),
                           sizeof(mcx_buy));
    return;
  }

  const auto publish_and_wait = [&](hft::feed::Kind kind,
                                    hft::feed::InletFlags flags,
                                    const std::byte *data, std::size_t len,
                                    std::uint32_t uncompressed_len = 0) {
    hft::feed::publish(inlet, kind, flags, data, len, uncompressed_len);
    const std::uint64_t sequence =
        inlet.sequence.load(std::memory_order_acquire);
    hft::feed::wait_until_processed(inlet, sequence);
  };

  publish_and_wait(hft::feed::Kind::itch, hft::feed::InletFlags::none,
                   itch_frame.data(), itch_len);
  publish_and_wait(hft::feed::Kind::mtbt, hft::feed::InletFlags::none,
                   mtbt_frame.data(), mtbt_len);
  publish_and_wait(hft::feed::Kind::mcx, hft::feed::InletFlags::none,
                   reinterpret_cast<const std::byte *>(&mcx_ref),
                   sizeof(mcx_ref));
  publish_and_wait(hft::feed::Kind::mcx, hft::feed::InletFlags::none,
                   reinterpret_cast<const std::byte *>(&mcx_buy),
                   sizeof(mcx_buy));
}

void drain_gateway(Gateway &gateway, std::size_t rounds) {
  for (std::size_t i = 0; i < rounds; ++i) {
    gateway.poll_once();
  }
}

hft::proto::mcx::TopOfBook make_mcx_tob(std::int64_t bid, std::int64_t ask,
                                        std::uint32_t security_id) {
  hft::proto::mcx::TopOfBook tick{};
  tick.simple_security_id = security_id;
  tick.bid_price = bid;
  tick.ask_price = ask;
  tick.bid_qty = 10;
  tick.ask_qty = 10;
  const char *sym = "GOLD";
  std::memcpy(tick.symbol, sym, 4);
  return tick;
}

hft::proto::sbe::BestObRpi make_sbe_bbo(std::int64_t bid, std::int64_t ask,
                                        std::int8_t price_exp) {
  hft::proto::sbe::BestObRpi tick{};
  tick.body.bid_normal_price = bid;
  tick.body.ask_normal_price = ask;
  tick.body.bid_normal_size = 100;
  tick.body.ask_normal_size = 100;
  tick.body.price_exponent = price_exp;
  tick.body.size_exponent = 0;
  tick.body.u = 1;
  const char *sym = "BTCUSDT";
  tick.symbol_len = 7;
  std::memcpy(tick.symbol, sym, tick.symbol_len);
  return tick;
}

} // namespace

int main() {
  std::printf("target arch: %s (x86_64=%d arm64=%d page=%zu)\n",
              hft::arch::name, hft::arch::is_x86_64, hft::arch::is_arm64,
              hft::arch::page_size);

  hft::mem::TaggedPool<kPoolCap> pool;
  hft::MPMC<kQueueCap> market_queue;
  hft::MPMC<kQueueCap> signal_queue;
  hft::MPMC<kQueueCap> pending_queue;
  hft::MPMC<kQueueCap> confirm_queue;

  hft::os::Region inlet_region;
  hft::feed::SharedInlet *inlet = hft::feed::map_inlet(inlet_region);
  new (inlet) hft::feed::SharedInlet{};

  hft::feed::Reader<kPoolCap, kQueueCap> reader;
  hft::feed::MulticastHub multicast_hub;
  hft::feed::MulticastSimulator multicast_sim;

  reader.start(inlet, pool, market_queue);

  const bool multicast_ok =
      multicast_hub.start(inlet) && multicast_sim.start();

  if (multicast_ok) {
    publish_feed_frames(*inlet, &multicast_sim);
    for (int i = 0; i < 100; ++i) {
      if (multicast_hub.datagrams(hft::feed::MulticastChannel::itch) >= 1 &&
          multicast_hub.datagrams(hft::feed::MulticastChannel::mtbt) >= 1 &&
          multicast_hub.datagrams(hft::feed::MulticastChannel::mcx) >= 2) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  const bool multicast_received =
      multicast_ok &&
      multicast_hub.datagrams(hft::feed::MulticastChannel::itch) >= 1 &&
      multicast_hub.datagrams(hft::feed::MulticastChannel::mtbt) >= 1 &&
      multicast_hub.datagrams(hft::feed::MulticastChannel::mcx) >= 2;

  if (multicast_ok) {
    multicast_hub.stop();
  }

  if (!multicast_received) {
    publish_feed_frames(*inlet, nullptr);
  }

  std::array<std::byte, hft::frame::kCapacity> mtbt_plain{};
  std::size_t mtbt_plain_len = 0;
  append_mtbt_order(mtbt_plain, mtbt_plain_len, 3, 333, 10, 249000);

  std::array<std::byte, hft::frame::kCapacity> compressed{};
  std::size_t compressed_len = 0;
  if (!hft::lz4::compress(mtbt_plain.data(), mtbt_plain_len, compressed.data(),
                          compressed.size(), compressed_len)) {
    reader.stop();
    multicast_hub.stop();
    return 2;
  }

  hft::feed::publish(*inlet, hft::feed::Kind::mtbt,
                     hft::feed::InletFlags::lz4_compressed, compressed.data(),
                     compressed_len,
                     static_cast<std::uint32_t>(mtbt_plain_len));

  const std::uint64_t expected_sequences =
      inlet->sequence.load(std::memory_order_acquire);
  hft::feed::wait_until_processed(*inlet, expected_sequences);

  std::printf(
      "multicast ok: enabled=%d itch=%llu mtbt=%llu mcx=%llu sequences=%llu\n",
      multicast_ok ? 1 : 0,
      static_cast<unsigned long long>(
          multicast_hub.datagrams(hft::feed::MulticastChannel::itch)),
      static_cast<unsigned long long>(
          multicast_hub.datagrams(hft::feed::MulticastChannel::mtbt)),
      static_cast<unsigned long long>(
          multicast_hub.datagrams(hft::feed::MulticastChannel::mcx)),
      static_cast<unsigned long long>(expected_sequences));

  if (multicast_ok) {
    multicast_sim.stop();
  }
  reader.stop();

  ItchProducer itch_producer(market_queue, signal_queue, pool);
  MtbtProducer mtbt_producer(market_queue, signal_queue, pool);
  McxProducer mcx_producer(market_queue, signal_queue, pool);
  OuchConsumer ouch_consumer(signal_queue, pending_queue, pool);
  NnfConsumer nnf_consumer(signal_queue, pending_queue, pool);
  McxConsumer mcx_consumer(signal_queue, pending_queue, pool, 1, 5001);
  Gateway gateway(pending_queue, confirm_queue, pool);

  for (std::size_t i = 0; i < 32; ++i) {
    hft::proto::TaggedMessage *market_msg = nullptr;
    if (!market_queue.try_pop(market_msg)) {
      break;
    }
    route_market_tick(market_msg, itch_producer, mtbt_producer, mcx_producer,
                      pool);
  }

  drain_signals(ouch_consumer, nnf_consumer, mcx_consumer, 32);
  drain_gateway(gateway, 32);

  std::size_t ouch_confirmed = 0;
  std::size_t nnf_confirmed = 0;
  std::size_t mcx_confirmed = 0;
  for (std::size_t i = 0; i < 32; ++i) {
    hft::proto::TaggedMessage *confirm = nullptr;
    if (!confirm_queue.try_pop(confirm) || confirm == nullptr) {
      break;
    }
    if (confirm->kind == hft::proto::Kind::ouch_confirm) {
      ++ouch_confirmed;
    } else if (confirm->kind == hft::proto::Kind::nnf_confirm) {
      ++nnf_confirmed;
    } else if (confirm->kind == hft::proto::Kind::mcx_confirm) {
      ++mcx_confirmed;
    }
    pool.release(confirm);
  }

  std::printf(
      "strategy ok: itch=%llu mtbt=%llu mcx=%llu "
      "ouch_sub=%llu nnf_sub=%llu mcx_sub=%llu "
      "ouch_conf=%zu nnf_conf=%zu mcx_conf=%zu\n",
      static_cast<unsigned long long>(itch_producer.signals_emitted()),
      static_cast<unsigned long long>(mtbt_producer.signals_emitted()),
      static_cast<unsigned long long>(mcx_producer.signals_emitted()),
      static_cast<unsigned long long>(ouch_consumer.orders_submitted()),
      static_cast<unsigned long long>(nnf_consumer.orders_submitted()),
      static_cast<unsigned long long>(mcx_consumer.orders_submitted()),
      ouch_confirmed, nnf_confirmed, mcx_confirmed);

  hft::MPMC<kQueueCap> sbe_market_queue;
  hft::MPMC<kQueueCap> sbe_signal_queue;
  hft::MPMC<kQueueCap> sbe_pending_queue;
  hft::MPMC<kQueueCap> sbe_confirm_queue;

  SbeProducer sbe_producer(sbe_market_queue, sbe_signal_queue, pool);
  SbeConsumer sbe_order_consumer(sbe_signal_queue, sbe_pending_queue, pool,
                                 1001);
  Gateway sbe_gateway(sbe_pending_queue, sbe_confirm_queue, pool);

  const auto bbo_ref = make_sbe_bbo(6500000, 6500100, -2);
  const auto bbo_buy = make_sbe_bbo(6400000, 6400100, -2);

  if (auto *bbo_msg = pool.acquire_filled(bbo_ref)) {
    sbe_market_queue.push(bbo_msg);
  }
  if (auto *bbo_msg2 = pool.acquire_filled(bbo_buy)) {
    sbe_market_queue.push(bbo_msg2);
  }

  for (std::size_t i = 0; i < 8; ++i) {
    hft::proto::TaggedMessage *tick = nullptr;
    if (!sbe_market_queue.try_pop(tick) || tick == nullptr) {
      break;
    }
    if (tick->kind == hft::proto::Kind::sbe_market) {
      hft::proto::sbe::BestObRpi decoded{};
      std::memcpy(&decoded, tick->bytes.data(), sizeof(decoded));
      sbe_producer.on_tick(decoded, tick);
    } else {
      pool.release(tick);
    }
  }

  for (std::size_t i = 0; i < 8; ++i) {
    sbe_order_consumer.poll_once();
  }
  drain_gateway(sbe_gateway, 8);

  std::size_t sbe_confirmed = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    hft::proto::TaggedMessage *confirm = nullptr;
    if (!sbe_confirm_queue.try_pop(confirm) || confirm == nullptr) {
      break;
    }
    if (confirm->kind == hft::proto::Kind::sbe_fast_order) {
      ++sbe_confirmed;
    }
    pool.release(confirm);
  }

  std::array<std::byte, 512> wire{};
  const std::size_t wire_len = hft::proto::sbe::encode_create_order_req(
      [] {
        hft::proto::sbe::CreateOrderReqV5 req{};
        req.symbol_id = 1001;
        req.side = hft::proto::sbe::SideType::buy;
        req.qty.mantissa = 10;
        req.price.mantissa = 65000;
        return req;
      }(),
      wire.data(), wire.size());

  hft::proto::sbe::CreateOrderReqV5 decoded_req{};
  const bool req_ok = hft::proto::sbe::decode_create_order_req(
      wire.data(), wire_len, decoded_req);

  std::printf(
      "sbe ok: bbo_signals=%llu submitted=%llu confirmed=%zu req_decode=%d "
      "(ws/mmws path, not multicast)\n",
      static_cast<unsigned long long>(sbe_producer.signals_emitted()),
      static_cast<unsigned long long>(sbe_order_consumer.orders_submitted()),
      sbe_confirmed, req_ok ? 1 : 0);

  std::array<std::byte, 256> mcx_wire{};
  hft::proto::mcx::NewOrderSingleShort mcx_req{};
  mcx_req.simple_security_id = 5001;
  mcx_req.price = 7200000;
  mcx_req.order_qty = 10;
  mcx_req.cl_ord_id = 42;
  const std::size_t mcx_wire_len = hft::proto::mcx::encode_new_order_short(
      mcx_req, mcx_wire.data(), mcx_wire.size());

  hft::proto::mcx::NewOrderSingleShort mcx_decoded{};
  const bool mcx_req_ok = hft::proto::mcx::decode_new_order_short(
      mcx_wire.data(), mcx_wire_len, mcx_decoded);

  std::printf("mcx wire ok: req_decode=%d template_id=%u\n", mcx_req_ok ? 1 : 0,
              static_cast<unsigned>(mcx_decoded.template_id));

  hft::mpmc::Runtime<kQueueCap, kPoolCap> runtime(market_queue, pool);
  runtime.start(12345);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  runtime.stop();

  std::printf("mpmc runtime ok: pushed=%llu popped=%llu live=%zu\n",
              static_cast<unsigned long long>(runtime.pushed()),
              static_cast<unsigned long long>(runtime.popped()),
              runtime.live_threads());
  return 0;
}
