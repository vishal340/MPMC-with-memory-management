#include <core/arch.hpp>
#include <trading/consumer.hpp>
#include <trading/gateway.hpp>
#include <feed/feed_reader.hpp>
#include <feed/frame.hpp>
#include <codec/lz4_codec.hpp>
#include <mem/memory_pool.hpp>
#include <mpmc/mpmc_runtime.hpp>
#include <proto/mtbt_decode.hpp>
#include <trading/producer.hpp>
#include <proto/protocols.hpp>
#include <proto/sbe/decode.hpp>
#include <feed/shared_inlet_map.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <new>
#include <thread>

namespace {

constexpr int kQueueCap = 4096;

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

void route_market_tick(const hft::proto::TaggedMessage &msg,
                       hft::trading::ItchProducer<kQueueCap, kQueueCap> &itch,
                       hft::trading::MtbtProducer<kQueueCap, kQueueCap> &mtbt) {
  if (msg.kind == hft::proto::Kind::itch) {
    hft::proto::ItchAddOrder tick{};
    std::memcpy(&tick, msg.bytes.data(), sizeof(tick));
    itch.on_tick(tick);
    return;
  }
  if (msg.kind == hft::proto::Kind::mtbt) {
    hft::proto::MtbtNewOrder tick{};
    std::memcpy(&tick, msg.bytes.data(), sizeof(tick));
    mtbt.on_tick(tick);
  }
}

void drain_signals(hft::trading::OuchConsumer<kQueueCap, kQueueCap> &ouch,
                   hft::trading::NnfConsumer<kQueueCap, kQueueCap> &nnf,
                   hft::trading::SbeOrderEntryConsumer<kQueueCap, kQueueCap> &sbe,
                   std::size_t rounds) {
  for (std::size_t i = 0; i < rounds; ++i) {
    ouch.poll_once();
    nnf.poll_once();
    sbe.poll_once();
  }
}

void drain_gateway(hft::trading::ExchangeGateway<kQueueCap, kQueueCap> &gateway,
                   std::size_t rounds) {
  for (std::size_t i = 0; i < rounds; ++i) {
    gateway.poll_once();
  }
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

  hft::mem::ProtocolArena<1024, 1024, 1024, 256> arena;
  hft::MPMC<kQueueCap> market_queue;
  hft::MPMC<kQueueCap> signal_queue;
  hft::MPMC<kQueueCap> pending_queue;
  hft::MPMC<kQueueCap> confirm_queue;

  hft::os::Region inlet_region;
  hft::feed::SharedInlet *inlet = hft::feed::map_inlet(inlet_region);
  new (inlet) hft::feed::SharedInlet{};

  hft::feed::Reader<1024, 1024, 1024, 256, kQueueCap> reader;
  reader.start(inlet, arena, market_queue);

  std::array<std::byte, hft::frame::kCapacity> itch_frame{};
  std::size_t itch_len = 0;
  append_itch_add_order(itch_frame, itch_len, 10, 100, 10000);
  append_itch_add_order(itch_frame, itch_len, 20, 100, 9980);

  hft::feed::publish(*inlet, hft::feed::Kind::itch, hft::feed::InletFlags::none,
                     itch_frame.data(), itch_len);
  hft::feed::wait_until_processed(*inlet, 1);

  std::array<std::byte, hft::frame::kCapacity> mtbt_frame{};
  std::size_t mtbt_len = 0;
  append_mtbt_order(mtbt_frame, mtbt_len, 1, 111, 10, 250000);
  append_mtbt_order(mtbt_frame, mtbt_len, 2, 222, 10, 251000);

  hft::feed::publish(*inlet, hft::feed::Kind::mtbt, hft::feed::InletFlags::none,
                     mtbt_frame.data(), mtbt_len);
  hft::feed::wait_until_processed(*inlet, 2);

  std::array<std::byte, hft::frame::kCapacity> mtbt_plain{};
  std::size_t mtbt_plain_len = 0;
  append_mtbt_order(mtbt_plain, mtbt_plain_len, 3, 333, 10, 249000);

  std::array<std::byte, hft::frame::kCapacity> compressed{};
  std::size_t compressed_len = 0;
  if (!hft::lz4::compress(mtbt_plain.data(), mtbt_plain_len, compressed.data(),
                          compressed.size(), compressed_len)) {
    reader.stop();
    return 2;
  }

  hft::feed::publish(*inlet, hft::feed::Kind::mtbt,
                     hft::feed::InletFlags::lz4_compressed, compressed.data(),
                     compressed_len,
                     static_cast<std::uint32_t>(mtbt_plain_len));
  hft::feed::wait_until_processed(*inlet, 3);

  reader.stop();

  hft::trading::ItchProducer<kQueueCap, kQueueCap> itch_producer(market_queue,
                                                                 signal_queue);
  hft::trading::MtbtProducer<kQueueCap, kQueueCap> mtbt_producer(market_queue,
                                                                 signal_queue);
  hft::trading::OuchConsumer<kQueueCap, kQueueCap> ouch_consumer(signal_queue,
                                                                 pending_queue);
  hft::trading::NnfConsumer<kQueueCap, kQueueCap> nnf_consumer(signal_queue,
                                                               pending_queue);
  hft::trading::SbeOrderEntryConsumer<kQueueCap, kQueueCap> sbe_consumer(
      signal_queue, pending_queue, 1001);
  hft::trading::ExchangeGateway<kQueueCap, kQueueCap> gateway(pending_queue,
                                                               confirm_queue);

  for (std::size_t i = 0; i < 16; ++i) {
    hft::proto::TaggedMessage market_msg{};
    if (!market_queue.try_pop(market_msg)) {
      break;
    }
    route_market_tick(market_msg, itch_producer, mtbt_producer);
  }

  drain_signals(ouch_consumer, nnf_consumer, sbe_consumer, 16);
  drain_gateway(gateway, 16);

  std::size_t ouch_confirmed = 0;
  std::size_t nnf_confirmed = 0;
  for (std::size_t i = 0; i < 16; ++i) {
    hft::proto::TaggedMessage confirm{};
    if (!confirm_queue.try_pop(confirm)) {
      break;
    }
    if (confirm.kind == hft::proto::Kind::ouch_confirm) {
      ++ouch_confirmed;
    } else if (confirm.kind == hft::proto::Kind::nnf_confirm) {
      ++nnf_confirmed;
    }
  }

  std::printf(
      "strategy ok: itch_signals=%llu mtbt_signals=%llu "
      "ouch_submitted=%llu nnf_submitted=%llu ouch_confirmed=%zu nnf_confirmed=%zu\n",
      static_cast<unsigned long long>(itch_producer.signals_emitted()),
      static_cast<unsigned long long>(mtbt_producer.signals_emitted()),
      static_cast<unsigned long long>(ouch_consumer.orders_submitted()),
      static_cast<unsigned long long>(nnf_consumer.orders_submitted()),
      ouch_confirmed, nnf_confirmed);

  hft::MPMC<kQueueCap> sbe_market_queue;
  hft::MPMC<kQueueCap> sbe_signal_queue;
  hft::MPMC<kQueueCap> sbe_pending_queue;
  hft::MPMC<kQueueCap> sbe_confirm_queue;

  hft::trading::SbeBboProducer<kQueueCap, kQueueCap> sbe_producer(
      sbe_market_queue, sbe_signal_queue);
  hft::trading::SbeOrderEntryConsumer<kQueueCap, kQueueCap> sbe_order_consumer(
      sbe_signal_queue, sbe_pending_queue, 1001);
  hft::trading::ExchangeGateway<kQueueCap, kQueueCap> sbe_gateway(
      sbe_pending_queue, sbe_confirm_queue);

  const auto bbo_ref = make_sbe_bbo(6500000, 6500100, -2);
  const auto bbo_buy = make_sbe_bbo(6400000, 6400100, -2);

  hft::proto::TaggedMessage bbo_msg{};
  bbo_msg.kind = hft::proto::Kind::sbe_market;
  std::memcpy(bbo_msg.bytes.data(), &bbo_ref, sizeof(bbo_ref));
  sbe_market_queue.push(bbo_msg);

  hft::proto::TaggedMessage bbo_msg2{};
  bbo_msg2.kind = hft::proto::Kind::sbe_market;
  std::memcpy(bbo_msg2.bytes.data(), &bbo_buy, sizeof(bbo_buy));
  sbe_market_queue.push(bbo_msg2);

  for (std::size_t i = 0; i < 8; ++i) {
    hft::proto::TaggedMessage tick{};
    if (!sbe_market_queue.try_pop(tick)) {
      break;
    }
    if (tick.kind == hft::proto::Kind::sbe_market) {
      hft::proto::sbe::BestObRpi decoded{};
      std::memcpy(&decoded, tick.bytes.data(), sizeof(decoded));
      sbe_producer.on_tick(decoded);
    }
  }

  for (std::size_t i = 0; i < 8; ++i) {
    sbe_order_consumer.poll_once();
  }
  drain_gateway(sbe_gateway, 8);

  std::size_t sbe_confirmed = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    hft::proto::TaggedMessage confirm{};
    if (!sbe_confirm_queue.try_pop(confirm)) {
      break;
    }
    if (confirm.kind == hft::proto::Kind::sbe_fast_order) {
      ++sbe_confirmed;
    }
  }

  std::array<std::byte, 512> wire{};
  const std::size_t wire_len =
      hft::proto::sbe::encode_create_order_req(
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
  const bool req_ok =
      hft::proto::sbe::decode_create_order_req(wire.data(), wire_len, decoded_req);

  hft::proto::sbe::FastOrderResp fast{};
  fast.body.category = 1;
  fast.body.side = 1;
  fast.body.price = 65000;
  fast.body.leaves_qty = 10;
  fast.body.symbol_id = 1001;
  fast.order_id_len = 3;
  std::memcpy(fast.order_id, "oid", 3);

  std::array<std::byte, 256> fast_wire{};
  const std::size_t fast_wire_len = hft::proto::sbe::encode_fast_order_resp(
      fast, fast_wire.data(), fast_wire.size());

  hft::proto::sbe::FastOrderResp decoded_fast{};
  const bool fast_ok = hft::proto::sbe::decode_fast_order_resp(
      fast_wire.data(), fast_wire_len, decoded_fast);

  std::printf(
      "sbe ok: bbo_signals=%llu submitted=%llu confirmed=%zu req_decode=%d "
      "fast_decode=%d symbol_id=%lld\n",
      static_cast<unsigned long long>(sbe_producer.signals_emitted()),
      static_cast<unsigned long long>(sbe_order_consumer.orders_submitted()),
      sbe_confirmed, req_ok ? 1 : 0, fast_ok ? 1 : 0,
      static_cast<long long>(decoded_req.symbol_id));

  hft::mpmc::Runtime<kQueueCap> runtime(market_queue);
  runtime.start(12345);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  runtime.stop();

  std::printf("mpmc runtime ok: pushed=%llu popped=%llu live=%zu\n",
              static_cast<unsigned long long>(runtime.pushed()),
              static_cast<unsigned long long>(runtime.popped()),
              runtime.live_threads());
  return 0;
}
