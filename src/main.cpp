#include <arch.hpp>
#include <memory_pool.hpp>
#include <mpmc.hpp>
#include <mtbt_decode.hpp>
#include <protocols.hpp>

#include <array>
#include <cstdio>
#include <cstring>

int main() {
  std::printf("target arch: %s (x86_64=%d arm64=%d page=%zu)\n",
              hft::arch::name, hft::arch::is_x86_64, hft::arch::is_arm64,
              hft::arch::page_size);

  hft::mem::ProtocolArena<1024, 1024, 1024, 256> arena;

  hft::MPMC<4096> queue;

  auto* itch = arena.acquire_itch();
  if (itch == nullptr) {
    return 1;
  }
  itch->stock_locate = 42;
  itch->shares = 100;
  std::memcpy(itch->stock, "AAPL    ", 8);

  hft::proto::TaggedMessage tagged{};
  arena.copy_to_tagged(*itch, tagged);
  arena.release(itch);

  queue.push(0, tagged);
  const hft::proto::TaggedMessage out = queue.pop(0);

  if (out.kind != hft::proto::Kind::itch) {
    return 2;
  }

  hft::proto::ItchAddOrder decoded{};
  std::memcpy(&decoded, out.bytes.data(), sizeof(decoded));
  if (decoded.stock_locate != 42 || decoded.shares != 100) {
    return 3;
  }

  std::printf("os-backed pool + mpmc ok: itch locate=%u shares=%u\n",
              decoded.stock_locate, decoded.shares);

  std::array<std::byte, hft::proto::kMtbtOrderWireSize> wire{};
  hft::proto::MtbtStreamHeader hdr{};
  hdr.msg_len = static_cast<std::int16_t>(hft::proto::kMtbtOrderWireSize);
  hdr.stream_id = 7;
  hdr.seq_no = 99;
  std::memcpy(wire.data(), &hdr, sizeof(hdr));

  hft::proto::MtbtNewOrder wire_body{};
  wire_body.token = 12345;
  wire_body.quantity = 50;
  wire_body.order_type = 'B';
  std::memcpy(wire.data() + hft::proto::MtbtStreamHeader::kWireSize, &wire_body,
              sizeof(wire_body));

  hft::proto::MtbtDecodedOrder mtbt{};
  if (!hft::proto::decode_mtbt_order(wire.data(), wire.size(), mtbt)) {
    return 4;
  }
  if (mtbt.header.stream_id != 7 || mtbt.order.token != 12345 ||
      mtbt.order.quantity != 50) {
    return 5;
  }

  auto* mtbt_slot = arena.acquire_mtbt();
  if (mtbt_slot == nullptr) {
    return 6;
  }
  *mtbt_slot = mtbt.order;
  arena.release(mtbt_slot);

  std::printf("mtbt decode ok: stream=%d token=%d qty=%d\n", mtbt.header.stream_id,
              mtbt.order.token, mtbt.order.quantity);
  return 0;
}
