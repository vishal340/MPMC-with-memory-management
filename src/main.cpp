#include <arch.hpp>
#include <feed_reader.hpp>
#include <frame.hpp>
#include <lz4_codec.hpp>
#include <memory_pool.hpp>
#include <mpmc.hpp>
#include <mtbt_decode.hpp>
#include <protocols.hpp>
#include <shared_inlet_map.hpp>

#include <array>
#include <cstdio>
#include <cstring>
#include <new>

namespace {

void append_itch_add_order(std::array<std::byte, hft::frame::kCapacity>& frame,
                           std::size_t& offset, std::uint16_t stock_locate,
                           std::uint32_t shares) {
  hft::proto::ItchAddOrder body{};
  body.stock_locate = stock_locate;
  body.shares = shares;
  std::memcpy(body.stock, "AAPL    ", 8);

  const auto body_len = static_cast<std::uint16_t>(hft::proto::ItchAddOrder::kWireSize);
  frame[offset++] = static_cast<std::byte>(body_len >> 8);
  frame[offset++] = static_cast<std::byte>(body_len & 0xFF);
  std::memcpy(frame.data() + offset, &body, sizeof(body));
  offset += sizeof(body);
}

void append_mtbt_order(std::array<std::byte, hft::frame::kCapacity>& frame,
                       std::size_t& offset, std::int16_t stream_id,
                       std::int32_t token, std::int32_t quantity) {
  hft::proto::MtbtStreamHeader header{};
  header.msg_len =
      static_cast<std::int16_t>(hft::proto::kMtbtOrderWireSize);
  header.stream_id = stream_id;
  header.seq_no = static_cast<std::int32_t>(offset);

  hft::proto::MtbtNewOrder body{};
  body.token = token;
  body.quantity = quantity;
  body.order_type = 'B';

  std::memcpy(frame.data() + offset, &header, sizeof(header));
  offset += sizeof(header);
  std::memcpy(frame.data() + offset, &body, sizeof(body));
  offset += sizeof(body);
}

hft::proto::ItchAddOrder as_itch(const hft::proto::TaggedMessage& tagged) {
  hft::proto::ItchAddOrder msg{};
  std::memcpy(&msg, tagged.bytes.data(), sizeof(msg));
  return msg;
}

hft::proto::MtbtNewOrder as_mtbt(const hft::proto::TaggedMessage& tagged) {
  hft::proto::MtbtNewOrder msg{};
  std::memcpy(&msg, tagged.bytes.data(), sizeof(msg));
  return msg;
}

} // namespace

int main() {
  std::printf("target arch: %s (x86_64=%d arm64=%d page=%zu)\n",
              hft::arch::name, hft::arch::is_x86_64, hft::arch::is_arm64,
              hft::arch::page_size);

  hft::mem::ProtocolArena<1024, 1024, 1024, 256> arena;
  hft::MPMC<4096> queue;

  hft::os::Region inlet_region;
  hft::feed::SharedInlet* inlet = hft::feed::map_inlet(inlet_region);
  new (inlet) hft::feed::SharedInlet{};

  int queue_slot = 0;
  hft::feed::Reader<1024, 1024, 1024, 256, 4096> reader;
  reader.start(inlet, arena, queue, queue_slot);

  std::array<std::byte, hft::frame::kCapacity> itch_frame{};
  std::size_t itch_len = 0;
  append_itch_add_order(itch_frame, itch_len, 10, 100);
  append_itch_add_order(itch_frame, itch_len, 20, 200);

  hft::feed::publish(*inlet, hft::feed::Kind::itch, hft::feed::InletFlags::none,
                     itch_frame.data(), itch_len);
  hft::feed::wait_until_processed(*inlet, 1);

  std::array<std::byte, hft::frame::kCapacity> mtbt_frame{};
  std::size_t mtbt_len = 0;
  append_mtbt_order(mtbt_frame, mtbt_len, 1, 111, 10);
  append_mtbt_order(mtbt_frame, mtbt_len, 2, 222, 20);

  hft::feed::publish(*inlet, hft::feed::Kind::mtbt, hft::feed::InletFlags::none,
                     mtbt_frame.data(), mtbt_len);
  hft::feed::wait_until_processed(*inlet, 2);

  std::array<std::byte, hft::frame::kCapacity> mtbt_plain{};
  std::size_t mtbt_plain_len = 0;
  append_mtbt_order(mtbt_plain, mtbt_plain_len, 3, 333, 30);

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

  const auto itch_a = as_itch(queue.pop(0));
  const auto itch_b = as_itch(queue.pop(1));
  const auto mtbt_a = as_mtbt(queue.pop(2));
  const auto mtbt_b = as_mtbt(queue.pop(3));
  const auto mtbt_c = as_mtbt(queue.pop(4));

  if (itch_a.stock_locate != 10 || itch_b.stock_locate != 20) {
    return 3;
  }
  if (mtbt_a.token != 111 || mtbt_b.token != 222 || mtbt_c.token != 333) {
    return 4;
  }

  std::printf("feed reader ok: itch=%u/%u mtbt=%d/%d/%d lz4=%s\n",
              itch_a.stock_locate, itch_b.stock_locate, mtbt_a.token, mtbt_b.token,
              mtbt_c.token, compressed_len < mtbt_plain_len ? "yes" : "no");
  return 0;
}
