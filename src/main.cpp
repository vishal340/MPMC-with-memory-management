#include <core/arch.hpp>
#include <trading/consumer.hpp>
#include <feed/feed_reader.hpp>
#include <feed/frame.hpp>
#include <codec/lz4_codec.hpp>
#include <mem/memory_pool.hpp>
#include <mpmc/mpmc_runtime.hpp>
#include <proto/mtbt_decode.hpp>
#include <trading/producer.hpp>
#include <proto/protocols.hpp>
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
                   std::size_t rounds) {
  for (std::size_t i = 0; i < rounds; ++i) {
    ouch.poll_once();
    nnf.poll_once();
  }
}

} // namespace

int main() {
  std::printf("target arch: %s (x86_64=%d arm64=%d page=%zu)\n",
              hft::arch::name, hft::arch::is_x86_64, hft::arch::is_arm64,
              hft::arch::page_size);

  hft::mem::ProtocolArena<1024, 1024, 1024, 256> arena;
  hft::MPMC<kQueueCap> market_queue;
  hft::MPMC<kQueueCap> signal_queue;
  hft::MPMC<kQueueCap> order_queue;

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
                                                                 order_queue);
  hft::trading::NnfConsumer<kQueueCap, kQueueCap> nnf_consumer(signal_queue,
                                                               order_queue);

  for (std::size_t i = 0; i < 16; ++i) {
    hft::proto::TaggedMessage market_msg{};
    if (!market_queue.try_pop(market_msg)) {
      break;
    }
    route_market_tick(market_msg, itch_producer, mtbt_producer);
  }

  drain_signals(ouch_consumer, nnf_consumer, 16);

  std::size_t ouch_orders = 0;
  std::size_t nnf_orders = 0;
  for (std::size_t i = 0; i < 16; ++i) {
    hft::proto::TaggedMessage order{};
    if (!order_queue.try_pop(order)) {
      break;
    }
    if (order.kind == hft::proto::Kind::ouch) {
      ++ouch_orders;
    } else if (order.kind == hft::proto::Kind::nnf) {
      ++nnf_orders;
    }
  }

  std::printf(
      "strategy ok: itch_signals=%llu mtbt_signals=%llu ouch=%zu nnf=%zu\n",
      static_cast<unsigned long long>(itch_producer.signals_emitted()),
      static_cast<unsigned long long>(mtbt_producer.signals_emitted()),
      ouch_orders, nnf_orders);

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
